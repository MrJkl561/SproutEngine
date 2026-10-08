// The built-in script editor: one open text file (a C++ script, a scene's
// JSON...) with syntax colors, undo, find, and marks for compile errors.
// The Editor shows these in its Script tab.
#pragma once

#include <raylib.h>

#include <filesystem>
#include <string>
#include <vector>

#include "autocomplete.hpp"

namespace s2d {

enum class Language { Text, Cpp, Json };

// A compiler message about one line (1-based) of the file.
struct Diagnostic {
    int line = 1, column = 0;
    bool error = true;  // else a warning
    std::string message;
    int endLine = 0, endColumn = 0;  // where it ends (0 = just the word at line/column)
};

// What a name is (from clangd), for coloring it.
enum class SemanticKind : unsigned char { Type, Function, Member, Parameter, Macro, EnumMember, Namespace, Comment };
struct SemanticSpan {
    int line = 0, start = 0, end = 0;  // 0-based line; byte columns
    SemanticKind kind = SemanticKind::Type;
};

// How the editor draws: the font and sizes, in UI pixels.
struct TextStyle {
    Font font{};
    float fontSize = 16;
    float uiScale = 1;  // for clipping (scissor rectangles are in screen pixels)
};

class TextDocument {
public:
    std::filesystem::path path;  // full path
    Language language = Language::Text;
    std::vector<Diagnostic> diagnostics;

    bool load(const std::filesystem::path& file, std::string& error);
    bool save(std::string& error);  // writes the file (safely: a temp file, then renamed)
    std::string text() const;
    void setText(const std::string& text);  // replaces everything (undoable)
    bool dirty() const { return version_ != savedVersion_; }
    long version() const { return version_; }  // changes with every edit (and undo)
    // Colors from clangd for the text as it was at `version`. They stay on the
    // lines that haven't changed since, until newer ones come.
    void setSemanticSpans(long version, std::vector<SemanticSpan> spans);

    // The file was changed by something else since it was loaded or saved.
    bool changedOnDisk() const;
    void reloadFromDisk();          // keeps the cursor and scroll where they were
    void acceptDiskVersion();       // keep this text, stop reporting the change

    // Draws the editor in r; handles the keyboard if `keyboard` (it has the
    // focus) and the mouse if `mouse` (nothing is on top of it).
    void draw(Rectangle r, bool keyboard, bool mouse, const TextStyle& style);
    bool diskConflict = false;  // changed on disk while it had unsaved changes (the Editor asks)

    void goToLine(int line, int column = 0);  // 1-based line; scrolls it into view
    int cursorLine() const { return cursor_.line + 1; }
    int cursorColumn() const;  // 1-based, as shown
    int lineCount() const { return static_cast<int>(lines_.size()); }

    // Find / replace (the Editor draws the find bar). Matching ignores case
    // unless matchCase is set.
    // fromSelection: the current selection counts as a match (for find-as-you-type).
    bool findNext(const std::string& needle, bool backwards, bool matchCase, bool fromSelection = false);
    bool replaceSelection(const std::string& needle, const std::string& with, bool matchCase);
    int replaceAll(const std::string& needle, const std::string& with, bool matchCase);
    int countMatches(const std::string& needle, bool matchCase) const;
    std::string selectedText() const;
    std::string highlightNeedle;  // shown highlighted everywhere (the find bar's text)
    bool highlightMatchCase = false;

    bool undo();
    bool redo();

    bool hoverText = false;  // after draw(): the mouse is over the text (show the I-beam cursor)
    CompletionIndex* completer = nullptr;  // autocomplete for C++ files (set by the Editor)

private:
    struct Pos {
        int line = 0, col = 0;  // col is a byte index into the line
        bool operator==(const Pos& o) const { return line == o.line && col == o.col; }
        bool operator!=(const Pos& o) const { return !(*this == o); }
        bool operator<(const Pos& o) const { return line < o.line || (line == o.line && col < o.col); }
    };
    struct State {
        std::vector<std::string> lines;
        Pos cursor, anchor;
        long version = 0;
    };
    enum class EditKind { Other, Typing, Deleting };

    // Editing
    void beginEdit(EditKind kind);  // records an undo step (typing is grouped)
    void insert(const std::string& text);  // at the cursor, replacing the selection
    void eraseSelection();
    bool hasSelection() const { return cursor_ != anchor_; }
    std::pair<Pos, Pos> selection() const;
    std::string textBetween(Pos a, Pos b) const;
    void setLines(std::vector<std::string> lines);
    void indentLines(bool outdent);
    void toggleComment();
    void duplicateLine();
    void handleKeys();
    void typeChar(int codepoint);
    void newline();

    // Moving
    Pos clampPos(Pos p) const;
    Pos left(Pos p, bool word) const;
    Pos right(Pos p, bool word) const;
    void moveTo(Pos p, bool select, bool keepColumn = false);
    void selectWord(Pos p);

    // Layout
    int visualCol(int line, int byteCol) const;      // tabs count as several columns
    int byteColAt(int line, float visual) const;      // nearest byte index to a visual column
    Pos posAt(Vector2 mouse) const;
    void scrollToCursor();
    void moveVertical(int lines, bool select);
    static bool readFile(const std::filesystem::path& file, std::vector<std::string>& lines, bool& crlf,
                         std::string& error);
    void updateHighlight();

    std::vector<std::string> lines_{""};
    bool crlf_ = false;  // the file used \r\n line endings (kept when saving)
    Pos cursor_, anchor_;
    float wantedCol_ = -1;  // the visual column up/down tries to keep
    Vector2 scroll_{0, 0};  // in pixels
    bool keepCursorVisible_ = false;
    bool centerCursor_ = false;  // scroll so the cursor's line is in the middle (go to line)

    std::vector<State> undo_, redo_;
    EditKind lastEdit_ = EditKind::Other;
    double lastEditTime_ = 0;
    long version_ = 0, savedVersion_ = 0, nextVersion_ = 1;
    std::filesystem::file_time_type diskTime_{};

    // Mouse
    bool selecting_ = false, draggingBar_ = false, draggingHBar_ = false;
    float hbarGrab_ = 0;

    // Autocomplete popup
    void updateCompletion(bool forced);
    bool completionKeys();  // true if the key went to the popup
    void acceptCompletion();
    void drawCompletion(Rectangle r, const TextStyle& style);
    bool acOpen_ = false, acForced_ = false;
    // Closing brackets and quotes typed for you (on this line): typing one of
    // them again steps over it instead of adding another.
    int autoCloseLine_ = -1, autoClosed_ = 0;
    bool autoPair(int codepoint);   // brackets and quotes; true if it handled the key
    bool deletePair();              // Backspace between ( and ): both go
    std::vector<Completion> acItems_;
    int acSel_ = 0, acTop_ = 0;
    Pos acStart_;
    Rectangle acRect_{};
    float barGrab_ = 0;
    double lastClickTime_ = -10;
    Pos lastClickPos_;
    int clickCount_ = 0;
    double blinkStart_ = 0;

    // Layout of the last draw (for the mouse)
    Rectangle area_{};  // the text, right of the line numbers
    float charW_ = 8, lineH_ = 20, gutterW_ = 40;

    // Syntax colors, per byte, recomputed when the text changes
    long highlightVersion_ = -1;
    std::vector<std::vector<unsigned char>> colors_;
    int widestLine_ = 0;  // in columns, for horizontal scrolling
    std::vector<SemanticSpan> semantic_;
    std::vector<std::string> semanticLines_;  // the text the spans are for
};

Language languageFor(const std::filesystem::path& file);
// Whether the built-in editor should open this file (text files we know).
bool isEditableText(const std::filesystem::path& file);

}  // namespace s2d
