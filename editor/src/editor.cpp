#include "editor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

#include <nlohmann/json.hpp>

#include <rlgl.h>

#include "assets.hpp"
#include "raygui_include.hpp"
#include "export.hpp"
#include "platform.hpp"
#include "projects.hpp"
#include "scene_io.hpp"
#include "script.hpp"
#include "theme.hpp"
#include "ui_scale.hpp"

namespace fs = std::filesystem;

namespace s2d {

namespace {

// ---------------------------------------------------------------------------
// Look
// ---------------------------------------------------------------------------
// All of these come from the theme (editor/theme/*.cpp); see loadTheme().
float kToolbarH = 40;
float kHeaderH = 28;
float kRowH = 24;

Color kBg, kPanel, kHeader, kBorder, kText, kDim, kAccent, kSelect, kHover, kOrange, kFieldBg, kGood, kBad;

void loadTheme() {
    const Theme& t = theme();
    kToolbarH = t.layout.toolbarHeight;
    kHeaderH = t.layout.headerHeight;
    kRowH = t.layout.rowHeight;
    kBg = t.colors.background;
    kPanel = t.colors.panel;
    kHeader = t.colors.header;
    kBorder = t.colors.border;
    kText = t.colors.text;
    kDim = t.colors.dimText;
    kAccent = t.colors.accent;
    kSelect = t.colors.selection;
    kHover = t.colors.hover;
    kOrange = t.colors.warning;
    kFieldBg = t.colors.field;
    kGood = t.colors.good;
    kBad = t.colors.bad;
    GizmoColors& g = gizmoColors();
    g.collisionShape = t.gizmos.collisionShape;
    g.collisionShapeDisabled = t.gizmos.collisionShapeDisabled;
    g.camera = t.gizmos.camera;
    g.missingTexture = t.gizmos.missingTexture;
}

// Pictures named by the theme (panel backgrounds, node icons...), loaded once.
std::map<std::string, std::unique_ptr<raylib::Texture>> gThemeImages;

raylib::Texture* themeImage(const std::string& file) {
    if (file.empty()) return nullptr;
    auto it = gThemeImages.find(file);
    if (it == gThemeImages.end()) {
        std::unique_ptr<raylib::Texture> tex;
        std::string path = themePath(file);
        if (FileExists(path.c_str())) {
            try {
                tex = std::make_unique<raylib::Texture>(path);
                tex->SetFilter(TEXTURE_FILTER_BILINEAR);
            } catch (const raylib::RaylibException&) {
                tex.reset();
            }
        }
        it = gThemeImages.emplace(file, std::move(tex)).first;
    }
    return it->second.get();
}

// A theme picture over r: stretched, or repeated at its own size.
void drawThemeImage(const std::string& file, Rectangle r, bool tile, Color tint) {
    raylib::Texture* tex = themeImage(file);
    if (!tex || r.width <= 0 || r.height <= 0) return;
    if (!tile) {
        DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height}, r, {0, 0}, 0, tint);
        return;
    }
    for (float y = r.y; y < r.y + r.height; y += tex->height)
        for (float x = r.x; x < r.x + r.width; x += tex->width) {
            float w = std::min((float)tex->width, r.x + r.width - x), h = std::min((float)tex->height, r.y + r.height - y);
            DrawTexturePro(*tex, {0, 0, w, h}, {x, y, w, h}, {0, 0}, 0, tint);
        }
}

struct CompilerMessage {
    std::string file;
    int line = 0, column = 0;
    bool error = true;
    std::string message;
};

// "file.cpp:12:5: error: ..." (GCC, Clang) or "file.cpp(12,5): error C2065: ..." (MSVC)
std::optional<CompilerMessage> parseCompilerLine(const std::string& text) {
    if (text.find("error") == std::string::npos && text.find("warning") == std::string::npos) return std::nullopt;
    static const std::regex gcc(R"(^\s*(.+?):(\d+):(?:(\d+):)?\s*(fatal error|error|warning):\s*(.*)$)");
    static const std::regex msvc(R"(^\s*(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(fatal error|error|warning)\s*[A-Z]*\d*\s*:\s*(.*)$)");
    std::smatch m;
    if (!std::regex_match(text, m, msvc) && !std::regex_match(text, m, gcc)) return std::nullopt;
    CompilerMessage out;
    out.file = m[1].str();
    out.line = std::stoi(m[2].str());
    out.column = m[3].matched ? std::stoi(m[3].str()) : 0;
    out.error = m[4].str() != "warning";
    out.message = m[5].str();
    for (const char* curly : {"\xE2\x80\x98", "\xE2\x80\x99"})
        for (size_t p; (p = out.message.find(curly)) != std::string::npos;) out.message.replace(p, 3, "'");
    if (size_t proj = out.message.rfind(" ["); proj != std::string::npos && out.message.back() == ']')
        out.message.erase(proj);  // MSBuild adds " [something.vcxproj]"
    return out;
}

float gUiScale = 1.0f;  // Editor::uiScale(), for the helpers below

float screenW() { return GetScreenWidth() / gUiScale; }  // the window in UI pixels
float screenH() { return GetScreenHeight() / gUiScale; }

// Scissor rectangles are in real screen pixels.
void beginClip(Rectangle r) {
    BeginScissorMode(static_cast<int>(std::floor(r.x * gUiScale)), static_cast<int>(std::floor(r.y * gUiScale)),
                     static_cast<int>(std::ceil(r.width * gUiScale)), static_cast<int>(std::ceil(r.height * gUiScale)));
}

float textSize() { return static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SIZE)); }

void drawText(const std::string& s, float x, float y, Color c) {
    DrawTextEx(GuiGetFont(), s.c_str(), {x, y}, textSize(), static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING)), c);
}

float textWidth(const std::string& s) { return static_cast<float>(GuiGetTextWidth(s.c_str())); }

void drawHeader(Rectangle r, const std::string& title) {
    DrawRectangleRec(r, kPanel);
    const Theme& t = theme();
    drawThemeImage(t.images.panels, r, t.images.tilePanels, t.images.panelTint);
    DrawRectangleRec({r.x, r.y, r.width, kHeaderH}, kHeader);
    drawText(title, r.x + 10, r.y + 6, kText);
    DrawRectangleLinesEx(r, 1, kBorder);
}

bool mouseIn(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

bool ctrlDown() {
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) ||
           IsKeyDown(KEY_RIGHT_SUPER);
}
bool shiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }

std::string formatFloat(float v) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}

const NodeStyle& styleFor(const Node* n) {
    const Theme& t = theme();
    auto it = t.nodes.find(n->typeName());
    return it != t.nodes.end() ? it->second : t.otherNodes;
}

Color colorFor(const Node* n) { return styleFor(n).color; }

// The node type's icon: the theme's picture, or a raygui icon.
void drawNodeIcon(const Node* n, float x, float y) {
    const NodeStyle& s = styleFor(n);
    if (raylib::Texture* tex = themeImage(s.image)) {
        DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height}, {x, y, 16, 16}, {0, 0}, 0, WHITE);
        return;
    }
    GuiDrawIcon(s.icon, static_cast<int>(x), static_cast<int>(y), 1, s.color);
}

std::vector<int> pathOf(Node* n) {
    std::vector<int> path;
    while (n && n->parent()) {
        path.push_back(n->indexInParent());
        n = n->parent();
    }
    std::reverse(path.begin(), path.end());
    return path;
}

Node* nodeAt(Node* root, const std::vector<int>& path) {
    Node* n = root;
    for (int i : path) {
        if (!n || i < 0 || i >= static_cast<int>(n->children().size())) return nullptr;
        n = n->children()[i].get();
    }
    return n;
}

std::string snakeCase(const std::string& name) {
    std::string out;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (std::isupper(static_cast<unsigned char>(c))) {
            if (i > 0 && !out.empty() && out.back() != '_') out += '_';
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            out += c;
        }
    }
    return out;
}

std::string pascalCase(const std::string& name) {
    std::string out;
    bool upper = true;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            upper = true;
            continue;
        }
        out += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        upper = false;
    }
    return out.empty() ? "Sprite" : out;
}

bool isImageFile(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext == ".png" || ext == ".bmp" || ext == ".tga" || ext == ".qoi" || ext == ".gif";
}

const char* kScriptTemplate = R"(// A script for the Sprout editor. Attach it to a node in the inspector,
// then press Play. ready() runs once, process() runs every frame.
// Save this file and the editor reloads it, even while the game is running.
#include "script.hpp"

using namespace s2d;

class $NAME : public Script {
public:
    void ready() override {
        print("$NAME is ready on " + node()->name);
    }

    void process(float delta) override {
        // Example: move the node with the arrow keys.
        // if (auto* me = self<Node2D>()) {
        //     if (IsKeyDown(KEY_RIGHT)) me->position.x += 200 * delta;
        //     if (IsKeyDown(KEY_LEFT))  me->position.x -= 200 * delta;
        // }
        (void)delta;
    }
};

REGISTER_SCRIPT($NAME)
)";

// Locks the GUI while the mouse is outside a scroll area, so widgets that are
// scrolled out of sight can't be clicked through the panel's edges.
struct ClipLock {
    bool& flag;
    bool locked = false;
    ClipLock(Rectangle view, bool& clipFlag) : flag(clipFlag) {
        if (!GuiIsLocked() && !mouseIn(view)) {
            GuiLock();
            locked = true;
            flag = true;
        }
    }
    ~ClipLock() {
        if (locked) {
            GuiUnlock();
            flag = false;
        }
    }
};

}  // namespace

// ===========================================================================
// Setup
// ===========================================================================
Editor::Editor(fs::path projectDir)
    : projectDir_(std::move(projectDir)), scriptsDir_(projectDir_ / "scripts"),
      workDir_(projects::workDir(projectDir_, SPROUT_EDITOR_BUILD_DIR)) {
    Assets::setProjectDir(projectDir_);
    loadTheme();
    loadEditorStyle();
    loadLayout();
    codeEditor_ = loadCodeEditor();
    completions_.setFolders(fs::path(SPROUT_EDITOR_DIR) / "src", SPROUT_EDITOR_DEPS_DIR, scriptsDir_);
    setUiScale(systemUiScale());
    setPrintHandler([this](const std::string& msg) { log(msg, theme().colors.scriptPrint); });

    std::error_code ec;
    fs::create_directories(projectDir_ / "scenes", ec);
    fs::create_directories(scriptsDir_, ec);
    ensureDefaultAssets();
    rescanFiles();

    settings_ = ProjectSettings::load(projectDir_);
    log("Sprout Editor - project '" + settings_.name + "' in " + projectDir_.string());
    scripts_ = std::make_unique<ScriptReloader>(scriptsDir_, workDir_ / "scripts");
    if (ScriptReloader::supported()) {
        log("C++ scripts in " + scriptsDir_.string() + " reload automatically when you save them.");
        if (scripts_->libraryExists()) reloadScripts();
        if (scripts_->libraryOutOfDate()) {
            if (!scripts_->libraryExists()) log("Building this project's scripts (the first time takes a little longer)...");
            scripts_->requestBuild();
        }
    } else {
        log(std::to_string(ScriptRegistry::names().size()) + " C++ scripts compiled in, from " + scriptsDir_.string());
    }
    editTree_.gameSize = raylib::Vector2{static_cast<float>(settings_.width), static_cast<float>(settings_.height)};
    exporter_ = std::make_unique<Exporter>(Exporter::Paths{
        SPROUT_EDITOR_DIR, projectDir_, scriptsDir_, workDir_ / "exports",
        SPROUT_EDITOR_DEPS_DIR, SPROUT_EDITOR_CMAKE});
    scenePath_ = settings_.mainScene;
    if (!openScene(scenePath_)) newScene();
    undoStack_.clear();
    dirty_ = false;
}

Editor::~Editor() {
    setPrintHandler(nullptr);
    if (uiFont_.texture.id != 0) {
        setEditorFont(GetFontDefault());
        UnloadFont(uiFont_);
    }
    if (codeFont_.texture.id != 0) UnloadFont(codeFont_);
    playTree_.reset();
    inspectorScript_.reset();
    scripts_.reset();  // after the game: scripts' code lives in the library
    gameTexture_.reset();
    Assets::clear();
    gThemeImages.clear();  // (textures must go before the window does)
}

void Editor::ensureDefaultAssets() {
    fs::path dir = projectDir_ / "assets";
    std::error_code ec;
    fs::create_directories(dir, ec);

    auto make = [&](const char* file, int w, int h, const std::function<void(Image&)>& paint) {
        fs::path p = dir / file;
        if (fs::exists(p)) return;
        Image img = GenImageColor(w, h, BLANK);
        paint(img);
        ExportImage(img, p.string().c_str());
        UnloadImage(img);
    };
    make("icon.png", 64, 64, [](Image& img) {
        ImageDrawRectangle(&img, 4, 4, 56, 56, Color{40, 120, 70, 255});
        ImageDrawRectangle(&img, 8, 8, 48, 48, Color{60, 170, 95, 255});
        ImageDrawRectangle(&img, 30, 30, 4, 22, Color{230, 245, 220, 255});  // stem
        ImageDrawCircle(&img, 24, 26, 9, Color{230, 245, 220, 255});         // leaves
        ImageDrawCircle(&img, 40, 22, 11, Color{230, 245, 220, 255});
    });
    make("player.png", 32, 48, [](Image& img) {
        ImageDrawRectangle(&img, 2, 6, 28, 42, Color{60, 110, 220, 255});
        ImageDrawRectangle(&img, 4, 8, 24, 38, Color{90, 150, 255, 255});
        ImageDrawRectangle(&img, 8, 16, 6, 8, WHITE);
        ImageDrawRectangle(&img, 18, 16, 6, 8, WHITE);
        ImageDrawRectangle(&img, 10, 19, 3, 4, Color{20, 20, 40, 255});
        ImageDrawRectangle(&img, 20, 19, 3, 4, Color{20, 20, 40, 255});
    });
    make("box.png", 64, 64, [](Image& img) {
        ImageDrawRectangle(&img, 0, 0, 64, 64, Color{110, 70, 40, 255});
        ImageDrawRectangle(&img, 4, 4, 56, 56, Color{170, 115, 65, 255});
        ImageDrawRectangle(&img, 4, 29, 56, 6, Color{110, 70, 40, 255});
        ImageDrawRectangle(&img, 29, 4, 6, 56, Color{110, 70, 40, 255});
    });
    make("coin.png", 24, 24, [](Image& img) {
        ImageDrawCircle(&img, 12, 12, 11, Color{200, 140, 20, 255});
        ImageDrawCircle(&img, 12, 12, 8, Color{255, 210, 60, 255});
        ImageDrawRectangle(&img, 11, 6, 3, 12, Color{255, 240, 160, 255});
    });

    // Sprite sheets for animations. The player: 8 frames of 32x48 in a row:
    // 0-1 standing (eyes open, blink), 2-5 running, 6 jumping, 7 falling.
    make("player_sheet.png", 32 * 8, 48, [](Image& img) {
        const Color dark{60, 110, 220, 255}, body{90, 150, 255, 255}, leg{40, 80, 170, 255};
        const Color eye = WHITE, pupil{20, 20, 40, 255};
        for (int f = 0; f < 8; ++f) {
            int x = f * 32;
            int bob = (f == 3 || f == 5) ? 1 : 0;  // the body dips a little mid-stride
            int top = 4 + bob;
            ImageDrawRectangle(&img, x + 4, top, 24, 34, dark);
            ImageDrawRectangle(&img, x + 6, top + 2, 20, 30, body);
            // Eyes (closed on the blink frame, looking up when jumping, down when falling)
            if (f == 1) {
                ImageDrawRectangle(&img, x + 8, top + 12, 7, 2, pupil);
                ImageDrawRectangle(&img, x + 18, top + 12, 7, 2, pupil);
            } else {
                int look = f == 6 ? -2 : f == 7 ? 2 : 0;
                ImageDrawRectangle(&img, x + 9, top + 8, 6, 8, eye);
                ImageDrawRectangle(&img, x + 19, top + 8, 6, 8, eye);
                ImageDrawRectangle(&img, x + 11, top + 11 + look, 3, 4, pupil);
                ImageDrawRectangle(&img, x + 21, top + 11 + look, 3, 4, pupil);
            }
            // Legs: apart and together while running, tucked when in the air
            int legY = 38;
            int l1 = 8, l2 = 18;  // standing
            if (f >= 2 && f <= 5) {
                const int strides[4][2] = {{4, 22}, {9, 17}, {12, 14}, {9, 17}};
                l1 = strides[f - 2][0];
                l2 = strides[f - 2][1];
            }
            int legH = (f >= 6) ? 6 : 10;
            ImageDrawRectangle(&img, x + l1, legY, 6, legH, leg);
            ImageDrawRectangle(&img, x + l2, legY, 6, legH, leg);
        }
    });
    // A spinning coin: 6 frames of 24x24, getting narrower and wider again.
    make("coin_sheet.png", 24 * 6, 24, [](Image& img) {
        const int widths[6] = {11, 8, 4, 1, 4, 8};
        for (int f = 0; f < 6; ++f) {
            int cx = f * 24 + 12;
            for (int dy = -11; dy <= 11; ++dy) {
                float k = std::sqrt(std::max(0.0f, 1.0f - (dy * dy) / 121.0f));
                int outer = static_cast<int>(std::round(widths[f] * k));
                int inner = static_cast<int>(std::round(std::max(0, widths[f] - 3) * k));
                ImageDrawRectangle(&img, cx - outer, 12 + dy, outer * 2 + 1, 1, Color{200, 140, 20, 255});
                if (std::abs(dy) <= 8 && inner > 0)
                    ImageDrawRectangle(&img, cx - inner, 12 + dy, inner * 2 + 1, 1, Color{255, 210, 60, 255});
            }
            if (widths[f] >= 8) ImageDrawRectangle(&img, cx - 1, 6, 3, 12, Color{255, 240, 160, 255});
        }
    });
}

// ===========================================================================
// Frame
// ===========================================================================
void Editor::frame() {
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) touchedThisDrag_ = false;
    if (swallowMouse_ && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        swallowMouse_ = false;
    if (GetTime() - lastScan_ > 2.0) rescanFiles();
    mouseCursor_ = MOUSE_CURSOR_DEFAULT;

    SetMouseScale(1 / uiScale_, 1 / uiScale_);  // mouse positions in UI pixels
    float sw = screenW();
    float sh = screenH();
    // Panel sizes: the user's (drag the borders), kept inside the window with
    // room left for the viewport.
    float kLeftW = std::clamp(layout_.left, 160.0f, std::max(160.0f, sw * 0.45f));
    float kRightW = std::clamp(layout_.right, 220.0f, std::max(220.0f, sw * 0.45f));
    if (float squeeze = 240 - (sw - kLeftW - kRightW); squeeze > 0) {
        kLeftW = std::max(120.0f, kLeftW - squeeze / 2);
        kRightW = std::max(160.0f, kRightW - squeeze / 2);
    }
    kLeftW = std::floor(kLeftW);
    kRightW = std::floor(kRightW);
    float bodyH = sh - kToolbarH;
    float kOutputH = std::floor(std::clamp(animationTabShown() ? layout_.animation : layout_.bottom, 80.0f,
                                           std::max(80.0f, bodyH - 140)));
    float sceneH = std::floor(bodyH * std::clamp(layout_.sceneShare, 0.15f, 0.85f));
    Rectangle toolbar{0, 0, sw, kToolbarH};
    Rectangle sceneDock{0, kToolbarH, kLeftW, sceneH};
    Rectangle fileDock{0, kToolbarH + sceneDock.height, kLeftW, bodyH - sceneDock.height};
    Rectangle inspector{sw - kRightW, kToolbarH, kRightW, bodyH};
    Rectangle output{kLeftW, sh - kOutputH, sw - kLeftW - kRightW, kOutputH};
    Rectangle viewport{kLeftW, kToolbarH, sw - kLeftW - kRightW, bodyH - kOutputH};
    Rectangle mainTabs{viewport.x, viewport.y, viewport.width, 30};
    Rectangle mainArea{viewport.x, viewport.y + 30, viewport.width, viewport.height - 30};
    viewportRect_ = mainView_ == MainView::View2D ? mainArea : Rectangle{};
    inspectorRect_ = inspector;
    dropTarget_ = nullptr;

    // Files dropped onto the window from the desktop are copied into assets/.
    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        for (unsigned i = 0; i < dropped.count; ++i) {
            fs::path src = dropped.paths[i];
            if (!isImageFile(src)) {
                log("Only pictures can be dropped in: " + src.filename().string(), kBad);
                continue;
            }
            std::error_code ec;
            fs::copy_file(src, projectDir_ / "assets" / src.filename(), fs::copy_options::overwrite_existing, ec);
            std::string rel = "assets/" + src.filename().string();
            Assets::reload(rel);
            log("Imported " + rel, kGood);
            if (!playTree_ && mouseIn(viewportRect_)) createSpriteAt(rel, viewCam_.GetScreenToWorld(GetMousePosition()));
        }
        UnloadDroppedFiles(dropped);
        rescanFiles();
    }

    // Clicking anywhere but the script editor takes the keyboard away from it.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !mouseIn(codeRect_)) codeFocused_ = false;
    handleShortcuts();
    updateScripts();
    checkDocumentsOnDisk();

    if (playTree_) {
        try {
            playTree_->process(std::min(GetFrameTime(), 1.0f / 20.0f));
        } catch (const std::exception& e) {
            log(std::string("A script threw an exception: ") + e.what(), kBad);
            stop();
        }
    }
    if (playTree_) renderGame();

    BeginDrawing();
    ClearBackground(kBg);
    applyUiScale();

    pumpExport();
    pumpFolderPicker();
    updateSplitters(sw, sh, kLeftW, kRightW, kOutputH, sceneH);
    bool modal = popup_.open || dialog_.open || confirm_.open || export_.open || projects_.open ||
                 folderPick_.valid() || sheet_.open || codeEditorDialog_.open || swallowMouse_ ||
                 splitter_ != Splitter::None;
    if (modal) GuiLock();
    {
        // While the game runs the scene can't be edited (it's a copy that's playing).
        if (playTree_ && !modal) GuiLock();
        drawInspector(inspector);  // first, so a typed value is kept when you click elsewhere
        drawSceneDock(sceneDock);
        drawFileSystem(fileDock);
        if (playTree_ && !modal) GuiUnlock();
        drawOutput(output);
        drawMainTabs(mainTabs);
        if (mainView_ == MainView::Script) drawScriptView(mainArea);
        else drawViewport(mainArea);
        drawToolbar(toolbar);
    }
    if (modal) GuiUnlock();
    finishDrag();
    // Dialogs, then popups on top (a dialog can open a popup, e.g. to pick a scene).
    if (popup_.open) GuiLock();
    drawModal();
    if (popup_.open) GuiUnlock();
    drawPopup();

    if (!screenshotPath.empty()) {
        rlDrawRenderBatchActive();  // flush pending drawing so it's in the picture
        TakeScreenshot(screenshotPath.c_str());
        screenshotPath.clear();
    }
    EndDrawing();

    SetMouseCursor(mouseCursor_);
    std::string title = theme().title + " - " + settings_.name + " - " + scenePath_ + (dirty_ ? " *" : "") + (playTree_ ? "  [playing]" : "");
    if (title != windowTitle_) {
        windowTitle_ = title;
        SetWindowTitle(title.c_str());
    }
}

void Editor::applyUiScale() {
    rlDrawRenderBatchActive();
    rlLoadIdentity();
    rlScalef(uiScale_, uiScale_, 1);
}

void Editor::setUiScale(float scale) {
    scale = std::clamp(std::round(scale * 4) / 4, 0.75f, 3.0f);
    uiScale_ = scale;
    gUiScale = scale;

    // Load the font at the size it really appears on screen, so text stays sharp.
    static const Font styleFont = GuiGetFont();  // the theme's own font, the fallback
    Font old = uiFont_;
    uiFont_ = Font{};
    std::string fontFile = themePath(theme().font);
#if defined(SPROUT_EDITOR_FONT)
    if (fontFile.empty()) fontFile = SPROUT_EDITOR_FONT;
#endif
    if (!fontFile.empty() && FileExists(fontFile.c_str())) {
        uiFont_ = LoadFontEx(fontFile.c_str(), static_cast<int>(std::lround(theme().fontSize * scale)), nullptr, 0);
        if (uiFont_.texture.id != 0) {
            SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_BILINEAR);
            setEditorFont(uiFont_);
        }
    }
    if (uiFont_.texture.id == 0 && old.texture.id != 0) setEditorFont(styleFont);
    if (old.texture.id != 0) UnloadFont(old);

    // The script editor's own font, if the theme has one
    if (codeFont_.texture.id != 0) UnloadFont(codeFont_);
    codeFont_ = Font{};
    if (std::string codeFile = themePath(theme().code.font); !codeFile.empty() && FileExists(codeFile.c_str())) {
        int size = theme().code.fontSize > 0 ? theme().code.fontSize : theme().fontSize;
        codeFont_ = LoadFontEx(codeFile.c_str(), static_cast<int>(std::lround(size * scale)), nullptr, 0);
        if (codeFont_.texture.id != 0) SetTextureFilter(codeFont_.texture, TEXTURE_FILTER_BILINEAR);
    }
}

void Editor::resetLayout() {
    const Theme::Layout& d = theme().layout;
    layout_ = PanelSizes{d.leftWidth, d.rightWidth, d.bottomHeight, d.animationHeight, d.sceneDockShare};
}

void Editor::loadLayout() {
    resetLayout();
    std::ifstream in(projects::configDir() / "layout.json");
    if (!in) return;
    try {
        nlohmann::json j = nlohmann::json::parse(in);
        layout_.left = j.value("left", layout_.left);
        layout_.right = j.value("right", layout_.right);
        layout_.bottom = j.value("bottom", layout_.bottom);
        layout_.animation = j.value("animation", layout_.animation);
        layout_.sceneShare = j.value("scene_share", layout_.sceneShare);
    } catch (const nlohmann::json::exception&) {
    }
}

void Editor::saveLayout() const {
    nlohmann::json j = {{"left", layout_.left},       {"right", layout_.right},
                        {"bottom", layout_.bottom},   {"animation", layout_.animation},
                        {"scene_share", layout_.sceneShare}};
    std::error_code ec;
    fs::create_directories(projects::configDir(), ec);
    std::ofstream(projects::configDir() / "layout.json") << j.dump(2) << '\n';
}

void Editor::updateSplitters(float sw, float sh, float leftW, float rightW, float bottomH, float sceneH) {
    raylib::Vector2 m = GetMousePosition();
    float bodyH = sh - kToolbarH;
    const float grab = 4;  // how far from a border it can be grabbed
    Rectangle zones[] = {
        {},                                                          // None
        {leftW - grab, kToolbarH, grab * 2, bodyH},                   // Left
        {sw - rightW - grab, kToolbarH, grab * 2, bodyH},             // Right
        {leftW + grab, sh - bottomH - grab, sw - leftW - rightW - grab * 2, grab * 2},  // Bottom
        {0, kToolbarH + sceneH - grab, leftW - grab, grab * 2},       // SceneFiles
    };
    auto cursorFor = [](Splitter s) {
        return s == Splitter::Left || s == Splitter::Right ? MOUSE_CURSOR_RESIZE_EW : MOUSE_CURSOR_RESIZE_NS;
    };

    if (splitter_ == Splitter::None) {
        bool busy = popup_.open || dialog_.open || confirm_.open || export_.open || projects_.open || sheet_.open ||
                    codeEditorDialog_.open ||
                    folderPick_.valid() || swallowMouse_ || drag_.kind != DragKind::None ||
                    viewDrag_ != ViewDrag::None || panning_ || !scrubField_.empty() || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        if (busy && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
        if (popup_.open || dialog_.open || confirm_.open || export_.open || projects_.open || sheet_.open ||
            codeEditorDialog_.open)
            return;
        for (int i = 1; i <= 4; ++i) {
            if (!CheckCollisionPointRec(m, zones[i])) continue;
            Splitter s = static_cast<Splitter>(i);
            mouseCursor_ = cursorFor(s);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (lastSplitterClick_ == s && GetTime() - lastSplitterClickTime_ < 0.35) {
                    // Double-click: back to the theme's size.
                    const Theme::Layout& d = theme().layout;
                    if (s == Splitter::Left) layout_.left = d.leftWidth;
                    if (s == Splitter::Right) layout_.right = d.rightWidth;
                    if (s == Splitter::Bottom) (animationTabShown() ? layout_.animation : layout_.bottom) =
                        animationTabShown() ? d.animationHeight : d.bottomHeight;
                    if (s == Splitter::SceneFiles) layout_.sceneShare = d.sceneDockShare;
                    saveLayout();
                    lastSplitterClick_ = Splitter::None;
                    swallowMouse_ = true;
                } else {
                    splitter_ = s;
                    lastSplitterClick_ = s;
                    lastSplitterClickTime_ = GetTime();
                }
            }
            return;
        }
        return;
    }

    // Dragging a border
    mouseCursor_ = cursorFor(splitter_);
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        splitter_ = Splitter::None;
        saveLayout();
        return;
    }
    switch (splitter_) {
        case Splitter::Left: layout_.left = std::max(120.0f, m.x); break;
        case Splitter::Right: layout_.right = std::max(160.0f, sw - m.x); break;
        case Splitter::Bottom:
            (animationTabShown() ? layout_.animation : layout_.bottom) = std::max(80.0f, sh - m.y);
            break;
        case Splitter::SceneFiles: layout_.sceneShare = std::clamp((m.y - kToolbarH) / bodyH, 0.15f, 0.85f); break;
        case Splitter::None: break;
    }
}

void Editor::handleShortcuts() {
    bool scriptView = mainView_ == MainView::Script;
    if (playTree_) {
        bool typing = scriptView && (codeFocused_ || find_.findEdit || find_.withEdit);
        if (IsKeyPressed(KEY_F8) || (IsKeyPressed(KEY_ESCAPE) && !typing && !find_.open)) stop();
        if (scriptView && !popup_.open && !dialogOpen()) handleScriptShortcuts();  // edit scripts while it runs
        return;
    }
    if (scriptView) {
        if (!popup_.open && !dialogOpen()) handleScriptShortcuts();
        return;
    }
    if (isTyping() || popup_.open) return;

    if (IsKeyPressed(KEY_F5)) play(true);
    if (IsKeyPressed(KEY_F6)) play(false);
    if (ctrlDown()) {
        if (IsKeyPressed(KEY_S)) saveScene();
        if (IsKeyPressed(KEY_Z)) shiftDown() ? redo() : undo();
        if (IsKeyPressed(KEY_Y)) redo();
        if (IsKeyPressed(KEY_D)) duplicateSelected();
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) setUiScale(uiScale_ + 0.25f);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) setUiScale(uiScale_ - 0.25f);
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) setUiScale(systemUiScale());
        if (IsKeyPressed(KEY_A))
            openPopup(GetMousePosition(), nodeTypeNames(), [this](int i) { addNode(nodeTypeNames()[i]); });
    }
    if (IsKeyPressed(KEY_DELETE)) deleteSelected();
}

// ===========================================================================
// Toolbar
// ===========================================================================
void Editor::drawToolbar(Rectangle r) {
    DrawRectangleRec(r, kHeader);
    drawThemeImage(theme().images.toolbar, r, false, WHITE);
    DrawLineEx({r.x, r.y + r.height - 1}, {r.x + r.width, r.y + r.height - 1}, 1, kBorder);

    float x = 8, y = 6, h = 28;
    bool playing = playTree_ != nullptr;
    if (playing) GuiDisable();
    if (GuiButton({x, y, 100, h}, GuiIconText(ICON_FILETYPE_HOME, "Projects"))) openProjectManager();
    x += 108;
    if (GuiButton({x, y, 76, h}, GuiIconText(ICON_FILE_NEW, "New"))) confirmUnsaved([this] { newScene(); });
    x += 80;
    if (GuiButton({x, y, 80, h}, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open"))) {
        std::vector<std::string> scenes;
        for (auto& f : files_)
            if (f.kind == FileEntry::Scene) scenes.push_back(f.path);
        if (scenes.empty()) {
            log("No scenes in " + (projectDir_ / "scenes").string() + " yet.", kDim);
        } else {
            openPopup({x, y + h + 2}, scenes, [this, scenes](int i) { requestOpenScene(scenes[i]); });
        }
    }
    x += 84;
    if (GuiButton({x, y, 76, h}, GuiIconText(ICON_FILE_SAVE_CLASSIC, "Save"))) saveScene();
    x += 80;
    if (GuiButton({x, y, 92, h}, GuiIconText(ICON_FILE_SAVE, "Save As"))) {
        openTextDialog("Save Scene As", "Path inside the project folder:", scenePath_, [this](const std::string& p) {
            if (p.empty()) return;
            scenePath_ = p;
            saveScene();
        });
    }
    x += 96;
    if (GuiButton({x, y, 84, h}, GuiIconText(ICON_FILE_EXPORT, "Export"))) openExportDialog();
    x += 88;
    if (GuiButton({x, y, 34, h}, GuiIconText(ICON_UNDO, nullptr))) undo();
    x += 36;
    if (GuiButton({x, y, 34, h}, GuiIconText(ICON_REDO, nullptr))) redo();
    x += 44;
    if (playing) GuiEnable();

    // Play / Stop in the middle, like Godot (but never on top of the buttons)
    // Play runs the game from its main scene (F5); Play Scene runs the one being edited (F6).
    float px = std::max(std::floor(r.width / 2 - 150), x);
    if (playing) GuiDisable();
    if (GuiButton({px, y, 84, h}, GuiIconText(ICON_PLAYER_PLAY, "Play"))) play(true);
    if (GuiButton({px + 88, y, 120, h}, GuiIconText(ICON_FILETYPE_PLAY, "Play Scene"))) play(false);
    if (playing) GuiEnable();
    if (!playing) GuiDisable();
    if (GuiButton({px + 212, y, 84, h}, GuiIconText(ICON_PLAYER_STOP, "Stop"))) stop();
    if (!playing) GuiEnable();
    x = px + 312;

    // Script hot reload status
    if (ScriptReloader::supported()) {
        if (GuiButton({x, y, 34, h}, GuiIconText(ICON_ROTATE_FILL, nullptr))) scripts_->requestBuild();
        std::string status = scripts_->isBuilding()        ? "Scripts: compiling..."
                             : scripts_->lastBuildFailed() ? "Scripts: build failed"
                                                           : "Scripts: " + std::to_string(scripts_->scriptNames().size()) +
                                                                 " loaded";
        Color c = scripts_->isBuilding() ? kOrange : scripts_->lastBuildFailed() ? kBad : kDim;
        drawText(status, x + 42, y + 6, c);
        x += 42 + textWidth(status) + 16;
    }
    if (exporter_ && exporter_->busy()) {
        std::string status = "Exporting " + exporter_->currentLabel() + "...";
        drawText(status, x, y + 6, kOrange);
        x += textWidth(status) + 16;
    }

    // Interface size, on the right (Ctrl + / Ctrl - / Ctrl 0 do the same)
    float ux = r.width - 8 - 30;
    if (GuiButton({ux, y, 30, h}, "+")) setUiScale(uiScale_ + 0.25f);
    std::string pct = std::to_string(static_cast<int>(std::lround(uiScale_ * 100))) + "%";
    ux -= 4 + textWidth("300%") + 4;
    drawText(pct, ux + 4 + (textWidth("300%") - textWidth(pct)) / 2, y + 6, kText);
    ux -= 34;
    if (GuiButton({ux, y, 30, h}, "-")) setUiScale(uiScale_ - 0.25f);
    ux -= textWidth("UI size") + 8;
    drawText("UI size", ux, y + 6, kDim);
    ux -= 42;
    if (GuiButton({ux, y, 34, h}, GuiIconText(ICON_GEAR, nullptr))) {
        openPopup({ux, y + h + 2}, {"Code Editor...", "Reset Panel Layout"}, [this](int i) {
            if (i == 0) openCodeEditorDialog();
            if (i == 1) {
                resetLayout();
                saveLayout();
            }
        });
    }

    // The scene's file name fills whatever room is left (shortened from the left)
    std::string path = scenePath_ + (dirty_ ? " (unsaved)" : "");
    float room = ux - 16 - x;
    if (room > 40) {
        bool cut = false;
        while (path.size() > 4 && textWidth((cut ? "..." : "") + path) > room) {
            path.erase(0, 1);
            cut = true;
        }
        if (cut) path = "..." + path;
        drawText(path, ux - 16 - textWidth(path), y + 6, dirty_ ? kOrange : kDim);
    }
}

// ===========================================================================
// Scene dock (the node tree)
// ===========================================================================
void Editor::drawSceneDock(Rectangle r) {
    drawHeader(r, "Scene");

    float y = r.y + kHeaderH + 4, x = r.x + 6;
    float addW = std::min(88.0f, r.width - 12 - 5 * 33 - 4);  // the other five buttons are 30 wide
    if (GuiButton({x, y, addW, 26}, GuiIconText(ICON_CIRCLE_ADD, addW >= 64 ? "Add" : nullptr))) {
        openPopup({x, y + 28}, nodeTypeNames(), [this](int i) { addNode(nodeTypeNames()[i]); });
    }
    x += addW + 4;
    // Instance a scene (like Godot's chain-link button)
    if (GuiButton({x, y, 30, 26}, GuiIconText(ICON_LAYERS, nullptr))) {
        std::vector<std::string> scenes;
        for (auto& f : files_)
            if (f.kind == FileEntry::Scene && f.path != scenePath_) scenes.push_back(f.path);
        if (scenes.empty()) {
            log("No other scenes to instance yet. Make one: select a node and use 'Save Branch as Scene'.", kDim);
        } else {
            openPopup({x, y + 28}, scenes, [this, scenes](int i) {
                Node* parent = selected_ ? selected_ : editTree_.root();
                instanceScene(scenes[i], parent);
            });
        }
    }
    x += 33;
    if (GuiButton({x, y, 30, 26}, GuiIconText(ICON_FILE_COPY, nullptr))) duplicateSelected();
    x += 33;
    if (GuiButton({x, y, 30, 26}, GuiIconText(ICON_BIN, nullptr))) deleteSelected();
    x += 33;
    if (GuiButton({x, y, 30, 26}, GuiIconText(ICON_ARROW_UP_FILL, nullptr))) moveSelected(-1);
    x += 33;
    if (GuiButton({x, y, 30, 26}, GuiIconText(ICON_ARROW_DOWN_FILL, nullptr))) moveSelected(1);

    Rectangle area{r.x, y + 32, r.width, r.y + r.height - (y + 32)};
    Node* root = editTree_.root();
    if (playTree_) root = playTree_->root();  // show the running scene while playing

    struct Row {
        Node* node;
        int depth;
    };
    std::vector<Row> rows;
    std::function<void(Node*, int)> collect = [&](Node* n, int depth) {
        rows.push_back({n, depth});
        if (n->collapsed) return;  // (instanced scenes start folded up)
        for (auto& c : n->children()) collect(c.get(), depth + 1);
    };
    if (root) collect(root, 0);

    Rectangle content{0, 0, area.width - 16, rows.size() * kRowH + 8};
    Rectangle view{};
    GuiScrollPanel(area, nullptr, content, &treeScroll_, &view);

    if (!root) {
        drawText("Empty scene.", area.x + 12, area.y + 12, kDim);
        drawText("Press Add to create a root node.", area.x + 12, area.y + 32, kDim);
        return;
    }

    beginClip(view);
    Vector2 mouse = GetMousePosition();
    bool canClick = !GuiIsLocked() && mouseIn(view);
    for (size_t i = 0; i < rows.size(); ++i) {
        Node* n = rows[i].node;
        Rectangle row{view.x, view.y + treeScroll_.y + 4 + i * kRowH, view.width, kRowH};
        if (row.y + row.height < view.y || row.y > view.y + view.height) continue;
        bool hover = canClick && CheckCollisionPointRec(mouse, row);

        if (n == selected_) DrawRectangleRec(row, kSelect);
        else if (hover && drag_.kind == DragKind::None) DrawRectangleRec(row, kHover);

        float ix = row.x + 6 + rows[i].depth * 16.0f;
        Rectangle arrow{ix, row.y + 4, 16, 16};
        bool foldable = !n->children().empty();
        if (foldable) {
            GuiDrawIcon(n->collapsed ? ICON_ARROW_RIGHT_FILL : ICON_ARROW_DOWN_FILL, static_cast<int>(ix),
                        static_cast<int>(row.y + 4), 1, kDim);
        }
        drawNodeIcon(n, ix + 18, row.y + (kRowH - 16) / 2);
        // Nodes that come from an instanced scene are drawn in a softer colour.
        drawText(n->name + (n->instanceMissing ? "  (missing scene)" : ""), ix + 40, row.y + 4,
                 n->instanceMissing ? kBad : n->inInstance ? theme().colors.instancedText : kText);
        if (!n->sceneFile.empty()) {  // instanced scene: a scene icon at the right
            GuiDrawIcon(ICON_LAYERS, static_cast<int>(row.x + row.width - 46), static_cast<int>(row.y + 4), 1,
                        n->instanceMissing ? kBad : kAccent);
        }
        if (!n->scriptName.empty()) {
            Color sc = ScriptRegistry::has(n->scriptName) ? kGood : kOrange;
            GuiDrawIcon(ICON_FILETYPE_TEXT, static_cast<int>(row.x + row.width - 24), static_cast<int>(row.y + 4), 1,
                        sc);
        }

        // Where a dragged node or file would be dropped
        if (drag_.active && hover) {
            float rel = (mouse.y - row.y) / row.height;
            int zone = 0;
            if (drag_.kind == DragKind::TreeNode && n->parent()) zone = rel < 0.25f ? -1 : (rel > 0.75f ? 1 : 0);
            dropTarget_ = n;
            dropZone_ = zone;
            if (zone == 0) DrawRectangleLinesEx(row, 2, kAccent);
            else DrawRectangleRec({row.x, zone < 0 ? row.y - 1 : row.y + row.height - 2, row.width, 3}, kAccent);
        }

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !playTree_) {
            if (foldable && CheckCollisionPointRec(mouse, arrow)) {
                n->collapsed = !n->collapsed;
            } else {
                select(n);
                drag_ = DragState{DragKind::TreeNode, n, 0, mouse, false};
            }
        }
    }
    EndScissorMode();
}

// ===========================================================================
// FileSystem dock
// ===========================================================================
void Editor::rescanFiles() {
    lastScan_ = GetTime();
    std::vector<FileEntry> found;
    std::error_code ec;

    auto scanProject = [&](const char* sub, FileEntry::Kind kind, const std::function<bool(const fs::path&)>& want) {
        fs::path dir = projectDir_ / sub;
        if (!fs::is_directory(dir, ec)) return;
        std::vector<FileEntry> group;
        for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator();
             it.increment(ec)) {
            if (!it->is_regular_file(ec) || !want(it->path())) continue;
            std::string rel = fs::relative(it->path(), projectDir_, ec).generic_string();
            group.push_back({kind, rel, fs::relative(it->path(), dir, ec).generic_string(), ""});
        }
        std::sort(group.begin(), group.end(), [](auto& a, auto& b) { return a.path < b.path; });
        found.insert(found.end(), group.begin(), group.end());
    };
    scanProject("assets", FileEntry::Texture, isImageFile);
    scanProject("scenes", FileEntry::Scene, [](const fs::path& p) { return p.extension() == ".json"; });

    std::vector<FileEntry> scripts;
    static const std::regex registerRe(R"(REGISTER_SCRIPT\s*\(\s*(\w+)\s*\))");
    if (fs::is_directory(scriptsDir_, ec)) {
        for (auto it = fs::directory_iterator(scriptsDir_, ec); !ec && it != fs::directory_iterator();
             it.increment(ec)) {
            if (!it->is_regular_file(ec) || it->path().extension() != ".cpp") continue;
            std::ifstream in(it->path());
            std::stringstream ss;
            ss << in.rdbuf();
            std::string text = ss.str();
            std::smatch m;
            std::string cls = std::regex_search(text, m, registerRe) ? m[1].str() : "";
            scripts.push_back({FileEntry::Script, it->path().string(), it->path().filename().string(), cls});
        }
    }
    std::sort(scripts.begin(), scripts.end(), [](auto& a, auto& b) { return a.label < b.label; });
    found.insert(found.end(), scripts.begin(), scripts.end());

    // Keep a drag going across a rescan by finding its file again.
    if (drag_.kind == DragKind::File && drag_.file < files_.size()) {
        std::string path = files_[drag_.file].path;
        drag_.kind = DragKind::None;
        for (size_t i = 0; i < found.size(); ++i)
            if (found[i].path == path) {
                drag_.kind = DragKind::File;
                drag_.file = i;
            }
    }
    files_ = std::move(found);
}

void Editor::drawFileSystem(Rectangle r) {
    drawHeader(r, "FileSystem");
    if (GuiButton({r.x + r.width - 34, r.y + 2, 28, 24}, GuiIconText(ICON_ROTATE_FILL, nullptr))) rescanFiles();

    struct Row {
        bool header;
        std::string text;
        size_t file;
    };
    std::vector<Row> rows;
    int lastKind = -1;
    for (size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].kind != lastKind) {
            lastKind = files_[i].kind;
            const char* title = lastKind == FileEntry::Texture ? "res://assets"
                                : lastKind == FileEntry::Scene ? "res://scenes"
                                                               : "res://scripts (C++)";
            rows.push_back({true, title, 0});
        }
        rows.push_back({false, files_[i].label, i});
    }

    Rectangle area{r.x, r.y + kHeaderH, r.width, r.height - kHeaderH};
    Rectangle content{0, 0, area.width - 16, rows.size() * kRowH + 8};
    Rectangle view{};
    GuiScrollPanel(area, nullptr, content, &filesScroll_, &view);

    beginClip(view);
    Vector2 mouse = GetMousePosition();
    bool canClick = !GuiIsLocked() && mouseIn(view);
    for (size_t i = 0; i < rows.size(); ++i) {
        Rectangle row{view.x, view.y + filesScroll_.y + 4 + i * kRowH, view.width, kRowH};
        if (row.y + row.height < view.y || row.y > view.y + view.height) continue;
        if (rows[i].header) {
            GuiDrawIcon(ICON_FOLDER_OPEN, static_cast<int>(row.x + 6), static_cast<int>(row.y + 4), 1, kDim);
            drawText(rows[i].text, row.x + 28, row.y + 4, kDim);
            continue;
        }
        const FileEntry& f = files_[rows[i].file];
        bool hover = canClick && CheckCollisionPointRec(mouse, row);
        if (hover) DrawRectangleRec(row, kHover);

        float ix = row.x + 22;
        if (f.kind == FileEntry::Texture) {
            // Tiny preview of the picture
            if (raylib::Texture* tex = Assets::texture(f.path)) {
                float s = 18.0f / std::max(tex->width, tex->height);
                DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height},
                               {ix + 9 - tex->width * s / 2, row.y + 12 - tex->height * s / 2, tex->width * s,
                                tex->height * s},
                               {0, 0}, 0, WHITE);
            }
        } else {
            int icon = f.kind == FileEntry::Scene ? ICON_LAYERS : ICON_FILETYPE_TEXT;
            Color c = f.kind == FileEntry::Scene ? kAccent
                      : ScriptRegistry::has(f.scriptClass) ? kGood
                                                           : kOrange;
            GuiDrawIcon(icon, static_cast<int>(ix), static_cast<int>(row.y + 4), 1, c);
        }
        std::string label = f.label;
        if (f.kind == FileEntry::Script && !f.scriptClass.empty() && !ScriptRegistry::has(f.scriptClass))
            label += ScriptReloader::supported() ? "  (not loaded)" : "  (rebuild)";
        drawText(label, ix + 26, row.y + 4, f.kind == FileEntry::Scene && f.path == scenePath_ ? kAccent : kText);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            drag_ = DragState{DragKind::File, nullptr, rows[i].file, mouse, false};
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) fileMenu(rows[i].file, mouse);
    }
    EndScissorMode();
}

void Editor::activateFile(size_t index) {
    if (index >= files_.size()) return;
    const FileEntry f = files_[index];
    switch (f.kind) {
        case FileEntry::Texture:
            if (auto* sprite = selected_ ? selected_->as<Sprite2D>() : nullptr) {
                touch();
                sprite->texture = f.path;
                log("Set " + sprite->name + ".texture = " + f.path);
            } else {
                log("Drag " + f.path + " into the viewport to make a Sprite2D, or select a Sprite2D first.", kDim);
            }
            break;
        case FileEntry::Scene: requestOpenScene(f.path); break;
        case FileEntry::Script:
            if (f.scriptClass.empty()) {
                log(f.label + " has no REGISTER_SCRIPT(...) line, so it can't be attached.", kBad);
            } else if (selected_) {
                touch();
                selected_->scriptName = f.scriptClass;
                log("Attached " + f.scriptClass + " to " + selected_->name);
            } else {
                log("Select a node first, then click a script to attach it. Right-click it to open it in your code editor.",
                    kDim);
            }
            break;
    }
}

// ===========================================================================
// Drag and drop
// ===========================================================================
void Editor::finishDrag() {
    if (drag_.kind == DragKind::None) return;
    raylib::Vector2 mouse = GetMousePosition();
    if (!drag_.active && mouse.Distance(drag_.start) > 6) drag_.active = true;

    if (drag_.active) {
        std::string label = drag_.kind == DragKind::TreeNode
                                ? (drag_.node ? drag_.node->name : std::string())
                                : (drag_.file < files_.size() ? files_[drag_.file].label : std::string());
        float w = textWidth(label) + 16;
        DrawRectangleRec({mouse.x + 14, mouse.y + 6, w, 24}, Fade(kHeader, 0.9f));
        DrawRectangleLinesEx({mouse.x + 14, mouse.y + 6, w, 24}, 1, kAccent);
        drawText(label, mouse.x + 22, mouse.y + 10, kText);
    }

    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) return;

    DragState d = drag_;
    drag_ = DragState{};
    if (!d.active) {
        if (d.kind == DragKind::File) activateFile(d.file);
        return;
    }
    if (d.kind == DragKind::TreeNode) {
        if (dropTarget_) reparent(d.node, dropTarget_, dropZone_);
        return;
    }
    if (d.file >= files_.size() || playTree_) return;
    const FileEntry f = files_[d.file];
    raylib::Vector2 world = viewCam_.GetScreenToWorld(mouse);

    if (mouseIn(viewportRect_)) {
        if (f.kind == FileEntry::Texture) {
            createSpriteAt(f.path, world);
        } else if (f.kind == FileEntry::Scene) {
            instanceScene(f.path, editTree_.root(), &world);  // drop a scene in: instance it there
        } else if (Node2D* hit = pickNode(world)) {
            select(selectableFor(hit));
            activateFile(d.file);
        }
    } else if (dropTarget_ && f.kind == FileEntry::Scene) {
        instanceScene(f.path, dropTarget_);  // onto a node in the tree: instance it as a child
    } else if (dropTarget_) {
        select(dropTarget_);
        activateFile(d.file);
    } else if (mouseIn(inspectorRect_)) {
        activateFile(d.file);
    }
}

void Editor::reparent(Node* node, Node* target, int zone) {
    if (!node || !target || node == target || node->isAncestorOf(target) || !node->parent()) return;
    if (lockedByInstance(node, "moved")) return;
    if ((zone == 0 && (!target->sceneFile.empty() || target->inInstance)) || (zone != 0 && target->inInstance)) {
        log("Nodes can't be added inside an instanced scene here; open its scene file to change what's in it.", kDim);
        return;
    }
    Node* newParent = target;
    int index = -1;
    if (zone != 0) {
        if (!target->parent()) return;
        newParent = target->parent();
        index = target->indexInParent() + (zone > 0 ? 1 : 0);
        if (node->parent() == newParent && node->indexInParent() < index) --index;
    }
    touch();
    auto* n2 = node->as<Node2D>();
    raylib::Vector2 global = n2 ? n2->globalPosition() : raylib::Vector2{0, 0};
    auto owned = node->detach();
    owned->name = uniqueChildName(newParent, owned->name);
    newParent->addChild(std::move(owned), index);
    if (n2) n2->setGlobalPosition(global);  // stays where it was on screen
    newParent->collapsed = false;
}

// ===========================================================================
// Viewport
// ===========================================================================
raylib::Vector2 Editor::rotateHandlePos() const {
    auto* n = selected_ ? selected_->as<Node2D>() : nullptr;
    if (!n) return {-1000, -1000};
    raylib::Vector2 origin = viewCam_.GetWorldToScreen(n->globalPosition());
    float a = n->globalRotation() * DEG2RAD;
    return origin + raylib::Vector2{std::sin(a), -std::cos(a)} * 60.0f;
}

bool Editor::overRotateHandle(Vector2 mouse) {
    return selected_ && selected_->as<Node2D>() && rotateHandlePos().Distance(mouse) < 10;
}

Node2D* Editor::selectableFor(Node2D* hit) {
    if (hit == selected_) return hit;  // keep dragging what's already selected
    // Part of an instanced scene: select the instance (edit the rest in its own file).
    Node* n = hit;
    while (n->inInstance && n->parent()) n = n->parent();
    if (auto* n2 = n->as<Node2D>()) hit = n2;
    // A sprite or shape inside a physics body picks the body, so the whole thing
    // moves together. Pick the child in the Scene dock to move it on its own.
    if (hit != selected_ && hit->sceneFile.empty() && dynamic_cast<CollisionObject2D*>(hit->parent()))
        hit = hit->parent2D();
    return hit;
}

Node2D* Editor::pickNode(Vector2 world) {
    std::vector<Node2D*> order;
    forEachNode(editTree_.root(), [&](Node* n) {
        if (auto* n2 = n->as<Node2D>(); n2 && n2->isVisibleInTree()) order.push_back(n2);
    });
    // Last drawn is on top, so check from the end.
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        Node2D* n = *it;
        Rectangle lr = n->localRect();
        if (lr.width > 0 && lr.height > 0) {
            if (CheckCollisionPointRec(n->toLocal(world), lr)) return n;
        } else if (n->globalPosition().Distance(world) * viewCam_.zoom < 10) {
            return n;
        }
    }
    return nullptr;
}

void Editor::updateViewportInput(Rectangle r) {
    raylib::Vector2 mouse = GetMousePosition();
    bool inside = CheckCollisionPointRec(mouse, r);
    raylib::Vector2 world = viewCam_.GetScreenToWorld(mouse);

    // Zoom towards the mouse
    float wheel = inside ? GetMouseWheelMove() : 0.0f;
    if (wheel != 0) {
        viewCam_.zoom = std::clamp(viewCam_.zoom * std::pow(1.15f, wheel), 0.05f, 20.0f);
        raylib::Vector2 after = viewCam_.GetScreenToWorld(mouse);
        viewCam_.target = raylib::Vector2(viewCam_.target) + (world - after);
        world = after;
    }

    // Pan with the middle or right mouse button, or Space + left drag
    bool spaceDrag = IsKeyDown(KEY_SPACE) && !isTyping();
    if (inside && (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                   (spaceDrag && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))))
        panning_ = true;
    if (panning_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
            (spaceDrag && IsMouseButtonDown(MOUSE_BUTTON_LEFT))) {
            viewCam_.target = raylib::Vector2(viewCam_.target) - raylib::Vector2(GetMouseDelta()) / viewCam_.zoom;
            mouseCursor_ = MOUSE_CURSOR_RESIZE_ALL;
            return;
        }
        panning_ = false;
    }

    auto* sel = selected_ ? selected_->as<Node2D>() : nullptr;
    if (inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && drag_.kind == DragKind::None) {
        if (sel && overRotateHandle(mouse)) {
            viewDrag_ = ViewDrag::Rotate;
        } else if (Node2D* hit = pickNode(world)) {
            hit = selectableFor(hit);
            select(hit);
            viewDrag_ = ViewDrag::Move;
            grabOffset_ = hit->globalPosition() - world;
        } else {
            select(nullptr);
        }
        sel = selected_ ? selected_->as<Node2D>() : nullptr;
    }
    if (inside && sel && overRotateHandle(mouse)) mouseCursor_ = MOUSE_CURSOR_POINTING_HAND;

    if (viewDrag_ != ViewDrag::None) {
        if (!sel || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            viewDrag_ = ViewDrag::None;
        } else if (raylib::Vector2(GetMouseDelta()).Length() > 0) {
            touch();
            if (viewDrag_ == ViewDrag::Move) {
                raylib::Vector2 p = world + grabOffset_;
                if (ctrlDown()) p = raylib::Vector2{std::round(p.x / 8) * 8, std::round(p.y / 8) * 8};  // snap to the grid
                else if (viewCam_.zoom <= 2) p = raylib::Vector2{std::round(p.x), std::round(p.y)};   // whole pixels
                sel->setGlobalPosition(p);
                mouseCursor_ = MOUSE_CURSOR_RESIZE_ALL;
            } else {
                raylib::Vector2 o = sel->globalPosition();
                float angle = std::atan2(world.y - o.y, world.x - o.x) * RAD2DEG + 90.0f;
                Node2D* p = sel->parent2D();
                angle -= p ? p->globalRotation() : 0.0f;
                if (shiftDown()) angle = std::round(angle / 15.0f) * 15.0f;
                angle = std::fmod(angle + 540.0f, 360.0f) - 180.0f;
                sel->rotation = angle;
            }
        }
    }

    // Nudge with the arrow keys
    if (sel && inside && !isTyping()) {
        float step = shiftDown() ? 10.0f : 1.0f;
        raylib::Vector2 move{0, 0};
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) move.x -= step;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) move.x += step;
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) move.y -= step;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) move.y += step;
        if (move.x != 0 || move.y != 0) {
            touch();
            sel->setGlobalPosition(sel->globalPosition() + move);
        }
    }
}

void Editor::drawGrid(Rectangle r) {
    raylib::Vector2 a = viewCam_.GetScreenToWorld({r.x, r.y});
    raylib::Vector2 b = viewCam_.GetScreenToWorld({r.x + r.width, r.y + r.height});
    float step = 64;
    while (step * viewCam_.zoom < 24) step *= 2;
    while (step * viewCam_.zoom > 160 && step > 8) step /= 2;
    float line = 1.0f / viewCam_.zoom;
    Color minor = theme().viewport.grid;
    for (float x = std::floor(a.x / step) * step; x <= b.x; x += step) DrawLineEx({x, a.y}, {x, b.y}, line, minor);
    for (float y = std::floor(a.y / step) * step; y <= b.y; y += step) DrawLineEx({a.x, y}, {b.x, y}, line, minor);
    DrawLineEx({a.x, 0}, {b.x, 0}, line * 1.5f, theme().viewport.xAxis);
    DrawLineEx({0, a.y}, {0, b.y}, line * 1.5f, theme().viewport.yAxis);
}

void Editor::drawViewport(Rectangle r) {
    if (playTree_) {
        drawGameView(r);
        return;
    }
    viewCam_.offset = {r.x + r.width / 2, r.y + r.height / 2};
    if (!viewFramed_ || (IsKeyPressed(KEY_HOME) && !isTyping())) {
        // Show the whole game window (at most 100%) when the editor starts, or on Home.
        raylib::Vector2 game = editTree_.gameSize;
        float fit = std::min((r.width - 40) / game.x, (r.height - 60) / game.y);
        viewCam_.zoom = std::clamp(std::floor(fit * 20) / 20, 0.1f, 1.0f);
        viewCam_.target = game / 2;
        viewFramed_ = true;
    }
    if (!GuiIsLocked()) updateViewportInput(r);
    viewCam_.offset = {r.x + r.width / 2, r.y + r.height / 2};

    beginClip(r);
    DrawRectangleRec(r, theme().viewport.background);
    drawThemeImage(theme().images.viewport, r, false, theme().images.viewportTint);

    // The view camera works in UI pixels; draw through a copy scaled to screen pixels.
    raylib::Camera2D drawCam = viewCam_;
    drawCam.offset = raylib::Vector2(viewCam_.offset) * uiScale_;
    drawCam.zoom = viewCam_.zoom * uiScale_;
    drawCam.BeginMode();
    drawGrid(r);
    float px = 1.0f / viewCam_.zoom;  // one screen pixel in world units
    DrawRectangleLinesEx({-px, -px, editTree_.gameSize.x + 2 * px, editTree_.gameSize.y + 2 * px}, 2 * px,
                         theme().viewport.gameFrame);  // the game window
    editTree_.draw({true, viewCam_.zoom});

    // Nodes with no size get a small cross so they can be seen and clicked
    forEachNode(editTree_.root(), [&](Node* n) {
        auto* n2 = n->as<Node2D>();
        if (!n2 || !n2->isVisibleInTree()) return;
        Rectangle lr = n2->localRect();
        if (lr.width > 0 && lr.height > 0) return;
        raylib::Vector2 p = n2->globalPosition();
        Color c = Fade(colorFor(n), 0.8f);
        DrawLineEx({p.x - 6 * px, p.y}, {p.x + 6 * px, p.y}, 2 * px, c);
        DrawLineEx({p.x, p.y - 6 * px}, {p.x, p.y + 6 * px}, 2 * px, c);
    });

    auto* sel = selected_ ? selected_->as<Node2D>() : nullptr;
    if (sel) {
        Rectangle lr = sel->localRect();
        Color selColor = theme().viewport.selection;
        if (lr.width > 0 && lr.height > 0) drawLocalRect(*sel, lr, BLANK, selColor, 2 * px);
        else DrawCircleLinesV(sel->globalPosition(), 9 * px, selColor);
    }
    drawCam.EndMode();
    applyUiScale();

    // Move/rotate handle, drawn in screen space so it's the same size at any zoom
    if (sel) {
        raylib::Vector2 o = viewCam_.GetWorldToScreen(sel->globalPosition());
        raylib::Vector2 h = rotateHandlePos();
        bool hot = overRotateHandle(GetMousePosition()) || viewDrag_ == ViewDrag::Rotate;
        Color selColor = theme().viewport.selection;
        DrawLineEx(o, h, 2, Fade(selColor, 0.8f));
        DrawCircleV(h, hot ? 8.0f : 6.0f, hot ? theme().viewport.handleHot : selColor);
        DrawCircleV(o, 4, selColor);
    }

    if (drag_.active && drag_.kind == DragKind::File && drag_.file < files_.size() && mouseIn(r)) {
        const FileEntry& f = files_[drag_.file];
        if (f.kind == FileEntry::Texture) {
            if (raylib::Texture* tex = Assets::texture(f.path)) {
                raylib::Vector2 m = GetMousePosition();
                float w = tex->width * viewCam_.zoom, h = tex->height * viewCam_.zoom;
                DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height}, {m.x - w / 2, m.y - h / 2, w, h},
                               {0, 0}, 0, Fade(WHITE, 0.6f));
            }
        }
    }

    // Info overlay
    raylib::Vector2 world = viewCam_.GetScreenToWorld(GetMousePosition());
    char info[128];
    std::snprintf(info, sizeof info, "Zoom %d%%   Mouse %.0f, %.0f", static_cast<int>(viewCam_.zoom * 100 + 0.5f),
                  world.x, world.y);
    drawText(info, r.x + 10, r.y + 8, kDim);
    drawText("Drag: move  Handle: rotate (Shift snaps)  Ctrl: grid snap  Wheel: zoom  Right/Middle drag: pan  Home: show all",
             r.x + 10, r.y + r.height - 24, Fade(kDim, 0.8f));
    EndScissorMode();
    DrawRectangleLinesEx(r, 1, kBorder);
}

void Editor::renderGame() {
    int w = static_cast<int>(playTree_->gameSize.x), h = static_cast<int>(playTree_->gameSize.y);
    if (!gameTexture_ || gameTexture_->texture.width != w || gameTexture_->texture.height != h)
        gameTexture_ = std::make_unique<raylib::RenderTexture>(w, h);

    gameTexture_->BeginMode();
    ClearBackground(playTree_->clearColor);
    raylib::Camera2D cam({0, 0}, {0, 0}, 0, 1);
    if (Camera2D* c = playTree_->currentCamera()) {
        cam.offset = playTree_->gameSize / 2;
        cam.target = c->globalPosition();
        cam.zoom = c->zoom > 0.01f ? c->zoom : 0.01f;
    }
    cam.BeginMode();
    playTree_->draw({false, cam.zoom});
    cam.EndMode();
    gameTexture_->EndMode();
}

void Editor::drawGameView(Rectangle r) {
    DrawRectangleRec(r, theme().viewport.playingBackground);
    if (gameTexture_) {
        float gw = static_cast<float>(gameTexture_->texture.width);
        float gh = static_cast<float>(gameTexture_->texture.height);
        float s = std::min((r.width - 20) / gw, (r.height - 50) / gh);
        Rectangle dst{r.x + (r.width - gw * s) / 2, r.y + 36 + (r.height - 36 - gh * s) / 2, gw * s, gh * s};
        DrawTexturePro(gameTexture_->texture, {0, 0, gw, -gh}, dst, {0, 0}, 0, WHITE);
    }
    drawText("Game running - press F8 or Esc (or Stop) to go back to editing. Edits made while playing are not kept.",
             r.x + 10, r.y + 10, kOrange);
    DrawRectangleLinesEx(r, 2, theme().viewport.playingBorder);
}

// ===========================================================================
// Inspector
// ===========================================================================
bool Editor::floatField(Rectangle r, const std::string& id, float& value, float step) {
    bool changed = false;
    raylib::Vector2 mouse = GetMousePosition();

    if (activeField_ == id) {
        bool relock = clipLocked_;
        if (relock) GuiUnlock();  // keep typing even if the mouse leaves the panel
        bool done = GuiTextBox(r, fieldBuffer_, 64, true);
        if (relock) GuiLock();
        if (IsKeyPressed(KEY_ESCAPE)) {
            activeField_.clear();
        } else if (done) {
            char* end = nullptr;
            float v = std::strtof(fieldBuffer_, &end);
            if (end != fieldBuffer_ && v != value) {
                touch();
                value = v;
                changed = true;
            }
            activeField_.clear();
        }
        return changed;
    }

    bool hover = !GuiIsLocked() && CheckCollisionPointRec(mouse, r);
    DrawRectangleRec(r, kFieldBg);
    DrawRectangleLinesEx(r, 1, (hover || scrubField_ == id) ? kAccent : kBorder);
    drawText(formatFloat(value), r.x + 6, r.y + (r.height - textSize()) / 2, kText);
    if (hover) mouseCursor_ = MOUSE_CURSOR_RESIZE_EW;

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        scrubField_ = id;
        scrubStartValue_ = value;
        scrubStartX_ = mouse.x;
        scrubMoved_ = false;
    }
    if (scrubField_ == id) {
        mouseCursor_ = MOUSE_CURSOR_RESIZE_EW;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float dx = mouse.x - scrubStartX_;
            if (std::fabs(dx) > 2) scrubMoved_ = true;
            if (scrubMoved_) {
                float v = scrubStartValue_ + dx * step * (shiftDown() ? 0.1f : 1.0f);
                if (step >= 1.0f) v = std::round(v);
                if (v != value) {
                    touch();
                    value = v;
                    changed = true;
                }
            }
        } else {
            if (!scrubMoved_) {  // a click without dragging: type a value instead
                activeField_ = id;
                std::snprintf(fieldBuffer_, sizeof fieldBuffer_, "%s", formatFloat(value).c_str());
                moveTextCursorToEnd();
            }
            scrubField_.clear();
        }
    }
    return changed;
}

bool Editor::textField(Rectangle r, const std::string& id, std::string& value) {
    if (activeField_ == id) {
        bool relock = clipLocked_;
        if (relock) GuiUnlock();
        bool done = GuiTextBox(r, fieldBuffer_, sizeof fieldBuffer_, true);
        if (relock) GuiLock();
        if (IsKeyPressed(KEY_ESCAPE)) {
            activeField_.clear();
        } else if (done) {
            activeField_.clear();
            if (value != fieldBuffer_) {
                touch();
                value = fieldBuffer_;
                return true;
            }
        }
        return false;
    }
    char shown[256];
    std::snprintf(shown, sizeof shown, "%s", value.c_str());
    if (GuiTextBox(r, shown, sizeof shown, false)) {
        activeField_ = id;
        std::snprintf(fieldBuffer_, sizeof fieldBuffer_, "%s", value.c_str());
        moveTextCursorToEnd();
    }
    return false;
}

void Editor::propertyRow(Property& p, float x, float& y, float width, const std::string& idPrefix) {
    if (p.kind == PropKind::Section) {
        y += 4;
        DrawRectangleRec({x - 4, y, width + 8, 24}, kHeader);
        drawText(p.name, x + 4, y + 4, kAccent);
        y += 30;
        return;
    }

    const float labelW = 112;
    float fx = x + labelW, fw = width - labelW;
    std::string id = std::to_string(reinterpret_cast<uintptr_t>(selected_)) + "/" + idPrefix + p.name;
    std::string label = p.name;
    std::replace(label.begin(), label.end(), '_', ' ');

    // Changed from the original scene (this node is part of an instance)? Show it, with a revert button.
    if (inspectorOriginal_.is_object() && inspectorOriginal_.contains(p.name) && inspectorCurrent_.contains(p.name) &&
        inspectorOriginal_[p.name] != inspectorCurrent_[p.name]) {
        DrawRectangleRec({x - 6, y + 4, 3, 16}, kOrange);
        drawText(label, x, y + 4, kOrange);
        if (GuiButton({x + labelW - 26, y + 2, 22, 20}, GuiIconText(ICON_UNDO, nullptr))) {
            touch();
            nlohmann::json one;
            one[p.name] = inspectorOriginal_[p.name];
            applyPropertyList({p}, one);
        }
    } else {
        drawText(label, x, y + 4, kDim);
    }

    switch (p.kind) {
        case PropKind::Section: break;
        case PropKind::Bool: {
            bool* b = static_cast<bool*>(p.ptr);
            bool v = *b;
            GuiCheckBox({fx, y + 3, 18, 18}, v ? "On" : "Off", &v);
            if (v != *b) {
                touch();
                *b = v;
            }
            break;
        }
        case PropKind::Int: {
            int* iv = static_cast<int*>(p.ptr);
            float f = static_cast<float>(*iv);
            if (floatField({fx, y, fw, 24}, id, f, p.step)) *iv = static_cast<int>(std::lround(f));
            break;
        }
        case PropKind::Float: floatField({fx, y, fw, 24}, id, *static_cast<float*>(p.ptr), p.step); break;
        case PropKind::Vec2: {
            auto* v = static_cast<Vector2*>(p.ptr);
            float half = (fw - 4) / 2;
            floatField({fx, y, half, 24}, id + ".x", v->x, p.step);
            floatField({fx + half + 4, y, half, 24}, id + ".y", v->y, p.step);
            DrawRectangleRec({fx + 1, y + 1, 3, 22}, theme().viewport.vectorX);  // x is red, y is green
            DrawRectangleRec({fx + half + 5, y + 1, 3, 22}, theme().viewport.vectorY);
            break;
        }
        case PropKind::String: textField({fx, y, fw, 24}, id, *static_cast<std::string*>(p.ptr)); break;
        case PropKind::Color: {
            auto* c = static_cast<Color*>(p.ptr);
            bool open = openColorPicker_ == id;
            DrawRectangleRec({fx, y, fw - 62, 24}, *c);
            DrawRectangleLinesEx({fx, y, fw - 62, 24}, 1, kBorder);
            if (GuiButton({fx + fw - 58, y, 58, 24}, open ? "Done" : "Edit")) openColorPicker_ = open ? "" : id;
            if (open) {
                y += 30;
                Color tmp = *c;
                GuiColorPicker({x, y, width - 30, 110}, nullptr, &tmp);
                y += 116;
                float alpha = tmp.a / 255.0f;
                GuiColorBarAlpha({x, y, width - 30, 16}, nullptr, &alpha);
                tmp.a = static_cast<unsigned char>(std::lround(alpha * 255));
                if (tmp.r != c->r || tmp.g != c->g || tmp.b != c->b || tmp.a != c->a) {
                    touch();
                    *c = tmp;
                }
                y -= 6;
            }
            break;
        }
        case PropKind::AnimationName: {
            auto* name = static_cast<std::string*>(p.ptr);
            if (GuiButton({fx, y, fw, 24}, name->empty() ? "<none>" : name->c_str())) {
                std::vector<std::string> items;
                if (auto* sprite = selected_->as<AnimatedSprite2D>())
                    for (auto& a : sprite->animations) items.push_back(a.name);
                Node* target = selected_;
                if (!items.empty())
                    openPopup({fx, y + 26}, items, [this, items, target, name](int i) {
                        if (selected_ != target) return;
                        touch();
                        *name = items[i];
                        storeScriptVars();
                    });
            }
            break;
        }
        case PropKind::Animations: {
            auto* list = static_cast<std::vector<SpriteAnimation>*>(p.ptr);
            y += 26;
            animationsEditor(*list, id, x, y, width);
            return;  // (it moved y itself)
        }
        case PropKind::Scene: {
            auto* path = static_cast<std::string*>(p.ptr);
            if (GuiButton({fx, y, fw - 28, 24}, path->empty() ? "<none>" : path->c_str())) {
                std::vector<std::string> items{"<none>"};
                for (auto& f : files_)
                    if (f.kind == FileEntry::Scene) items.push_back(f.path);
                Node* target = selected_;
                openPopup({fx, y + 26}, items, [this, items, target, path](int i) {
                    if (selected_ != target) return;
                    touch();
                    *path = i == 0 ? "" : items[i];
                    storeScriptVars();  // (if this is a script variable, keep it before the next frame)
                });
            }
            if (GuiButton({fx + fw - 24, y, 24, 24}, GuiIconText(ICON_CROSS_SMALL, nullptr)) && !path->empty()) {
                touch();
                path->clear();
            }
            break;
        }
        case PropKind::Texture: {
            auto* path = static_cast<std::string*>(p.ptr);
            if (GuiButton({fx, y, fw - 28, 24}, path->empty() ? "<empty>" : path->c_str())) {
                std::vector<std::string> items{"<empty>"};
                for (auto& f : files_)
                    if (f.kind == FileEntry::Texture) items.push_back(f.path);
                Node* target = selected_;
                openPopup({fx, y + 26}, items, [this, items, target, path](int i) {
                    if (selected_ != target) return;
                    touch();
                    *path = i == 0 ? "" : items[i];
                    storeScriptVars();  // (if this is a script variable, keep it before the next frame)
                });
            }
            if (GuiButton({fx + fw - 24, y, 24, 24}, GuiIconText(ICON_CROSS_SMALL, nullptr)) && !path->empty()) {
                touch();
                path->clear();
            }
            break;
        }
    }
    y += 30;
}

void Editor::animationsEditor(std::vector<SpriteAnimation>& list, const std::string& id, float x, float& y,
                              float width) {
    // The sprite sheet, with each frame's number on it, so frames are easy to pick.
    if (auto* sprite = selected_->as<Sprite2D>()) {
        if (raylib::Texture* tex = Assets::texture(sprite->texture)) {
            int cols = std::max(sprite->hframes, 1), rows = std::max(sprite->vframes, 1);
            float s = std::min(width / tex->width, 140.0f / tex->height);
            s = s >= 1 ? std::floor(s) : s;  // whole-pixel zoom when it fits
            Rectangle dst{x, y, tex->width * s, tex->height * s};
            DrawRectangleRec(dst, kFieldBg);
            DrawTexturePro(*tex, {0, 0, (float)tex->width, (float)tex->height}, dst, {0, 0}, 0, WHITE);
            float cw = dst.width / cols, ch = dst.height / rows;
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    Rectangle cell{dst.x + c * cw, dst.y + r * ch, cw, ch};
                    DrawRectangleLinesEx(cell, 1, Fade(kAccent, 0.6f));
                    std::string n = std::to_string(r * cols + c);
                    DrawRectangleRec({cell.x + 1, cell.y + 1, textWidth(n) + 4, 16}, Fade(BLACK, 0.6f));
                    drawText(n, cell.x + 3, cell.y + 1, kText);
                }
            }
            y += dst.height + 8;
            if (cols * rows == 1) {
                drawText("Set hframes / vframes to cut the picture into frames.", x, y, kOrange);
                y += 22;
            }
        }
    }

    // One block per animation: name, speed, loop, delete; then its frames.
    int remove = -1;
    for (size_t i = 0; i < list.size(); ++i) {
        SpriteAnimation& a = list[i];
        std::string aid = id + "/" + std::to_string(i);
        DrawRectangleRec({x - 4, y - 2, width + 8, 60}, Fade(kHeader, 0.6f));
        float nameW = width - 182;
        textField({x, y, nameW, 24}, aid + "/name", a.name);
        drawText("fps", x + nameW + 6, y + 4, kDim);
        floatField({x + nameW + 34, y, 52, 24}, aid + "/fps", a.fps, 0.1f);
        bool loop = a.loop;
        GuiCheckBox({x + nameW + 94, y + 3, 18, 18}, "loop", &loop);
        if (loop != a.loop) {
            touch();
            a.loop = loop;
        }
        if (GuiButton({x + width - 24, y, 24, 24}, GuiIconText(ICON_BIN, nullptr))) remove = static_cast<int>(i);
        y += 28;
        drawText("frames", x, y + 4, kDim);
        textField({x + 60, y, width - 60, 24}, aid + "/frames", a.frames);
        y += 34;
    }
    if (remove >= 0) {
        touch();
        list.erase(list.begin() + remove);
    }
    if (GuiButton({x, y, width, 26}, GuiIconText(ICON_CIRCLE_ADD, "Add Animation"))) {
        touch();
        SpriteAnimation a;
        a.name = "anim" + std::to_string(list.size() + 1);
        list.push_back(a);
    }
    y += 32;
    drawText("frames: 0-3 or 0, 2, 4 (sheet numbers)", x, y, kDim);
    y += 26;
}

void Editor::scriptVariableRows(float x, float& y, float width) {
    if (!selected_ || !ScriptRegistry::has(selected_->scriptName)) {
        inspectorScript_.reset();
        return;
    }
    if (!inspectorScript_ || inspectorScriptNode_ != selected_ || inspectorScriptName_ != selected_->scriptName) {
        inspectorScript_ = ScriptRegistry::create(selected_->scriptName);
        inspectorScriptNode_ = selected_;
        inspectorScriptName_ = selected_->scriptName;
        Exports defaults;
        inspectorScript_->exports(defaults);
        inspectorScriptDefaults_ = propertyListToJson(defaults.list);
        applyPropertyList(defaults.list, scriptVarsToJson(*selected_));
    }
    Exports e;
    inspectorScript_->exports(e);
    if (e.list.empty()) return;

    // The node's saved values go into the copy, the rows edit the copy...
    applyPropertyList(e.list, scriptVarsToJson(*selected_));
    nlohmann::json before = propertyListToJson(e.list);
    Property section{"Script Variables", PropKind::Section};
    propertyRow(section, x, y, width);
    // (orange + revert = different from the script's default)
    nlohmann::json savedOriginal = inspectorOriginal_, savedCurrent = inspectorCurrent_;
    inspectorOriginal_ = inspectorScriptDefaults_;
    inspectorCurrent_ = before;
    for (Property& p : e.list) propertyRow(p, x, y, width, "script:");
    inspectorOriginal_ = savedOriginal;
    inspectorCurrent_ = savedCurrent;
    storeScriptVars();  // ...and whatever changed goes back to the node
}

void Editor::storeScriptVars() {
    if (!selected_ || !inspectorScript_ || inspectorScriptNode_ != selected_) return;
    Exports e;
    inspectorScript_->exports(e);
    // Only values that differ from the script's defaults are kept.
    nlohmann::json now = propertyListToJson(e.list);
    std::map<std::string, std::string> vars;
    for (auto& [name, value] : now.items())
        if (!inspectorScriptDefaults_.contains(name) || inspectorScriptDefaults_[name] != value) vars[name] = value.dump();
    // Keep values for variables the script doesn't have (any more), so nothing is lost.
    for (auto& [name, text] : selected_->scriptVars)
        if (!now.contains(name)) vars[name] = text;
    if (vars != selected_->scriptVars) selected_->scriptVars = vars;
}

void Editor::drawInspector(Rectangle r) {
    drawHeader(r, "Inspector");
    if (!selected_) {
        drawText("Select a node in the Scene dock", r.x + 12, r.y + kHeaderH + 14, kDim);
        drawText("or click one in the viewport.", r.x + 12, r.y + kHeaderH + 34, kDim);
        return;
    }

    Rectangle area{r.x, r.y + kHeaderH, r.width, r.height - kHeaderH};
    Rectangle content{0, 0, area.width - 16, inspectorContentHeight_};
    Rectangle view{};
    GuiScrollPanel(area, nullptr, content, &inspectorScroll_, &view);
    beginClip(view);
    ClipLock clip(view, clipLocked_);

    float x = view.x + 10, w = view.width - 20;
    float top = view.y + inspectorScroll_.y + 8, y = top;

    drawNodeIcon(selected_, x, y + 2);
    drawText(selected_->typeName(), x + 24, y + 2, kText);
    y += 30;

    // In an instanced scene? Find the original values, to mark what this copy changed.
    Node* instanceRoot = selected_;
    while (instanceRoot->inInstance && instanceRoot->parent()) instanceRoot = instanceRoot->parent();
    if (instanceRoot->sceneFile.empty() || instanceRoot->instanceMissing) instanceRoot = nullptr;
    inspectorOriginal_ = nullptr;
    if (instanceRoot) {
        std::string key = std::to_string(reinterpret_cast<uintptr_t>(instanceRoot)) + instanceRoot->sceneFile;
        if (key != originalKey_ || GetTime() - originalTime_ > 0.5) {  // the original file may have changed
            originalTree_ = instanceSource(instanceRoot->sceneFile);
            originalKey_ = key;
            originalTime_ = GetTime();
        }
        Node* original = !originalTree_ ? nullptr
                         : instanceRoot == selected_
                             ? originalTree_.get()
                             : originalTree_->getNode(pathBetween(instanceRoot, selected_));
        if (original) {
            inspectorOriginal_ = propertiesToJson(*original);
            inspectorCurrent_ = propertiesToJson(*selected_);
        }
    }

    Property nameProp{"name", PropKind::String, &selected_->name};
    std::string oldName = selected_->name;
    if (selected_->inInstance) {
        // Changes inside an instance are remembered by node name, so it stays fixed.
        drawText("name", x, y + 4, kDim);
        drawText(selected_->name, x + 112, y + 4, kText);
        y += 30;
    } else {
        propertyRow(nameProp, x, y, w);
    }
    if (selected_->name.empty()) selected_->name = oldName;
    if (selected_->name != oldName && selected_->parent()) {
        std::string wanted = selected_->name;
        selected_->name = "";  // so it doesn't clash with itself
        selected_->name = uniqueChildName(selected_->parent(), wanted);
    }

    std::vector<Property> props;
    selected_->getProperties(props);
    for (Property& p : props) propertyRow(p, x, y, w);

    // Script
    Property section{"Script", PropKind::Section};
    propertyRow(section, x, y, w);
    std::string status = selected_->scriptName.empty() ? "<none>" : selected_->scriptName;
    Color statusColor = selected_->scriptName.empty() ? kDim
                        : ScriptRegistry::has(selected_->scriptName) ? kGood
                                                                     : kOrange;
    drawText(status, x, y + 4, statusColor);
    if (fs::path file = scriptFileOf(selected_->scriptName); !file.empty()) {
        if (GuiButton({x + w - 76, y, 76, 24}, GuiIconText(ICON_PENCIL, "Edit"))) openInCodeEditor(file);
    }
    if (!selected_->scriptName.empty() && !ScriptRegistry::has(selected_->scriptName)) {
        y += 22;
        drawText(ScriptReloader::supported() ? "not loaded - check the Output panel" : "not compiled yet - rebuild the editor",
                 x, y + 4, kOrange);
    }
    y += 30;
    float bw = (w - 8) / 3;
    if (GuiButton({x, y, bw, 26}, GuiIconText(ICON_LINK_BOXES, "Attach"))) {
        std::vector<std::string> names = ScriptRegistry::names();
        if (names.empty()) {
            log("No scripts are compiled in yet. Press New to make one.", kDim);
        } else {
            Node* target = selected_;
            openPopup({x, y + 28}, names, [this, names, target](int i) {
                if (selected_ != target) return;
                touch();
                target->scriptName = names[i];
                log("Attached " + names[i] + " to " + target->name);
            });
        }
    }
    if (GuiButton({x + bw + 4, y, bw, 26}, GuiIconText(ICON_FILE_ADD, "New"))) {
        openTextDialog("New C++ Script", "Class name (e.g. PlayerController):",
                       pascalCase(selected_->name) + "Script", [this](const std::string& n) { createScript(n); });
    }
    if (GuiButton({x + 2 * (bw + 4), y, bw, 26}, GuiIconText(ICON_CROSS, "Detach")) && !selected_->scriptName.empty()) {
        touch();
        selected_->scriptName.clear();
    }
    y += 34;
    scriptVariableRows(x, y, w);

    // Scene: instancing (like Godot's "instance" and "Save Branch as Scene")
    if (selected_->parent()) {
        Property sceneSection{"Scene", PropKind::Section};
        propertyRow(sceneSection, x, y, w);
        if (selected_->inInstance) {
            Node* root = selected_;
            while (root->inInstance && root->parent()) root = root->parent();
            drawText("Part of the instance '" + root->name + "'.", x, y + 4, kDim);
            drawText("Changes here only affect this copy;", x, y + 24, kDim);
            drawText("orange fields differ from the original.", x, y + 44, kDim);
            y += 70;
            if (GuiButton({x, y, w, 26}, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open Original Scene")))
                requestOpenScene(root->sceneFile);
        } else if (!selected_->sceneFile.empty()) {
            drawText("Instance of", x, y + 4, kDim);
            drawText(selected_->sceneFile, x + textWidth("Instance of") + 8, y + 4,
                     selected_->instanceMissing ? kBad : kAccent);
            y += 26;
            if (selected_->instanceMissing) {
                drawText("That file is missing.", x, y + 4, kBad);
                y += 26;
            } else {
                drawText("Changes here only affect this copy.", x, y + 4, kDim);
                drawText("Edit the original to change every copy.", x, y + 24, kDim);
                y += 50;
            }
            float hw = (w - 4) / 2;
            if (GuiButton({x, y, w, 26}, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open Original Scene")) &&
                !selected_->instanceMissing)
                requestOpenScene(selected_->sceneFile);
            y += 30;
            if (GuiButton({x, y, hw, 26}, GuiIconText(ICON_UNDO, "Reset Copy")) && !selected_->instanceMissing)
                resetInstance(selected_);
            if (GuiButton({x + hw + 4, y, hw, 26}, GuiIconText(ICON_LINK_BOXES, "Make Local"))) makeLocal(selected_);
        } else {
            drawText("Move this node and its children", x, y + 4, kDim);
            drawText("into their own scene file:", x, y + 24, kDim);
            y += 48;
            if (GuiButton({x, y, w, 26}, GuiIconText(ICON_FILE_SAVE, "Save Branch as Scene"))) {
                Node* target = selected_;
                openTextDialog("Save Branch as Scene", "New scene file (inside the project):",
                               "scenes/" + snakeCase(pascalCase(selected_->name)) + ".scene.json",
                               [this, target](const std::string& p) {
                                   if (selected_ == target) saveBranchAsScene(target, p);
                               });
            }
        }
        y += 40;
    }
    inspectorContentHeight_ = y - top + 8;
    EndScissorMode();
}

// ===========================================================================
// Output
// ===========================================================================
void Editor::log(const std::string& message, Color color) {
    std::stringstream ss(message);
    std::string line;
    while (std::getline(ss, line)) {
        // Compilers quote names with curly quotes, which the pixel font doesn't have.
        for (const char* curly : {"\xE2\x80\x98", "\xE2\x80\x99"})
            for (size_t p; (p = line.find(curly)) != std::string::npos;) line.replace(p, 3, "'");
        for (const char* curly : {"\xE2\x80\x9C", "\xE2\x80\x9D"})
            for (size_t p; (p = line.find(curly)) != std::string::npos;) line.replace(p, 3, "\"");
        LogLine entry{line, color, {}, 0, 0};
        if (auto m = parseCompilerLine(line)) {
            // Compiler messages can be clicked to open the file at that line.
            std::error_code ec;
            fs::path file = m->file;
            if (!fs::exists(file, ec)) file = scriptsDir_ / fs::path(m->file).filename();
            if (fs::exists(file, ec)) {
                entry.file = file.string();
                entry.line = m->line;
                entry.column = m->column;
            }
        }
        logLines_.push_back(std::move(entry));
    }
    if (logLines_.size() > 500) logLines_.erase(logLines_.begin(), logLines_.begin() + (logLines_.size() - 500));
    scrollLogToBottom_ = true;
    TraceLog(LOG_INFO, "EDITOR: %s", message.c_str());
}

bool Editor::animationTabShown() const {
    return bottomTab_ == 1 && selected_ && selected_->as<AnimatedSprite2D>();
}

void Editor::drawOutput(Rectangle r) {
    // Tabs: Output, and Animation when an AnimatedSprite2D is selected.
    auto* sprite = selected_ ? selected_->as<AnimatedSprite2D>() : nullptr;
    drawHeader(r, "");
    auto tab = [&](float x, float w, const char* text, int index) {
        bool active = bottomTab_ == index;
        Rectangle t{x, r.y + 2, w, kHeaderH - 2};
        if (active) DrawRectangleRec(t, kPanel);
        drawText(text, x + 10, r.y + 6, active ? kText : kDim);
        if (!GuiIsLocked() && mouseIn(t) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) bottomTab_ = index;
    };
    tab(r.x + 2, 80, "Output", 0);
    if (sprite) tab(r.x + 84, 100, "Animation", 1);
    if (animationTabShown()) {
        drawAnimationPanel({r.x, r.y + kHeaderH, r.width, r.height - kHeaderH}, *sprite);
        return;
    }
    if (GuiButton({r.x + r.width - 64, r.y + 2, 58, 24}, "Clear")) logLines_.clear();

    Rectangle area{r.x, r.y + kHeaderH, r.width, r.height - kHeaderH};
    const float lineH = 20;
    Rectangle content{0, 0, area.width - 16, logLines_.size() * lineH + 8};
    if (scrollLogToBottom_) {
        outputScroll_.y = std::min(0.0f, -(content.height - (area.height - 2)));
        scrollLogToBottom_ = false;
    }
    Rectangle view{};
    GuiScrollPanel(area, nullptr, content, &outputScroll_, &view);
    beginClip(view);
    for (size_t i = 0; i < logLines_.size(); ++i) {
        float y = view.y + outputScroll_.y + 4 + i * lineH;
        if (y + lineH < view.y || y > view.y + view.height) continue;
        drawText(logLines_[i].text, view.x + 8, y, logLines_[i].color);
        const LogLine& l = logLines_[i];
        if (!l.file.empty() && !GuiIsLocked() && mouseIn(view) && mouseIn({view.x, y, view.width, lineH})) {
            DrawLineEx({view.x + 8, y + 17}, {view.x + 8 + textWidth(l.text), y + 17}, 1, l.color);
            mouseCursor_ = MOUSE_CURSOR_POINTING_HAND;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (codeEditor_.builtin) openDocument(l.file, l.line, l.column);
                else openInCodeEditor(l.file);
            }
        }
    }
    EndScissorMode();
}

// ===========================================================================
// Animation panel (like Godot's SpriteFrames)
// ===========================================================================
namespace {

// Where frame `index` of a sheet with cols x rows frames is, in texture pixels.
Rectangle sheetCell(const raylib::Texture& tex, int cols, int rows, int index) {
    cols = std::max(cols, 1);
    rows = std::max(rows, 1);
    float w = static_cast<float>(tex.width) / cols, h = static_cast<float>(tex.height) / rows;
    index = std::clamp(index, 0, cols * rows - 1);
    return {(index % cols) * w, (index / cols) * h, w, h};
}

// Draws a frame of the sheet scaled to fit (and centred in) box, on a checkerboard.
void drawFrameIn(const raylib::Texture& tex, Rectangle cell, Rectangle box, bool flipH = false) {
    for (int cy = 0; cy * 8 < box.height; ++cy)
        for (int cx = 0; cx * 8 < box.width; ++cx)
            DrawRectangleRec({box.x + cx * 8.0f, box.y + cy * 8.0f, std::min(8.0f, box.width - cx * 8),
                              std::min(8.0f, box.height - cy * 8)},
                             (cx + cy) % 2 ? theme().colors.checkerA : theme().colors.checkerB);
    float s = std::min(box.width / cell.width, box.height / cell.height);
    if (s >= 1) s = std::floor(s);  // whole-pixel zoom keeps pixel art sharp
    float w = cell.width * s, h = cell.height * s;
    Rectangle src = cell;
    if (flipH) src.width = -src.width;
    DrawTexturePro(tex, src, {box.x + (box.width - w) / 2, box.y + (box.height - h) / 2, w, h}, {0, 0}, 0, WHITE);
}

}  // namespace

void Editor::syncAnimationToGame() {
    if (!playTree_ || !selected_ || !playTree_->root()) return;
    auto* from = selected_->as<AnimatedSprite2D>();
    Node* match = nodeAt(playTree_->root(), pathOf(selected_));
    auto* to = match ? match->as<AnimatedSprite2D>() : nullptr;
    if (!from || !to || to->name != from->name) return;  // the game has moved on (e.g. another scene)
    to->animations = from->animations;
    to->hframes = from->hframes;
    to->vframes = from->vframes;
    to->texture = from->texture;
    to->speedScale = from->speedScale;
}

void Editor::drawAnimationPanel(Rectangle r, AnimatedSprite2D& sprite) {
    DrawRectangleRec(r, kPanel);
    auto& anims = sprite.animations;
    animSel_ = std::clamp(animSel_, 0, std::max(0, static_cast<int>(anims.size()) - 1));
    SpriteAnimation* anim = anims.empty() ? nullptr : &anims[animSel_];
    std::vector<int> frames = anim ? anim->frameList() : std::vector<int>{};
    frameSel_ = std::clamp(frameSel_, 0, std::max(0, static_cast<int>(frames.size()) - 1));
    raylib::Texture* tex = Assets::texture(sprite.texture);
    bool changed = false;
    std::string id = std::to_string(reinterpret_cast<uintptr_t>(&sprite)) + "/anim/";

    // --- Left: the animations ---------------------------------------------
    const float leftW = 240;
    float x = r.x + 8, y = r.y + 6, w = leftW - 16;
    if (GuiButton({x, y, (w - 4) / 2, 26}, GuiIconText(ICON_CIRCLE_ADD, "New"))) {
        touch();
        SpriteAnimation a;
        a.frames = "";
        std::string base = "new_animation";
        a.name = base;
        for (int n = 2; sprite.findAnimation(a.name); ++n) a.name = base + std::to_string(n);
        anims.push_back(a);  // (may move the list: look the selection up again)
        animSel_ = static_cast<int>(anims.size()) - 1;
        frameSel_ = 0;
        anim = &anims[animSel_];
        frames.clear();
        changed = true;
    }
    if (!anim) GuiDisable();
    if (GuiButton({x + (w + 4) / 2, y, (w - 4) / 2, 26}, GuiIconText(ICON_BIN, "Delete")) && anim) {
        touch();
        anims.erase(anims.begin() + animSel_);
        animSel_ = std::max(0, animSel_ - 1);
        changed = true;
        anim = nullptr;  // drawn again next frame
        frames.clear();
        GuiDisable();    // (so the GuiEnable below matches)
    }
    if (!anim) GuiEnable();
    y += 32;
    if (anim) {
        // Rename (and keep the node's current animation pointing at it)
        drawText("name", x, y + 4, kDim);
        std::string oldName = anim->name;
        if (textField({x + 48, y, w - 48, 24}, id + std::to_string(animSel_) + "/name", anim->name)) {
            if (anim->name.empty()) anim->name = oldName;
            for (auto& other : anims)
                if (&other != anim && other.name == anim->name) anim->name = oldName;  // names must differ
            if (sprite.animation == oldName) sprite.animation = anim->name;
            changed = true;
        }
        y += 30;
        drawText("fps", x, y + 4, kDim);
        if (floatField({x + 48, y, 70, 24}, id + std::to_string(animSel_) + "/fps", anim->fps, 0.1f)) {
            anim->fps = std::max(0.0f, anim->fps);
            changed = true;
        }
        bool loop = anim->loop;
        GuiCheckBox({x + 130, y + 3, 18, 18}, "loop", &loop);
        if (loop != anim->loop) {
            touch();
            anim->loop = loop;
            changed = true;
        }
        y += 30;
    }
    // The list
    Rectangle listArea{r.x + 4, y, leftW - 8, r.y + r.height - y - 4};
    Rectangle listContent{0, 0, listArea.width - 16, anims.size() * 24.0f + 4};
    Rectangle listView{};
    GuiScrollPanel(listArea, nullptr, listContent, &animListScroll_, &listView);
    beginClip(listView);
    for (size_t i = 0; i < anims.size(); ++i) {
        Rectangle row{listView.x, listView.y + animListScroll_.y + 2 + i * 24, listView.width, 24};
        bool hover = !GuiIsLocked() && mouseIn(row) && mouseIn(listView);
        if (static_cast<int>(i) == animSel_) DrawRectangleRec(row, kSelect);
        else if (hover) DrawRectangleRec(row, kHover);
        bool startsWith = sprite.autoplay && sprite.animation == anims[i].name;
        drawText(anims[i].name, row.x + 8, row.y + 4, kText);
        if (startsWith) drawText("(start)", row.x + row.width - textWidth("(start)") - 8, row.y + 4, kGood);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            animSel_ = static_cast<int>(i);
            frameSel_ = 0;
            animPreviewStart_ = GetTime();
        }
    }
    EndScissorMode();
    DrawLineEx({r.x + leftW, r.y}, {r.x + leftW, r.y + r.height}, 1, kBorder);

    // --- Right: the live preview ---------------------------------------------
    float previewSize = std::min(r.height - 34, 200.0f);
    Rectangle preview{r.x + r.width - previewSize - 10, r.y + 6, previewSize, previewSize};
    int shownIndex = frameSel_;
    if (anim && animPreview_ && !frames.empty())
        shownIndex = static_cast<int>(anim->indexAt((GetTime() - animPreviewStart_) * sprite.speedScale, frames.size()));
    if (tex && !frames.empty()) {
        drawFrameIn(*tex, sheetCell(*tex, sprite.hframes, sprite.vframes, frames[shownIndex]), preview, sprite.flipH);
    } else {
        DrawRectangleRec(preview, kFieldBg);
    }
    DrawRectangleLinesEx(preview, 1, kBorder);
    std::string counter = frames.empty() ? "no frames" : "frame " + std::to_string(shownIndex + 1) + " / " +
                                                             std::to_string(frames.size());
    drawText(counter, preview.x, preview.y + preview.height + 6, kDim);

    // --- Middle: toolbar and filmstrip ---------------------------------------
    float mx = r.x + leftW + 10, mw = preview.x - 10 - mx, my = r.y + 6;
    if (!anim) GuiDisable();
    if (GuiButton({mx, my, 84, 26}, GuiIconText(animPreview_ ? ICON_PLAYER_STOP : ICON_PLAYER_PLAY,
                                                 animPreview_ ? "Stop" : "Play"))) {
        animPreview_ = !animPreview_;
        animPreviewStart_ = GetTime();
    }
    float bx = mx + 88;
    if (GuiButton({bx, my, 190, 26}, GuiIconText(ICON_BOX_GRID, "Add Frames from Sheet")) && anim) {
        if (!tex) {
            log("Pick a sprite sheet as the node's texture first (in the inspector).", kOrange);
        } else {
            sheet_ = SheetDialog{};
            sheet_.open = true;
            sheet_.hframes = std::max(sprite.hframes, 1);
            sheet_.vframes = std::max(sprite.vframes, 1);
        }
    }
    bx += 194;
    bool haveFrame = anim && !frames.empty();
    if (!haveFrame) GuiDisable();
    auto moveFrame = [&](int to) {
        if (to < 0 || to >= static_cast<int>(frames.size())) return;
        touch();
        std::swap(frames[frameSel_], frames[to]);
        frameSel_ = to;
        anim->frames = SpriteAnimation::framesText(frames);
        changed = true;
    };
    if (GuiButton({bx, my, 30, 26}, GuiIconText(ICON_ARROW_LEFT_FILL, nullptr))) moveFrame(frameSel_ - 1);
    if (GuiButton({bx + 33, my, 30, 26}, GuiIconText(ICON_ARROW_RIGHT_FILL, nullptr))) moveFrame(frameSel_ + 1);
    if (GuiButton({bx + 66, my, 30, 26}, GuiIconText(ICON_BIN, nullptr)) && haveFrame) {
        touch();
        frames.erase(frames.begin() + frameSel_);
        frameSel_ = std::max(0, frameSel_ - 1);
        anim->frames = SpriteAnimation::framesText(frames);
        changed = true;
    }
    if (!haveFrame) GuiEnable();
    bx += 104;
    bool isStart = anim && sprite.autoplay && sprite.animation == anim->name;
    if (GuiButton({bx, my, 120, 26}, GuiIconText(ICON_PLAYER_PLAY, isStart ? "Plays first" : "Play at start")) &&
        anim && !isStart) {
        touch();
        sprite.animation = anim->name;
        sprite.autoplay = true;
        changed = true;
    }
    if (!anim) GuiEnable();

    // The filmstrip
    Rectangle film{mx, my + 34, mw, r.y + r.height - (my + 34) - 6};
    float thumb = std::clamp(film.height - 34, 40.0f, 120.0f);
    Rectangle filmContent{0, 0, std::max(film.width - 4, frames.size() * (thumb + 8) + 8), film.height - 16};
    Rectangle filmView{};
    GuiScrollPanel(film, nullptr, filmContent, &filmScroll_, &filmView);
    beginClip(filmView);
    if (!anim) {
        drawText("Make an animation with New.", filmView.x + 10, filmView.y + 10, kDim);
    } else if (frames.empty()) {
        drawText("No frames yet: use Add Frames from Sheet.", filmView.x + 10, filmView.y + 10, kDim);
    }
    for (size_t i = 0; i < frames.size(); ++i) {
        Rectangle cell{filmView.x + filmScroll_.x + 8 + i * (thumb + 8), filmView.y + 6, thumb, thumb};
        if (tex) drawFrameIn(*tex, sheetCell(*tex, sprite.hframes, sprite.vframes, frames[i]), cell);
        bool sel = static_cast<int>(i) == frameSel_ && !animPreview_;
        bool now = static_cast<int>(i) == shownIndex && animPreview_;
        DrawRectangleLinesEx(cell, sel || now ? 2.0f : 1.0f, sel ? kOrange : now ? kAccent : kBorder);
        drawText(std::to_string(i + 1), cell.x + 4, cell.y + 2, kText);
        drawText("#" + std::to_string(frames[i]), cell.x + 4, cell.y + cell.height + 4, kDim);
        if (!GuiIsLocked() && mouseIn(cell) && mouseIn(filmView) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            frameSel_ = static_cast<int>(i);
            animPreview_ = false;  // show the frame that was clicked
        }
    }
    EndScissorMode();

    // The viewport shows the same thing as the preview box.
    sprite.editorPreviewUntil = GetTime() + 0.3;
    sprite.editorPreviewAnimation = anim ? anim->name : "";
    sprite.editorPreviewStart = animPreviewStart_;
    sprite.editorPreviewFrame = (anim && !animPreview_ && !frames.empty()) ? frames[frameSel_] : -1;

    if (changed) syncAnimationToGame();
}

void Editor::drawSheetDialog() {
    auto* sprite = selected_ ? selected_->as<AnimatedSprite2D>() : nullptr;
    raylib::Texture* tex = sprite ? Assets::texture(sprite->texture) : nullptr;
    if (!sprite || !tex || sprite->animations.empty()) {
        sheet_.open = false;
        return;
    }
    float sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.55f));
    float bw = std::min(820.0f, sw - 40), bh = std::min(600.0f, sh - 40);
    Rectangle box{std::floor((sw - bw) / 2), std::floor((sh - bh) / 2), bw, bh};
    bool close = GuiWindowBox(box, "Add Frames from Sheet") != 0;
    float x = box.x + 16, y = box.y + 38, w = box.width - 32;

    drawText("Columns", x, y + 5, kDim);
    if (GuiValueBox({x + 72, y, 70, 26}, nullptr, &sheet_.hframes, 1, 256, sheet_.hEdit)) sheet_.hEdit = !sheet_.hEdit;
    drawText("Rows", x + 160, y + 5, kDim);
    if (GuiValueBox({x + 206, y, 70, 26}, nullptr, &sheet_.vframes, 1, 256, sheet_.vEdit)) sheet_.vEdit = !sheet_.vEdit;
    int count = sheet_.hframes * sheet_.vframes;
    if (GuiButton({x + 300, y, 100, 26}, "Select All")) {
        sheet_.picked.clear();
        for (int i = 0; i < count; ++i) sheet_.picked.push_back(i);
    }
    if (GuiButton({x + 404, y, 70, 26}, "Clear")) sheet_.picked.clear();
    y += 36;
    drawText("Click frames in the order they should play (click again to unpick).", x, y, kDim);
    y += 24;

    // The sheet with its grid; picked frames show their order.
    float by = box.y + box.height - 44;
    Rectangle area{x, y, w, by - y - 10};
    float s = std::min(area.width / tex->width, area.height / tex->height);
    if (s >= 1) s = std::floor(s);
    Rectangle img{area.x + (area.width - tex->width * s) / 2, area.y + (area.height - tex->height * s) / 2,
                  tex->width * s, tex->height * s};
    drawFrameIn(*tex, {0, 0, (float)tex->width, (float)tex->height}, img);
    float cw = img.width / sheet_.hframes, ch = img.height / sheet_.vframes;
    for (int i = 0; i < count; ++i) {
        Rectangle cell{img.x + (i % sheet_.hframes) * cw, img.y + (i / sheet_.hframes) * ch, cw, ch};
        auto it = std::find(sheet_.picked.begin(), sheet_.picked.end(), i);
        bool picked = it != sheet_.picked.end();
        bool hover = !GuiIsLocked() && mouseIn(cell);
        if (picked) DrawRectangleRec(cell, Fade(kAccent, 0.25f));
        else if (hover) DrawRectangleRec(cell, Fade(WHITE, 0.1f));
        DrawRectangleLinesEx(cell, picked ? 2.0f : 1.0f, picked ? kAccent : Fade(kAccent, 0.5f));
        if (picked) {
            std::string order = std::to_string(it - sheet_.picked.begin() + 1);
            DrawRectangleRec({cell.x + 2, cell.y + 2, textWidth(order) + 6, 18}, kAccent);
            drawText(order, cell.x + 5, cell.y + 3, WHITE);
        }
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (picked) sheet_.picked.erase(it);
            else sheet_.picked.push_back(i);
        }
    }

    std::string addText = "Add " + std::to_string(sheet_.picked.size()) + " Frames";
    if (sheet_.picked.empty()) GuiDisable();
    if (GuiButton({box.x + box.width - 260, by, 150, 30}, addText.c_str()) && !sheet_.picked.empty()) {
        touch();
        sprite->hframes = sheet_.hframes;
        sprite->vframes = sheet_.vframes;
        animSel_ = std::clamp(animSel_, 0, static_cast<int>(sprite->animations.size()) - 1);
        SpriteAnimation& anim = sprite->animations[animSel_];
        auto frames = anim.frameList();
        frames.insert(frames.end(), sheet_.picked.begin(), sheet_.picked.end());
        anim.frames = SpriteAnimation::framesText(frames);
        animPreview_ = true;
        animPreviewStart_ = GetTime();
        syncAnimationToGame();
        sheet_.open = false;
    }
    if (sheet_.picked.empty()) GuiEnable();
    if (GuiButton({box.x + box.width - 104, by, 90, 30}, "Cancel")) close = true;
    if (close || (IsKeyPressed(KEY_ESCAPE) && !sheet_.hEdit && !sheet_.vEdit)) sheet_.open = false;
}

// ===========================================================================
// Popups and dialogs
// ===========================================================================
void Editor::openPopup(Vector2 at, std::vector<std::string> items, std::function<void(int)> onPick) {
    popup_ = Popup{true, at, std::move(items), std::move(onPick), true};
}

void Editor::drawPopup() {
    if (!popup_.open) return;
    float w = 160;
    for (auto& s : popup_.items) w = std::max(w, textWidth(s) + 32);
    float h = popup_.items.size() * kRowH + 8;
    float sw = screenW(), sh = screenH();
    Rectangle box{std::min(popup_.pos.x, sw - w - 4), std::min(popup_.pos.y, sh - h - 4), w, h};

    DrawRectangleRec({box.x + 4, box.y + 4, box.width, box.height}, Fade(BLACK, 0.4f));
    DrawRectangleRec(box, kPanel);
    DrawRectangleLinesEx(box, 1, kAccent);
    int picked = -1;
    for (size_t i = 0; i < popup_.items.size(); ++i) {
        Rectangle row{box.x + 4, box.y + 4 + i * kRowH, box.width - 8, kRowH};
        bool hover = mouseIn(row);
        if (hover) {
            DrawRectangleRec(row, kSelect);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !popup_.justOpened) picked = static_cast<int>(i);
        }
        drawText(popup_.items[i], row.x + 10, row.y + 4, kText);
    }

    if (picked >= 0) {
        auto onPick = popup_.onPick;
        popup_.open = false;
        if (onPick) onPick(picked);  // may open another popup
    } else if (!popup_.justOpened && ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !mouseIn(box)) ||
                                      IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE))) {
        popup_.open = false;
        swallowMouse_ = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    }
    popup_.justOpened = false;
}

// ===========================================================================
// Code editor (opening scripts and scene files in VS Code, Visual Studio...)
// ===========================================================================
fs::path Editor::scriptFileOf(const std::string& className) const {
    if (className.empty()) return {};
    for (const FileEntry& f : files_)
        if (f.kind == FileEntry::Script && f.scriptClass == className) return f.path;
    return {};
}

void Editor::openInCodeEditor(const fs::path& file) {
    fs::path full = file.is_absolute() ? file : projectDir_ / file;
    if (codeEditor_.builtin) {
        openDocument(full);
        return;
    }
    std::string error;
    if (s2d::openInCodeEditor(codeEditor_, full, projectDir_, error)) {
        log("Opened " + full.filename().string() + " in " + codeEditor_.name, kDim);
    } else {
        log("Couldn't open " + full.filename().string() + ": " + error, kBad);
        log("Pick another code editor with the gear button on the toolbar.", kDim);
    }
}

void Editor::fileMenu(size_t index, Vector2 at) {
    if (index >= files_.size()) return;
    const FileEntry f = files_[index];
    fs::path full = f.kind == FileEntry::Script ? fs::path(f.path) : projectDir_ / f.path;
    // The external editor, if one is picked (the built-in one is always offered)
    std::string external = codeEditor_.builtin           ? ""
                           : codeEditor_.command.empty() ? "Open in Default App"
                                                         : "Open in " + codeEditor_.name;
    std::vector<std::string> items;
    std::vector<std::function<void()>> actions;
    auto add = [&](std::string label, std::function<void()> action) {
        items.push_back(std::move(label));
        actions.push_back(std::move(action));
    };
    switch (f.kind) {
        case FileEntry::Script:
            add("Edit in Script Tab", [this, full] { openDocument(full); });
            if (!external.empty()) add(external, [this, full] { openInCodeEditor(full); });
            if (selected_ && !f.scriptClass.empty())
                add("Attach to " + selected_->name, [this, index] { activateFile(index); });
            break;
        case FileEntry::Scene:
            add("Open Scene", [this, path = f.path] { requestOpenScene(path); });
            add("Edit JSON in Script Tab", [this, full] { openDocument(full); });
            if (!external.empty()) add(external + " (JSON)", [this, full] { openInCodeEditor(full); });
            break;
        case FileEntry::Texture:
            add("Open in Image Viewer", [this, full] {
                if (!platform::openWithSystem(full)) log("Couldn't open " + full.string(), kBad);
            });
            break;
    }
    add("Show in Folder", [this, full] {
        if (!platform::showInFileManager(full)) log("Couldn't open the folder of " + full.string(), kBad);
    });
    if (f.kind != FileEntry::Texture) add("Choose Code Editor...", [this] { openCodeEditorDialog(); });
    openPopup(at, items, [actions](int i) { actions[i](); });
}

void Editor::openCodeEditorDialog() {
    activeField_.clear();
    const auto& editors = installedCodeEditors();
    codeEditorDialog_ = CodeEditorDialog{};
    codeEditorDialog_.open = true;
    codeEditorDialog_.selected = static_cast<int>(editors.size());  // Custom, unless it's one of the list
    for (size_t i = 0; i < editors.size(); ++i)
        if (editors[i].name == codeEditor_.name && editors[i].command == codeEditor_.command &&
            editors[i].builtin == codeEditor_.builtin)
            codeEditorDialog_.selected = static_cast<int>(i);
    std::snprintf(codeEditorDialog_.command, sizeof codeEditorDialog_.command, "%s", codeEditor_.command.c_str());
}

void Editor::drawCodeEditorDialog() {
    CodeEditorDialog& d = codeEditorDialog_;
    const auto& editors = installedCodeEditors();
    float sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.5f));
    float listH = (editors.size() + 1) * kRowH + 8;
    float w = 560, h = 96 + listH + 96;
    Rectangle box{std::floor(sw / 2 - w / 2), std::floor(std::max(8.0f, sh / 2 - h / 2)), w, h};
    bool cancel = GuiWindowBox(box, "Code Editor") != 0;
    float x = box.x + 14, y = box.y + 36, iw = box.width - 28;
    drawText("Open scripts and scene files in:", x, y, kText);
    y += 24;

    // The editors found on this computer, then Custom
    Rectangle list{x, y, iw, listH};
    DrawRectangleRec(list, kFieldBg);
    DrawRectangleLinesEx(list, 1, kBorder);
    for (size_t i = 0; i <= editors.size(); ++i) {
        Rectangle row{x + 4, y + 4 + i * kRowH, iw - 8, kRowH};
        bool isCustom = i == editors.size();
        bool chosen = d.selected == static_cast<int>(i);
        if (chosen) DrawRectangleRec(row, kSelect);
        else if (mouseIn(row) && !GuiIsLocked()) DrawRectangleRec(row, kHover);
        GuiDrawIcon(chosen ? ICON_OK_TICK : ICON_NONE, static_cast<int>(row.x + 4), static_cast<int>(row.y + 4), 1, kAccent);
        std::string label = isCustom ? "Custom command" : editors[i].name;
        drawText(label, row.x + 28, row.y + 4, kText);
        std::string hint = isCustom                    ? ""
                           : editors[i].builtin         ? "the Script tab, inside Sprout"
                           : editors[i].command.empty() ? "whatever the system opens the file with"
                                                        : editors[i].command;
        if (!hint.empty()) drawText(hint, row.x + row.width - textWidth(hint) - 8, row.y + 4, kDim);
        if (mouseIn(row) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !GuiIsLocked()) {
            d.selected = static_cast<int>(i);
            if (!isCustom) std::snprintf(d.command, sizeof d.command, "%s", editors[i].command.c_str());
            d.commandEdit = isCustom;
            if (isCustom) moveTextCursorToEnd();
        }
    }
    y += listH + 12;

    drawText("Command:", x, y + 6, kDim);
    float cx = x + textWidth("Command:") + 10;
    std::string before = d.command;
    if (GuiTextBox({cx, y, x + iw - cx, 28}, d.command, sizeof d.command, d.commandEdit)) d.commandEdit = !d.commandEdit;
    // Typing a command of your own makes it a custom one.
    if (before != d.command) d.selected = static_cast<int>(editors.size());
    y += 34;
    drawText("{file} = the file, {project} = the project folder.", x, y, kDim);
    y += 20;
    drawText("Saved for you on this computer (all projects).", x, y, kDim);

    bool ok = (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
    if (GuiButton({box.x + box.width - 200, box.y + box.height - 42, 90, 30}, "OK")) ok = true;
    if (GuiButton({box.x + box.width - 104, box.y + box.height - 42, 90, 30}, "Cancel") ||
        (IsKeyPressed(KEY_ESCAPE)))
        cancel = true;
    if (cancel) {
        d.open = false;
        return;
    }
    if (!ok) return;

    CodeEditor chosen;
    if (d.selected < static_cast<int>(editors.size())) {
        chosen = editors[d.selected];
    } else {
        chosen.name = "Custom";
        chosen.builtin = false;
        chosen.command = d.command;
        while (!chosen.command.empty() && chosen.command.back() == ' ') chosen.command.pop_back();
        if (chosen.command.empty()) chosen.name = "System default";
        // Warn (but keep it) if the program isn't on the PATH.
        std::string program = chosen.command.substr(0, chosen.command.find(' '));
        if (!program.empty() && program[0] != '"' && program[0] != '\'' && !fs::path(program).is_absolute() &&
            platform::findProgram(program).empty())
            log("'" + program + "' isn't on the PATH; opening files may not work.", kOrange);
    }
    codeEditor_ = chosen;
    saveCodeEditor(chosen);
    d.open = false;
    log("Code editor: " + chosen.name + (chosen.command.empty() ? "" : " (" + chosen.command + ")"), kGood);
}


// ===========================================================================
// The Script tab: the built-in script editor
// ===========================================================================

TextDocument* Editor::openDocument(const fs::path& file, int line, int column) {
    std::error_code ec;
    fs::path full = fs::weakly_canonical(fs::absolute(file.is_absolute() ? file : projectDir_ / file, ec), ec);
    TextDocument* doc = nullptr;
    for (auto& d : docs_)
        if (d->path == full) doc = d.get();
    if (!doc) {
        auto fresh = std::make_unique<TextDocument>();
        std::string error;
        if (!fresh->load(full, error)) {
            log("Couldn't open " + full.string() + ": " + error, kBad);
            return nullptr;
        }
        setDiagnostics(*fresh);
        if (fresh->language == Language::Cpp) fresh->completer = &completions_;
        doc = fresh.get();
        docs_.push_back(std::move(fresh));
    }
    currentDoc_ = doc;
    mainView_ = MainView::Script;
    codeFocused_ = true;
    if (line > 0) doc->goToLine(line, column);
    return doc;
}

void Editor::setDiagnostics(TextDocument& doc) const {
    if (doc.language != Language::Cpp) return;  // JSON files get theirs when saving
    doc.diagnostics.clear();
    std::error_code ec;
    for (const BuildMessage& m : buildMessages_)
        if (fs::equivalent(m.file, doc.path, ec)) doc.diagnostics.push_back(m.diagnostic);
}

bool Editor::saveDocument(TextDocument& doc) {
    std::string name = doc.path.filename().string();
    bool isScene = name.size() > 11 && name.substr(name.size() - 11) == ".scene.json";
    if (doc.language == Language::Json) {
        // Never save a JSON file that can't be read back.
        std::string text = doc.text();
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(text);
        } catch (const nlohmann::json::parse_error& e) {
            int line = 1, column = 1;
            for (size_t i = 0; i + 1 < e.byte && i < text.size(); ++i) {
                if (text[i] == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            std::string why = e.what();
            if (size_t colon = why.find("]: "); colon != std::string::npos) why = why.substr(colon + 3);
            if (size_t colon = why.rfind(": "); colon != std::string::npos) why = why.substr(colon + 2);
            doc.diagnostics = {{line, column, true, why}};
            doc.goToLine(line, column);
            log("Not saved: " + name + " isn't valid JSON (line " + std::to_string(line) + ": " + why + ")", kBad);
            return false;
        }
        if (isScene) {
            std::string why;
            try {
                if (!nodeFromJson(j)) why = "a node has an unknown type";
            } catch (const std::exception& e) {
                why = e.what();
            }
            if (!why.empty()) {
                doc.diagnostics = {{1, 0, true, "not a scene the editor can load: " + why}};
                log("Not saved: " + name + " isn't a scene the editor can load (" + why + ")", kBad);
                return false;
            }
        }
        doc.diagnostics.clear();
    }
    std::string error;
    if (!doc.save(error)) {
        log("Couldn't save: " + error, kBad);
        return false;
    }
    doc.diskConflict = false;
    std::error_code ec;
    std::string rel = fs::relative(doc.path, projectDir_, ec).generic_string();
    if (ec || rel.empty() || rel.rfind("..", 0) == 0) rel = doc.path.string();
    log("Saved " + rel, kGood);
    if (isScene) {
        rescanFiles();
        // Show the change in the 2D view if the open scene is (or uses) this file.
        std::ifstream in(projectDir_ / scenePath_);
        std::string current((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bool affected = rel == scenePath_ || current.find("\"" + rel + "\"") != std::string::npos;
        if (affected && editTree_.root() && !playTree_) {
            if (!dirty_) openScene(scenePath_);
            else log("The scene in the 2D view has unsaved changes; saving it there will replace these edits.", kOrange);
        }
    }
    return true;
}

void Editor::closeDocument(TextDocument* doc) {
    auto remove = [this, doc] {
        auto it = std::find_if(docs_.begin(), docs_.end(), [doc](auto& d) { return d.get() == doc; });
        if (it == docs_.end()) return;
        size_t index = static_cast<size_t>(it - docs_.begin());
        docs_.erase(it);
        if (currentDoc_ == doc) currentDoc_ = docs_.empty() ? nullptr : docs_[std::min(index, docs_.size() - 1)].get();
        if (!currentDoc_) codeFocused_ = false;
    };
    if (!doc->dirty()) {
        remove();
        return;
    }
    confirm_ = ConfirmDialog{true, "Save changes to " + doc->path.filename().string() + " first?", remove,
                             [this, doc] { return saveDocument(*doc); }};
}

void Editor::confirmUnsavedDocuments(std::function<void()> then) {
    std::vector<TextDocument*> dirty;
    std::string names;
    for (auto& d : docs_)
        if (d->dirty()) {
            dirty.push_back(d.get());
            names += (names.empty() ? "" : ", ") + d->path.filename().string();
        }
    if (dirty.empty()) {
        then();
        return;
    }
    if (names.size() > 40) names = std::to_string(dirty.size()) + " files";
    confirm_ = ConfirmDialog{true, "Save changes to " + names + " first?", std::move(then), [this] {
                                 bool ok = true;
                                 for (auto& d : docs_)
                                     if (d->dirty()) ok = saveDocument(*d) && ok;
                                 return ok;
                             }};
}

void Editor::confirmUnsavedAll(std::function<void()> then) {
    confirmUnsaved([this, then] { confirmUnsavedDocuments(then); });
}

void Editor::requestQuit() {
    if (confirm_.open || quit_) return;
    confirmUnsavedAll([this] { quit_ = true; });
}

void Editor::checkDocumentsOnDisk() {
    if (GetTime() - lastDiskCheck_ < 0.5) return;
    lastDiskCheck_ = GetTime();
    for (auto& d : docs_) {
        if (d->diskConflict || !d->changedOnDisk()) continue;
        if (d->dirty()) d->diskConflict = true;  // ask (a banner over the editor)
        else d->reloadFromDisk();                // nothing to lose: just show the new version
    }
}

void Editor::handleScriptShortcuts() {
    if (IsKeyPressed(KEY_F5)) play(true);
    if (IsKeyPressed(KEY_F6)) play(false);
    TextDocument* doc = currentDoc_;
    auto openFind = [this, doc](bool replace) {
        find_.open = true;
        find_.replace = replace || (find_.replace && find_.open);
        std::string sel = doc->selectedText();
        if (!sel.empty() && sel.find('\n') == std::string::npos)
            std::snprintf(find_.find, sizeof find_.find, "%s", sel.c_str());
        find_.findEdit = true;
        find_.withEdit = false;
        codeFocused_ = false;
        moveTextCursorToEnd();
    };
    if (ctrlDown()) {
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) setUiScale(uiScale_ + 0.25f);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) setUiScale(uiScale_ - 0.25f);
        if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) setUiScale(systemUiScale());
    }
    if (!doc) return;
    if (ctrlDown()) {
        if (IsKeyPressed(KEY_S)) {
            if (shiftDown()) {
                for (auto& d : docs_)
                    if (d->dirty()) saveDocument(*d);
            } else {
                saveDocument(*doc);
            }
        }
        if (IsKeyPressed(KEY_F)) openFind(false);
        if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_R)) openFind(true);
        if (IsKeyPressed(KEY_G)) {
            openTextDialog("Go to Line", "Line number (1 - " + std::to_string(doc->lineCount()) + "):", "",
                           [this, doc](const std::string& text) {
                               for (auto& d : docs_)
                                   if (d.get() == doc) {
                                       doc->goToLine(std::atoi(text.c_str()));
                                       codeFocused_ = true;
                                   }
                           });
        }
        if (IsKeyPressed(KEY_W)) {
            closeDocument(doc);
            return;
        }
    }
    if (IsKeyPressed(KEY_F3)) doc->findNext(find_.find, shiftDown(), find_.matchCase);
    if (IsKeyPressed(KEY_ESCAPE) && find_.open) {
        find_.open = find_.findEdit = find_.withEdit = false;
        codeFocused_ = true;
    }
}

void Editor::drawMainTabs(Rectangle r) {
    DrawRectangleRec(r, kHeader);
    DrawLineEx({r.x, r.y + r.height - 0.5f}, {r.x + r.width, r.y + r.height - 0.5f}, 1, kBorder);
    bool anyDirty = false;
    for (auto& d : docs_) anyDirty = anyDirty || d->dirty();
    struct Tab {
        std::string label;
        int icon;
        MainView view;
    };
    Tab tabs[] = {{playTree_ ? "Game" : "2D", ICON_MODE_2D, MainView::View2D},
                  {anyDirty ? "Script *" : "Script", ICON_FILETYPE_TEXT, MainView::Script}};
    float x = r.x + 6;
    for (const Tab& t : tabs) {
        float w = textWidth(t.label) + 40;
        Rectangle b{x, r.y + 3, w, r.height - 6};
        bool active = mainView_ == t.view;
        bool hover = mouseIn(b) && !GuiIsLocked();
        if (active || hover) DrawRectangleRec(b, active ? kSelect : kHover);
        float ty = std::floor(b.y + (b.height - 16) / 2);
        GuiDrawIcon(t.icon, static_cast<int>(b.x + 8), static_cast<int>(ty), 1, active ? kText : kDim);
        drawText(t.label, b.x + 30, ty, active ? kText : kDim);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            mainView_ = t.view;
            codeFocused_ = t.view == MainView::Script && currentDoc_;
        }
        x += w + 4;
    }
    if (mainView_ == MainView::Script) {
        // As many shortcut hints as fit
        std::string hints;
        for (const char* h : {"Ctrl+S save", "Ctrl+Space suggest", "Ctrl+F find", "Ctrl+H replace", "Ctrl+G go to line",
                              "Ctrl+/ comment"}) {
            std::string more = hints.empty() ? h : hints + "   " + h;
            if (x + 12 + textWidth(more) > r.x + r.width - 8) break;
            hints = more;
        }
        drawText(hints, x + 12, std::floor(r.y + (r.height - 16) / 2), kDim);
    }
}

void Editor::drawFindBar(Rectangle r, TextDocument& doc) {
    DrawRectangleRec(r, kHeader);
    DrawLineEx({r.x, r.y + r.height - 0.5f}, {r.x + r.width, r.y + r.height - 0.5f}, 1, kBorder);
    float labelW = std::max(textWidth("Find"), textWidth("Replace")) + 16;
    float x = r.x + 8, y = r.y + 4;
    float boxW = std::clamp(r.width - labelW - 330, 120.0f, 360.0f);
    bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);

    drawText("Find", x, y + 5, kDim);
    std::string before = find_.find;
    Rectangle box{x + labelW, y, boxW, 26};
    if (GuiTextBox(box, find_.find, sizeof find_.find, find_.findEdit)) {
        if (enter && find_.findEdit) {
            doc.findNext(find_.find, shiftDown(), find_.matchCase);
        } else {
            find_.findEdit = !find_.findEdit;
            if (find_.findEdit) find_.withEdit = codeFocused_ = false;
        }
    }
    if (before != find_.find) doc.findNext(find_.find, false, find_.matchCase, true);  // as you type
    float bx = box.x + box.width + 6;
    if (GuiButton({bx, y, 30, 26}, GuiIconText(ICON_ARROW_LEFT, nullptr))) doc.findNext(find_.find, true, find_.matchCase);
    if (GuiButton({bx + 34, y, 30, 26}, GuiIconText(ICON_ARROW_RIGHT, nullptr))) doc.findNext(find_.find, false, find_.matchCase);
    Rectangle caseBox{bx + 68, y, 36, 26};
    if (GuiButton(caseBox, "Aa")) find_.matchCase = !find_.matchCase;
    if (find_.matchCase) DrawRectangleLinesEx(caseBox, 2, kAccent);
    int count = find_.find[0] ? doc.countMatches(find_.find, find_.matchCase) : 0;
    if (find_.find[0])
        drawText(count == 0 ? "No matches" : std::to_string(count) + (count == 1 ? " match" : " matches"), bx + 112,
                 y + 5, count ? kDim : kBad);
    if (GuiButton({r.x + r.width - 34, y, 28, 26}, GuiIconText(ICON_CROSS_SMALL, nullptr))) {
        find_.open = find_.findEdit = find_.withEdit = false;
        codeFocused_ = true;
    }
    if (!find_.replace) {
        if (GuiButton({r.x + r.width - 120, y, 82, 26}, "Replace...")) find_.replace = true;
    } else {
        y += 30;
        drawText("Replace", x, y + 5, kDim);
        Rectangle with{x + labelW, y, boxW, 26};
        if (GuiTextBox(with, find_.with, sizeof find_.with, find_.withEdit)) {
            if (enter && find_.withEdit) {
                doc.replaceSelection(find_.find, find_.with, find_.matchCase);
            } else {
                find_.withEdit = !find_.withEdit;
                if (find_.withEdit) find_.findEdit = codeFocused_ = false;
            }
        }
        if (GuiButton({bx, y, 80, 26}, "Replace")) doc.replaceSelection(find_.find, find_.with, find_.matchCase);
        if (GuiButton({bx + 84, y, 100, 26}, "Replace All")) {
            int n = doc.replaceAll(find_.find, find_.with, find_.matchCase);
            log("Replaced " + std::to_string(n) + " in " + doc.path.filename().string(), kDim);
        }
    }
}

void Editor::drawScriptView(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    const float tabH = 30;
    DrawRectangleRec({r.x, r.y, r.width, tabH}, kHeader);
    TextDocument* doc = currentDoc_;
    bool locked = GuiIsLocked();

    // Buttons on the right of the tab row
    float bx = r.x + r.width - 6;
    auto button = [&](float w, int icon, const char* text) {
        bx -= w;
        bool hit = GuiButton({bx, r.y + 3, w, tabH - 6}, GuiIconText(icon, text));
        bx -= 4;
        return hit;
    };
    if (doc) {
        if (button(30, ICON_LENS, nullptr)) {
            find_.open = true;
            find_.findEdit = true;
            codeFocused_ = false;
        }
        if (button(74, ICON_FILE_SAVE_CLASSIC, "Save")) saveDocument(*doc);
    }

    // One tab per open file
    TextDocument* toClose = nullptr;
    beginClip({r.x, r.y, bx - r.x, tabH});
    float x = r.x + 4;
    for (auto& d : docs_) {
        std::string label = d->path.filename().string() + (d->dirty() ? " *" : "");
        float w = textWidth(label) + 52;
        Rectangle t{x, r.y + 3, w, tabH - 3};
        bool active = d.get() == doc;
        bool hover = !locked && mouseIn(t) && mouseIn({r.x, r.y, bx - r.x, tabH});
        DrawRectangleRec(t, active ? kPanel : hover ? kHover : kHeader);
        if (active) DrawRectangleRec({t.x, t.y, t.width, 2}, kAccent);
        float ty = std::floor(t.y + (t.height - 16) / 2);
        GuiDrawIcon(d->language == Language::Json ? ICON_LAYERS : ICON_FILETYPE_TEXT, static_cast<int>(t.x + 6),
                    static_cast<int>(ty), 1, d->language == Language::Json ? kAccent : kGood);
        drawText(label, t.x + 26, ty, active ? kText : kDim);
        Rectangle closeBox{t.x + t.width - 22, ty, 16, 16};
        GuiDrawIcon(ICON_CROSS_SMALL, static_cast<int>(closeBox.x), static_cast<int>(closeBox.y), 1,
                    hover && mouseIn(closeBox) ? kText : kDim);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mouseIn(closeBox)) {
                toClose = d.get();
            } else {
                currentDoc_ = d.get();
                codeFocused_ = true;
            }
        }
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) toClose = d.get();
        x += w + 2;
    }
    EndScissorMode();
    doc = currentDoc_;
    float y = r.y + tabH;

    if (!doc) {
        codeRect_ = {};
        float cy = std::floor(r.y + r.height / 2 - 30);
        const char* lines[] = {"No files open.",
                               "Open a script with the Edit button in the Inspector, or right-click",
                               "a script or scene in the FileSystem dock. New scripts open here too."};
        for (const char* line : lines) {
            drawText(line, std::floor(r.x + (r.width - textWidth(line)) / 2), cy, kDim);
            cy += 22;
        }
        if (toClose) closeDocument(toClose);
        return;
    }

    if (find_.open) {
        float h = find_.replace ? 64 : 34;
        drawFindBar({r.x, y, r.width, h}, *doc);
        y += h;
    }
    doc->highlightNeedle = find_.open ? find_.find : "";
    doc->highlightMatchCase = find_.matchCase;

    if (doc->diskConflict) {
        Rectangle banner{r.x, y, r.width, 34};
        DrawRectangleRec(banner, Fade(kOrange, 0.22f));
        drawText("This file was changed outside the editor, and it has unsaved changes here.", r.x + 10, y + 9, kText);
        if (GuiButton({r.x + r.width - 220, y + 4, 100, 26}, "Reload")) {
            doc->reloadFromDisk();
            doc->diskConflict = false;
        }
        if (GuiButton({r.x + r.width - 114, y + 4, 106, 26}, "Keep Mine")) {
            doc->acceptDiskVersion();
            doc->diskConflict = false;
        }
        y += 34;
    }

    const float statusH = 24;
    Rectangle area{r.x, y, r.width, r.y + r.height - statusH - y};
    codeRect_ = area;
    if (!locked && mouseIn(area) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        codeFocused_ = true;
        find_.findEdit = find_.withEdit = false;
    }
    TextStyle style;
    style.font = codeFont_.texture.id != 0 ? codeFont_ : GuiGetFont();
    style.fontSize = theme().code.fontSize > 0 ? static_cast<float>(theme().code.fontSize) : textSize();
    style.uiScale = uiScale_;
    doc->draw(area, codeFocused_ && !locked && !dialogOpen() && !popup_.open, !locked, style);
    if (doc->hoverText && !locked) mouseCursor_ = MOUSE_CURSOR_IBEAM;

    // Status bar
    Rectangle status{r.x, r.y + r.height - statusH, r.width, statusH};
    DrawRectangleRec(status, kHeader);
    float sy = std::floor(status.y + (statusH - 16) / 2);
    std::string where = "Ln " + std::to_string(doc->cursorLine()) + ", Col " + std::to_string(doc->cursorColumn());
    drawText(where, status.x + 10, sy, kDim);
    float sx = status.x + 10 + textWidth(where) + 24;
    int errors = 0, warnings = 0;
    for (const Diagnostic& d : doc->diagnostics) (d.error ? errors : warnings)++;
    if (errors || warnings) {
        std::string text = std::to_string(errors) + (errors == 1 ? " error" : " errors");
        if (warnings) text += ", " + std::to_string(warnings) + (warnings == 1 ? " warning" : " warnings");
        drawText(text, sx, sy, errors ? kBad : kOrange);
        sx += textWidth(text) + 24;
    }
    if (doc->language == Language::Cpp && ScriptReloader::supported() && scripts_->isBuilding())
        drawText("compiling...", sx, sy, kOrange);
    std::error_code ec;
    std::string rel = fs::relative(doc->path, projectDir_, ec).generic_string();
    if (ec || rel.empty() || rel.rfind("..", 0) == 0) rel = doc->path.string();
    std::string right = std::string(doc->language == Language::Cpp ? "C++" : doc->language == Language::Json ? "JSON" : "Text") +
                        "   " + rel;
    drawText(right, status.x + status.width - textWidth(right) - 10, sy, kDim);

    if (toClose) closeDocument(toClose);
}

void Editor::openTextDialog(std::string title, std::string message, std::string initial,
                            std::function<void(const std::string&)> onOk) {
    activeField_.clear();
    dialog_.open = true;
    dialog_.title = std::move(title);
    dialog_.message = std::move(message);
    std::snprintf(dialog_.text, sizeof dialog_.text, "%s", initial.c_str());
    dialog_.onOk = std::move(onOk);
    moveTextCursorToEnd();
}

// ===========================================================================
// Projects
// ===========================================================================
void Editor::pickFolder(const std::string& title, const fs::path& start, std::function<void(const fs::path&)> then) {
    if (folderPick_.valid()) return;  // one at a time
    activeField_.clear();
    projects_.nameEdit = projects_.locationEdit = projects_.pathEdit = false;
    export_.nameEdit = export_.dirEdit = false;
    folderPicked_ = std::move(then);
    void* owner = GetWindowHandle();
    folderPick_ = std::async(std::launch::async, [title, start, owner] { return platform::pickFolder(title, start, owner); });
}

void Editor::pumpFolderPicker() {
    if (!folderPick_.valid() || folderPick_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    std::optional<fs::path> picked = folderPick_.get();  // (this also makes folderPick_ invalid again)
    auto then = std::move(folderPicked_);
    folderPicked_ = nullptr;
    if (picked && then) then(*picked);
}

std::optional<fs::path> Editor::takeProjectSwitch() {
    auto next = switchTo_;
    switchTo_.reset();
    return next;
}

void Editor::openProjectManager() {
    if (playTree_) stop();
    activeField_.clear();
    projects_ = ProjectManagerDialog{};
    projects_.open = true;
    for (auto& p : projects::loadRecent().list)
        if (projects::isProject(p)) projects_.list.push_back(p);
    std::error_code ec;
    fs::path current = fs::weakly_canonical(fs::absolute(projectDir_, ec), ec);
    for (size_t i = 0; i < projects_.list.size(); ++i)
        if (projects_.list[i] == current) projects_.selected = static_cast<int>(i);
    std::snprintf(projects_.location, sizeof projects_.location, "%s",
                  projects::defaultProjectsFolder().string().c_str());
}

void Editor::switchProject(const fs::path& project) {
    std::error_code ec;
    fs::path target = fs::weakly_canonical(fs::absolute(project, ec), ec);
    fs::path current = fs::weakly_canonical(fs::absolute(projectDir_, ec), ec);
    if (target == current) {
        projects_.open = false;  // already open
        return;
    }
    confirmUnsavedAll([this, target] {
        projects_.open = false;
        switchTo_ = target;
    });
}

void Editor::drawProjectManager() {
    float sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.55f));
    float bw = std::min(720.0f, sw - 40), bh = std::min(520.0f, sh - 40);
    Rectangle box{std::floor((sw - bw) / 2), std::floor((sh - bh) / 2), bw, bh};
    const char* titles[] = {"Projects", "New Project", "Open Project Folder"};
    bool close = GuiWindowBox(box, titles[projects_.mode]) != 0;
    bool editing = projects_.nameEdit || projects_.locationEdit || projects_.pathEdit;
    float x = box.x + 16, w = box.width - 32, y = box.y + 40;
    float by = box.y + box.height - 44;  // the button row

    if (projects_.mode == 0) {
        // The list of projects
        float listW = w - 190;
        Rectangle area{x, y, listW, by - y - 10};
        const float rowH = 46;
        Rectangle content{0, 0, area.width - 16, std::max(1.0f, projects_.list.size() * rowH)};
        Rectangle view{};
        GuiScrollPanel(area, nullptr, content, &projects_.scroll, &view);
        beginClip(view);
        std::error_code ec;
        fs::path current = fs::weakly_canonical(fs::absolute(projectDir_, ec), ec);
        Vector2 mouse = GetMousePosition();
        for (size_t i = 0; i < projects_.list.size(); ++i) {
            Rectangle row{view.x, view.y + projects_.scroll.y + i * rowH, view.width, rowH};
            bool hover = !GuiIsLocked() && CheckCollisionPointRec(mouse, row) && CheckCollisionPointRec(mouse, view);
            if (static_cast<int>(i) == projects_.selected) DrawRectangleRec(row, kSelect);
            else if (hover) DrawRectangleRec(row, kHover);
            const fs::path& p = projects_.list[i];
            std::string name = projects::projectName(p) + (p == current ? "   (open now)" : "");
            drawText(name, row.x + 10, row.y + 5, p == current ? kGood : kText);
            drawText(p.string(), row.x + 10, row.y + 25, kDim);
            if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (projects_.selected == static_cast<int>(i)) switchProject(p);  // a second click opens it
                projects_.selected = static_cast<int>(i);
            }
        }
        if (projects_.list.empty()) drawText("No projects yet: make one with New Project.", view.x + 10, view.y + 10, kDim);
        EndScissorMode();

        // Buttons on the right
        float bx = x + listW + 14, bw2 = w - listW - 14;
        bool hasSel = projects_.selected >= 0 && projects_.selected < static_cast<int>(projects_.list.size());
        if (!hasSel) GuiDisable();
        if (GuiButton({bx, y, bw2, 30}, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open"))) switchProject(projects_.list[projects_.selected]);
        if (!hasSel) GuiEnable();
        if (GuiButton({bx, y + 38, bw2, 30}, GuiIconText(ICON_FOLDER_ADD, "New Project..."))) {
            projects_.mode = 1;
            projects_.error.clear();
            std::snprintf(projects_.name, sizeof projects_.name, "My Game");
        }
        if (GuiButton({bx, y + 76, bw2, 30}, GuiIconText(ICON_FOLDER_OPEN, "Open Folder..."))) {
            projects_.error.clear();
            if (platform::canPickFolder()) {
                pickFolder("Open a project folder", projects::defaultProjectsFolder(), [this](const fs::path& p) {
                    if (projects::isProject(p)) {
                        projects::remember(p);
                        switchProject(p);
                    } else {
                        projects_.error = "That folder isn't a Sprout project (no project.json or scenes/ in it).";
                    }
                });
            } else {
                projects_.mode = 2;  // no folder window on this system: type the path
            }
        }
        if (!hasSel) GuiDisable();
        if (GuiButton({bx, y + 130, bw2, 30}, GuiIconText(ICON_CROSS, "Remove from List"))) {
            projects::forget(projects_.list[projects_.selected]);
            projects_.list.erase(projects_.list.begin() + projects_.selected);
            projects_.selected = -1;
        }
        if (!hasSel) GuiEnable();
        drawText("Removing only forgets it here;", bx, y + 168, kDim);
        drawText("its files stay on disk.", bx, y + 186, kDim);
        if (GuiButton({box.x + box.width - 106, by, 90, 30}, "Close")) close = true;
    } else if (projects_.mode == 1) {
        const float labelW = 110;
        drawText("Name", x, y + 5, kDim);
        if (GuiTextBox({x + labelW, y, w - labelW, 26}, projects_.name, sizeof projects_.name, projects_.nameEdit))
            projects_.nameEdit = !projects_.nameEdit;
        y += 36;
        drawText("Location", x, y + 5, kDim);
        float browseW = platform::canPickFolder() ? 100 : 0;
        if (GuiTextBox({x + labelW, y, w - labelW - browseW, 26}, projects_.location, sizeof projects_.location,
                       projects_.locationEdit))
            projects_.locationEdit = !projects_.locationEdit;
        if (browseW > 0 && GuiButton({x + w - browseW + 4, y, browseW - 4, 26}, "Browse...")) {
            pickFolder("Where to make the project", projects_.location, [this](const fs::path& p) {
                std::snprintf(projects_.location, sizeof projects_.location, "%s", p.string().c_str());
            });
        }
        y += 40;
        std::string folder;
        for (char c : std::string(projects_.name)) folder += (c == '/' || c == '\\' || c == ':') ? '_' : c;
        fs::path target = fs::path(projects_.location) / folder;
        drawText("The project will be made in:", x, y, kDim);
        drawText(target.string(), x, y + 20, kAccent);
        drawText("with scenes/, assets/ and scripts/ folders and a first scene.", x, y + 44, kDim);
        if (GuiButton({box.x + box.width - 210, by, 100, 30}, "Back")) projects_.mode = 0;
        if (GuiButton({box.x + box.width - 106, by, 90, 30}, "Create")) {
            std::string error;
            if (folder.empty()) {
                projects_.error = "Give the project a name.";
            } else if (projects::createProject(target, projects_.name, error)) {
                projects::remember(target);
                switchProject(target);
            } else {
                projects_.error = error;
            }
        }
    } else {
        drawText("Folder", x, y + 5, kDim);
        float browseW = platform::canPickFolder() ? 100 : 0;
        if (GuiTextBox({x + 110, y, w - 110 - browseW, 26}, projects_.path, sizeof projects_.path, projects_.pathEdit))
            projects_.pathEdit = !projects_.pathEdit;
        if (browseW > 0 && GuiButton({x + w - browseW + 4, y, browseW - 4, 26}, "Browse...")) {
            pickFolder("Open a project folder", projects::defaultProjectsFolder(), [this](const fs::path& p) {
                std::snprintf(projects_.path, sizeof projects_.path, "%s", p.string().c_str());
            });
        }
        y += 40;
        drawText("Type or paste the path of a project folder (the one with project.json in it).", x, y, kDim);
        if (GuiButton({box.x + box.width - 210, by, 100, 30}, "Back")) projects_.mode = 0;
        if (GuiButton({box.x + box.width - 106, by, 90, 30}, "Open")) {
            fs::path p = projects_.path;
            if (!projects::isProject(p)) {
                projects_.error = "That folder isn't a Sprout project (no project.json or scenes/ in it).";
            } else {
                projects::remember(p);
                switchProject(p);
            }
        }
    }
    if (!projects_.error.empty()) drawText(projects_.error, x, by + 6, kBad);

    if (close || (IsKeyPressed(KEY_ESCAPE) && !editing)) {
        if (projects_.mode != 0) projects_.mode = 0;
        else projects_.open = false;
    }
}

// ===========================================================================
// Export
// ===========================================================================
std::string Editor::defaultExportDir() const { return (projectDir_ / "exports").string(); }

void Editor::openExportDialog() {
    if (playTree_) return;
    activeField_.clear();
    export_ = ExportDialog{};
    export_.open = true;
    export_.targets = exporter_->detectTargets();
    for (auto& t : export_.targets) {
        bool wanted = settings_.exportTargets.empty()
                          ? t.available && (t.id == "linux" || t.id == "windows" || t.id == "macos")
                          : std::find(settings_.exportTargets.begin(), settings_.exportTargets.end(), t.id) !=
                                settings_.exportTargets.end();
        export_.chosen.push_back(wanted && t.available);
    }
    std::snprintf(export_.name, sizeof export_.name, "%s", settings_.name.c_str());
    std::string dir = settings_.exportDir.empty() ? defaultExportDir() : settings_.exportDir;
    std::snprintf(export_.dir, sizeof export_.dir, "%s", dir.c_str());
    export_.width = settings_.width;
    export_.height = settings_.height;
}

void Editor::drawExportDialog() {
    float sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.5f));
    float bw = std::min(640.0f, sw - 40), bh = std::min(560.0f, sh - 40);
    Rectangle box{std::floor((sw - bw) / 2), std::floor((sh - bh) / 2), bw, bh};
    bool close = GuiWindowBox(box, "Export Game") != 0;
    bool editing = export_.nameEdit || export_.dirEdit || export_.widthEdit || export_.heightEdit;

    float x = box.x + 16, w = box.width - 32, y = box.y + 40;
    const float labelW = 130;
    auto label = [&](const char* text) { drawText(text, x, y + 5, kDim); };

    label("Game name");
    if (GuiTextBox({x + labelW, y, w - labelW, 26}, export_.name, sizeof export_.name, export_.nameEdit))
        export_.nameEdit = !export_.nameEdit;
    y += 34;

    label("Main scene");
    if (GuiButton({x + labelW, y, w - labelW, 26}, settings_.mainScene.c_str())) {
        std::vector<std::string> scenes;
        for (auto& f : files_)
            if (f.kind == FileEntry::Scene) scenes.push_back(f.path);
        if (!scenes.empty())
            openPopup({x + labelW, y + 28}, scenes, [this, scenes](int i) { settings_.mainScene = scenes[i]; });
    }
    y += 34;

    label("Window size");
    if (GuiValueBox({x + labelW, y, 90, 26}, nullptr, &export_.width, 64, 4096, export_.widthEdit))
        export_.widthEdit = !export_.widthEdit;
    drawText("x", x + labelW + 98, y + 5, kDim);
    if (GuiValueBox({x + labelW + 114, y, 90, 26}, nullptr, &export_.height, 64, 4096, export_.heightEdit))
        export_.heightEdit = !export_.heightEdit;
    y += 42;

    drawText("Platforms", x, y, kAccent);
    y += 26;
    for (size_t i = 0; i < export_.targets.size(); ++i) {
        const ExportTarget& t = export_.targets[i];
        bool on = export_.chosen[i];
        if (!t.available) GuiDisable();
        GuiCheckBox({x + 4, y + 4, 18, 18}, nullptr, &on);
        if (!t.available) GuiEnable();
        export_.chosen[i] = on && t.available;
        drawText(t.label, x + 32, y + 4, t.available ? kText : kDim);
        drawText(t.available ? "ready" : "not available", x + 120, y + 4, t.available ? kGood : kOrange);
        // The note, shortened to fit
        std::string note = t.note;
        float room = w - 250;
        while (note.size() > 4 && textWidth(note) > room) note = note.substr(0, note.size() - 4) + "...";
        drawText(note, x + 240, y + 4, kDim);
        y += 30;
    }
    y += 4;

    // Emscripten's folder (for Web)
    {
        fs::path emsdk = findEmsdk();
        bool ready = !emsdk.empty() && emsdkInstalled(emsdk);
        label("Emscripten");
        std::string where = emsdk.empty() ? "emsdk not found" : emsdk.string() + (ready ? "" : "  (not set up yet)");
        float buttonsW = (platform::canPickFolder() ? 104 : 0) + (!emsdk.empty() && !ready ? 104 : 0);
        float room = w - labelW - buttonsW - 8;
        while (where.size() > 4 && textWidth(where) > room) where = "..." + where.substr(4);
        drawText(where, x + labelW, y + 5, ready ? kText : kOrange);
        float bx = x + w - buttonsW;
        if (!emsdk.empty() && !ready) {
            if (exporter_->busy()) GuiDisable();
            if (GuiButton({bx, y, 100, 26}, "Set Up")) {
                if (exporter_->setUpEmsdk(emsdk)) log("Setting up Emscripten (see below)...", kAccent);
            }
            if (exporter_->busy()) GuiEnable();
            bx += 104;
        }
        if (platform::canPickFolder() && GuiButton({bx, y, 100, 26}, "Choose...")) {
            std::error_code ec;
            pickFolder("Emscripten's emsdk folder", emsdk.empty() ? fs::path() : emsdk, [this](const fs::path& p) {
                if (!isEmsdkFolder(p)) {
                    log(p.string() + " isn't an emsdk folder (it has no emsdk.py). Get emsdk with: git clone "
                        "https://github.com/emscripten-core/emsdk.git", kBad);
                    return;
                }
                setEmsdkFolder(p);
                log("Emscripten folder: " + p.string(), kGood);
                auto chosen = export_.chosen;
                export_.targets = exporter_->detectTargets();
                for (size_t i = 0; i < export_.targets.size(); ++i)
                    export_.chosen[i] = chosen[i] && export_.targets[i].available;
            });
        }
        y += 34;
    }

    label("Export to");
    float browseW = platform::canPickFolder() ? 100 : 0;
    if (GuiTextBox({x + labelW, y, w - labelW - browseW, 26}, export_.dir, sizeof export_.dir, export_.dirEdit))
        export_.dirEdit = !export_.dirEdit;
    if (browseW > 0 && GuiButton({x + w - browseW + 4, y, browseW - 4, 26}, "Browse...")) {
        std::error_code ec;
        fs::path start = fs::is_directory(export_.dir, ec) ? fs::path(export_.dir) : projectDir_;
        pickFolder("Export the game to", start, [this](const fs::path& p) {
            std::snprintf(export_.dir, sizeof export_.dir, "%s", p.string().c_str());
        });
    }
    y += 34;
    drawText("Each platform goes in its own folder inside it (linux/, windows/, macos/, web/).", x, y, kDim);

    // Bottom row
    float by = box.y + box.height - 44;
    bool busy = exporter_->busy();
    if (export_.wasBusy && !busy) {  // e.g. Emscripten was just set up
        auto chosen = export_.chosen;
        export_.targets = exporter_->detectTargets();
        for (size_t i = 0; i < export_.targets.size(); ++i) export_.chosen[i] = chosen[i] && export_.targets[i].available;
    }
    export_.wasBusy = busy;
    if (busy) {
        drawText("Exporting " + exporter_->currentLabel() + "... (see Output)", x, by + 6, kOrange);
    } else if (std::none_of(export_.chosen.begin(), export_.chosen.end(), [](bool b) { return b; })) {
        drawText("Tick a platform to export.", x, by + 6, kDim);
    }
    float bx = box.x + box.width - 16;
    bx -= 90;
    if (GuiButton({bx, by, 90, 30}, "Close")) close = true;
    bx -= 100;
    if (busy) GuiDisable();
    if (GuiButton({bx, by, 96, 30}, GuiIconText(ICON_FILE_EXPORT, "Export"))) startExport();
    if (busy) GuiEnable();
    bx -= 100;
    if (GuiButton({bx, by, 96, 30}, GuiIconText(ICON_ROTATE_FILL, "Recheck"))) {
        auto chosen = export_.chosen;
        export_.targets = exporter_->detectTargets();
        for (size_t i = 0; i < export_.targets.size(); ++i) export_.chosen[i] = chosen[i] && export_.targets[i].available;
    }

    if (close || (IsKeyPressed(KEY_ESCAPE) && !editing)) {
        // Keep what was typed for next time.
        settings_.name = export_.name;
        settings_.width = export_.width;
        settings_.height = export_.height;
        settings_.exportDir = export_.dir == defaultExportDir() ? "" : export_.dir;
        export_.open = false;
    }
}

void Editor::startExport() {
    if (!fs::exists(projectDir_ / settings_.mainScene)) {
        log("The main scene " + settings_.mainScene + " doesn't exist. Pick another one.", kBad);
        return;
    }
    if (dirty_) {
        saveScene();  // the export uses the files on disk
        if (dirty_) return;
    }
    settings_.name = export_.name;
    settings_.width = export_.width;
    settings_.height = export_.height;
    settings_.exportDir = export_.dir == defaultExportDir() ? "" : export_.dir;  // keep project.json portable
    settings_.exportTargets.clear();
    for (size_t i = 0; i < export_.targets.size(); ++i)
        if (export_.chosen[i]) settings_.exportTargets.push_back(export_.targets[i].id);
    if (!settings_.save(projectDir_)) log("Couldn't save project.json.", kBad);
    editTree_.gameSize = raylib::Vector2{static_cast<float>(settings_.width), static_cast<float>(settings_.height)};
    if (settings_.exportTargets.empty()) return;
    log("Exporting '" + settings_.name + "' (the first export for a platform takes a few minutes)...", kAccent);
    exporter_->start(settings_.exportTargets, settings_.name, export_.dir);
}

void Editor::pumpExport() {
    if (!exporter_) return;
    for (auto& line : exporter_->takeLines()) {
        Color c = line.kind == 1 ? kGood : line.kind == 2 ? kBad : line.kind == 3 ? kAccent : theme().colors.buildOutput;
        log(line.text, c);
    }
}

void Editor::confirmUnsaved(std::function<void()> then) {
    if (!dirty_ || !editTree_.root() || playTree_) {
        then();
        return;
    }
    confirm_ = ConfirmDialog{true, "Save changes to " + scenePath_ + " first?", std::move(then), {}};
}

void Editor::requestOpenScene(const std::string& scenePath) {
    confirmUnsaved([this, scenePath] { openScene(scenePath); });
}

void Editor::drawModal() {
    if (sheet_.open) drawSheetDialog();
    if (codeEditorDialog_.open) drawCodeEditorDialog();
    bool picking = folderPick_.valid();
    if (picking) GuiLock();  // the folder window is open: everything here waits
    if (export_.open) drawExportDialog();
    if (projects_.open) drawProjectManager();
    if (picking) {
        GuiUnlock();
        float sw = screenW(), sh = screenH();
        DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.45f));
        std::string text = "Choose a folder in the window that opened...";
        Rectangle box{std::floor((sw - textWidth(text) - 40) / 2), std::floor(sh / 2 - 24), textWidth(text) + 40, 48};
        DrawRectangleRec(box, kPanel);
        DrawRectangleLinesEx(box, 1, kAccent);
        drawText(text, box.x + 20, box.y + 15, kText);
        return;
    }
    if (confirm_.open) {
        float sw = screenW(), sh = screenH();
        DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.5f));
        Rectangle box{sw / 2 - 230, sh / 2 - 70, 460, 140};
        bool cancel = GuiWindowBox(box, "Unsaved changes") != 0;
        GuiLabel({box.x + 14, box.y + 40, box.width - 28, 24}, confirm_.message.c_str());
        int choice = -1;
        if (GuiButton({box.x + box.width - 318, box.y + 94, 96, 30}, "Save")) choice = 0;
        if (GuiButton({box.x + box.width - 216, box.y + 94, 110, 30}, "Don't Save")) choice = 1;
        if (GuiButton({box.x + box.width - 100, box.y + 94, 86, 30}, "Cancel") || IsKeyPressed(KEY_ESCAPE)) cancel = true;
        if (cancel) {
            confirm_.open = false;
        } else if (choice >= 0) {
            auto then = confirm_.then;
            auto save = confirm_.save;
            confirm_.open = false;
            bool saved = true;
            if (choice == 0) {
                if (save) {
                    saved = save();
                } else {
                    saveScene();
                    saved = !dirty_;
                }
            }
            if (saved) then();  // don't go on if saving failed
        }
        return;
    }
    if (!dialog_.open) return;
    float sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), Fade(BLACK, 0.5f));
    Rectangle box{sw / 2 - 220, sh / 2 - 85, 440, 170};
    bool cancel = GuiWindowBox(box, dialog_.title.c_str()) != 0;
    GuiLabel({box.x + 14, box.y + 36, box.width - 28, 24}, dialog_.message.c_str());
    GuiTextBox({box.x + 14, box.y + 64, box.width - 28, 30}, dialog_.text, sizeof dialog_.text, true);
    bool ok = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    if (GuiButton({box.x + box.width - 200, box.y + 124, 90, 30}, "OK")) ok = true;
    if (GuiButton({box.x + box.width - 104, box.y + 124, 90, 30}, "Cancel") || IsKeyPressed(KEY_ESCAPE)) cancel = true;

    if (ok && !cancel) {
        auto onOk = dialog_.onOk;
        std::string text = dialog_.text;
        dialog_.open = false;
        if (onOk) onOk(text);
    } else if (cancel) {
        dialog_.open = false;
    }
}

// ===========================================================================
// Actions
// ===========================================================================
void Editor::setRoot(std::unique_ptr<Node> root) {
    editTree_.setRoot(std::move(root));
    select(nullptr);
}

void Editor::select(Node* node) {
    if (node == selected_) return;
    selected_ = node;
    inspectorScript_.reset();
    if (auto* sprite = node ? node->as<AnimatedSprite2D>() : nullptr) {
        // Like Godot: selecting an AnimatedSprite2D opens its animations.
        bottomTab_ = 1;
        animSel_ = 0;
        for (size_t i = 0; i < sprite->animations.size(); ++i)
            if (sprite->animations[i].name == sprite->animation) animSel_ = static_cast<int>(i);
        frameSel_ = 0;
        animPreview_ = true;
        animPreviewStart_ = GetTime();
    }
    activeField_.clear();
    scrubField_.clear();
    openColorPicker_.clear();
    inspectorScroll_ = raylib::Vector2{0, 0};
}

std::string Editor::uniqueChildName(Node* parent, const std::string& base) const {
    auto taken = [&](const std::string& n) {
        for (auto& c : parent->children())
            if (c->name == n) return true;
        return false;
    };
    if (!parent || !taken(base)) return base;
    std::string stem = base;
    while (!stem.empty() && std::isdigit(static_cast<unsigned char>(stem.back()))) stem.pop_back();
    for (int i = 2;; ++i) {
        std::string n = stem + std::to_string(i);
        if (!taken(n)) return n;
    }
}

void Editor::newScene() {
    if (editTree_.root()) touch();
    auto root = createNode("Node2D");
    root->name = "Main";
    setRoot(std::move(root));
    scenePath_ = "scenes/new_scene.scene.json";
    log("New scene. Save it with Ctrl+S.");
}

bool Editor::openScene(const std::string& relativePath) {
    if (playTree_) return false;
    std::string error;
    auto root = s2d::loadScene(projectDir_ / relativePath, &error);
    if (!root) {
        if (fs::exists(projectDir_ / relativePath)) log("Couldn't open " + relativePath + ": " + error, kBad);
        return false;
    }
    setRoot(std::move(root));
    scenePath_ = relativePath;
    undoStack_.clear();
    redoStack_.clear();
    dirty_ = false;
    log("Opened " + relativePath, kGood);
    return true;
}

void Editor::saveScene() {
    if (playTree_) return;
    if (!editTree_.root()) {
        log("Nothing to save: the scene is empty.", kBad);
        return;
    }
    std::string error;
    if (s2d::saveScene(*editTree_.root(), projectDir_ / scenePath_, &error)) {
        dirty_ = false;
        log("Saved " + scenePath_, kGood);
        rescanFiles();
    } else {
        log("Couldn't save: " + error, kBad);
    }
}

void Editor::play(bool mainScene) {
    if (playTree_) return;
    // The main scene, unless it's the one open here (then play it with any unsaved edits).
    std::string scene = mainScene ? settings_.mainScene : scenePath_;
    std::unique_ptr<Node> root;
    if (scene == scenePath_) {
        if (!editTree_.root()) {
            log("Add some nodes before pressing Play.", kBad);
            return;
        }
        root = cloneNode(*editTree_.root());
    } else {
        std::string error;
        root = s2d::loadScene(projectDir_ / scene, &error);
        if (!root) {
            log("Couldn't load the main scene " + scene + ": " + error +
                    " (set it in the Export window, or use Play Scene).",
                kBad);
            return;
        }
    }
    activeField_.clear();
    scrubField_.clear();
    viewDrag_ = ViewDrag::None;
    playTree_ = std::make_unique<SceneTree>();
    playTree_->gameSize = editTree_.gameSize;
    playTree_->clearColor = editTree_.clearColor;
    playTree_->setRoot(std::move(root));
    playTree_->setCurrentScene(scene);
    log("Playing " + scene + " ...", kAccent);
    try {
        playTree_->start();
    } catch (const std::exception& e) {
        log(std::string("A script threw an exception in ready(): ") + e.what(), kBad);
        stop();
    }
}

void Editor::stop() {
    if (!playTree_) return;
    playTree_.reset();
    log("Stopped.", kAccent);
}

void Editor::updateScripts() {
    if (!ScriptReloader::supported()) return;
    scripts_->update(GetTime());
    bool ok = false;
    std::string output;
    if (!scripts_->pollBuild(ok, output)) return;
    // Remember the compiler's messages, to mark the lines in the Script tab.
    buildMessages_.clear();
    {
        std::stringstream all(output);
        std::string line;
        while (std::getline(all, line)) {
            auto m = parseCompilerLine(line);
            if (!m) continue;
            std::error_code ec;
            fs::path file = m->file;
            if (!fs::exists(file, ec)) file = scriptsDir_ / fs::path(m->file).filename();
            if (fs::exists(file, ec)) buildMessages_.push_back({file, Diagnostic{m->line, m->column, m->error, m->message}});
        }
        for (auto& d : docs_) setDiagnostics(*d);
    }
    if (!ok) {
        log("Scripts didn't compile - still using the last working version:", kBad);
        // Show the compiler's error lines (with a little context), not the whole build log.
        std::stringstream ss(output);
        std::string line;
        int shown = 0, context = 0;
        while (std::getline(ss, line) && shown < 40) {
            bool isError = line.find("error") != std::string::npos || line.find("Error") != std::string::npos;
            if (isError) context = 3;
            if (context > 0) {
                log("  " + line, isError ? kBad : kDim);
                --context;
                ++shown;
            }
        }
        if (shown == 0) log(output, kDim);
        return;
    }
    reloadScripts();
}

void Editor::reloadScripts() {
    // Scripts made from the old library must go before the library does.
    inspectorScript_.reset();
    if (playTree_) playTree_->dropScripts();
    std::string error;
    bool firstLoad = !scripts_->isLoaded();
    if (scripts_->load(error)) {
        std::string names;
        for (auto& n : scripts_->scriptNames()) names += (names.empty() ? "" : ", ") + n;
        log((firstLoad ? "Loaded scripts: " : "Reloaded scripts: ") + (names.empty() ? "(none)" : names), kGood);
    } else {
        log("Couldn't load the scripts library: " + error, kBad);
    }
    if (playTree_) playTree_->restartScripts();  // the running game picks up the new code
    rescanFiles();
}

void Editor::addNode(const std::string& type) {
    auto node = createNode(type);
    if (!node) return;
    touch();
    if (auto* sprite = node->as<Sprite2D>(); sprite && fs::exists(projectDir_ / "assets/icon.png"))
        sprite->texture = "assets/icon.png";
    if (!editTree_.root()) {
        if (type == "Node2D") node->name = "Main";
        Node* raw = node.get();
        setRoot(std::move(node));
        select(raw);
        return;
    }
    Node* parent = selected_ ? selected_ : editTree_.root();
    if (!parent->sceneFile.empty() || parent->inInstance) {
        log("Can't add nodes inside an instanced scene here; open its scene file to change what's in it "
            "(or select another node).",
            kDim);
        return;
    }
    node->name = uniqueChildName(parent, type);
    Node* raw = parent->addChild(std::move(node));
    parent->collapsed = false;
    select(raw);
}

void Editor::instanceScene(const std::string& scenePath, Node* parent, const Vector2* world) {
    if (playTree_) return;
    if (sceneUses(scenePath, scenePath_)) {
        log("Can't put " + scenePath + " inside " + scenePath_ + ": it would contain itself.", kBad);
        return;
    }
    nlohmann::json link;
    link["instance"] = scenePath;
    auto node = nodeFromJson(link);
    if (!node || node->instanceMissing) return;  // nodeFromJson already said why
    touch();
    if (!editTree_.root()) {
        auto root = createNode("Node2D");
        root->name = "Main";
        setRoot(std::move(root));
    }
    if (!parent) parent = editTree_.root();
    while (parent->inInstance && parent->parent()) parent = parent->parent();
    if (!parent->sceneFile.empty() && parent->parent()) parent = parent->parent();  // not inside another instance
    node->name = uniqueChildName(parent, node->name);
    Node* raw = parent->addChild(std::move(node));
    if (auto* n2 = raw->as<Node2D>(); n2 && world) {
        raylib::Vector2 p = *world;
        if (ctrlDown()) p = raylib::Vector2{std::round(p.x / 8) * 8, std::round(p.y / 8) * 8};
        else p = raylib::Vector2{std::round(p.x), std::round(p.y)};
        n2->setGlobalPosition(p);
    }
    parent->collapsed = false;
    select(raw);
    log("Instanced " + scenePath + " as '" + raw->name + "'");
}

void Editor::saveBranchAsScene(Node* node, const std::string& scenePath) {
    if (!node || !node->parent() || !node->sceneFile.empty()) return;
    if (scenePath.empty() || scenePath == scenePath_) {
        log("Pick a different file name for the new scene.", kBad);
        return;
    }
    if (fs::exists(projectDir_ / scenePath)) {
        log(scenePath + " already exists. Pick another name (or instance that scene instead).", kBad);
        return;
    }
    std::string error;
    if (!s2d::saveScene(*node, projectDir_ / scenePath, &error)) {
        log("Couldn't save " + scenePath + ": " + error, kBad);
        return;
    }
    touch();
    node->sceneFile = scenePath;
    markInstanceChildren(*node, true);
    rescanFiles();
    log("Saved '" + node->name + "' as " + scenePath + ". It's now an instance of that scene.", kGood);
    log("Save this scene too (Ctrl+S) to keep the link.", kDim);
}

void Editor::resetInstance(Node* node) {
    if (!node || node->sceneFile.empty() || !node->parent()) return;
    auto fresh = instanceSource(node->sceneFile);
    if (!fresh) return;
    touch();
    // Keep where it is and what it's called; everything else goes back to the original.
    fresh->name = node->name;
    auto* oldPlace = node->as<Node2D>();
    if (auto* n2 = fresh->as<Node2D>(); n2 && oldPlace) {
        n2->position = oldPlace->position;
        n2->rotation = oldPlace->rotation;
        n2->scale = oldPlace->scale;
    }
    fresh->collapsed = node->collapsed;
    Node* parent = node->parent();
    int index = node->indexInParent();
    select(nullptr);
    node->detach();  // freed here
    Node* raw = parent->addChild(std::move(fresh), index);
    select(raw);
    log("Reset '" + raw->name + "' to match " + raw->sceneFile + " (kept its position).");
}

void Editor::makeLocal(Node* node) {
    if (!node || node->sceneFile.empty()) return;
    touch();
    log("'" + node->name + "' is no longer linked to " + node->sceneFile + "; its nodes are now part of this scene.");
    node->sceneFile.clear();
    node->instanceMissing = false;
    markInstanceChildren(*node, false);
}

bool Editor::lockedByInstance(Node* node, const char* action) {
    if (!node || !node->inInstance) return false;
    log(std::string("'") + node->name + "' belongs to an instanced scene, so it can't be " + action +
            " here. Open the original scene to change it there (that changes every copy).",
        kDim);
    return true;
}

void Editor::deleteSelected() {
    if (!selected_ || lockedByInstance(selected_, "deleted")) return;
    touch();
    if (!selected_->parent()) {
        setRoot(nullptr);
        return;
    }
    Node* parent = selected_->parent();
    selected_->detach();  // freed here
    selected_ = nullptr;
    select(parent);
}

void Editor::duplicateSelected() {
    if (!selected_ || lockedByInstance(selected_, "duplicated")) return;
    if (!selected_->parent()) {
        log("The root node can't be duplicated.", kDim);
        return;
    }
    touch();
    Node* parent = selected_->parent();
    auto copy = cloneNode(*selected_);
    copy->name = uniqueChildName(parent, selected_->name);
    if (auto* n2 = copy->as<Node2D>()) n2->position = n2->position + raylib::Vector2{16, 16};
    Node* raw = parent->addChild(std::move(copy), selected_->indexInParent() + 1);
    select(raw);
}

void Editor::moveSelected(int direction) {
    if (!selected_ || !selected_->parent() || lockedByInstance(selected_, "reordered")) return;
    Node* parent = selected_->parent();
    int index = selected_->indexInParent() + direction;
    if (index < 0 || index >= static_cast<int>(parent->children().size())) return;
    touch();
    parent->addChild(selected_->detach(), index);
}

void Editor::createSpriteAt(const std::string& texture, Vector2 world) {
    touch();
    if (!editTree_.root()) {
        auto root = createNode("Node2D");
        root->name = "Main";
        setRoot(std::move(root));
    }
    auto sprite = std::make_unique<Sprite2D>();
    Node* parent = editTree_.root();
    sprite->name = uniqueChildName(parent, pascalCase(fs::path(texture).stem().string()));
    sprite->texture = texture;
    auto* raw = static_cast<Sprite2D*>(parent->addChild(std::move(sprite)));
    raylib::Vector2 p = world;
    if (ctrlDown()) p = raylib::Vector2{std::round(p.x / 8) * 8, std::round(p.y / 8) * 8};
    raw->setGlobalPosition(p);
    parent->collapsed = false;
    select(raw);
    log("Added Sprite2D '" + raw->name + "' using " + texture);
}

void Editor::createScript(const std::string& className) {
    static const std::regex identifier("[A-Za-z_][A-Za-z0-9_]*");
    if (!std::regex_match(className, identifier)) {
        log("'" + className + "' isn't a valid C++ class name.", kBad);
        return;
    }
    fs::path file = scriptsDir_ / (snakeCase(className) + ".cpp");
    if (fs::exists(file)) {
        log(file.filename().string() + " already exists.", kBad);
        return;
    }
    std::string code = kScriptTemplate;
    for (size_t pos; (pos = code.find("$NAME")) != std::string::npos;) code.replace(pos, 5, className);
    std::ofstream out(file);
    if (!out) {
        log("Couldn't write " + file.string(), kBad);
        return;
    }
    out << code;
    out.close();
    if (selected_) {
        touch();
        selected_->scriptName = className;
    }
    rescanFiles();
    log("Created " + file.string(), kGood);
    openInCodeEditor(file);
    if (ScriptReloader::supported()) {
        log("It compiles and loads by itself; edit it and save to reload.", kGood);
    } else {
        log("Rebuild the editor (cmake --build build) to compile it, then press Play.", kOrange);
    }
}

// ===========================================================================
// Undo / redo (whole-scene snapshots: simple and always correct)
// ===========================================================================
Editor::Snapshot Editor::snapshot() {
    Snapshot s;
    if (editTree_.root()) s.scene = nodeToJson(*editTree_.root()).dump();
    s.selection = selected_ ? pathOf(selected_) : std::vector<int>{-1};
    return s;
}

void Editor::restore(const Snapshot& s) {
    if (s.scene.empty()) setRoot(nullptr);
    else setRoot(nodeFromJson(nlohmann::json::parse(s.scene)));
    if (!(s.selection.size() == 1 && s.selection[0] == -1)) select(nodeAt(editTree_.root(), s.selection));
}

void Editor::touch() {
    dirty_ = true;
    if (touchedThisDrag_) return;
    touchedThisDrag_ = IsMouseButtonDown(MOUSE_BUTTON_LEFT);  // one undo step per mouse drag
    undoStack_.push_back(snapshot());
    if (undoStack_.size() > 200) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void Editor::undo() {
    if (playTree_ || undoStack_.empty()) return;
    redoStack_.push_back(snapshot());
    restore(undoStack_.back());
    undoStack_.pop_back();
    dirty_ = true;
}

void Editor::redo() {
    if (playTree_ || redoStack_.empty()) return;
    undoStack_.push_back(snapshot());
    restore(redoStack_.back());
    redoStack_.pop_back();
    dirty_ = true;
}

}  // namespace s2d
