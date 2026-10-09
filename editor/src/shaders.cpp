#include "shaders.hpp"

#include <rlgl.h>

#include <chrono>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

#include "assets.hpp"
#include "nodes.hpp"
#include "script.hpp"  // print()

namespace fs = std::filesystem;

namespace s2d {

namespace {

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Entry {
    // The file
    fs::file_time_type mtime{};
    double checkedAt = -100;
    bool readable = false;
    std::vector<ShaderUniform> uniforms;
    std::string body;  // the file with its uniforms made plain GLSL (same lines)
    // On the graphics card
    bool compiled = false, ok = false, told = false;
    ::Shader shader{};
    std::vector<int> locations;  // one per uniform
    int timeLoc = -1, pixelLoc = -1;
    std::vector<ShaderProblem> problems;
};

std::map<std::string, Entry>& entries() {
    static auto* map = new std::map<std::string, Entry>;  // (never freed: shared with the scripts library)
    return *map;
}

std::vector<float> numbersIn(const std::string& text) {
    std::vector<float> out;
    static const std::regex number(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)");
    for (auto m = std::sregex_iterator(text.begin(), text.end(), number); m != std::sregex_iterator(); ++m)
        out.push_back(std::stof(m->str()));
    return out;
}

// Reads the uniforms (with their defaults and hints) and turns them into
// plain GLSL declarations, keeping every line where it was.
void parse(const std::string& source, std::vector<ShaderUniform>& uniforms, std::string& body) {
    uniforms.clear();
    body.clear();
    static const std::regex uniformRe(R"(uniform\s+(\w+)\s+(\w+)\s*(?::\s*([^=;]*?))?\s*(?:=\s*([^;]*?))?\s*;)");
    size_t last = 0;
    for (auto m = std::sregex_iterator(source.begin(), source.end(), uniformRe); m != std::sregex_iterator(); ++m) {
        std::string whole = m->str();
        body += source.substr(last, m->position() - last);
        body += "uniform " + (*m)[1].str() + " " + (*m)[2].str() + ";";
        for (char c : whole)
            if (c == '\n') body += '\n';  // (a declaration over several lines keeps its lines)
        last = m->position() + m->length();

        ShaderUniform u;
        u.glslType = (*m)[1].str();
        u.name = (*m)[2].str();
        std::string hint = (*m)[3].str(), value = (*m)[4].str();
        const std::string& t = u.glslType;
        if (t == "float") u.kind = ShaderUniform::Float;
        else if (t == "int") u.kind = ShaderUniform::Int;
        else if (t == "bool") u.kind = ShaderUniform::Bool;
        else if (t == "vec2") u.kind = ShaderUniform::Vec2;
        else if (t == "vec3" || t == "vec4") u.kind = ShaderUniform::Color;
        else if (t == "sampler2D") u.kind = ShaderUniform::Texture;
        else continue;  // other types work in the shader, but aren't shown in the Inspector
        if (u.kind == ShaderUniform::Int) u.step = 0.2f;
        if (u.kind == ShaderUniform::Vec2) u.step = 0.1f;
        if (hint.rfind("hint_range", 0) == 0) {
            std::vector<float> r = numbersIn(hint);
            if (r.size() >= 3 && r[2] > 0) u.step = r[2];
            else if (r.size() >= 2 && r[1] > r[0]) u.step = (r[1] - r[0]) / 100;
        }
        if (u.kind == ShaderUniform::Bool) {
            u.value[0] = value.find("true") != std::string::npos ? 1.0f : 0.0f;
        } else if (u.kind == ShaderUniform::Texture) {
            // (a picture is picked in the Inspector)
        } else if (!value.empty()) {
            // vec4(1.0, 0.5, ...): the numbers in the brackets (not the 4 of "vec4")
            size_t open = value.find('(');
            std::vector<float> n = numbersIn(open == std::string::npos ? value : value.substr(open + 1));
            if (n.size() == 1) {
                for (float& v : u.value) v = n[0];  // vec4(1.0): all four
                if (u.kind == ShaderUniform::Color && t == "vec3") u.value[3] = 1;
            } else {
                for (size_t i = 0; i < n.size() && i < 4; ++i) u.value[i] = n[i];
            }
        } else if (u.kind == ShaderUniform::Color) {
            u.value[0] = u.value[1] = u.value[2] = u.value[3] = 1;  // white
        }
        uniforms.push_back(u);
    }
    body += source.substr(last);
}

// The GLSL the graphics card gets: a header for this OpenGL version, the
// file (from line 1), and a main() around its fragment().
std::string glsl(const std::string& body) {
    int version = rlGetVersion();
    bool es = version == RL_OPENGL_ES_20 || version == RL_OPENGL_ES_30;
    bool old = es || version == RL_OPENGL_11 || version == RL_OPENGL_21;
    std::string s;
    if (es) s = "#version 100\nprecision mediump float;\n";
    else if (old) s = "#version 120\n";
    else s = "#version 330\n";
    if (old) {
        s += "varying vec2 fragTexCoord;\nvarying vec4 fragColor;\n#define texture texture2D\n";
    } else {
        s += "in vec2 fragTexCoord;\nin vec4 fragColor;\nout vec4 sproutFragColor;\n";
    }
    s += "uniform sampler2D texture0;\nuniform vec4 colDiffuse;\nuniform float TIME;\nuniform vec2 TEXTURE_PIXEL_SIZE;\n"
         "#define TEXTURE texture0\n#define UV fragTexCoord\n#define MODULATE (fragColor * colDiffuse)\n"
         "vec4 COLOR;\n#line 1\n";
    s += body;
    s += "\nvoid main() {\n    COLOR = texture(TEXTURE, UV) * MODULATE;\n    fragment();\n";
    s += old ? "    gl_FragColor = COLOR;\n}\n" : "    sproutFragColor = COLOR;\n}\n";
    return s;
}

// raylib's messages while compiling (the compiler's errors are among them)
std::string* gCaptured = nullptr;
void capture(int level, const char* text, va_list args) {
    char buf[4096];
    std::vsnprintf(buf, sizeof buf, text, args);
    if (gCaptured && level >= LOG_WARNING) *gCaptured += std::string(buf) + "\n";
    else if (level >= LOG_WARNING) std::fprintf(stderr, "%s\n", buf);
}

// The compiler's messages, as problems on the file's lines. Compilers say
// where differently: "0:12(5): error: ..." (Mesa), "ERROR: 0:12: ..." (most),
// "0(12) : error ..." (NVIDIA).
std::vector<ShaderProblem> problemsIn(const std::string& log, int fileLines) {
    std::vector<ShaderProblem> out;
    static const std::regex lineRe(R"((?:ERROR:\s*|WARNING:\s*)?\d+\s*[:(]\s*(\d+)\s*\)?(?:\((\d+)\))?\s*:\s*(.*))");
    std::stringstream ss(log);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find("SHADER:") != std::string::npos && line.find("Compile error:") == std::string::npos) continue;
        if (size_t p = line.find("Compile error:"); p != std::string::npos) line = line.substr(p + 14);
        std::smatch m;
        if (!std::regex_search(line, m, lineRe)) continue;
        if (m[3].str().find("warning") == 0) continue;
        ShaderProblem pr;
        pr.line = std::clamp(std::stoi(m[1].str()), 1, std::max(fileLines, 1));
        if (m[2].matched) pr.column = std::stoi(m[2].str());
        pr.message = m[3].str();
        while (!pr.message.empty() && (pr.message.back() == '\r' || pr.message.back() == ' ')) pr.message.pop_back();
        out.push_back(pr);
    }
    if (out.empty() && !log.empty()) out.push_back({fileLines, 0, "the shader doesn't compile: " + log});
    return out;
}

Entry* get(const std::string& path) {
    if (path.empty()) return nullptr;
    Entry& e = entries()[path];
    if (now() - e.checkedAt < 0.5) return e.readable ? &e : nullptr;
    e.checkedAt = now();
    fs::path file = Assets::projectDir() / path;
    std::error_code ec;
    auto mtime = fs::last_write_time(file, ec);
    if (ec) {
        e.readable = false;
        return nullptr;
    }
    if (e.readable && mtime == e.mtime) return &e;
    // New, or changed on disk: read it again (and compile it again when next drawn)
    std::ifstream in(file, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    e.mtime = mtime;
    e.readable = true;
    parse(ss.str(), e.uniforms, e.body);
    if (e.ok && e.shader.id > 0 && IsWindowReady()) UnloadShader(e.shader);
    e.shader = {};
    e.compiled = e.ok = e.told = false;
    e.problems.clear();
    return &e;
}

void compile(Entry& e, const std::string& path) {
    e.compiled = true;
    std::string log;
    gCaptured = &log;
    SetTraceLogCallback(capture);
    std::string code = glsl(e.body);
    ::Shader s = LoadShaderFromMemory(nullptr, code.c_str());
    SetTraceLogCallback(nullptr);
    gCaptured = nullptr;
    int lines = 1;
    for (char c : e.body) lines += c == '\n';
    e.ok = s.id > 0 && s.id != rlGetShaderIdDefault();
    if (!e.ok) {
        e.problems = problemsIn(log, lines);
        if (!e.told) {
            e.told = true;
            for (const ShaderProblem& p : e.problems)
                print("Shader " + path + ":" + std::to_string(p.line) + ": " + p.message);
        }
        return;
    }
    e.shader = s;
    e.locations.clear();
    for (const ShaderUniform& u : e.uniforms) e.locations.push_back(GetShaderLocation(s, u.name.c_str()));
    e.timeLoc = GetShaderLocation(s, "TIME");
    e.pixelLoc = GetShaderLocation(s, "TEXTURE_PIXEL_SIZE");
}

}  // namespace

const std::vector<ShaderUniform>* Shaders::uniforms(const std::string& path) {
    Entry* e = get(path);
    return e ? &e->uniforms : nullptr;
}

bool Shaders::begin(const Node2D& node) {
    if (node.shader.empty() || !IsWindowReady()) return false;
    Entry* e = get(node.shader);
    if (!e) return false;
    if (!e->compiled) compile(*e, node.shader);
    if (!e->ok) return false;
    float t = static_cast<float>(GetTime());
    if (e->timeLoc >= 0) SetShaderValue(e->shader, e->timeLoc, &t, SHADER_UNIFORM_FLOAT);
    if (e->pixelLoc >= 0) {
        Vector2 px{1, 1};
        if (auto* sprite = dynamic_cast<const Sprite2D*>(&node))
            if (raylib::Texture* tex = Assets::texture(sprite->texture))
                px = {1.0f / std::max(tex->width, 1), 1.0f / std::max(tex->height, 1)};
        SetShaderValue(e->shader, e->pixelLoc, &px, SHADER_UNIFORM_VEC2);
    }
    for (size_t i = 0; i < e->uniforms.size(); ++i) {
        const ShaderUniform& u = e->uniforms[i];
        int loc = e->locations[i];
        if (loc < 0) continue;  // (declared but not used: the compiler dropped it)
        auto it = node.shaderParams.find(u.name);
        const ShaderParam* p = it == node.shaderParams.end() ? nullptr : &it->second;
        switch (u.kind) {
            case ShaderUniform::Float: {
                float v = p ? p->f : u.value[0];
                SetShaderValue(e->shader, loc, &v, SHADER_UNIFORM_FLOAT);
                break;
            }
            case ShaderUniform::Int:
            case ShaderUniform::Bool: {
                int v = p ? (u.kind == ShaderUniform::Bool ? p->b : p->i) : static_cast<int>(u.value[0]);
                SetShaderValue(e->shader, loc, &v, SHADER_UNIFORM_INT);
                break;
            }
            case ShaderUniform::Vec2: {
                Vector2 v = p ? Vector2{p->v.x, p->v.y} : Vector2{u.value[0], u.value[1]};
                SetShaderValue(e->shader, loc, &v, SHADER_UNIFORM_VEC2);
                break;
            }
            case ShaderUniform::Color: {
                float v[4] = {u.value[0], u.value[1], u.value[2], u.value[3]};
                if (p) {
                    v[0] = p->c.r / 255.0f;
                    v[1] = p->c.g / 255.0f;
                    v[2] = p->c.b / 255.0f;
                    v[3] = p->c.a / 255.0f;
                }
                SetShaderValue(e->shader, loc, v, u.glslType == "vec3" ? SHADER_UNIFORM_VEC3 : SHADER_UNIFORM_VEC4);
                break;
            }
            case ShaderUniform::Texture: {
                std::string path = p ? p->texture : u.texture;
                if (raylib::Texture* tex = Assets::texture(path)) SetShaderValueTexture(e->shader, loc, *tex);
                break;
            }
        }
    }
    BeginShaderMode(e->shader);
    return true;
}

void Shaders::end() { EndShaderMode(); }

std::vector<ShaderProblem> Shaders::check(const std::string& path) {
    entries()[path].checkedAt = -100;  // (it was probably just saved: look at the file now)
    Entry* e = get(path);
    if (!e) return {{1, 0, "can't read " + path}};
    if (!IsWindowReady()) return {};
    if (!e->compiled) {
        e->told = true;  // (the caller shows them)
        compile(*e, path);
    }
    return e->problems;
}

void Shaders::clear() {
    for (auto& [path, e] : entries())
        if (e.ok && e.shader.id > 0 && IsWindowReady()) UnloadShader(e.shader);
    entries().clear();
}

const char* Shaders::templateText() {
    return R"(// A shader: what each pixel of a node looks like (like Godot's).
// Pick it under Shader in the Inspector (Sprite2D, AnimatedSprite2D,
// ColorRect, Label). Save it and the node updates straight away.
//
//   UV       where in the picture this pixel is, from (0, 0) to (1, 1)
//   COLOR    starts as the picture's colour; what you set is what's drawn
//   TEXTURE  the picture: texture(TEXTURE, UV) reads it
//   TIME     seconds since the start, for things that move
//
// The uniforms show up in the Inspector under Shader Parameters, and scripts
// can set them: self<Sprite2D>()->setShaderParam("wave_size", 0.05f);
uniform vec4 tint : source_color = vec4(1.0, 1.0, 1.0, 1.0);
uniform float wave_speed = 3.0;
uniform float wave_size : hint_range(0.0, 0.1) = 0.0;

void fragment() {
    vec2 uv = UV;
    uv.x += sin(uv.y * 20.0 + TIME * wave_speed) * wave_size;
    COLOR = texture(TEXTURE, uv) * MODULATE * tint;
}
)";
}

}  // namespace s2d
