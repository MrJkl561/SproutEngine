// The editor window: a Godot-style layout made with raygui.
//
//   +-----------------------------------------------------------+
//   | toolbar: New  Open  Save  Save As        Play  Stop       |
//   +-----------+-----------------------------------+-----------+
//   | Scene     |                                   | Inspector |
//   | (tree)    |          2D viewport              |           |
//   |-----------|                                   |           |
//   | FileSys.  +-----------------------------------+           |
//   |           |  Output                           |           |
//   +-----------+-----------------------------------+-----------+
#pragma once

#include <raylib-cpp.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "export.hpp"
#include "hot_reload.hpp"
#include "nodes.hpp"
#include "project.hpp"
#include "scene_tree.hpp"

namespace s2d {

class Editor {
public:
    Editor(std::filesystem::path projectDir, std::filesystem::path scriptsDir);
    ~Editor();

    void frame();  // handle input and draw one frame
    bool openScene(const std::string& relativePath);
    void log(const std::string& message, Color color = Color{200, 200, 200, 255});

    std::string screenshotPath;

    // Interface size: 1 = 100%, 1.5 = 150%. Starts at the system's display scale.
    void setUiScale(float scale);
    float uiScale() const { return uiScale_; }  // if set, the next frame is saved to this file

private:
    // --- panels -----------------------------------------------------------
    void drawToolbar(Rectangle r);
    void applyUiScale();  // makes everything drawn after it uiScale_ times bigger
    void drawSceneDock(Rectangle r);
    void drawFileSystem(Rectangle r);
    void drawInspector(Rectangle r);
    void drawOutput(Rectangle r);
    void drawViewport(Rectangle r);
    void drawGameView(Rectangle r);
    void drawPopup();
    void drawModal();
    void handleShortcuts();
    void finishDrag();

    // --- viewport helpers -------------------------------------------------
    void updateViewportInput(Rectangle r);
    void drawGrid(Rectangle r);
    Node2D* pickNode(Vector2 world);
    bool overRotateHandle(Vector2 mouse);
    raylib::Vector2 rotateHandlePos() const;

    // --- inspector widgets ------------------------------------------------
    bool floatField(Rectangle r, const std::string& id, float& value, float step);
    bool textField(Rectangle r, const std::string& id, std::string& value);
    void propertyRow(Property& p, float x, float& y, float width);

    // --- actions ----------------------------------------------------------
    void newScene();
    // Instanced scenes
    void instanceScene(const std::string& scenePath, Node* parent, const Vector2* world = nullptr);
    void saveBranchAsScene(Node* node, const std::string& scenePath);
    void makeLocal(Node* node);
    void resetInstance(Node* node);  // throw away this copy's changes
    bool lockedByInstance(Node* node, const char* action);  // true (and says why) inside an instance
    Node2D* selectableFor(Node2D* hit);  // what clicking `hit` in the viewport selects
    // Asks "Save changes?" first if the scene has unsaved changes.
    void confirmUnsaved(std::function<void()> then);
    void requestOpenScene(const std::string& scenePath);
    void saveScene();
    void play();
    void stop();
    void updateScripts();  // hot reloading: watch, rebuild, reload
    void reloadScripts();
    void addNode(const std::string& type);
    void deleteSelected();
    void duplicateSelected();
    void moveSelected(int direction);
    void createSpriteAt(const std::string& texture, Vector2 world);
    void createScript(const std::string& className);
    void activateFile(size_t index);
    void reparent(Node* node, Node* target, int zone);
    void renderGame();
    void rescanFiles();
    void ensureDefaultAssets();
    void setRoot(std::unique_ptr<Node> root);
    void select(Node* node);
    std::string uniqueChildName(Node* parent, const std::string& base) const;

    // --- undo -------------------------------------------------------------
    struct Snapshot {
        std::string scene;
        std::vector<int> selection;
    };
    Snapshot snapshot();
    void restore(const Snapshot& s);
    void touch();  // call before changing the scene: records an undo step (once per mouse drag)
    void undo();
    void redo();

    // --- popups and dialogs -----------------------------------------------
    void openPopup(Vector2 at, std::vector<std::string> items, std::function<void(int)> onPick);
    void openTextDialog(std::string title, std::string message, std::string initial,
                        std::function<void(const std::string&)> onOk);
    bool isTyping() const { return !activeField_.empty() || dialog_.open || confirm_.open || export_.open; }

    // --- exporting -----------------------------------------------------------
    void openExportDialog();
    std::string defaultExportDir() const;  // editor/exports
    void drawExportDialog();
    void startExport();
    void pumpExport();  // moves the exporter's progress lines into the Output panel

    // --- state ------------------------------------------------------------
    std::filesystem::path projectDir_;
    std::filesystem::path scriptsDir_;
    std::string scenePath_ = "scenes/main.scene.json";
    bool dirty_ = false;
    std::string windowTitle_;

    SceneTree editTree_;
    std::unique_ptr<SceneTree> playTree_;
    std::unique_ptr<ScriptReloader> scripts_;
    std::unique_ptr<raylib::RenderTexture> gameTexture_;
    Node* selected_ = nullptr;

    raylib::Camera2D viewCam_{{0, 0}, {400, 225}, 0.0f, 1.0f};
    Rectangle viewportRect_{};
    float uiScale_ = 1.0f;
    Font uiFont_{};  // the interface font, loaded at the size it's shown on screen
    Rectangle inspectorRect_{};
    int mouseCursor_ = 0;
    bool swallowMouse_ = false;  // a popup closed on mouse press: ignore the mouse until it's released
    bool clipLocked_ = false;    // the GUI is locked because the mouse is outside a scroll area
    bool panning_ = false;
    bool viewFramed_ = false;  // the view has been fitted to the game window
    enum class ViewDrag { None, Move, Rotate } viewDrag_ = ViewDrag::None;
    raylib::Vector2 grabOffset_{0, 0};

    std::vector<Snapshot> undoStack_, redoStack_;
    bool touchedThisDrag_ = false;

    // Inspector fields: which one is being typed in / scrubbed with the mouse.
    std::string activeField_;
    char fieldBuffer_[256] = {};
    std::string scrubField_;
    float scrubStartValue_ = 0;
    float scrubStartX_ = 0;
    bool scrubMoved_ = false;
    std::string openColorPicker_;
    raylib::Vector2 inspectorScroll_{0, 0};
    float inspectorContentHeight_ = 0;
    // For a node in an instanced scene: its values in the original scene, to
    // show which ones this copy changed (with a revert button).
    nlohmann::json inspectorOriginal_;
    nlohmann::json inspectorCurrent_;
    std::unique_ptr<Node> originalTree_;
    std::string originalKey_;
    double originalTime_ = -100;

    // Scene dock
    raylib::Vector2 treeScroll_{0, 0};
    raylib::Vector2 filesScroll_{0, 0};
    raylib::Vector2 outputScroll_{0, 0};

    // Drag and drop between panels
    enum class DragKind { None, TreeNode, File };
    struct DragState {
        DragKind kind = DragKind::None;
        Node* node = nullptr;
        size_t file = 0;
        raylib::Vector2 start{0, 0};
        bool active = false;  // moved far enough to count as a drag
    } drag_;
    // Filled in by the scene dock while drawing: where a dragged node would go.
    Node* dropTarget_ = nullptr;
    int dropZone_ = 0;  // -1 before, 0 inside, 1 after

    struct FileEntry {
        enum Kind { Texture, Scene, Script } kind;
        std::string path;         // textures/scenes: relative to the project; scripts: full path
        std::string label;        // what the FileSystem dock shows
        std::string scriptClass;  // scripts: the class REGISTER_SCRIPT names
    };
    std::vector<FileEntry> files_;
    double lastScan_ = -100;

    struct LogLine {
        std::string text;
        Color color;
    };
    std::vector<LogLine> logLines_;
    bool scrollLogToBottom_ = false;

    struct Popup {
        bool open = false;
        raylib::Vector2 pos{0, 0};
        std::vector<std::string> items;
        std::function<void(int)> onPick;
        bool justOpened = false;
    } popup_;

    struct TextDialog {
        bool open = false;
        std::string title, message;
        char text[128] = {};
        std::function<void(const std::string&)> onOk;
    } dialog_;

    ProjectSettings settings_;
    std::unique_ptr<Exporter> exporter_;
    struct ExportDialog {
        bool open = false;
        std::vector<ExportTarget> targets;
        std::vector<bool> chosen;
        char name[64] = {};
        char dir[512] = {};
        int width = 800, height = 450;
        bool nameEdit = false, dirEdit = false, widthEdit = false, heightEdit = false;
    } export_;

    struct ConfirmDialog {
        bool open = false;
        std::string message;
        std::function<void()> then;
    } confirm_;
};

}  // namespace s2d
