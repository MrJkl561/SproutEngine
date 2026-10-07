#include "script_checker.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

#include "platform.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace s2d {

// What the reader thread (clangd's answers) and compiler checks share with the editor.
struct ScriptChecker::Shared {
    std::mutex mutex;       // everything below
    std::mutex writeMutex;  // writing to clangd
    std::shared_ptr<platform::ChildProcess> process;
    bool ready = false;              // clangd answered "initialize"
    std::vector<std::string> legend;  // clangd's names for token kinds
    std::map<std::string, Result> results;  // by file key
    // The text sent for each (file, LSP version), to turn clangd's positions into bytes
    struct Sent {
        long docVersion;
        std::string text;
    };
    std::map<std::string, std::map<long, Sent>> sent;
    std::map<int, std::pair<std::string, long>> tokenRequests;  // id -> (file key, LSP version)
    std::atomic<bool> compilerRunning{false};

    void write(const std::string& body) {
        std::lock_guard<std::mutex> lock(writeMutex);
        if (process) process->write("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
    }
};

namespace {

std::string keyOf(const fs::path& p) {
    std::error_code ec;
    std::string k = fs::weakly_canonical(fs::absolute(p, ec), ec).generic_string();
#if defined(_WIN32)
    for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
    return k;
}

std::string uriOf(const fs::path& p) {
    std::error_code ec;
    std::string path = fs::absolute(p, ec).generic_string();
    std::string out = "file://";
#if defined(_WIN32)
    out += "/";
#endif
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : path) {
        if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string pathOfUri(const std::string& uri) {
    std::string s = uri.rfind("file://", 0) == 0 ? uri.substr(7) : uri;
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
#if defined(_WIN32)
    if (out.size() > 2 && out[0] == '/' && out[2] == ':') out.erase(0, 1);
#endif
    return out;
}

std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines{""};
    for (char c : text) {
        if (c == '\n') lines.emplace_back();
        else if (c != '\r') lines.back() += c;
    }
    return lines;
}

// LSP counts columns in UTF-16 units; the editor in bytes.
int byteColumn(const std::vector<std::string>& lines, int line, int utf16) {
    if (line < 0 || line >= static_cast<int>(lines.size())) return 0;
    const std::string& s = lines[line];
    int units = 0;
    size_t i = 0;
    while (i < s.size() && units < utf16) {
        unsigned char c = s[i];
        int bytes = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        units += bytes == 4 ? 2 : 1;
        i += bytes;
    }
    return static_cast<int>(std::min(i, s.size()));
}

std::string firstLine(const std::string& s) {
    std::string out = s.substr(0, s.find('\n'));
    while (!out.empty() && (out.back() == ' ' || out.back() == '\r')) out.pop_back();
    return out;
}

bool kindFor(const std::string& name, ScriptChecker::TokenKind& kind) {
    using K = ScriptChecker::TokenKind;
    static const std::map<std::string, K> kinds = {
        {"namespace", K::Namespace}, {"type", K::Type},         {"class", K::Type},
        {"enum", K::Type},           {"interface", K::Type},    {"struct", K::Type},
        {"typeParameter", K::Type},  {"concept", K::Type},      {"parameter", K::Parameter},
        {"property", K::Member},     {"enumMember", K::EnumMember}, {"function", K::Function},
        {"method", K::Function},     {"macro", K::Macro},       {"comment", K::Comment}};
    auto it = kinds.find(name);
    if (it == kinds.end()) return false;
    kind = it->second;
    return true;
}

// clangd's messages, read on a thread of their own until it ends.
void readClangd(std::shared_ptr<ScriptChecker::Shared> sh) {
    std::shared_ptr<platform::ChildProcess> process = sh->process;
    std::string buffer;
    char chunk[16384];
    for (;;) {
        // One message: "Content-Length: N\r\n\r\n" + N bytes of JSON
        size_t headerEnd;
        while ((headerEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
            size_t n = process->read(chunk, sizeof chunk);
            if (n == 0) return;
            buffer.append(chunk, n);
        }
        size_t length = 0;
        std::string header = buffer.substr(0, headerEnd);
        if (size_t p = header.find("Content-Length:"); p != std::string::npos)
            length = std::stoul(header.substr(p + 15));
        while (buffer.size() < headerEnd + 4 + length) {
            size_t n = process->read(chunk, sizeof chunk);
            if (n == 0) return;
            buffer.append(chunk, n);
        }
        json msg = json::parse(buffer.substr(headerEnd + 4, length), nullptr, false);
        buffer.erase(0, headerEnd + 4 + length);
        if (!msg.is_object()) continue;

        if (msg.contains("method") && msg.contains("id")) {
            // A question from clangd: we have nothing to say
            sh->write(json{{"jsonrpc", "2.0"}, {"id", msg["id"]}, {"result", nullptr}}.dump());
            continue;
        }
        if (msg.value("id", json()) == json(1)) {  // initialize
            std::vector<std::string> legend;
            try {
                for (auto& t : msg.at("result").at("capabilities").at("semanticTokensProvider").at("legend").at("tokenTypes"))
                    legend.push_back(t.get<std::string>());
            } catch (const json::exception&) {
            }
            sh->write(json{{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", json::object()}}.dump());
            std::lock_guard<std::mutex> lock(sh->mutex);
            sh->legend = std::move(legend);
            sh->ready = true;
            continue;
        }
        std::string method = msg.value("method", "");
        if (method == "textDocument/publishDiagnostics") {
            const json& params = msg["params"];
            std::string key = keyOf(pathOfUri(params.value("uri", "")));
            std::lock_guard<std::mutex> lock(sh->mutex);
            auto sentIt = sh->sent.find(key);
            if (sentIt == sh->sent.end() || sentIt->second.empty()) continue;
            long lspVersion = params.contains("version") && params["version"].is_number() ? params["version"].get<long>()
                                                                                            : sentIt->second.rbegin()->first;
            auto textIt = sentIt->second.find(lspVersion);
            if (textIt == sentIt->second.end()) continue;  // an older text: never mind
            std::vector<std::string> lines = linesOf(textIt->second.text);
            ScriptChecker::Result& r = sh->results[key];
            if (r.version != textIt->second.docVersion) r = ScriptChecker::Result{};
            r.version = textIt->second.docVersion;
            r.hasDiagnostics = true;
            r.diagnostics.clear();
            for (const json& d : params.value("diagnostics", json::array())) {
                int severity = d.value("severity", 1);
                if (severity > 2) continue;  // information and hints
                const json& range = d["range"];
                int line = range["start"].value("line", 0), endLine = range["end"].value("line", line);
                Diagnostic diag;
                diag.line = line + 1;
                diag.column = byteColumn(lines, line, range["start"].value("character", 0)) + 1;
                diag.endLine = endLine + 1;
                diag.endColumn = byteColumn(lines, endLine, range["end"].value("character", 0)) + 1;
                diag.error = severity == 1;
                diag.message = firstLine(d.value("message", ""));
                if (size_t fix = diag.message.rfind(" (fix available)"); fix != std::string::npos) diag.message.erase(fix);
                r.diagnostics.push_back(diag);
            }
            continue;
        }
        if (msg.contains("id") && msg["id"].is_number_integer()) {
            int id = msg["id"].get<int>();
            std::lock_guard<std::mutex> lock(sh->mutex);
            auto req = sh->tokenRequests.find(id);
            if (req == sh->tokenRequests.end()) continue;
            auto [key, lspVersion] = req->second;
            sh->tokenRequests.erase(req);
            auto sentIt = sh->sent.find(key);
            if (sentIt == sh->sent.end()) continue;
            auto textIt = sentIt->second.find(lspVersion);
            if (textIt == sentIt->second.end() || !msg.contains("result") || !msg["result"].is_object()) continue;
            std::vector<std::string> lines = linesOf(textIt->second.text);
            ScriptChecker::Result& r = sh->results[key];
            if (r.version != textIt->second.docVersion) r = ScriptChecker::Result{};
            r.version = textIt->second.docVersion;
            r.hasTokens = true;
            r.tokens.clear();
            const json& data = msg["result"].value("data", json::array());
            int line = 0, start = 0;
            for (size_t i = 0; i + 5 <= data.size(); i += 5) {
                int dLine = data[i].get<int>(), dStart = data[i + 1].get<int>(), len = data[i + 2].get<int>();
                int type = data[i + 3].get<int>();
                line += dLine;
                start = dLine ? dStart : start + dStart;
                ScriptChecker::TokenKind kind;
                if (type < 0 || type >= static_cast<int>(sh->legend.size()) || !kindFor(sh->legend[type], kind)) continue;
                ScriptChecker::Token t;
                t.line = line;
                t.start = byteColumn(lines, line, start);
                t.end = byteColumn(lines, line, start + len);
                t.kind = kind;
                r.tokens.push_back(t);
            }
        }
    }
}

}  // namespace

ScriptChecker::ScriptChecker(Setup setup) : setup_(std::move(setup)), shared_(std::make_shared<Shared>()) {
    std::error_code ec;
    if (!setup_.clangd.empty() && fs::exists(setup_.clangd, ec)) startClangd();
    if (!clangd_ && !setup_.compiler.empty() && setup_.compilerId != "MSVC" && fs::exists(setup_.compiler, ec))
        engine_ = "compiler";
}

ScriptChecker::~ScriptChecker() {
    // Don't wait for clangd: stop it, and let the reader thread see it end.
    if (clangd_) {
        shared_->write(json{{"jsonrpc", "2.0"}, {"method", "exit"}}.dump());
        clangd_->terminate();
    }
    if (reader_.joinable()) reader_.detach();
}

std::string ScriptChecker::engine() const { return engine_; }

bool ScriptChecker::busy() const {
    for (auto& [key, doc] : docs_)
        if (doc.pending) return true;
    if (shared_->compilerRunning) return true;
    std::lock_guard<std::mutex> lock(shared_->mutex);
    return !shared_->tokenRequests.empty();
}

void ScriptChecker::writeCompileCommands() const {
    fs::path dir = setup_.workDir / "clangd";
    std::error_code ec;
    fs::create_directories(dir, ec);
    std::vector<std::string> includes = {setup_.editorSrc.string(), (setup_.depsDir / "raylib-src" / "src").string(),
                                         (setup_.depsDir / "raylib_cpp-src" / "include").string(),
                                         setup_.scriptsDir.string()};
    std::vector<fs::path> files;
    for (fs::directory_iterator it(setup_.scriptsDir, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".cpp") files.push_back(it->path());
    if (files.empty()) files.push_back(setup_.scriptsDir / "script.cpp");  // so new files get the same settings
    json db = json::array();
    for (const fs::path& f : files) {
        json args = json::array();
        if (setup_.compilerId == "MSVC") {
            // Visual Studio's headers, read the way cl.exe would
            args = {"clang-cl.exe", "/std:c++20", "/EHsc", "/utf-8", "/permissive-"};
            for (auto& i : includes) args.push_back("/I" + i);
        } else {
            args = {setup_.compiler.string(), "-std=c++20"};
            for (auto& i : includes) args.push_back("-I" + i);
        }
        args.push_back("-c");
        args.push_back(f.string());
        db.push_back({{"directory", setup_.scriptsDir.string()}, {"file", f.string()}, {"arguments", args}});
    }
    std::ofstream(dir / "compile_commands.json") << db.dump(2);
}

void ScriptChecker::startClangd() {
    writeCompileCommands();
    std::vector<std::string> args = {setup_.clangd.string(),
                                     "--compile-commands-dir=" + (setup_.workDir / "clangd").string(),
                                     "--background-index=false",
                                     "--header-insertion=never",
                                     "--clang-tidy=false",
                                     "--pch-storage=memory",
                                     "--log=error",
                                     "-j=2"};
    // Ask the real compiler where its standard headers are (macOS, Linux)
    if (setup_.compilerId != "MSVC" && !setup_.compiler.empty())
        args.push_back("--query-driver=" + setup_.compiler.string());
    std::string error;
    auto process = platform::startProcess(args, error);
    if (!process) return;
    // Shared with the reader thread: whichever is done last frees it.
    clangd_ = std::shared_ptr<platform::ChildProcess>(std::move(process));
    shared_->process = clangd_;
    engine_ = "clangd";

    json init = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "initialize"},
        {"params",
         {{"processId", platform::processId()},
          {"rootUri", uriOf(setup_.scriptsDir)},
          {"capabilities",
           {{"textDocument",
             {{"publishDiagnostics", {{"versionSupport", true}}},
              {"semanticTokens",
               {{"requests", {{"full", true}}},
                {"tokenTypes", {"namespace", "type", "class", "enum", "interface", "struct", "typeParameter",
                                "parameter", "variable", "property", "enumMember", "function", "method", "macro",
                                "comment", "concept"}},
                {"tokenModifiers", json::array()},
                {"formats", {"relative"}}}}}}}}}}};
    shared_->write(init.dump());
    reader_ = std::thread(readClangd, shared_);
}

void ScriptChecker::update(const fs::path& file, const std::string& text, long version) {
    if (engine_.empty()) return;
    std::string key = keyOf(file);
    Doc& d = docs_[key];
    if (d.uri.empty()) {
        d.path = file;
        d.uri = uriOf(file);
    }
    if (d.opened && d.text == text) return;
    d.text = text;
    d.version = version;
    d.pending = true;
    d.changedAt = -1;  // set by poll()
}

void ScriptChecker::close(const fs::path& file) {
    std::string key = keyOf(file);
    auto it = docs_.find(key);
    if (it == docs_.end()) return;
    if (clangd_ && it->second.opened)
        shared_->write(json{{"jsonrpc", "2.0"},
                            {"method", "textDocument/didClose"},
                            {"params", {{"textDocument", {{"uri", it->second.uri}}}}}}
                           .dump());
    docs_.erase(it);
    std::lock_guard<std::mutex> lock(shared_->mutex);
    shared_->results.erase(key);
    shared_->sent.erase(key);
}

void ScriptChecker::poll(double now) {
    if (engine_.empty()) return;
    if (clangd_) {
        std::lock_guard<std::mutex> lock(shared_->mutex);
        if (!shared_->ready) return;
    }
    for (auto& [key, d] : docs_) {
        if (!d.pending) continue;
        if (d.changedAt < 0) d.changedAt = now;
        // Wait for a pause in the typing (but check a newly opened file at once)
        if (d.opened && now - d.changedAt < 0.35) continue;
        if (!clangd_) {
            if (shared_->compilerRunning) continue;  // one at a time
            d.pending = false;
            d.opened = true;
            runCompilerCheck(key, d.version, d.text);
            continue;
        }
        static long lspVersion = 0;
        long v = ++lspVersion;
        {
            std::lock_guard<std::mutex> lock(shared_->mutex);
            auto& sent = shared_->sent[key];
            sent[v] = Shared::Sent{d.version, d.text};
            while (sent.size() > 4) sent.erase(sent.begin());
        }
        if (!d.opened) {
            shared_->write(json{{"jsonrpc", "2.0"},
                                {"method", "textDocument/didOpen"},
                                {"params",
                                 {{"textDocument", {{"uri", d.uri}, {"languageId", "cpp"}, {"version", v}, {"text", d.text}}}}}}
                               .dump());
            d.opened = true;
        } else {
            shared_->write(json{{"jsonrpc", "2.0"},
                                {"method", "textDocument/didChange"},
                                {"params",
                                 {{"textDocument", {{"uri", d.uri}, {"version", v}}},
                                  {"contentChanges", json::array({{{"text", d.text}}})}}}}
                               .dump());
        }
        int id = nextId_++;
        {
            std::lock_guard<std::mutex> lock(shared_->mutex);
            shared_->tokenRequests[id] = {key, v};
        }
        shared_->write(json{{"jsonrpc", "2.0"},
                            {"id", id},
                            {"method", "textDocument/semanticTokens/full"},
                            {"params", {{"textDocument", {{"uri", d.uri}}}}}}
                           .dump());
        d.pending = false;
    }
}

bool ScriptChecker::take(const fs::path& file, Result& out) {
    std::lock_guard<std::mutex> lock(shared_->mutex);
    auto it = shared_->results.find(keyOf(file));
    if (it == shared_->results.end() || (!it->second.hasDiagnostics && !it->second.hasTokens)) return false;
    out = std::move(it->second);
    shared_->results.erase(it);
    return true;
}

// Without clangd: the compiler checks a copy of the text (errors only, no colors).
void ScriptChecker::runCompilerCheck(const std::string& key, long version, const std::string& text) {
    fs::path dir = setup_.workDir / "check";
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path copy = dir / fs::path(key).filename();
    std::ofstream(copy, std::ios::binary) << text;
    auto q = [](const fs::path& p) { return "\"" + p.string() + "\""; };
    std::string cmd = q(setup_.compiler) + " -fsyntax-only -std=c++20 -x c++";
    if (setup_.compilerId == "GNU") cmd += " -fdiagnostics-column-unit=byte -fno-diagnostics-show-caret";
    for (const fs::path& i : {setup_.scriptsDir, setup_.editorSrc, setup_.depsDir / "raylib-src" / "src",
                              setup_.depsDir / "raylib_cpp-src" / "include"})
        cmd += " -I" + q(i);
    cmd += " " + q(copy);
    shared_->compilerRunning = true;
    std::thread([sh = shared_, cmd, key, version, name = copy.filename().string()] {
        std::string output;
        platform::runCommand(cmd, output);
        static const std::regex re(R"(^(.*?):(\d+):(\d+): (fatal error|error|warning): (.*)$)");
        Result r;
        r.version = version;
        r.hasDiagnostics = true;
        std::stringstream lines(output);
        for (std::string line; std::getline(lines, line);) {
            std::smatch m;
            if (!std::regex_match(line, m, re)) continue;
            if (fs::path(m[1].str()).filename().string() != name) continue;  // in a header
            Diagnostic d;
            d.line = std::stoi(m[2].str());
            d.column = std::stoi(m[3].str());
            d.error = m[4].str() != "warning";
            d.message = m[5].str();
            r.diagnostics.push_back(d);
        }
        std::lock_guard<std::mutex> lock(sh->mutex);
        sh->results[key] = std::move(r);
        sh->compilerRunning = false;
    }).detach();
}

}  // namespace s2d
