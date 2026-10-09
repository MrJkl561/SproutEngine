// Custom shaders for nodes, like Godot's canvas_item shaders: a .shader file
// (in the project's shaders/ folder) with a fragment() function and uniform
// parameters, written once and turned into the right GLSL for desktop
// (OpenGL 3.3) and the web (WebGL):
//
//     uniform float speed = 2.0;
//     uniform vec4 tint : source_color = vec4(1.0, 0.8, 0.8, 1.0);
//     uniform float amount : hint_range(0.0, 0.1) = 0.02;
//
//     void fragment() {
//         vec2 uv = UV;
//         uv.x += sin(uv.y * 20.0 + TIME * speed) * amount;
//         COLOR = texture(TEXTURE, uv) * tint;
//     }
//
// Built in: UV (0..1 across the texture), COLOR (in: the texture's colour times
// the node's modulate; out: what's drawn), TEXTURE, TEXTURE_PIXEL_SIZE
// (1 / its size in pixels), TIME (seconds), MODULATE.
#pragma once

#include <raylib-cpp.hpp>

#include <string>
#include <vector>

namespace s2d {

class Node2D;

// One uniform parameter: shown in the Inspector under Shader Parameters.
struct ShaderUniform {
    enum Kind { Float, Int, Bool, Vec2, Color, Texture };
    std::string name;
    Kind kind = Float;
    std::string glslType;     // float, int, bool, vec2, vec3, vec4, sampler2D
    float value[4] = {0, 0, 0, 1};  // the default (Color: 0..1)
    std::string texture;      // the default, for a sampler2D
    float step = 0.01f;       // mouse-drag step in the Inspector (from hint_range)
};

// A problem in a shader file, on one of its lines (1-based)
struct ShaderProblem {
    int line = 1;
    int column = 0;  // 0: not known
    std::string message;
};

class Shaders {
public:
    // The parameters of a shader file ("" path or unreadable: nullptr).
    // Reads the file only (no graphics needed); re-read when it changes.
    static const std::vector<ShaderUniform>* uniforms(const std::string& path);

    // Draws what follows with the node's shader and its parameters; false
    // (nothing changed) if it has none or it doesn't compile. Pair with end().
    static bool begin(const Node2D& node);
    static void end();

    // Compiles a shader file now and says what's wrong with it (empty: fine).
    // Needs a window (graphics).
    static std::vector<ShaderProblem> check(const std::string& path);

    // A new shader file's text
    static const char* templateText();

    static void clear();  // unload them all (before the window closes)
};

}  // namespace s2d
