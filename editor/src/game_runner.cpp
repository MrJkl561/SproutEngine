#include "game_runner.hpp"

#include <raylib-cpp.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

#include "assets.hpp"
#include "console.hpp"
#include "multiplayer.hpp"
#include "project.hpp"
#include "scene_io.hpp"
#include "scene_tree.hpp"
#include "script.hpp"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

namespace fs = std::filesystem;

namespace s2d {

namespace {

struct Game {
    SceneTree tree;
    std::unique_ptr<raylib::RenderTexture> target;  // the game is drawn here, then scaled to the window
    long frame = 0;
};

Game* gGame = nullptr;
std::atomic<bool> gStop{false};

void drawFrame(Game& g) {
    // Draw the game at its own size...
    g.target->BeginMode();
    ClearBackground(g.tree.clearColor);
    raylib::Camera2D cam({0, 0}, {0, 0}, 0, 1);
    if (Camera2D* c = g.tree.currentCamera()) {
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
}

void frame() {
    Game& g = *gGame;
    g.tree.process(std::min(GetFrameTime(), 1.0f / 20.0f));
    drawFrame(g);
    ++g.frame;
}

}  // namespace

GameOptions gameOptionsFromArgs(const std::vector<std::string>& args) {
    GameOptions o;
    o.args = args;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        bool hasValue = i + 1 < args.size();
        if (a == "--server" || a == "--headless") o.headless = true;
        else if (a == "--scene" && hasValue) o.scene = args[++i];
        else if (a == "--instance" && hasValue) {
            o.titleSuffix = " (" + args[++i] + ")";
            o.stopWhenInputCloses = true;
        } else if (a == "--frames" && hasValue) o.testFrames = std::strtol(args[++i].c_str(), nullptr, 10);
        else if (a == "--window-pos" && hasValue) {
            const std::string& v = args[++i];
            size_t comma = v.find(',');
            if (comma != std::string::npos) {
                o.windowX = std::atoi(v.substr(0, comma).c_str());
                o.windowY = std::atoi(v.substr(comma + 1).c_str());
            }
        }
    }
    if (const char* frames = std::getenv("SPROUT_TEST_FRAMES"); frames && o.testFrames == 0)
        o.testFrames = std::strtol(frames, nullptr, 10);
    return o;
}

int runGame(const GameOptions& o) {
    Assets::setProjectDir(o.project);
    ProjectSettings settings = ProjectSettings::load(o.project);
    setCommandLineArgs(o.args);
    setHeadless(o.headless);
    // print() goes to the terminal (and to the editor's Output panel for Run Instances)
    setPrintHandler([](const std::string& message) {
        std::printf("%s\n", message.c_str());
        std::fflush(stdout);
    });

    Game game;
    gGame = &game;
    game.tree.gameSize = Vector2{static_cast<float>(settings.width), static_cast<float>(settings.height)};

    SetTraceLogLevel(LOG_WARNING);  // raylib's own messages: only problems
#if !defined(PLATFORM_WEB)
    if (o.stopWhenInputCloses) {
        // The editor's Stop closes our input: finish the frame and leave properly.
        std::thread([] {
            char c;
            while (std::fread(&c, 1, 1, stdin) == 1) {
            }
            gStop = true;
        }).detach();
    }
#endif
    if (!o.headless) {
#if !defined(PLATFORM_WEB)
        // No MSAA: the game is drawn into a texture, which MSAA doesn't smooth anyway,
        // and some GPUs (like virtual Macs) can't open a window with it.
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#endif
        InitWindow(settings.width, settings.height, (settings.name + o.titleSuffix).c_str());
        if (!IsWindowReady()) {
            TraceLog(LOG_ERROR, "GAME: couldn't open a window");
            return 1;
        }
        if (o.windowX >= 0 && o.windowY >= 0) SetWindowPosition(o.windowX, o.windowY);
        SetExitKey(KEY_NULL);  // Esc belongs to the game
        SetTargetFPS(60);
        game.target = std::make_unique<raylib::RenderTexture>(settings.width, settings.height);
        SetTextureFilter(game.target->texture, TEXTURE_FILTER_POINT);
    }

    std::string scene = o.scene.empty() ? settings.mainScene : o.scene;
    std::string error;
    auto root = loadScene(o.project / scene, &error);
    if (!root) {
        std::printf("Couldn't load the scene %s: %s\n", scene.c_str(), error.c_str());
        if (!o.headless) CloseWindow();
        return 1;
    }
    game.tree.setAutoloads(settings.autoloads);  // (Project Settings > Autoloads)
    game.tree.setRoot(std::move(root));
    game.tree.setCurrentScene(scene);
    game.tree.start();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    if (o.headless) {
        useParentConsole();
        // A dedicated server: 60 updates a second, until Ctrl+C (or the frame limit).
        std::signal(SIGINT, [](int) { gStop = true; });
        std::signal(SIGTERM, [](int) { gStop = true; });
        print("Running without a window. Press Ctrl+C to stop.");
        const auto step = std::chrono::microseconds(16667);
        auto next = std::chrono::steady_clock::now();
        while (!gStop) {
            game.tree.process(1.0f / 60.0f);
            ++game.frame;
            if (o.testFrames > 0 && game.frame >= o.testFrames) break;
            next += step;
            auto now = std::chrono::steady_clock::now();
            if (next < now - std::chrono::milliseconds(250)) next = now;  // fell behind: don't race to catch up
            std::this_thread::sleep_until(next);
        }
    } else {
        std::signal(SIGTERM, [](int) { gStop = true; });
        while (!WindowShouldClose() && !gStop) {
            frame();
            if (o.testFrames > 0 && game.frame >= o.testFrames) break;
        }
    }
    multiplayer().close();          // tell the other players we're going
    game.tree.setRoot(nullptr);     // scripts and nodes go before the window does
    game.target.reset();
    Assets::clear();
    if (!o.headless) CloseWindow();
#endif
    return 0;
}

}  // namespace s2d
