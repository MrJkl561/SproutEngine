#include "game_runner.hpp"

#include <raylib-cpp.hpp>
#include <rlgl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "assets.hpp"
#include "console.hpp"
#include "multiplayer.hpp"
#include "project.hpp"
#include "scene_io.hpp"
#include "scene_tree.hpp"
#include "script.hpp"
#include "shaders.hpp"

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
    long screenshotAt = -1;   // SPROUT_GAME_SCREENSHOT=file.png (with --frames): a picture of the last frame
    std::string screenshot;
    std::string pixelTest;    // SPROUT_TEST_PIXELS="x,y=r,g,b;..." (with --frames): check the last frame's colours
    // SPROUT_TEST_CLICKS="x,y@frame;...": the left button goes down at x,y (game pixels) on
    // that frame and up on the next one, instead of the real mouse
    struct Click {
        float x, y;
        long frame;
    };
    std::vector<Click> clicks;
};

// The tests' pixel check: each "x,y=r,g,b" (in game pixels) within 12 of what's drawn
void checkPixels(Game& g) {
    Image img = LoadImageFromTexture(g.target->texture);
    ImageFlipVertical(&img);  // (render textures are upside down)
    bool ok = true;
    std::string spec = g.pixelTest;
    for (size_t start = 0; start < spec.size();) {
        size_t end = spec.find(';', start);
        if (end == std::string::npos) end = spec.size();
        int x = 0, y = 0, r = 0, gr = 0, b = 0;
        if (std::sscanf(spec.substr(start, end - start).c_str(), "%d,%d=%d,%d,%d", &x, &y, &r, &gr, &b) == 5) {
            Color c = GetImageColor(img, x, y);
            bool good = std::abs(c.r - r) <= 12 && std::abs(c.g - gr) <= 12 && std::abs(c.b - b) <= 12;
            std::printf("PIXEL %d,%d is %d,%d,%d (wanted %d,%d,%d): %s\n", x, y, c.r, c.g, c.b, r, gr, b,
                        good ? "OK" : "WRONG");
            ok = ok && good;
        }
        start = end + 1;
    }
    std::printf(ok ? "PIXELS OK\n" : "PIXELS WRONG\n");
    std::fflush(stdout);
    UnloadImage(img);
}

Game* gGame = nullptr;
std::atomic<bool> gStop{false};

// Where the game is in the window: as big as fits, with black bars if the shapes differ.
Rectangle gameRect(const Game& g) {
    float gw = g.tree.gameSize.x, gh = g.tree.gameSize.y;
    float s = std::min(GetScreenWidth() / gw, GetScreenHeight() / gh);
    return {(GetScreenWidth() - gw * s) / 2, (GetScreenHeight() - gh * s) / 2, gw * s, gh * s};
}

void drawFrame(Game& g) {
    // Draw the game at its own size...
    g.target->BeginMode();
    ClearBackground(g.tree.clearColor);
    raylib::Camera2D cam = g.tree.view();
    raylib::Vector2 a = cam.GetScreenToWorld({0, 0}), b = cam.GetScreenToWorld(g.tree.gameSize);
    cam.BeginMode();
    g.tree.draw({false, cam.zoom, {a.x, a.y, b.x - a.x, b.y - a.y}});
    cam.EndMode();
    g.tree.drawCanvasLayers({false, 1});  // the HUD and menus, on the screen
    g.target->EndMode();

    // ...then scale it to fit the window, with black bars if the shapes differ.
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle dst = gameRect(g);
    DrawTexturePro(g.target->texture, {0, 0, g.tree.gameSize.x, -g.tree.gameSize.y}, dst, {0, 0}, 0, WHITE);
    if (g.frame + 1 == g.screenshotAt && !g.pixelTest.empty()) checkPixels(g);
    if (g.frame + 1 == g.screenshotAt && !g.screenshot.empty()) {
        rlDrawRenderBatchActive();  // (what's drawn so far, into the picture)
        Image shot = LoadImageFromScreen();
        ExportImage(shot, g.screenshot.c_str());
        UnloadImage(shot);
    }
    EndDrawing();
}

void frame() {
    Game& g = *gGame;
    if (g.clicks.empty()) {
        Rectangle r = gameRect(g);  // the mouse, in game pixels
        raylib::Vector2 m = GetMousePosition();
        g.tree.mouseScreen = raylib::Vector2((m.x - r.x) * g.tree.gameSize.x / r.width, (m.y - r.y) * g.tree.gameSize.y / r.height);
    } else {
        g.tree.testMouse = 0;
        for (const Game::Click& c : g.clicks) {
            if (g.frame == c.frame) {
                g.tree.mouseScreen = raylib::Vector2(c.x, c.y);
                g.tree.testMouse = 1;
            }
        }
    }
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
    if (o.testFrames > 0) {
        game.screenshotAt = o.testFrames;
        if (const char* shot = std::getenv("SPROUT_GAME_SCREENSHOT")) game.screenshot = shot;
        if (const char* pixels = std::getenv("SPROUT_TEST_PIXELS")) game.pixelTest = pixels;
        if (const char* clicks = std::getenv("SPROUT_TEST_CLICKS")) {
            std::string spec = clicks;
            for (size_t start = 0; start < spec.size();) {
                size_t end = spec.find(';', start);
                if (end == std::string::npos) end = spec.size();
                Game::Click c{};
                if (std::sscanf(spec.substr(start, end - start).c_str(), "%f,%f@%ld", &c.x, &c.y, &c.frame) == 3)
                    game.clicks.push_back(c);
                start = end + 1;
            }
        }
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
        while (!gStop && !game.tree.quitRequested()) {
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
        while (!WindowShouldClose() && !gStop && !game.tree.quitRequested()) {
            frame();
            if (o.testFrames > 0 && game.frame >= o.testFrames) break;
        }
    }
    multiplayer().close();          // tell the other players we're going
    game.tree.setRoot(nullptr);     // scripts and nodes go before the window does
    game.target.reset();
    Shaders::clear();
    Assets::clear();
    if (!o.headless) CloseWindow();
#endif
    return 0;
}

}  // namespace s2d
