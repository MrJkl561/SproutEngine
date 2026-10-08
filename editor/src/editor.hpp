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
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "code_editor.hpp"
#include "export.hpp"
#include "hot_reload.hpp"
#include "nodes.hpp"
#include "platform.hpp"
#include "project.hpp"
#include "projects.hpp"
#include "scene_tree.hpp"
#include "script_checker.hpp"
#include "text_editor.hpp"
#include "theme.hpp"

namespace s2d {

class Editor {
public:
    explicit Editor(std::filesystem::path projectDir);

    // Set when the user picked another project in the Project Manager; the
    // program then closes this editor and opens one for that project.
    std::optional<std::filesystem::path> takeProjectSwitch();
    void openProjectManager();
    ~Editor();

    void frame();  // handle input and draw one frame
    // The window's close button: asks about unsaved changes first; quitting()
    // turns true once it's fine to close.
    void requestQuit();
    bool quitting() const { return quit_; }
    bool openScene(const std::string& relativePath);
    void log(const std::string& message) { log(message, theme().colors.text); }
    void log(const std::string& message, Color color);

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
    void drawOutput(Rectangle r);  // the bottom panel: Output, or Animation for an AnimatedSprite2D
    void drawAnimationPanel(Rectangle r, AnimatedSprite2D& sprite);
    void drawSheetDialog();
    void syncAnimationToGame();  // edits made while playing reach the running game
    bool animationTabShown() const;
    void drawViewport(Rectangle r);
    void drawGameView(Rectangle r);
    void drawPopup();
    void drawModal();
    // A short explanation shown next to the mouse when it rests on r (for
    // buttons that show only an icon, and text that had to be cut short).
    void hint(Rectangle r, const std::string& text);
    // Text cut short with "..." to fit maxWidth; the whole of it shows as a
    // hint when the mouse is on it. fromLeft keeps the end (for paths).
    void drawFit(const std::string& text, float x, float y, float maxWidth, Color c, bool fromLeft = false);
    void drawHint();
    std::string hintText_, shownHint_;
    double hintSince_ = 0;
    void handleShortcuts();
    // Dragging the borders between panels (the sizes are kept per user).
    void updateSplitters(float sw, float sh, float leftW, float rightW, float bottomH, float sceneH);
    void loadLayout();
    void saveLayout() const;
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
    void propertyRow(Property& p, float x, float& y, float width, const std::string& idPrefix = "");
    void scriptVariableRows(float x, float& y, float width);  // the selected node's script variables
    void storeScriptVars();
    void animationsEditor(std::vector<SpriteAnimation>& list, const std::string& id, float x, float& y, float width);  // the inspector's script copy -> the node's saved values

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
    void play(bool mainScene);  // Play (the project's main scene) or Play Scene (this one)
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
    bool isTyping() const {
        return !activeField_.empty() || dialog_.open || confirm_.open || export_.open || projects_.open ||
               folderPick_.valid() || sheet_.open || codeEditorDialog_.open || instancesDialog_.open ||
               connectDialog_.open || (mainView_ == MainView::Script && (codeFocused_ || find_.findEdit || find_.withEdit));
    }

    // --- code editor (VS Code, Visual Studio...) -----------------------------
    void openInCodeEditor(const std::filesystem::path& file);  // a file in the project
    void openCodeEditorDialog();
    void drawCodeEditorDialog();
    void fileMenu(size_t index, Vector2 at);  // right-click on a file in the FileSystem dock
    std::filesystem::path scriptFileOf(const std::string& className) const;  // "" if not found
    void resetLayout();

    // --- the Script tab (built-in script editor) ------------------------------
    TextDocument* openDocument(const std::filesystem::path& file, int line = 0, int column = 0);
    void drawMainTabs(Rectangle r);  // 2D | Script
    void drawScriptView(Rectangle r);
    void drawFindBar(Rectangle r, TextDocument& doc);
    bool saveDocument(TextDocument& doc);
    void closeDocument(TextDocument* doc);  // asks first if it has unsaved changes
    void checkDocumentsOnDisk();
    void handleScriptShortcuts();
    void setDiagnostics(TextDocument& doc) const;  // the last build's messages for its file
    bool dialogOpen() const {
        return dialog_.open || confirm_.open || export_.open || projects_.open || folderPick_.valid() || sheet_.open ||
               codeEditorDialog_.open || instancesDialog_.open || connectDialog_.open;
    }

    // --- Signals (the Inspector's Signals list and its Connect dialog) --------
    void signalRows(float x, float& y, float w);
    void openConnectDialog(const SignalInfo& signal);
    void drawConnectDialog();
    // Adds "CALLABLE(method); void method(args) {}" to a script class (if it
    // isn't there) and shows it in the Script tab.
    void addCallableToScript(const std::string& className, const std::string& method, const std::string& args,
                             const std::string& comment);

    // --- Run Instances (several copies of the game, for multiplayer) ---------
    void openRunInstancesDialog();
    void drawRunInstancesDialog();
    void startInstances(const std::string& scene);  // the extra windows, for Play
    void stopInstances();
    void pumpInstances();  // their output, into the Output panel
    // Asks about the scene and any open files with unsaved changes, then goes on.
    void confirmUnsavedAll(std::function<void()> then);
    void confirmUnsavedDocuments(std::function<void()> then);

    // --- exporting -----------------------------------------------------------
    void drawProjectManager();
    // Opens the system's folder picker on a background thread (so the editor
    // keeps drawing); then runs with the chosen folder, if one was chosen.
    void pickFolder(const std::string& title, const std::filesystem::path& start,
                    std::function<void(const std::filesystem::path&)> then);
    void pumpFolderPicker();
    void switchProject(const std::filesystem::path& project);
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
    struct PanelSizes {
        float left = 290, right = 340, bottom = 170, animation = 280, sceneShare = 0.55f;
    } layout_;
    enum class Splitter { None, Left, Right, Bottom, SceneFiles } splitter_ = Splitter::None;
    Splitter lastSplitterClick_ = Splitter::None;
    double lastSplitterClickTime_ = -10;
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
    // A copy of the selected node's script, made only to list its variables and
    // their defaults (never run). Must go before the scripts library is unloaded.
    std::unique_ptr<Script> inspectorScript_;
    Node* inspectorScriptNode_ = nullptr;
    std::string inspectorScriptName_;
    nlohmann::json inspectorScriptDefaults_;
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
        std::string file;  // a compiler message's file and line: clicking it opens them
        int line = 0, column = 0;
        std::vector<std::string> rows;  // the text wrapped to the panel's width...
        float rowsWidth = -1;           // ...for this width
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
        bool wasBusy = false;  // to look at the platforms again when an export or setup finishes
    } export_;

    std::filesystem::path workDir_;  // this project's builds (scripts, exports)
    std::optional<std::filesystem::path> switchTo_;
    std::future<std::optional<std::filesystem::path>> folderPick_;
    std::function<void(const std::filesystem::path&)> folderPicked_;
    struct ProjectManagerDialog {
        bool open = false;
        int mode = 0;  // 0 the list, 1 new project, 2 open a folder
        int selected = -1;
        std::vector<std::filesystem::path> list;
        raylib::Vector2 scroll{0, 0};
        char name[64] = {};
        char location[512] = {};
        char path[512] = {};
        bool nameEdit = false, locationEdit = false, pathEdit = false;
        std::string error;
        std::vector<projects::ProjectTemplate> templates;  // what New Project can start from
        int templateIndex = 0;
    } projects_;

    // The Animation panel (like Godot's SpriteFrames panel)
    int bottomTab_ = 0;  // 0 Output, 1 Animation
    int animSel_ = 0;    // the animation picked in the panel's list
    int frameSel_ = 0;   // the frame picked in its filmstrip
    bool animPreview_ = true;  // playing the preview (else showing frameSel_)
    double animPreviewStart_ = 0;
    raylib::Vector2 animListScroll_{0, 0}, filmScroll_{0, 0};
    struct SheetDialog {  // "Add Frames from Sheet"
        bool open = false;
        int hframes = 1, vframes = 1;
        bool hEdit = false, vEdit = false;
        std::vector<int> picked;  // in the order they were clicked
    } sheet_;

    CodeEditor codeEditor_;
    CompletionIndex completions_;  // autocomplete for the Script tab
    // Live error checking and coloring of open scripts (clangd); made when the
    // first script is opened.
    std::unique_ptr<ScriptChecker> checker_;
    std::map<TextDocument*, long> checkedVersion_;  // the version last given to it
    void updateChecker();

    enum class MainView { View2D, Script } mainView_ = MainView::View2D;
    std::vector<std::unique_ptr<TextDocument>> docs_;
    TextDocument* currentDoc_ = nullptr;
    bool codeFocused_ = false;  // keys go to the script editor
    Rectangle codeRect_{};      // where it was drawn (clicking elsewhere takes the focus away)
    Font codeFont_{};           // the theme's code font, if it has one
    double lastDiskCheck_ = 0;
    struct FindBar {
        bool open = false, replace = false, matchCase = false;
        char find[256] = {};
        char with[256] = {};
        bool findEdit = false, withEdit = false;
    } find_;
    // Compiler messages from the last scripts build, by file.
    struct BuildMessage {
        std::filesystem::path file;
        Diagnostic diagnostic;
    };
    std::vector<BuildMessage> buildMessages_;
    bool quit_ = false;
    struct CodeEditorDialog {
        bool open = false;
        int selected = 0;  // in installedCodeEditors(); its size = Custom
        char command[512] = {};
        bool commandEdit = false;
    } codeEditorDialog_;

    struct ConnectDialog {
        bool open = false;
        SignalInfo signal;
        Node* emitter = nullptr;
        Node* target = nullptr;
        char method[96] = {};
        bool methodEdit = false;
        bool oneShot = false;
        bool scrollToTarget = true;  // (on opening)
        std::string autoName;        // the name it suggested (changes with the target until you type your own)
        raylib::Vector2 scroll{0, 0};
    } connectDialog_;

    struct RunInstancesDialog {
        bool open = false;
        int count = 1;
        char args[ProjectSettings::kMaxInstances][256] = {};
        int editing = -1;  // the text box being typed in
    } instancesDialog_;
    // Copies of the game running in windows of their own (sprout_editor --play)
    struct InstanceOutput {
        std::mutex mutex;
        std::vector<std::string> lines;
    };
    struct RunningInstance {
        int number = 0;
        std::unique_ptr<platform::ChildProcess> process;
        std::thread reader;
    };
    std::vector<RunningInstance> instances_;
    std::shared_ptr<InstanceOutput> instanceOutput_ = std::make_shared<InstanceOutput>();

    struct ConfirmDialog {
        bool open = false;
        std::string message;
        std::function<void()> then;
        std::function<bool()> save;  // what Save does ("" = save the scene); false = it failed
    } confirm_;
};

}  // namespace s2d
