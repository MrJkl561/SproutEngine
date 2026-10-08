// The game runtime: what an exported game runs. It loads the project folder
// (scenes, assets, project.json) that sits next to it, starts the main scene
// and runs it with the C++ scripts compiled in. No editor.
//
//   MyGame                      play
//   MyGame --server             a dedicated server: no window (see multiplayer.hpp)
//   MyGame --join 192.168.1.20  anything else is for your scripts: hasArg(), argValue()
#include <raylib-cpp.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "game_runner.hpp"

namespace fs = std::filesystem;

namespace {

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

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    s2d::GameOptions options = s2d::gameOptionsFromArgs(args);
    options.project = findProject();
    return s2d::runGame(options);
}
