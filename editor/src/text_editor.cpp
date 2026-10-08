#include "text_editor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

#include "theme.hpp"

namespace fs = std::filesystem;

namespace s2d {

namespace {

enum Kind : unsigned char {
    Plain, Keyword, Type, Function, String, Number, Comment, Preprocessor, Key,
    Member, Parameter, Macro, EnumMember, Namespace  // these come from clangd
};

const std::set<std::string> kCppKeywords = {
    "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "goto", "try",
    "catch", "throw", "new", "delete", "this", "nullptr", "true", "false", "class", "struct", "union", "enum",
    "namespace", "using", "typedef", "template", "typename", "public", "private", "protected", "virtual", "override",
    "final", "const", "constexpr", "consteval", "constinit", "static", "inline", "extern", "mutable", "volatile",
    "explicit", "friend", "operator", "sizeof", "decltype", "static_assert", "static_cast", "dynamic_cast",
    "const_cast", "reinterpret_cast", "noexcept", "co_await", "co_return", "co_yield", "concept", "requires",
    "and", "or", "not"};
const std::set<std::string> kCppTypes = {
    "void", "bool", "char", "wchar_t", "char8_t", "char16_t", "char32_t", "short", "int", "long", "float", "double",
    "signed", "unsigned", "auto", "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "std", "string", "vector", "map", "unique_ptr", "shared_ptr"};

bool isWordByte(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }
bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }
bool ctrlDown() {
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) ||
           IsKeyDown(KEY_RIGHT_SUPER);
}
bool shiftDown() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
bool pressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Every place the needle is found in the line (byte offsets).
std::vector<size_t> matchesIn(const std::string& line, const std::string& needle, bool matchCase) {
    std::vector<size_t> out;
    if (needle.empty()) return out;
    const std::string hay = matchCase ? line : lower(line);
    const std::string n = matchCase ? needle : lower(needle);
    for (size_t pos = hay.find(n); pos != std::string::npos; pos = hay.find(n, pos + n.size())) out.push_back(pos);
    return out;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines{""};
    for (char c : text) {
        if (c == '\n') lines.emplace_back();
        else if (c != '\r') lines.back() += c;
    }
    return lines;
}

void highlightCpp(const std::vector<std::string>& lines, std::vector<std::vector<unsigned char>>& out) {
    bool inBlock = false;
    for (size_t n = 0; n < lines.size(); ++n) {
        const std::string& s = lines[n];
        std::vector<unsigned char>& k = out[n];
        size_t len = s.size(), i = 0;
        auto fill = [&](size_t a, size_t b, Kind kind) { std::fill(k.begin() + a, k.begin() + std::min(b, len), kind); };
        if (inBlock) {
            size_t end = s.find("*/");
            fill(0, end == std::string::npos ? len : end + 2, Comment);
            if (end == std::string::npos) continue;
            inBlock = false;
            i = end + 2;
        }
        size_t first = s.find_first_not_of(" \t");
        if (i == 0 && first != std::string::npos && s[first] == '#') {
            size_t comment = s.find("//", first);
            fill(first, comment, Preprocessor);
            if (comment != std::string::npos) fill(comment, len, Comment);
            continue;
        }
        while (i < len) {
            unsigned char c = s[i];
            char next = i + 1 < len ? s[i + 1] : '\0';
            if (c == '/' && next == '/') {
                fill(i, len, Comment);
                break;
            }
            if (c == '/' && next == '*') {
                size_t end = s.find("*/", i + 2);
                if (end == std::string::npos) {
                    fill(i, len, Comment);
                    inBlock = true;
                    break;
                }
                fill(i, end + 2, Comment);
                i = end + 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                size_t j = i + 1;
                while (j < len) {
                    if (s[j] == '\\') j += 2;
                    else if (s[j++] == static_cast<char>(c)) break;
                }
                fill(i, j, String);
                i = std::min(j, len);
                continue;
            }
            if (std::isdigit(c) || (c == '.' && std::isdigit(static_cast<unsigned char>(next)))) {
                size_t j = i;
                while (j < len && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '.' || s[j] == '\'' ||
                                   ((s[j] == '-' || s[j] == '+') && j > i && (s[j - 1] == 'e' || s[j - 1] == 'E'))))
                    ++j;
                fill(i, j, Number);
                i = j;
                continue;
            }
            if (std::isalpha(c) || c == '_') {
                size_t j = i;
                while (j < len && isWordByte(static_cast<unsigned char>(s[j]))) ++j;
                std::string word = s.substr(i, j - i);
                size_t after = s.find_first_not_of(" \t", j);
                Kind kind = kCppKeywords.count(word)                           ? Keyword
                            : kCppTypes.count(word)                            ? Type
                            : after != std::string::npos && s[after] == '('    ? Function
                            : std::isupper(c)                                  ? Type
                                                                               : Plain;
                fill(i, j, kind);
                i = j;
                continue;
            }
            ++i;
        }
    }
}

void highlightJson(const std::vector<std::string>& lines, std::vector<std::vector<unsigned char>>& out) {
    for (size_t n = 0; n < lines.size(); ++n) {
        const std::string& s = lines[n];
        std::vector<unsigned char>& k = out[n];
        size_t len = s.size(), i = 0;
        auto fill = [&](size_t a, size_t b, Kind kind) { std::fill(k.begin() + a, k.begin() + std::min(b, len), kind); };
        while (i < len) {
            unsigned char c = s[i];
            if (c == '"') {
                size_t j = i + 1;
                while (j < len) {
                    if (s[j] == '\\') j += 2;
                    else if (s[j++] == '"') break;
                }
                j = std::min(j, len);
                size_t after = s.find_first_not_of(" \t", j);
                fill(i, j, after != std::string::npos && s[after] == ':' ? Key : String);
                i = j;
                continue;
            }
            if (std::isdigit(c) || c == '-') {
                size_t j = i + 1;
                while (j < len && (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '.' || s[j] == '-' ||
                                   s[j] == '+'))
                    ++j;
                fill(i, j, Number);
                i = j;
                continue;
            }
            if (std::isalpha(c)) {
                size_t j = i;
                while (j < len && std::isalpha(static_cast<unsigned char>(s[j]))) ++j;
                std::string word = s.substr(i, j - i);
                if (word == "true" || word == "false" || word == "null") fill(i, j, Keyword);
                i = j;
                continue;
            }
            ++i;
        }
    }
}

Color colorOf(unsigned char kind) {
    const auto& c = theme().code;
    switch (kind) {
        case Keyword: return c.keyword;
        case Type: return c.type;
        case Function: return c.function;
        case String: return c.string;
        case Number: return c.number;
        case Comment: return c.comment;
        case Preprocessor: return c.preprocessor;
        case Key: return c.key;
        case Member: return c.member;
        case Parameter: return c.parameter;
        case Macro: return c.macro;
        case EnumMember: return c.constant;
        case Namespace: return c.namespaceName;
        default: return c.text;
    }
}

}  // namespace

Language languageFor(const fs::path& file) {
    std::string ext = lower(file.extension().string());
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" || ext == ".cxx" || ext == ".hh" || ext == ".inl")
        return Language::Cpp;
    if (ext == ".json") return Language::Json;
    return Language::Text;
}

bool isEditableText(const fs::path& file) {
    std::string ext = lower(file.extension().string());
    return languageFor(file) != Language::Text || ext == ".txt" || ext == ".md" || ext == ".cmake" ||
           ext == ".glsl" || ext == ".fs" || ext == ".vs" || file.filename() == "CMakeLists.txt";
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
bool TextDocument::readFile(const fs::path& file, std::vector<std::string>& lines, bool& crlf, std::string& error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        error = "can't read " + file.string();
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    crlf = text.find("\r\n") != std::string::npos;
    lines = splitLines(text);
    return true;
}

bool TextDocument::load(const fs::path& file, std::string& error) {
    std::vector<std::string> lines;
    if (!readFile(file, lines, crlf_, error)) return false;
    path = file;
    language = languageFor(file);
    lines_ = std::move(lines);
    cursor_ = anchor_ = {};
    scroll_ = {0, 0};
    undo_.clear();
    redo_.clear();
    version_ = savedVersion_ = 0;
    std::error_code ec;
    diskTime_ = fs::last_write_time(file, ec);
    return true;
}

bool TextDocument::save(std::string& error) {
    std::string text;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i) text += crlf_ ? "\r\n" : "\n";
        text += lines_[i];
    }
    fs::path temp = path;
    temp += ".sprout-saving";
    {
        std::ofstream out(temp, std::ios::binary);
        if (!out || !(out << text)) {
            error = "can't write " + temp.string();
            return false;
        }
    }
    std::error_code ec;
    fs::rename(temp, path, ec);
    if (ec) {
        fs::remove(temp, ec);
        error = "can't replace " + path.string();
        return false;
    }
    savedVersion_ = version_;
    diskTime_ = fs::last_write_time(path, ec);
    return true;
}

std::string TextDocument::text() const {
    std::string out;
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (i) out += '\n';
        out += lines_[i];
    }
    return out;
}

void TextDocument::setText(const std::string& text) {
    beginEdit(EditKind::Other);
    lines_ = splitLines(text);
    cursor_ = anchor_ = clampPos(cursor_);
}

bool TextDocument::changedOnDisk() const {
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    return !ec && t != diskTime_;
}

void TextDocument::reloadFromDisk() {
    std::vector<std::string> lines;
    std::string error;
    if (!readFile(path, lines, crlf_, error)) return;
    if (lines != lines_) {
        beginEdit(EditKind::Other);  // so the reload can be undone
        lines_ = std::move(lines);
        cursor_ = clampPos(cursor_);
        anchor_ = clampPos(anchor_);
    }
    savedVersion_ = version_;
    keepCursorVisible_ = false;
    std::error_code ec;
    diskTime_ = fs::last_write_time(path, ec);
}

void TextDocument::acceptDiskVersion() {
    std::error_code ec;
    diskTime_ = fs::last_write_time(path, ec);
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------
void TextDocument::beginEdit(EditKind kind) {
    double now = GetTime();
    bool group = kind != EditKind::Other && kind == lastEdit_ && now - lastEditTime_ < 1.5 && !hasSelection() &&
                 !undo_.empty();
    if (!group) {
        undo_.push_back({lines_, cursor_, anchor_, version_});
        if (undo_.size() > 300) undo_.erase(undo_.begin());
    }
    redo_.clear();
    lastEdit_ = kind;
    lastEditTime_ = now;
    version_ = nextVersion_++;
    keepCursorVisible_ = true;
    blinkStart_ = now;
    if (language == Language::Json) diagnostics.clear();  // a JSON error is re-checked on the next save
}

bool TextDocument::undo() {
    if (undo_.empty()) return false;
    redo_.push_back({lines_, cursor_, anchor_, version_});
    State s = std::move(undo_.back());
    undo_.pop_back();
    lines_ = std::move(s.lines);
    cursor_ = s.cursor;
    anchor_ = s.anchor;
    version_ = s.version;
    lastEdit_ = EditKind::Other;
    keepCursorVisible_ = true;
    if (language == Language::Json) diagnostics.clear();
    return true;
}

bool TextDocument::redo() {
    if (redo_.empty()) return false;
    undo_.push_back({lines_, cursor_, anchor_, version_});
    State s = std::move(redo_.back());
    redo_.pop_back();
    lines_ = std::move(s.lines);
    cursor_ = s.cursor;
    anchor_ = s.anchor;
    version_ = s.version;
    lastEdit_ = EditKind::Other;
    keepCursorVisible_ = true;
    if (language == Language::Json) diagnostics.clear();
    return true;
}

std::pair<TextDocument::Pos, TextDocument::Pos> TextDocument::selection() const {
    return cursor_ < anchor_ ? std::pair{cursor_, anchor_} : std::pair{anchor_, cursor_};
}

std::string TextDocument::textBetween(Pos a, Pos b) const {
    if (a.line == b.line) return lines_[a.line].substr(a.col, b.col - a.col);
    std::string out = lines_[a.line].substr(a.col);
    for (int l = a.line + 1; l < b.line; ++l) out += "\n" + lines_[l];
    return out + "\n" + lines_[b.line].substr(0, b.col);
}

std::string TextDocument::selectedText() const {
    auto [a, b] = selection();
    return textBetween(a, b);
}

void TextDocument::eraseSelection() {
    if (!hasSelection()) return;
    auto [a, b] = selection();
    lines_[a.line] = lines_[a.line].substr(0, a.col) + lines_[b.line].substr(b.col);
    lines_.erase(lines_.begin() + a.line + 1, lines_.begin() + b.line + 1);
    cursor_ = anchor_ = a;
}

void TextDocument::insert(const std::string& text) {
    eraseSelection();
    std::vector<std::string> parts = splitLines(text);
    std::string& line = lines_[cursor_.line];
    std::string tail = line.substr(cursor_.col);
    line = line.substr(0, cursor_.col) + parts[0];
    if (parts.size() == 1) {
        line += tail;
        cursor_.col += static_cast<int>(parts[0].size());
    } else {
        lines_.insert(lines_.begin() + cursor_.line + 1, parts.begin() + 1, parts.end());
        cursor_.line += static_cast<int>(parts.size()) - 1;
        cursor_.col = static_cast<int>(lines_[cursor_.line].size());
        lines_[cursor_.line] += tail;
    }
    anchor_ = cursor_;
    wantedCol_ = -1;
}

void TextDocument::typeChar(int codepoint) {
    int size = 0;
    const char* utf8 = CodepointToUTF8(codepoint, &size);
    beginEdit(EditKind::Typing);
    int tab = std::max(1, theme().code.tabSize);
    if ((codepoint == '}' || codepoint == ']') && !hasSelection()) {
        // A closing brace on an empty line goes back one indent.
        const std::string& line = lines_[cursor_.line];
        bool blank = line.find_first_not_of(' ') >= static_cast<size_t>(cursor_.col);
        if (blank && cursor_.col >= tab) {
            lines_[cursor_.line].erase(cursor_.col - tab, tab);
            cursor_.col -= tab;
            anchor_ = cursor_;
        }
    }
    insert(std::string(utf8, size));

    // Autocomplete: while typing a name, or after . -> ::
    if (completer && language == Language::Cpp) {
        const std::string& line = lines_[cursor_.line];
        char before = cursor_.col >= 2 ? line[cursor_.col - 2] : '\0';
        bool word = codepoint < 128 && (std::isalnum(codepoint) || codepoint == '_');
        bool member = codepoint == '.' || (codepoint == '>' && before == '-') || (codepoint == ':' && before == ':');
        if (word || member) updateCompletion(word && acOpen_ && acForced_);
        else acOpen_ = false;
    }
}

void TextDocument::updateCompletion(bool forced) {
    if (!completer || language != Language::Cpp) {
        acOpen_ = false;
        return;
    }
    std::string prefix;
    std::vector<Completion> items = completer->complete(lines_, cursor_.line, cursor_.col, path, prefix, forced);
    if (items.empty()) {
        acOpen_ = false;
        return;
    }
    acItems_ = std::move(items);
    acStart_ = {cursor_.line, cursor_.col - static_cast<int>(prefix.size())};
    acSel_ = acTop_ = 0;
    acForced_ = forced;
    acOpen_ = true;
}

bool TextDocument::completionKeys() {
    if (!acOpen_) return false;
    int n = static_cast<int>(acItems_.size());
    if (pressed(KEY_DOWN)) acSel_ = (acSel_ + 1) % n;
    else if (pressed(KEY_UP)) acSel_ = (acSel_ - 1 + n) % n;
    else if (pressed(KEY_PAGE_DOWN)) acSel_ = std::min(n - 1, acSel_ + 8);
    else if (pressed(KEY_PAGE_UP)) acSel_ = std::max(0, acSel_ - 8);
    else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_TAB)) acceptCompletion();
    else if (IsKeyPressed(KEY_ESCAPE)) acOpen_ = false;
    else {
        if (pressed(KEY_LEFT) || pressed(KEY_RIGHT) || pressed(KEY_HOME) || pressed(KEY_END)) acOpen_ = false;
        return false;
    }
    return true;
}

void TextDocument::acceptCompletion() {
    if (!acOpen_ || acSel_ < 0 || acSel_ >= static_cast<int>(acItems_.size())) return;
    const Completion c = acItems_[acSel_];
    acOpen_ = false;
    beginEdit(EditKind::Other);
    // Replace the whole name under the cursor
    const std::string& line = lines_[cursor_.line];
    Pos end = cursor_;
    while (end.col < static_cast<int>(line.size()) && isWordByte(static_cast<unsigned char>(line[end.col]))) ++end.col;
    anchor_ = acStart_.line == cursor_.line ? acStart_ : cursor_;
    cursor_ = end;
    insert(c.name);
    if (c.kind == Completion::Kind::Function) {
        const std::string& now = lines_[cursor_.line];
        char next = cursor_.col < static_cast<int>(now.size()) ? now[cursor_.col] : '\0';
        if (next != '(' && next != '<') {
            if (c.templated) {
                insert("<>()");
                cursor_.col -= 3;  // between < and >
            } else {
                insert("()");
                if (c.params) cursor_.col -= 1;
            }
            anchor_ = cursor_;
        }
    }
}

void TextDocument::newline() {
    beginEdit(EditKind::Other);
    eraseSelection();
    const std::string& line = lines_[cursor_.line];
    std::string before = line.substr(0, cursor_.col);
    std::string indent = before.substr(0, before.find_first_not_of(" \t"));
    if (indent.size() > before.size()) indent = before;
    size_t lastChar = before.find_last_not_of(" \t");
    bool opens = lastChar != std::string::npos && (before[lastChar] == '{' || before[lastChar] == '[' || before[lastChar] == '(');
    size_t nextChar = line.find_first_not_of(" \t", cursor_.col);
    bool closes = nextChar != std::string::npos && (line[nextChar] == '}' || line[nextChar] == ']' || line[nextChar] == ')');
    std::string extra = opens ? std::string(std::max(1, theme().code.tabSize), ' ') : "";
    if (opens && closes) {
        insert("\n" + indent + extra + "\n" + indent);
        cursor_ = anchor_ = {cursor_.line - 1, static_cast<int>((indent + extra).size())};
        // drop the spaces before the closing brace that the split left behind
        std::string& next = lines_[cursor_.line + 1];
        size_t keep = indent.size();
        size_t brace = next.find_first_not_of(" \t", keep);
        if (brace != std::string::npos && brace > keep) next.erase(keep, brace - keep);
    } else {
        insert("\n" + indent + extra);
        // whitespace after the cursor is dropped (it moved to the new line's start)
        std::string& now = lines_[cursor_.line];
        size_t rest = now.find_first_not_of(" \t", cursor_.col);
        if (rest != std::string::npos && rest > static_cast<size_t>(cursor_.col)) now.erase(cursor_.col, rest - cursor_.col);
    }
}

void TextDocument::indentLines(bool outdent) {
    auto [a, b] = selection();
    int last = b.line;
    if (b.col == 0 && b.line > a.line) --last;
    int tab = std::max(1, theme().code.tabSize);
    beginEdit(EditKind::Other);
    for (int l = a.line; l <= last; ++l) {
        std::string& line = lines_[l];
        int delta = 0;
        if (outdent) {
            int n = 0;
            while (n < tab && n < static_cast<int>(line.size()) && line[n] == ' ') ++n;
            if (n == 0 && !line.empty() && line[0] == '\t') n = 1;
            line.erase(0, n);
            delta = -n;
        } else {
            if (line.empty()) continue;
            line.insert(0, std::string(tab, ' '));
            delta = tab;
        }
        for (Pos* p : {&cursor_, &anchor_})
            if (p->line == l) p->col = std::max(0, p->col + delta);
    }
}

void TextDocument::toggleComment() {
    if (language != Language::Cpp) return;
    auto [a, b] = selection();
    int last = b.line;
    if (b.col == 0 && b.line > a.line) --last;
    bool allCommented = true;
    size_t indent = std::string::npos;
    for (int l = a.line; l <= last; ++l) {
        size_t first = lines_[l].find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        indent = std::min(indent, first);
        if (lines_[l].compare(first, 2, "//") != 0) allCommented = false;
    }
    if (indent == std::string::npos) return;  // only blank lines
    beginEdit(EditKind::Other);
    for (int l = a.line; l <= last; ++l) {
        std::string& line = lines_[l];
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        int delta;
        size_t at;
        if (allCommented) {
            int n = line.compare(first, 3, "// ") == 0 ? 3 : 2;
            line.erase(first, n);
            delta = -n;
            at = first;
        } else {
            line.insert(indent, "// ");
            delta = 3;
            at = indent;
        }
        for (Pos* p : {&cursor_, &anchor_})
            if (p->line == l && p->col >= static_cast<int>(at)) p->col = std::max(static_cast<int>(at), p->col + delta);
    }
}

void TextDocument::duplicateLine() {
    auto [a, b] = selection();
    int last = b.line;
    if (b.col == 0 && b.line > a.line) --last;
    beginEdit(EditKind::Other);
    std::vector<std::string> copy(lines_.begin() + a.line, lines_.begin() + last + 1);
    lines_.insert(lines_.begin() + last + 1, copy.begin(), copy.end());
    int n = last - a.line + 1;
    cursor_.line += n;
    anchor_.line += n;
}

// ---------------------------------------------------------------------------
// Moving
// ---------------------------------------------------------------------------
TextDocument::Pos TextDocument::clampPos(Pos p) const {
    p.line = std::clamp(p.line, 0, static_cast<int>(lines_.size()) - 1);
    p.col = std::clamp(p.col, 0, static_cast<int>(lines_[p.line].size()));
    while (p.col > 0 && p.col < static_cast<int>(lines_[p.line].size()) &&
           isContinuation(static_cast<unsigned char>(lines_[p.line][p.col])))
        --p.col;
    return p;
}

TextDocument::Pos TextDocument::left(Pos p, bool word) const {
    const std::string& s = lines_[p.line];
    if (p.col == 0) return p.line > 0 ? Pos{p.line - 1, static_cast<int>(lines_[p.line - 1].size())} : p;
    if (!word) {
        do --p.col;
        while (p.col > 0 && isContinuation(static_cast<unsigned char>(s[p.col])));
        return p;
    }
    while (p.col > 0 && std::isspace(static_cast<unsigned char>(s[p.col - 1]))) --p.col;
    if (p.col > 0 && isWordByte(static_cast<unsigned char>(s[p.col - 1]))) {
        while (p.col > 0 && isWordByte(static_cast<unsigned char>(s[p.col - 1]))) --p.col;
    } else {
        while (p.col > 0 && !isWordByte(static_cast<unsigned char>(s[p.col - 1])) &&
               !std::isspace(static_cast<unsigned char>(s[p.col - 1])))
            --p.col;
    }
    return p;
}

TextDocument::Pos TextDocument::right(Pos p, bool word) const {
    const std::string& s = lines_[p.line];
    int len = static_cast<int>(s.size());
    if (p.col >= len) return p.line + 1 < static_cast<int>(lines_.size()) ? Pos{p.line + 1, 0} : p;
    if (!word) {
        do ++p.col;
        while (p.col < len && isContinuation(static_cast<unsigned char>(s[p.col])));
        return p;
    }
    if (isWordByte(static_cast<unsigned char>(s[p.col]))) {
        while (p.col < len && isWordByte(static_cast<unsigned char>(s[p.col]))) ++p.col;
    } else if (!std::isspace(static_cast<unsigned char>(s[p.col]))) {
        while (p.col < len && !isWordByte(static_cast<unsigned char>(s[p.col])) &&
               !std::isspace(static_cast<unsigned char>(s[p.col])))
            ++p.col;
    }
    while (p.col < len && std::isspace(static_cast<unsigned char>(s[p.col]))) ++p.col;
    return p;
}

void TextDocument::moveTo(Pos p, bool select, bool keepColumn) {
    cursor_ = clampPos(p);
    if (!select) anchor_ = cursor_;
    if (!keepColumn) wantedCol_ = -1;
    lastEdit_ = EditKind::Other;
    keepCursorVisible_ = true;
    blinkStart_ = GetTime();
}

void TextDocument::moveVertical(int lines, bool select) {
    if (wantedCol_ < 0) wantedCol_ = static_cast<float>(visualCol(cursor_.line, cursor_.col));
    int target = cursor_.line + lines;
    Pos p;
    if (target < 0) p = {0, 0};
    else if (target >= static_cast<int>(lines_.size()))
        p = {static_cast<int>(lines_.size()) - 1, static_cast<int>(lines_.back().size())};
    else p = {target, byteColAt(target, wantedCol_)};
    moveTo(p, select, true);
}

void TextDocument::selectWord(Pos p) {
    const std::string& s = lines_[p.line];
    int a = p.col, b = p.col;
    auto word = [&](int i) { return i >= 0 && i < static_cast<int>(s.size()) && isWordByte(static_cast<unsigned char>(s[i])); };
    if (!word(a) && word(a - 1)) a = b = a - 1;
    if (word(a)) {
        while (word(a - 1)) --a;
        while (word(b)) ++b;
    } else if (b < static_cast<int>(s.size())) {
        ++b;
    }
    anchor_ = {p.line, a};
    cursor_ = {p.line, b};
    wantedCol_ = -1;
}

void TextDocument::goToLine(int line, int column) {
    Pos p{line - 1, std::max(0, column - 1)};
    moveTo(p, false);
    centerCursor_ = true;
}

int TextDocument::cursorColumn() const { return visualCol(cursor_.line, cursor_.col) + 1; }

// ---------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------
bool TextDocument::findNext(const std::string& needle, bool backwards, bool matchCase, bool fromSelection) {
    if (needle.empty() || needle.find('\n') != std::string::npos) return false;
    auto [a, b] = selection();
    if (fromSelection) b = a;
    int n = static_cast<int>(lines_.size());
    int len = static_cast<int>(needle.size());
    for (int step = 0; step <= n; ++step) {
        int l = backwards ? ((a.line - step) % n + n) % n : (b.line + step) % n;
        std::vector<size_t> found = matchesIn(lines_[l], needle, matchCase);
        if (backwards) {
            for (auto it = found.rbegin(); it != found.rend(); ++it)
                if (step > 0 || static_cast<int>(*it) < a.col || (step == n)) {
                    anchor_ = {l, static_cast<int>(*it)};
                    cursor_ = {l, static_cast<int>(*it) + len};
                    centerCursor_ = true;
                    return true;
                }
        } else {
            for (size_t f : found)
                if (step > 0 || static_cast<int>(f) >= b.col) {
                    anchor_ = {l, static_cast<int>(f)};
                    cursor_ = {l, static_cast<int>(f) + len};
                    centerCursor_ = true;
                    return true;
                }
        }
    }
    return false;
}

bool TextDocument::replaceSelection(const std::string& needle, const std::string& with, bool matchCase) {
    std::string sel = selectedText();
    bool same = matchCase ? sel == needle : lower(sel) == lower(needle);
    if (hasSelection() && same) {
        beginEdit(EditKind::Other);
        insert(with);
    }
    return findNext(needle, false, matchCase);
}

int TextDocument::replaceAll(const std::string& needle, const std::string& with, bool matchCase) {
    int count = countMatches(needle, matchCase);
    if (count == 0) return 0;
    beginEdit(EditKind::Other);
    for (std::string& line : lines_) {
        std::vector<size_t> found = matchesIn(line, needle, matchCase);
        for (auto it = found.rbegin(); it != found.rend(); ++it) line.replace(*it, needle.size(), with);
    }
    cursor_ = anchor_ = clampPos(cursor_);
    return count;
}

int TextDocument::countMatches(const std::string& needle, bool matchCase) const {
    int count = 0;
    for (const std::string& line : lines_) count += static_cast<int>(matchesIn(line, needle, matchCase).size());
    return count;
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
int TextDocument::visualCol(int line, int byteCol) const {
    const std::string& s = lines_[line];
    int tab = std::max(1, theme().code.tabSize);
    int v = 0;
    for (int i = 0; i < byteCol && i < static_cast<int>(s.size()); ++i) {
        unsigned char c = s[i];
        if (c == '\t') v = (v / tab + 1) * tab;
        else if (!isContinuation(c)) ++v;
    }
    return v;
}

int TextDocument::byteColAt(int line, float visual) const {
    const std::string& s = lines_[line];
    int tab = std::max(1, theme().code.tabSize);
    int v = 0;
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        unsigned char c = s[i];
        if (isContinuation(c)) continue;
        int w = c == '\t' ? (v / tab + 1) * tab - v : 1;
        if (visual < v + w / 2.0f) return i;
        v += w;
    }
    return static_cast<int>(s.size());
}

TextDocument::Pos TextDocument::posAt(Vector2 mouse) const {
    int line = static_cast<int>(std::floor((mouse.y - area_.y + scroll_.y) / lineH_));
    line = std::clamp(line, 0, static_cast<int>(lines_.size()) - 1);
    float visual = (mouse.x - area_.x + scroll_.x) / charW_;
    return {line, byteColAt(line, std::max(0.0f, visual))};
}

void TextDocument::scrollToCursor() {
    float cy = cursor_.line * lineH_;
    if (centerCursor_) {
        scroll_.y = cy - area_.height / 2 + lineH_ / 2;
        centerCursor_ = false;
    } else {
        if (cy < scroll_.y) scroll_.y = cy;
        if (cy + lineH_ > scroll_.y + area_.height) scroll_.y = cy + lineH_ - area_.height;
    }
    float cx = visualCol(cursor_.line, cursor_.col) * charW_;
    float margin = charW_ * 4;
    if (cx < scroll_.x + margin) scroll_.x = std::max(0.0f, cx - margin);
    if (cx > scroll_.x + area_.width - margin) scroll_.x = cx - area_.width + margin;
}

void TextDocument::updateHighlight() {
    if (highlightVersion_ == version_ && colors_.size() == lines_.size()) return;
    highlightVersion_ = version_;
    colors_.assign(lines_.size(), {});
    widestLine_ = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
        colors_[i].assign(lines_[i].size(), Plain);
        widestLine_ = std::max(widestLine_, visualCol(static_cast<int>(i), static_cast<int>(lines_[i].size())));
    }
    if (language == Language::Cpp) highlightCpp(lines_, colors_);
    if (language == Language::Json) highlightJson(lines_, colors_);
    // clangd's colors, on the lines that are still as they were
    for (const SemanticSpan& span : semantic_) {
        if (span.line >= static_cast<int>(lines_.size()) || span.line >= static_cast<int>(semanticLines_.size()) ||
            lines_[span.line] != semanticLines_[span.line])
            continue;
        Kind kind;
        switch (span.kind) {
            case SemanticKind::Type: kind = Type; break;
            case SemanticKind::Function: kind = Function; break;
            case SemanticKind::Member: kind = Member; break;
            case SemanticKind::Parameter: kind = Parameter; break;
            case SemanticKind::Macro: kind = Macro; break;
            case SemanticKind::EnumMember: kind = EnumMember; break;
            case SemanticKind::Namespace: kind = Namespace; break;
            default: kind = Comment; break;  // code #if'd out
        }
        auto& row = colors_[span.line];
        for (int i = std::max(0, span.start); i < span.end && i < static_cast<int>(row.size()); ++i) row[i] = kind;
    }
}

void TextDocument::setSemanticSpans(long version, std::vector<SemanticSpan> spans) {
    if (version != version_) return;  // for text that has changed since
    semantic_ = std::move(spans);
    semanticLines_ = lines_;
    highlightVersion_ = -1;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void TextDocument::handleKeys() {
    bool ctrl = ctrlDown(), shift = shiftDown();
    if (completionKeys()) {
        while (GetCharPressed() > 0) {
        }
        return;
    }
    if (!ctrl) {
        for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed())
            if (ch >= 32 && ch != 127) typeChar(ch);
    } else {
        while (GetCharPressed() > 0) {
        }
    }

    if (pressed(KEY_LEFT)) {
        if (hasSelection() && !shift && !ctrl) moveTo(selection().first, false);
        else moveTo(left(cursor_, ctrl), shift);
    }
    if (pressed(KEY_RIGHT)) {
        if (hasSelection() && !shift && !ctrl) moveTo(selection().second, false);
        else moveTo(right(cursor_, ctrl), shift);
    }
    if (pressed(KEY_UP)) moveVertical(-1, shift);
    if (pressed(KEY_DOWN)) moveVertical(1, shift);
    int page = std::max(1, static_cast<int>(area_.height / lineH_) - 1);
    if (pressed(KEY_PAGE_UP)) {
        scroll_.y -= page * lineH_;
        moveVertical(-page, shift);
    }
    if (pressed(KEY_PAGE_DOWN)) {
        scroll_.y += page * lineH_;
        moveVertical(page, shift);
    }
    if (pressed(KEY_HOME)) {
        if (ctrl) {
            moveTo({0, 0}, shift);
        } else {
            // First press: where the text starts; again: the very start.
            size_t first = lines_[cursor_.line].find_first_not_of(" \t");
            int start = first == std::string::npos ? 0 : static_cast<int>(first);
            moveTo({cursor_.line, cursor_.col == start ? 0 : start}, shift);
        }
    }
    if (pressed(KEY_END)) {
        if (ctrl) moveTo({static_cast<int>(lines_.size()) - 1, static_cast<int>(lines_.back().size())}, shift);
        else moveTo({cursor_.line, static_cast<int>(lines_[cursor_.line].size())}, shift);
    }

    if (pressed(KEY_BACKSPACE)) {
        if (!hasSelection()) {
            Pos from = left(cursor_, ctrl);
            // In the indent, delete back to the previous tab stop.
            const std::string& s = lines_[cursor_.line];
            int tab = std::max(1, theme().code.tabSize);
            if (!ctrl && cursor_.col > 0 && s.find_first_not_of(' ') >= static_cast<size_t>(cursor_.col))
                from = {cursor_.line, cursor_.col - ((cursor_.col - 1) % tab + 1)};
            if (from != cursor_) {
                beginEdit(EditKind::Deleting);  // before selecting, so key presses group into one undo step
                anchor_ = from;
            }
        } else {
            beginEdit(EditKind::Other);
        }
        eraseSelection();
        if (acOpen_) {
            if (cursor_.line != acStart_.line || cursor_.col < acStart_.col) acOpen_ = false;
            else updateCompletion(acForced_);
        }
    }
    if (pressed(KEY_DELETE)) {
        if (!hasSelection()) {
            Pos to = right(cursor_, ctrl);
            if (to != cursor_) {
                beginEdit(EditKind::Deleting);
                anchor_ = to;
            }
        } else {
            beginEdit(EditKind::Other);
        }
        eraseSelection();
    }
    if (pressed(KEY_ENTER) || pressed(KEY_KP_ENTER)) newline();
    if (pressed(KEY_TAB)) {
        if (shift || (hasSelection() && cursor_.line != anchor_.line)) {
            indentLines(shift);
        } else {
            int tab = std::max(1, theme().code.tabSize);
            beginEdit(EditKind::Typing);
            int v = visualCol(cursor_.line, cursor_.col);
            insert(std::string(tab - v % tab, ' '));
        }
    }

    if (ctrl) {
        if (IsKeyPressed(KEY_SPACE)) updateCompletion(true);  // Ctrl+Space: suggest now
        if (IsKeyPressed(KEY_A)) {
            anchor_ = {0, 0};
            cursor_ = {static_cast<int>(lines_.size()) - 1, static_cast<int>(lines_.back().size())};
        }
        bool copy = IsKeyPressed(KEY_C), cut = IsKeyPressed(KEY_X);
        if (copy || cut) {
            if (!hasSelection()) {
                // No selection: the whole line
                anchor_ = {cursor_.line, 0};
                cursor_ = cursor_.line + 1 < static_cast<int>(lines_.size())
                              ? Pos{cursor_.line + 1, 0}
                              : Pos{cursor_.line, static_cast<int>(lines_[cursor_.line].size())};
                SetClipboardText((selectedText() + (cursor_.col == 0 ? "" : "\n")).c_str());
                if (copy) anchor_ = cursor_ = {anchor_.line, 0};
            } else {
                SetClipboardText(selectedText().c_str());
            }
            if (cut && hasSelection()) {
                beginEdit(EditKind::Other);
                eraseSelection();
            }
        }
        if (pressed(KEY_V)) {
            if (const char* clip = GetClipboardText(); clip && *clip) {
                beginEdit(EditKind::Other);
                insert(clip);
            }
        }
        if (pressed(KEY_Z)) shift ? redo() : undo();
        if (pressed(KEY_Y)) redo();
        if (IsKeyPressed(KEY_D)) duplicateLine();
        if (IsKeyPressed(KEY_SLASH)) toggleComment();
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void TextDocument::draw(Rectangle r, bool focused, bool mouse, const TextStyle& style) {
    const auto& c = theme().code;
    const Font& font = style.font;
    float size = style.fontSize;
    float scale = style.uiScale;
    auto snap = [scale](float v) { return std::round(v * scale) / scale; };
    charW_ = std::max({MeasureTextEx(font, "W", size, 0).x, MeasureTextEx(font, "M", size, 0).x,
                       MeasureTextEx(font, "m", size, 0).x});
    lineH_ = std::round(size * 1.4f);
    int digits = std::max(3, static_cast<int>(std::to_string(lines_.size()).size()));
    gutterW_ = std::round(digits * charW_ + 22);
    const float bar = 12;
    area_ = {r.x + gutterW_, r.y, r.width - gutterW_ - bar, r.height - bar};

    // The problems on the line the mouse is over (or else the cursor's line)
    // are shown in full in a strip under the text, wrapped to fit: the
    // message after the line itself is often too long to see.
    std::vector<const Diagnostic*> shown;
    {
        Vector2 mp = GetMousePosition();
        int hovered = -1;
        if (mouse && CheckCollisionPointRec(mp, {area_.x, area_.y, area_.width, area_.height}))
            hovered = static_cast<int>((mp.y - area_.y + scroll_.y) / std::max(1.0f, lineH_));
        auto collect = [&](int line) {
            shown.clear();
            for (const Diagnostic& d : diagnostics)
                if (d.line == line + 1) shown.push_back(&d);
            std::stable_sort(shown.begin(), shown.end(), [](auto* a, auto* b) { return a->error && !b->error; });
        };
        collect(hovered);
        if (shown.empty()) collect(cursor_.line);
    }
    std::vector<std::pair<std::string, bool>> strip;  // (text, error)
    if (!shown.empty()) {
        float maxW = r.width - bar - 24;
        auto width = [&](const std::string& t) { return MeasureTextEx(font, t.c_str(), size, 1).x; };
        for (const Diagnostic* d : shown) {
            std::string text = std::string(d->error ? "Error" : "Warning") + " (line " + std::to_string(d->line) + "): " +
                               d->message;
            // Word wrap (cutting inside a word only when it doesn't fit a line by itself)
            std::string line;
            size_t i = 0;
            while (i <= text.size()) {
                size_t sp = text.find(' ', i);
                if (sp == std::string::npos) sp = text.size();
                std::string word = text.substr(i, sp - i);
                std::string tried = line.empty() ? word : line + " " + word;
                if (width(tried) <= maxW) {
                    line = tried;
                } else {
                    if (!line.empty()) strip.push_back({line, d->error});
                    while (width(word) > maxW && word.size() > 1) {
                        size_t n = word.size();
                        while (n > 1 && width(word.substr(0, n)) > maxW) --n;
                        strip.push_back({word.substr(0, n), d->error});
                        word.erase(0, n);
                    }
                    line = word;
                }
                i = sp + 1;
            }
            if (!line.empty()) strip.push_back({line, d->error});
        }
        size_t maxRows = static_cast<size_t>(std::max(1.0f, (r.height - bar) / 3 / lineH_));
        if (strip.size() > maxRows) {
            strip.resize(maxRows);
            strip.back().first += " ...";
        }
    }
    float stripH = strip.empty() ? 0 : strip.size() * lineH_ + 8;
    area_.height -= stripH;

    // --- input ---------------------------------------------------------------
    if (focused) handleKeys();
    updateHighlight();
    Vector2 m = GetMousePosition();
    bool inside = mouse && CheckCollisionPointRec(m, r);
    float contentH = lines_.size() * lineH_ + area_.height * 0.5f;
    float maxY = std::max(0.0f, contentH - area_.height);
    // Wide enough to scroll to the end of the longest line, and of the messages after lines
    float widest = (widestLine_ + 4) * charW_;
    for (const Diagnostic& d : diagnostics) {
        int l = d.line - 1;
        if (l < 0 || l >= static_cast<int>(lines_.size())) continue;
        std::string msg = d.message.size() > 160 ? d.message.substr(0, 157) + "..." : d.message;
        widest = std::max(widest, (visualCol(l, static_cast<int>(lines_[l].size())) + 3) * charW_ +
                                      MeasureTextEx(font, msg.c_str(), size, 1).x + 2 * charW_);
    }
    float maxX = std::max(0.0f, widest - area_.width);
    Rectangle vbar{r.x + r.width - bar, r.y, bar, r.height - bar};
    float thumbH = std::max(24.0f, vbar.height * std::min(1.0f, area_.height / contentH));
    auto thumbY = [&] { return vbar.y + (maxY > 0 ? scroll_.y / maxY : 0) * (vbar.height - thumbH); };

    Rectangle hbar{area_.x, r.y + r.height - bar, area_.width, bar};
    float thumbW = maxX > 0 ? std::max(24.0f, hbar.width * area_.width / (area_.width + maxX)) : hbar.width;
    auto thumbX = [&] { return hbar.x + (maxX > 0 ? scroll_.x / maxX : 0) * (hbar.width - thumbW); };

    bool overPopup = acOpen_ && mouse && CheckCollisionPointRec(m, acRect_);
    if (inside || overPopup) {
        Vector2 wheel = GetMouseWheelMoveV();
        if (overPopup) {
            acTop_ -= static_cast<int>(wheel.y * 3);
        } else {
            if (shiftDown()) scroll_.x -= wheel.y * charW_ * 6;
            else scroll_.y -= wheel.y * lineH_ * 3;
            scroll_.x -= wheel.x * charW_ * 6;  // sideways on a touchpad
        }
    }
    hoverText = inside && !overPopup && CheckCollisionPointRec(m, area_);
    if (overPopup && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int row = acTop_ + static_cast<int>((m.y - acRect_.y - 2) / lineH_);
        if (row >= 0 && row < static_cast<int>(acItems_.size())) {
            acSel_ = row;
            acceptCompletion();
        }
    } else if (inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        acOpen_ = false;
        if (CheckCollisionPointRec(m, hbar)) {
            if (maxX > 0) {
                float tx = thumbX();
                if (m.x >= tx && m.x <= tx + thumbW) {
                    draggingHBar_ = true;
                    hbarGrab_ = m.x - tx;
                } else {
                    scroll_.x += (m.x < tx ? -1 : 1) * area_.width;
                }
            }
        } else if (CheckCollisionPointRec(m, vbar)) {
            float ty = thumbY();
            if (m.y >= ty && m.y <= ty + thumbH) {
                draggingBar_ = true;
                barGrab_ = m.y - ty;
            } else {
                scroll_.y += (m.y < ty ? -1 : 1) * area_.height;
            }
        } else if (m.y < area_.y + area_.height) {
            Pos p = posAt(m);
            double now = GetTime();
            clickCount_ = (now - lastClickTime_ < 0.4 && p.line == lastClickPos_.line) ? clickCount_ % 3 + 1 : 1;
            lastClickTime_ = now;
            lastClickPos_ = p;
            if (clickCount_ == 1) {
                moveTo(p, shiftDown());
                selecting_ = true;
            } else if (clickCount_ == 2) {
                selectWord(p);
            } else {
                anchor_ = {p.line, 0};
                cursor_ = p.line + 1 < static_cast<int>(lines_.size())
                              ? Pos{p.line + 1, 0}
                              : Pos{p.line, static_cast<int>(lines_[p.line].size())};
            }
            keepCursorVisible_ = false;
            blinkStart_ = now;
        }
    }
    if (draggingHBar_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hbar.width > thumbW)
            scroll_.x = (m.x - hbarGrab_ - hbar.x) / (hbar.width - thumbW) * maxX;
        else draggingHBar_ = false;
    }
    if (draggingBar_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && vbar.height > thumbH)
            scroll_.y = (m.y - barGrab_ - vbar.y) / (vbar.height - thumbH) * maxY;
        else draggingBar_ = false;
    }
    if (selecting_) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            cursor_ = posAt(m);
            if (m.y < area_.y) scroll_.y -= lineH_;
            if (m.y > area_.y + area_.height) scroll_.y += lineH_;
            if (m.x < area_.x) scroll_.x -= charW_ * 2;
            if (m.x > area_.x + area_.width) scroll_.x += charW_ * 2;
        } else {
            selecting_ = false;
        }
    }
    if (keepCursorVisible_ || centerCursor_) {
        scrollToCursor();
        keepCursorVisible_ = false;
    }
    scroll_.y = std::clamp(scroll_.y, 0.0f, maxY);
    scroll_.x = std::clamp(scroll_.x, 0.0f, maxX);

    // --- drawing --------------------------------------------------------------
    auto clip = [&](Rectangle rc) {
        BeginScissorMode(static_cast<int>(std::floor(rc.x * scale)), static_cast<int>(std::floor(rc.y * scale)),
                         static_cast<int>(std::ceil(rc.width * scale)), static_cast<int>(std::ceil(rc.height * scale)));
    };
    DrawRectangleRec(r, c.background);
    int first = std::max(0, static_cast<int>(scroll_.y / lineH_));
    int last = std::min(static_cast<int>(lines_.size()) - 1, static_cast<int>((scroll_.y + area_.height) / lineH_) + 1);
    auto [selA, selB] = selection();
    float textY = (lineH_ - size) / 2;
    auto worstDiagnostic = [&](int line) -> const Diagnostic* {
        const Diagnostic* worst = nullptr;
        for (const Diagnostic& d : diagnostics)
            if (d.line == line + 1 && (!worst || (d.error && !worst->error))) worst = &d;
        return worst;
    };

    clip(area_);
    for (int l = first; l <= last; ++l) {
        float y = snap(area_.y + l * lineH_ - scroll_.y);
        float x0 = snap(area_.x - scroll_.x);
        const std::string& s = lines_[l];
        if (l == cursor_.line && !hasSelection()) DrawRectangleRec({area_.x, y, area_.width, lineH_}, c.currentLine);
        const Diagnostic* diag = worstDiagnostic(l);
        if (diag) DrawRectangleRec({area_.x, y, area_.width, lineH_}, Fade(diag->error ? c.error : c.warning, 0.12f));
        // Find matches
        for (size_t f : matchesIn(s, highlightNeedle, highlightMatchCase)) {
            float a = static_cast<float>(visualCol(l, static_cast<int>(f)));
            float b = static_cast<float>(visualCol(l, static_cast<int>(f + highlightNeedle.size())));
            DrawRectangleRec({x0 + a * charW_, y, (b - a) * charW_, lineH_}, c.match);
        }
        // Selection
        if (hasSelection() && l >= selA.line && l <= selB.line) {
            float a = l == selA.line ? static_cast<float>(visualCol(l, selA.col)) : 0;
            float b = l == selB.line ? static_cast<float>(visualCol(l, selB.col))
                                     : static_cast<float>(visualCol(l, static_cast<int>(s.size())) + 1);
            DrawRectangleRec({x0 + a * charW_, y, std::max(2.0f, (b - a) * charW_), lineH_}, c.selection);
        }
        // Text, one character per column
        int tab = std::max(1, theme().code.tabSize);
        int v = 0;
        int firstCol = static_cast<int>(scroll_.x / charW_) - 1;
        int lastCol = firstCol + static_cast<int>(area_.width / charW_) + 3;
        for (size_t i = 0; i < s.size();) {
            unsigned char ch = s[i];
            if (ch == '\t') {
                v = (v / tab + 1) * tab;
                ++i;
                continue;
            }
            int bytes = 0;
            int cp = GetCodepointNext(s.c_str() + i, &bytes);
            if (bytes <= 0) bytes = 1;
            if (ch != ' ' && v >= firstCol && v <= lastCol) {
                int gi = GetGlyphIndex(font, cp);
                float adv = (font.glyphs[gi].advanceX ? font.glyphs[gi].advanceX : font.recs[gi].width) * size /
                            static_cast<float>(font.baseSize);
                Color col = i < colors_[l].size() ? colorOf(colors_[l][i]) : c.text;
                DrawTextCodepoint(font, cp, {snap(x0 + v * charW_ + (charW_ - adv) / 2), snap(y + textY)}, size, col);
            }
            ++v;
            i += bytes;
        }
        // A wavy line under each problem on this line
        for (const Diagnostic& d : diagnostics) {
            if (l + 1 < d.line || (d.endLine > 0 ? l + 1 > d.endLine : l + 1 != d.line)) continue;
            int from = l + 1 == d.line ? std::max(0, d.column - 1) : 0;
            int to;
            if (d.endLine > 0 && l + 1 < d.endLine) {
                to = static_cast<int>(s.size());
            } else if (d.endLine > 0 && d.endColumn - 1 > from) {
                to = d.endColumn - 1;
            } else {  // no end given: the word that starts there
                to = from;
                while (to < static_cast<int>(s.size()) &&
                       (std::isalnum(static_cast<unsigned char>(s[to])) || s[to] == '_'))
                    ++to;
                if (to == from) to = from + 1;
            }
            from = std::min(from, static_cast<int>(s.size()));
            to = std::min(std::max(to, from + 1), static_cast<int>(s.size()) + 1);
            float ax = x0 + visualCol(l, from) * charW_;
            float bx = x0 + (to > static_cast<int>(s.size()) ? visualCol(l, static_cast<int>(s.size())) + 1
                                                             : visualCol(l, to)) * charW_;
            Color wc = d.error ? c.error : c.warning;
            float wy = y + lineH_ - 3;
            for (float wx = ax; wx < bx; wx += 4) {
                float nx = std::min(wx + 2, bx), mx = std::min(wx + 4, bx);
                DrawLineEx({wx, wy + 1.5f}, {nx, wy - 1.5f}, 1.2f, wc);
                if (nx < bx) DrawLineEx({nx, wy - 1.5f}, {mx, wy + 1.5f}, 1.2f, wc);
            }
        }
        // The compiler's message, after the line
        if (diag) {
            std::string msg = diag->message;
            if (msg.size() > 160) msg = msg.substr(0, 157) + "...";
            DrawTextEx(font, msg.c_str(), {snap(x0 + (v + 3) * charW_), snap(y + textY)}, size, 1,
                       Fade(diag->error ? c.error : c.warning, 0.85f));
        }
    }
    // Cursor
    if (focused && std::fmod(GetTime() - blinkStart_, 1.0) < 0.6) {
        float x = snap(area_.x - scroll_.x + visualCol(cursor_.line, cursor_.col) * charW_);
        float y = snap(area_.y + cursor_.line * lineH_ - scroll_.y);
        DrawRectangleRec({x, y + 2, 2, lineH_ - 4}, c.cursor);
    }
    EndScissorMode();

    // Line numbers
    Rectangle gutter{r.x, r.y, gutterW_, area_.height};
    clip(gutter);
    DrawRectangleRec(gutter, c.background);
    for (int l = first; l <= last; ++l) {
        float y = snap(area_.y + l * lineH_ - scroll_.y);
        std::string num = std::to_string(l + 1);
        float w = MeasureTextEx(font, num.c_str(), size, 1).x;
        DrawTextEx(font, num.c_str(), {snap(r.x + gutterW_ - 10 - w), snap(y + textY)}, size, 1,
                   l == cursor_.line ? c.text : c.lineNumbers);
        if (const Diagnostic* d = worstDiagnostic(l))
            DrawCircleV({r.x + 7, y + lineH_ / 2}, 4, d->error ? c.error : c.warning);
    }
    EndScissorMode();
    DrawLineEx({r.x + gutterW_ - 0.5f, r.y}, {r.x + gutterW_ - 0.5f, r.y + area_.height}, 1, Fade(c.lineNumbers, 0.3f));

    // The problems strip
    if (stripH > 0) {
        Rectangle sr{r.x, area_.y + area_.height, r.width - bar, stripH};
        DrawRectangleRec(sr, c.background);
        bool anyError = !shown.empty() && shown.front()->error;
        DrawRectangleRec(sr, Fade(anyError ? c.error : c.warning, 0.10f));
        DrawLineEx({sr.x, sr.y + 0.5f}, {sr.x + sr.width, sr.y + 0.5f}, 1, Fade(anyError ? c.error : c.warning, 0.6f));
        for (size_t i = 0; i < strip.size(); ++i)
            DrawTextEx(font, strip[i].first.c_str(), {snap(sr.x + 12), snap(sr.y + 4 + i * lineH_ + textY)}, size, 1,
                       strip[i].second ? c.error : c.warning);
    }

    // Scroll bars (with marks where the errors are)
    DrawRectangleRec(vbar, Fade(c.lineNumbers, 0.08f));
    for (const Diagnostic& d : diagnostics) {
        float y = vbar.y + (d.line - 1) / std::max(1.0f, static_cast<float>(lines_.size())) * vbar.height;
        DrawRectangleRec({vbar.x + 2, y, vbar.width - 4, 3}, d.error ? c.error : c.warning);
    }
    if (maxY > 0) DrawRectangleRec({vbar.x + 3, thumbY(), vbar.width - 6, thumbH}, Fade(c.lineNumbers, draggingBar_ ? 0.8f : 0.5f));
    DrawRectangleRec({r.x, hbar.y, r.width, bar}, Fade(c.lineNumbers, 0.08f));
    if (maxX > 0)
        DrawRectangleRec({thumbX(), hbar.y + 3, thumbW, bar - 6}, Fade(c.lineNumbers, draggingHBar_ ? 0.8f : 0.5f));

    drawCompletion(r, style);
}

void TextDocument::drawCompletion(Rectangle r, const TextStyle& style) {
    acRect_ = {};
    if (!acOpen_ || acItems_.empty()) return;
    const auto& c = theme().code;
    const Font& font = style.font;
    float size = style.fontSize;
    float lineTop = area_.y + cursor_.line * lineH_ - scroll_.y;
    if (lineTop < area_.y - lineH_ || lineTop > area_.y + area_.height) return;  // scrolled away

    int n = static_cast<int>(acItems_.size());
    int visible = std::min(n, 9);
    acSel_ = std::clamp(acSel_, 0, n - 1);
    if (acSel_ < acTop_) acTop_ = acSel_;
    if (acSel_ >= acTop_ + visible) acTop_ = acSel_ - visible + 1;
    acTop_ = std::clamp(acTop_, 0, n - visible);

    auto width = [&](const std::string& s) { return MeasureTextEx(font, s.c_str(), size, 1).x; };
    auto fit = [&](std::string s, float max) {
        if (width(s) <= max) return s;
        while (!s.empty() && width(s + "...") > max) s.pop_back();
        return s + "...";
    };
    // What to show to the right of a name
    auto hint = [](const Completion& item) -> std::string {
        if (item.kind == Completion::Kind::Function) {
            size_t open = item.detail.find('(');
            std::string params = open == std::string::npos ? "()" : item.detail.substr(open);
            return params + "  " + item.type;
        }
        if (item.kind == Completion::Kind::Type) return item.detail.substr(0, item.detail.find(' '));
        return item.type;
    };

    float icon = lineH_;
    float nameW = 0, hintW = 0;
    for (int i = acTop_; i < acTop_ + visible; ++i) {
        nameW = std::max(nameW, width(acItems_[i].name));
        hintW = std::max(hintW, width(hint(acItems_[i])));
    }
    float w = std::clamp(icon + nameW + 28 + std::min(hintW, 340.0f), 240.0f, std::max(240.0f, std::min(680.0f, r.width - 8)));
    float x = area_.x - scroll_.x + visualCol(acStart_.line, acStart_.col) * charW_ - icon - 2;
    x = std::clamp(x, r.x + 2, r.x + r.width - w - 2);

    // The selected one's full signature and comment, under the list
    const Completion& sel = acItems_[acSel_];
    std::vector<std::string> docLines;
    if (!sel.detail.empty()) docLines.push_back(fit(sel.detail, w - 16));
    std::string line;
    std::stringstream words(sel.doc);
    for (std::string word; words >> word;) {
        if (!line.empty() && width(line + " " + word) > w - 16) {
            docLines.push_back(line);
            line.clear();
            if (docLines.size() >= 4) break;
        }
        line += (line.empty() ? "" : " ") + word;
    }
    if (!line.empty() && docLines.size() < 4) docLines.push_back(line);

    float listH = visible * lineH_ + 4;
    float docH = docLines.empty() ? 0 : docLines.size() * (lineH_ - 2) + 10;
    float y = lineTop + lineH_;
    if (y + listH + docH > r.y + r.height && lineTop - listH - docH > r.y) y = lineTop - listH - docH;
    acRect_ = {x, y, w, listH};

    Color bg = c.currentLine;
    bg.a = 255;
    Rectangle all{x, y, w, listH + docH};
    DrawRectangleRec({all.x + 3, all.y + 3, all.width, all.height}, Fade(BLACK, 0.35f));
    DrawRectangleRec(all, bg);
    DrawRectangleLinesEx(all, 1, Fade(c.lineNumbers, 0.7f));
    Vector2 m = GetMousePosition();
    float textY = (lineH_ - size) / 2;
    for (int i = 0; i < visible; ++i) {
        const Completion& item = acItems_[acTop_ + i];
        Rectangle row{x + 2, y + 2 + i * lineH_, w - 4, lineH_};
        if (acTop_ + i == acSel_) DrawRectangleRec(row, c.selection);
        else if (CheckCollisionPointRec(m, row)) DrawRectangleRec(row, Fade(c.selection, 0.4f));
        const char* badge = "v";
        Color badgeColor = c.text;
        switch (item.kind) {
            case Completion::Kind::Function: badge = "f"; badgeColor = c.function; break;
            case Completion::Kind::Type: badge = "T"; badgeColor = c.type; break;
            case Completion::Kind::Constant: badge = "c"; badgeColor = c.number; break;
            case Completion::Kind::Keyword: badge = "k"; badgeColor = c.keyword; break;
            case Completion::Kind::Namespace: badge = "n"; badgeColor = c.preprocessor; break;
            default: break;
        }
        float bw = width(badge);
        DrawTextEx(font, badge, {std::round(row.x + (icon - bw) / 2), std::round(row.y + textY)}, size, 1, badgeColor);
        DrawTextEx(font, item.name.c_str(), {std::round(row.x + icon), std::round(row.y + textY)}, size, 1, c.text);
        float room = row.width - icon - width(item.name) - 20;
        if (room > 30) {
            std::string h = fit(hint(item), room);
            DrawTextEx(font, h.c_str(), {std::round(row.x + row.width - width(h) - 6), std::round(row.y + textY)}, size,
                       1, c.lineNumbers);
        }
    }
    if (n > visible) {  // a little scroll indicator
        float th = listH * visible / n;
        DrawRectangleRec({x + w - 4, y + listH * acTop_ / n, 3, th}, Fade(c.lineNumbers, 0.6f));
    }
    if (!docLines.empty()) {
        float dy = y + listH;
        DrawLineEx({x, dy}, {x + w, dy}, 1, Fade(c.lineNumbers, 0.4f));
        for (size_t i = 0; i < docLines.size(); ++i)
            DrawTextEx(font, docLines[i].c_str(), {std::round(x + 8), std::round(dy + 5 + i * (lineH_ - 2))}, size, 1,
                       i == 0 ? c.text : c.comment);
    }
}

}  // namespace s2d
