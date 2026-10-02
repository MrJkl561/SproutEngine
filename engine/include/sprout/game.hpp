#pragma once
// =============================================================================
//  sprout/game.hpp - the window, the main loop and switching scenes
//
//    int main() {
//        Game game("My Awesome Game", 800, 600);   // opens the window
//        game.run<TitleScreen>();                  // runs until the window closes
//    }
//
//  From anywhere in your game:
//    changeScene<Level2>();          // go to another scene
//    changeScene<GameOver>(score);   // ...and give its constructor some values
//    restartScene();                 // start the current scene again
//    quitGame();                     // close the game
// =============================================================================
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "sprout/input.hpp"
#include "sprout/math.hpp"
#include "sprout/scene.hpp"

namespace sprout {

namespace detail {
using SceneFactory = std::function<std::unique_ptr<Scene>()>;

template <class T, class... Args>
SceneFactory makeSceneFactory(Args&&... args) {
    static_assert(std::is_base_of_v<Scene, T>,
                  "Scenes must inherit from Scene, e.g.  struct Level1 : Scene { ... };");
    // The values are copied so the scene can be made again by restartScene().
    return [... saved = std::forward<Args>(args)]() -> std::unique_ptr<Scene> {
        return std::make_unique<T>(saved...);
    };
}

void requestSceneChange(SceneFactory factory);
} // namespace detail

class Game {
public:
    /// Opens the game window.
    Game(const std::string& title, int width = 800, int height = 450);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /// Starts the game with the first scene and keeps running until the window
    /// is closed or quitGame() is called. Any values are given to the scene's constructor.
    template <class FirstScene, class... Args>
    void run(Args&&... args) {
        runWithFactory(detail::makeSceneFactory<FirstScene>(std::forward<Args>(args)...));
    }

private:
    void runWithFactory(detail::SceneFactory factory);
};

// --- Scenes ------------------------------------------------------------------
/// Switches to another scene at the end of this frame. Values are passed to its constructor.
template <class NextScene, class... Args>
void changeScene(Args&&... args) {
    detail::requestSceneChange(detail::makeSceneFactory<NextScene>(std::forward<Args>(args)...));
}
/// Starts the current scene again from the beginning (great for "press R to retry").
void restartScene();
/// The scene that is running right now.
Scene& currentScene();
/// Closes the game at the end of this frame.
void quitGame();

// --- Window ------------------------------------------------------------------
int screenWidth();
int screenHeight();
Vec2 screenSize();
/// The middle of the window.
Vec2 screenCenter();
void setWindowTitle(const std::string& title);
/// Lets the player drag the window edges to resize it.
void setResizable(bool resizable);
void setFullscreen(bool fullscreen);
bool isFullscreen();
void toggleFullscreen();
void showCursor();
void hideCursor();
/// Which key closes the game (Escape by default). Use Key::None to turn it off.
void setExitKey(Key key);
/// Saves a picture of the screen.
void takeScreenshot(const std::string& path = "screenshot.png");

// --- Time --------------------------------------------------------------------
/// Seconds since the last frame (the same as `dt` in update).
float deltaTime();
/// Seconds since the game started.
float timeSinceStart();
/// Frames per second right now.
int fps();
/// How many frames per second the game tries to run at (60 by default).
void setTargetFPS(int fps);

// --- Used inside the engine and by tests ---------------------------------------
namespace detail {
/// Game time used by Timer and Cooldown. It doesn't move while the scene is paused.
double gameClock();
void advanceGameClock(double seconds);
bool hasScene();
} // namespace detail

} // namespace sprout
