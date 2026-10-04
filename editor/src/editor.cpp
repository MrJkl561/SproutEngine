#include "editor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

#include <nlohmann/json.hpp>

#include <rlgl.h>

#include "assets.hpp"
#include "raygui_include.hpp"
#include "scene_io.hpp"
#include "script.hpp"

namespace fs = std::filesystem;

namespace s2d {

namespace {

// ---------------------------------------------------------------------------
// Look
// ---------------------------------------------------------------------------
constexpr float kToolbarH = 40;
constexpr float kLeftW = 290;
constexpr float kRightW = 340;
constexpr float kOutputH = 170;
constexpr float kHeaderH = 28;
constexpr float kRowH = 24;

const Color kBg{30, 32, 38, 255};
const Color kPanel{37, 40, 48, 255};
const Color kHeader{26, 28, 34, 255};
const Color kBorder{55, 60, 72, 255};
const Color kText{220, 222, 228, 255};
const Color kDim{140, 146, 158, 255};
const Color kAccent{100, 160, 255, 255};
const Color kSelect{55, 85, 140, 255};
const Color kHover{48, 53, 64, 255};
const Color kOrange{255, 160, 60, 255};
const Color kFieldBg{24, 26, 31, 255};
const Color kGood{120, 220, 140, 255};
const Color kBad{255, 110, 100, 255};

float textSize() { return static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SIZE)); }

void drawText(const std::string& s, float x, float y, Color c) {
    DrawTextEx(GuiGetFont(), s.c_str(), {x, y}, textSize(), static_cast<float>(GuiGetStyle(DEFAULT, TEXT_SPACING)), c);
}

float textWidth(const std::string& s) { return static_cast<float>(GuiGetTextWidth(s.c_str())); }

void drawHeader(Rectangle r, const std::string& title) {
    DrawRectangleRec(r, kPanel);
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

int iconFor(const Node* n) {
    std::string t = n->typeName();
    if (t == "Node2D") return ICON_TARGET_SMALL;
    if (t == "Sprite2D") return ICON_FILETYPE_IMAGE;
    if (t == "ColorRect") return ICON_BOX;
    if (t == "Label") return ICON_TEXT_A;
    if (t == "Camera2D") return ICON_EYE_ON;
    if (t == "CollisionShape2D") return ICON_BOX_CORNERS_SMALL;
    if (t == "Area2D") return ICON_BOX_DOTS_SMALL;
    if (t == "StaticBody2D") return ICON_BOX_GRID;
    if (t == "CharacterBody2D") return ICON_PLAYER;
    return ICON_EMPTYBOX_SMALL;
}

Color colorFor(const Node* n) {
    std::string t = n->typeName();
    if (t == "Node") return Color{210, 210, 210, 255};
    if (t == "Camera2D") return Color{190, 140, 255, 255};
    if (t == "CollisionShape2D" || t == "Area2D" || t == "StaticBody2D" || t == "CharacterBody2D")
        return Color{110, 200, 255, 255};
    return Color{130, 165, 255, 255};
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
Editor::Editor(fs::path projectDir, fs::path scriptsDir)
    : projectDir_(std::move(projectDir)), scriptsDir_(std::move(scriptsDir)) {
    Assets::setProjectDir(projectDir_);
    setPrintHandler([this](const std::string& msg) { log(msg, Color{230, 230, 160, 255}); });

    std::error_code ec;
    fs::create_directories(projectDir_ / "scenes", ec);
    fs::create_directories(scriptsDir_, ec);
    ensureDefaultAssets();
    rescanFiles();

    log("Sprout Editor - project: " + projectDir_.string());
    log(std::to_string(ScriptRegistry::names().size()) + " C++ scripts compiled in, from " + scriptsDir_.string());
    if (!openScene(scenePath_)) newScene();
    undoStack_.clear();
    dirty_ = false;
}

Editor::~Editor() {
    setPrintHandler(nullptr);
    playTree_.reset();
    gameTexture_.reset();
    Assets::clear();
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

    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());
    float bodyH = sh - kToolbarH;
    Rectangle toolbar{0, 0, sw, kToolbarH};
    Rectangle sceneDock{0, kToolbarH, kLeftW, std::floor(bodyH * 0.55f)};
    Rectangle fileDock{0, kToolbarH + sceneDock.height, kLeftW, bodyH - sceneDock.height};
    Rectangle inspector{sw - kRightW, kToolbarH, kRightW, bodyH};
    Rectangle output{kLeftW, sh - kOutputH, sw - kLeftW - kRightW, kOutputH};
    Rectangle viewport{kLeftW, kToolbarH, sw - kLeftW - kRightW, bodyH - kOutputH};
    viewportRect_ = viewport;
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
            if (!playTree_ && mouseIn(viewport)) createSpriteAt(rel, viewCam_.GetScreenToWorld(GetMousePosition()));
        }
        UnloadDroppedFiles(dropped);
        rescanFiles();
    }

    handleShortcuts();

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

    bool modal = popup_.open || dialog_.open || swallowMouse_;
    if (modal) GuiLock();
    {
        // While the game runs the scene can't be edited (it's a copy that's playing).
        if (playTree_ && !modal) GuiLock();
        drawInspector(inspector);  // first, so a typed value is kept when you click elsewhere
        drawSceneDock(sceneDock);
        drawFileSystem(fileDock);
        if (playTree_ && !modal) GuiUnlock();
        drawOutput(output);
        drawViewport(viewport);
        drawToolbar(toolbar);
    }
    if (modal) GuiUnlock();
    finishDrag();
    drawPopup();
    drawModal();

    if (!screenshotPath.empty()) {
        rlDrawRenderBatchActive();  // flush pending drawing so it's in the picture
        TakeScreenshot(screenshotPath.c_str());
        screenshotPath.clear();
    }
    EndDrawing();

    SetMouseCursor(mouseCursor_);
    std::string title = "Sprout Editor - " + scenePath_ + (dirty_ ? " *" : "") + (playTree_ ? "  [playing]" : "");
    if (title != windowTitle_) {
        windowTitle_ = title;
        SetWindowTitle(title.c_str());
    }
}

void Editor::handleShortcuts() {
    if (playTree_) {
        if (IsKeyPressed(KEY_F8) || IsKeyPressed(KEY_ESCAPE)) stop();
        return;
    }
    if (isTyping() || popup_.open) return;

    if (IsKeyPressed(KEY_F5)) play();
    if (ctrlDown()) {
        if (IsKeyPressed(KEY_S)) saveScene();
        if (IsKeyPressed(KEY_Z)) shiftDown() ? redo() : undo();
        if (IsKeyPressed(KEY_Y)) redo();
        if (IsKeyPressed(KEY_D)) duplicateSelected();
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
    DrawLineEx({r.x, r.y + r.height - 1}, {r.x + r.width, r.y + r.height - 1}, 1, kBorder);

    float x = 8, y = 6, h = 28;
    bool playing = playTree_ != nullptr;
    if (playing) GuiDisable();
    if (GuiButton({x, y, 76, h}, GuiIconText(ICON_FILE_NEW, "New"))) newScene();
    x += 80;
    if (GuiButton({x, y, 80, h}, GuiIconText(ICON_FOLDER_FILE_OPEN, "Open"))) {
        std::vector<std::string> scenes;
        for (auto& f : files_)
            if (f.kind == FileEntry::Scene) scenes.push_back(f.path);
        if (scenes.empty()) {
            log("No scenes in " + (projectDir_ / "scenes").string() + " yet.", kDim);
        } else {
            openPopup({x, y + h + 2}, scenes, [this, scenes](int i) { openScene(scenes[i]); });
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
    if (GuiButton({x, y, 34, h}, GuiIconText(ICON_UNDO, nullptr))) undo();
    x += 36;
    if (GuiButton({x, y, 34, h}, GuiIconText(ICON_REDO, nullptr))) redo();
    x += 44;
    if (playing) GuiEnable();
    drawText(scenePath_ + (dirty_ ? " (unsaved)" : ""), x, y + 6, dirty_ ? kOrange : kDim);

    // Play / Stop in the middle, like Godot
    float cx = r.width / 2 + 40;
    if (playing) GuiDisable();
    if (GuiButton({cx - 92, y, 88, h}, GuiIconText(ICON_PLAYER_PLAY, "Play"))) play();
    if (playing) GuiEnable();
    if (!playing) GuiDisable();
    if (GuiButton({cx, y, 88, h}, GuiIconText(ICON_PLAYER_STOP, "Stop"))) stop();
    if (!playing) GuiEnable();

    std::string brand = "Sprout Editor 2D";
    drawText(brand, r.width - textWidth(brand) - 14, y + 6, kAccent);
}

// ===========================================================================
// Scene dock (the node tree)
// ===========================================================================
void Editor::drawSceneDock(Rectangle r) {
    drawHeader(r, "Scene");

    float y = r.y + kHeaderH + 4, x = r.x + 6;
    if (GuiButton({x, y, 88, 26}, GuiIconText(ICON_CIRCLE_ADD, "Add"))) {
        openPopup({x, y + 28}, nodeTypeNames(), [this](int i) { addNode(nodeTypeNames()[i]); });
    }
    x += 92;
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
        if (n->collapsed) return;
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

    BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width),
                     static_cast<int>(view.height));
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
        if (!n->children().empty()) {
            GuiDrawIcon(n->collapsed ? ICON_ARROW_RIGHT_FILL : ICON_ARROW_DOWN_FILL, static_cast<int>(ix),
                        static_cast<int>(row.y + 4), 1, kDim);
        }
        GuiDrawIcon(iconFor(n), static_cast<int>(ix + 18), static_cast<int>(row.y + 4), 1, colorFor(n));
        drawText(n->name, ix + 40, row.y + 4, kText);
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
            if (!n->children().empty() && CheckCollisionPointRec(mouse, arrow)) {
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
                                                               : "C++ scripts (editor/scripts)";
            rows.push_back({true, title, 0});
        }
        rows.push_back({false, files_[i].label, i});
    }

    Rectangle area{r.x, r.y + kHeaderH, r.width, r.height - kHeaderH};
    Rectangle content{0, 0, area.width - 16, rows.size() * kRowH + 8};
    Rectangle view{};
    GuiScrollPanel(area, nullptr, content, &filesScroll_, &view);

    BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width),
                     static_cast<int>(view.height));
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
            label += "  (rebuild)";
        drawText(label, ix + 26, row.y + 4, f.kind == FileEntry::Scene && f.path == scenePath_ ? kAccent : kText);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            drag_ = DragState{DragKind::File, nullptr, rows[i].file, mouse, false};
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
        case FileEntry::Scene: openScene(f.path); break;
        case FileEntry::Script:
            if (f.scriptClass.empty()) {
                log(f.label + " has no REGISTER_SCRIPT(...) line, so it can't be attached.", kBad);
            } else if (selected_) {
                touch();
                selected_->scriptName = f.scriptClass;
                log("Attached " + f.scriptClass + " to " + selected_->name);
            } else {
                log("Select a node first, then click a script to attach it.", kDim);
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
            openScene(f.path);
        } else if (Node2D* hit = pickNode(world)) {
            // Clicking a sprite or shape inside a physics body picks the body, so
            // the whole thing moves together. Pick the child in the Scene dock to
            // move it on its own.
            if (hit != selected_ && dynamic_cast<CollisionObject2D*>(hit->parent())) hit = hit->parent2D();
            select(hit);
            activateFile(d.file);
        }
    } else if (dropTarget_) {
        select(dropTarget_);
        activateFile(d.file);
    } else if (mouseIn(inspectorRect_)) {
        activateFile(d.file);
    }
}

void Editor::reparent(Node* node, Node* target, int zone) {
    if (!node || !target || node == target || node->isAncestorOf(target) || !node->parent()) return;
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
            // Clicking a sprite or shape inside a physics body picks the body, so
            // the whole thing moves together. Pick the child in the Scene dock to
            // move it on its own.
            if (hit != selected_ && dynamic_cast<CollisionObject2D*>(hit->parent())) hit = hit->parent2D();
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
    Color minor{42, 45, 54, 255};
    for (float x = std::floor(a.x / step) * step; x <= b.x; x += step) DrawLineEx({x, a.y}, {x, b.y}, line, minor);
    for (float y = std::floor(a.y / step) * step; y <= b.y; y += step) DrawLineEx({a.x, y}, {b.x, y}, line, minor);
    DrawLineEx({a.x, 0}, {b.x, 0}, line * 1.5f, Color{170, 70, 70, 200});  // x axis
    DrawLineEx({0, a.y}, {0, b.y}, line * 1.5f, Color{70, 170, 90, 200});  // y axis
}

void Editor::drawViewport(Rectangle r) {
    if (playTree_) {
        drawGameView(r);
        return;
    }
    viewCam_.offset = {r.x + r.width / 2, r.y + r.height / 2};
    if (!GuiIsLocked()) updateViewportInput(r);
    viewCam_.offset = {r.x + r.width / 2, r.y + r.height / 2};

    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width),
                     static_cast<int>(r.height));
    DrawRectangleRec(r, Color{22, 24, 29, 255});

    viewCam_.BeginMode();
    drawGrid(r);
    float px = 1.0f / viewCam_.zoom;  // one screen pixel in world units
    DrawRectangleLinesEx({-px, -px, editTree_.gameSize.x + 2 * px, editTree_.gameSize.y + 2 * px}, 2 * px,
                         Color{150, 110, 230, 200});  // the game window
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
        if (lr.width > 0 && lr.height > 0) drawLocalRect(*sel, lr, BLANK, kOrange, 2 * px);
        else DrawCircleLinesV(sel->globalPosition(), 9 * px, kOrange);
    }
    viewCam_.EndMode();

    // Move/rotate handle, drawn in screen space so it's the same size at any zoom
    if (sel) {
        raylib::Vector2 o = viewCam_.GetWorldToScreen(sel->globalPosition());
        raylib::Vector2 h = rotateHandlePos();
        bool hot = overRotateHandle(GetMousePosition()) || viewDrag_ == ViewDrag::Rotate;
        DrawLineEx(o, h, 2, Fade(kOrange, 0.8f));
        DrawCircleV(h, hot ? 8.0f : 6.0f, hot ? YELLOW : kOrange);
        DrawCircleV(o, 4, kOrange);
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
    drawText("Drag: move  Handle: rotate (Shift snaps)  Ctrl: grid snap  Wheel: zoom  Right/Middle drag: pan",
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
    DrawRectangleRec(r, Color{12, 12, 15, 255});
    if (gameTexture_) {
        float gw = static_cast<float>(gameTexture_->texture.width);
        float gh = static_cast<float>(gameTexture_->texture.height);
        float s = std::min((r.width - 20) / gw, (r.height - 50) / gh);
        Rectangle dst{r.x + (r.width - gw * s) / 2, r.y + 36 + (r.height - 36 - gh * s) / 2, gw * s, gh * s};
        DrawTexturePro(gameTexture_->texture, {0, 0, gw, -gh}, dst, {0, 0}, 0, WHITE);
    }
    drawText("Game running - press F8 or Esc (or Stop) to go back to editing. Edits made while playing are not kept.",
             r.x + 10, r.y + 10, kOrange);
    DrawRectangleLinesEx(r, 2, Color{220, 90, 80, 255});
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

void Editor::propertyRow(Property& p, float x, float& y, float width) {
    if (p.kind == PropKind::Section) {
        y += 4;
        DrawRectangleRec({x - 4, y, width + 8, 24}, kHeader);
        drawText(p.name, x + 4, y + 4, kAccent);
        y += 30;
        return;
    }

    const float labelW = 112;
    float fx = x + labelW, fw = width - labelW;
    std::string id = std::to_string(reinterpret_cast<uintptr_t>(selected_)) + "/" + p.name;
    std::string label = p.name;
    std::replace(label.begin(), label.end(), '_', ' ');
    drawText(label, x, y + 4, kDim);

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
            DrawRectangleRec({fx + 1, y + 1, 3, 22}, Color{220, 90, 90, 255});  // x is red, y is green
            DrawRectangleRec({fx + half + 5, y + 1, 3, 22}, Color{90, 200, 110, 255});
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
    BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width),
                     static_cast<int>(view.height));
    ClipLock clip(view, clipLocked_);

    float x = view.x + 10, w = view.width - 20;
    float top = view.y + inspectorScroll_.y + 8, y = top;

    GuiDrawIcon(iconFor(selected_), static_cast<int>(x), static_cast<int>(y + 2), 1, colorFor(selected_));
    drawText(selected_->typeName(), x + 24, y + 2, kText);
    y += 30;

    Property nameProp{"name", PropKind::String, &selected_->name};
    std::string oldName = selected_->name;
    propertyRow(nameProp, x, y, w);
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
    if (!selected_->scriptName.empty() && !ScriptRegistry::has(selected_->scriptName)) {
        y += 22;
        drawText("not compiled yet - rebuild the editor", x, y + 4, kOrange);
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
    y += 40;
    inspectorContentHeight_ = y - top + 8;
    EndScissorMode();
}

// ===========================================================================
// Output
// ===========================================================================
void Editor::log(const std::string& message, Color color) {
    std::stringstream ss(message);
    std::string line;
    while (std::getline(ss, line)) logLines_.push_back({line, color});
    if (logLines_.size() > 500) logLines_.erase(logLines_.begin(), logLines_.begin() + (logLines_.size() - 500));
    scrollLogToBottom_ = true;
    TraceLog(LOG_INFO, "EDITOR: %s", message.c_str());
}

void Editor::drawOutput(Rectangle r) {
    drawHeader(r, "Output");
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
    BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width),
                     static_cast<int>(view.height));
    for (size_t i = 0; i < logLines_.size(); ++i) {
        float y = view.y + outputScroll_.y + 4 + i * lineH;
        if (y + lineH < view.y || y > view.y + view.height) continue;
        drawText(logLines_[i].text, view.x + 8, y, logLines_[i].color);
    }
    EndScissorMode();
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
    float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
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

void Editor::drawModal() {
    if (!dialog_.open) return;
    float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
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

void Editor::play() {
    if (playTree_) return;
    if (!editTree_.root()) {
        log("Add some nodes before pressing Play.", kBad);
        return;
    }
    activeField_.clear();
    scrubField_.clear();
    viewDrag_ = ViewDrag::None;
    playTree_ = std::make_unique<SceneTree>();
    playTree_->gameSize = editTree_.gameSize;
    playTree_->clearColor = editTree_.clearColor;
    playTree_->setRoot(cloneNode(*editTree_.root()));
    log("Playing " + scenePath_ + " ...", kAccent);
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
    node->name = uniqueChildName(parent, type);
    Node* raw = parent->addChild(std::move(node));
    parent->collapsed = false;
    select(raw);
}

void Editor::deleteSelected() {
    if (!selected_) return;
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
    if (!selected_) return;
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
    if (!selected_ || !selected_->parent()) return;
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
    log("Rebuild the editor (cmake --build build) to compile it, then press Play.", kOrange);
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
