// The game runtime: what an exported game runs. It loads the project folder
// (scenes, assets, project.json) that sits next to it, starts the main scene
// and runs it with the C++ scripts compiled in. No editor.
#include <raylib-cpp.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "assets.hpp"
#include "project.hpp"
#include "scene_io.hpp"
#include "scene_tree.hpp"
#include "script.hpp"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

namespace fs = std::filesystem;

namespace {

struct Game {
    s2d::SceneTree tree;
    std::unique_ptr<raylib::RenderTexture> target;  // the game is drawn here, then scaled to the window
    long frame = 0;
    long testFrames = 0;
};

Game* gGame = nullptr;

// Where the project folder is: next to the program, inside a macOS app's
// Resources folder, or (on the web) in the files packed with the page.
fs::path findProject() {
#if defined(PLATFORM_WEB)
    return "/project";
#else
    fs::path exeDir = GetApplicationDirectory();
    for (fs::path p : {exeDir / "project", exeDir / ".." / "Resources" / "project", fs::path("project")}) {
        std::error_code ec;
        if (fs::exists(p / "project.json", ec) || fs::exists(p / "scenes", ec)) return p;
    }
    return exeDir / "project";
#endif
}

void frame() {
    Game& g = *gGame;
    g.tree.process(std::min(GetFrameTime(), 1.0f / 20.0f));

    // Draw the game at its own size...
    g.target->BeginMode();
    ClearBackground(g.tree.clearColor);
    raylib::Camera2D cam({0, 0}, {0, 0}, 0, 1);
    if (s2d::Camera2D* c = g.tree.currentCamera()) {
        cam.offset = g.tree.gameSize / 2;
        cam.target = c->globalPosition();
        cam.zoom = c->zoom > 0.01f ? c->zoom : 0.01f;
    }
    cam.BeginMode();
    g.tree.draw({false, cam.zoom});
    cam.EndMode();
    g.target->EndMode();

    // ...then scale it to fit the window, with black bars if the shapes differ.
    BeginDrawing();
    ClearBackground(BLACK);
    float gw = g.tree.gameSize.x, gh = g.tree.gameSize.y;
    float s = std::min(GetScreenWidth() / gw, GetScreenHeight() / gh);
    Rectangle dst{(GetScreenWidth() - gw * s) / 2, (GetScreenHeight() - gh * s) / 2, gw * s, gh * s};
    DrawTexturePro(g.target->texture, {0, 0, gw, -gh}, dst, {0, 0}, 0, WHITE);
    EndDrawing();
    ++g.frame;
}

}  // namespace

int main() {
    fs::path project = findProject();
    s2d::Assets::setProjectDir(project);
    s2d::ProjectSettings settings = s2d::ProjectSettings::load(project);

#if !defined(PLATFORM_WEB)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
#endif
    InitWindow(settings.width, settings.height, settings.name.c_str());
    SetExitKey(KEY_NULL);  // Esc belongs to the game
    SetTargetFPS(60);

    Game game;
    gGame = &game;
    game.tree.gameSize = Vector2{static_cast<float>(settings.width), static_cast<float>(settings.height)};
    game.target = std::make_unique<raylib::RenderTexture>(settings.width, settings.height);
    SetTextureFilter(game.target->texture, TEXTURE_FILTER_POINT);
    if (const char* frames = std::getenv("SPROUT_TEST_FRAMES")) game.testFrames = std::strtol(frames, nullptr, 10);

    std::string error;
    auto root = s2d::loadScene(project / settings.mainScene, &error);
    if (!root) {
        TraceLog(LOG_ERROR, "GAME: couldn't load the main scene %s: %s", settings.mainScene.c_str(), error.c_str());
        CloseWindow();
        return 1;
    }
    game.tree.setRoot(std::move(root));
    game.tree.setCurrentScene(settings.mainScene);
    game.tree.start();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (!WindowShouldClose()) {
        frame();
        if (game.testFrames > 0 && game.frame >= game.testFrames) break;
    }
    game.tree.setRoot(nullptr);  // scripts and nodes go before the window does
    game.target.reset();
    s2d::Assets::clear();
    CloseWindow();
#endif
    return 0;
}
