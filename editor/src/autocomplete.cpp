#include "autocomplete.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace s2d {

namespace {

// The parts of the standard library scripts use most, written as a header so
// the same parser reads it. Comments become the help text.
const char* kStdHeader = R"(
namespace std {
// A list that grows: std::vector<int> list; list.push_back(3);
template <class T> class vector {
public:
    void push_back(const T& value);  // add to the end
    T& emplace_back(Args&&... args);  // make a new item at the end
    void pop_back();  // remove the last item
    size_t size() const;  // how many items
    bool empty() const;  // true if there are no items
    void clear();  // remove everything
    iterator erase(iterator position);  // remove an item: list.erase(list.begin() + i)
    iterator insert(iterator position, const T& value);  // add before position
    T& at(size_t index);  // the item at index (checked)
    T& front();  // the first item
    T& back();  // the last item
    iterator begin();
    iterator end();
    void reserve(size_t count);  // make room for count items
    void resize(size_t count);  // grow or shrink to count items
    T* data();
};
// Text: std::string name = "Player";
class string {
public:
    size_t size() const;  // how many characters
    size_t length() const;  // how many characters
    bool empty() const;
    string substr(size_t start, size_t count) const;  // part of the text
    size_t find(const string& text, size_t from) const;  // where text is, or std::string::npos
    size_t rfind(const string& text) const;  // the last place text is
    const char* c_str() const;  // for C functions like DrawText
    string& append(const string& text);
    string& replace(size_t start, size_t count, const string& with);
    string& insert(size_t at, const string& text);
    string& erase(size_t start, size_t count);
    void clear();
    bool starts_with(const string& text) const;
    bool ends_with(const string& text) const;
    char& front();
    char& back();
    void push_back(char c);
    static const size_t npos;  // "not found"
};
// Keys and values: std::map<std::string, int> scores; scores["ann"] = 3;
template <class Key, class T> class map {
public:
    iterator find(const Key& key);  // an iterator to the item, or end()
    size_t count(const Key& key) const;  // 1 if the key is there, else 0
    bool contains(const Key& key) const;
    pair insert(const value_type& item);
    pair emplace(Args&&... args);
    size_t erase(const Key& key);
    T& at(const Key& key);  // the value (checked)
    size_t size() const;
    bool empty() const;
    void clear();
    iterator begin();
    iterator end();
};
// Like std::map, but faster and unordered
template <class Key, class T> class unordered_map {
public:
    iterator find(const Key& key);
    size_t count(const Key& key) const;
    bool contains(const Key& key) const;
    pair insert(const value_type& item);
    pair emplace(Args&&... args);
    size_t erase(const Key& key);
    T& at(const Key& key);
    size_t size() const;
    bool empty() const;
    void clear();
    iterator begin();
    iterator end();
};
// A collection of unique values
template <class T> class set {
public:
    pair insert(const T& value);
    size_t erase(const T& value);
    size_t count(const T& value) const;
    bool contains(const T& value) const;
    iterator find(const T& value);
    size_t size() const;
    bool empty() const;
    void clear();
};
// A fixed-size array: std::array<int, 3> a{1, 2, 3};
template <class T, size_t N> class array {
public:
    size_t size() const;
    void fill(const T& value);
    T& at(size_t index);
    T& front();
    T& back();
    T* data();
};
// Two values together: .first and .second
template <class A, class B> struct pair {
    A first;
    B second;
};
// Owns an object and deletes it when it goes away: std::unique_ptr<Node>
template <class T> class unique_ptr {
public:
    T* get() const;  // the plain pointer
    void reset(T* object);  // delete the object (or swap in another)
    T* release();  // stop owning it and hand back the pointer
};
// A pointer that several owners share
template <class T> class shared_ptr {
public:
    T* get() const;
    void reset();
    long use_count() const;
};
// A value that may be missing: if (opt) use(*opt);
template <class T> class optional {
public:
    bool has_value() const;
    T& value();
    T value_or(const T& fallback) const;
    void reset();
};
// Any function or lambda: std::function<void(int)> onHit;
template <class F> class function {};
template <class T> class numeric_limits {
public:
    static T max();  // the largest value: std::numeric_limits<float>::max()
    static T min();
    static T lowest();
};

// Makes an object owned by a std::unique_ptr: std::make_unique<Sprite2D>()
template <class T> unique_ptr<T> make_unique(Args&&... args);
template <class T> shared_ptr<T> make_shared(Args&&... args);
// Lets a value be moved instead of copied
T&& move(T& value);
void swap(T& a, T& b);
// The smaller of two values
const T& min(const T& a, const T& b);
// The larger of two values
const T& max(const T& a, const T& b);
// Keeps a value between low and high
const T& clamp(const T& value, const T& low, const T& high);
float abs(float x);  // without the minus sign
float sqrt(float x);  // square root
float pow(float base, float exponent);
float sin(float radians);
float cos(float radians);
float tan(float radians);
float atan2(float y, float x);  // the angle of (x, y), in radians
float floor(float x);  // round down
float ceil(float x);  // round up
float round(float x);  // round to the nearest whole number
float fmod(float x, float y);  // remainder of x / y
float lerp(float a, float b, float t);  // a + (b - a) * t
float hypot(float x, float y);  // the length of (x, y)
string to_string(int value);  // 42 -> "42"
int stoi(const string& text);  // "42" -> 42
float stof(const string& text);  // "1.5" -> 1.5
// Sorts a range: std::sort(list.begin(), list.end());
void sort(iterator first, iterator last);
iterator find(iterator first, iterator last, const T& value);
iterator find_if(iterator first, iterator last, Predicate test);
iterator remove_if(iterator first, iterator last, Predicate test);
size_t count(iterator first, iterator last, const T& value);
void reverse(iterator first, iterator last);
bool any_of(iterator first, iterator last, Predicate test);
bool all_of(iterator first, iterator last, Predicate test);
void shuffle(iterator first, iterator last, Random& random);
int printf(const char* format, ...);
int snprintf(char* buffer, size_t size, const char* format, ...);
int rand();  // a random number (prefer raylib's GetRandomValue)
}
)";

const char* kKeywords[] = {
    "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return", "auto", "const",
    "constexpr", "static", "class", "struct", "enum", "public", "private", "protected", "virtual", "override",
    "template", "typename", "namespace", "using", "nullptr", "true", "false", "this", "new", "delete", "sizeof",
    "static_cast", "dynamic_cast", "int", "float", "double", "bool", "char", "void", "unsigned", "long", "short",
    "include", "std"};

const std::set<std::string> kNotATypeName = {
    "return", "else", "delete", "new", "case", "goto", "throw", "using", "typedef", "friend", "public", "private",
    "protected", "co_return", "co_yield", "operator", "sizeof", "typename", "template", "namespace", "if", "while",
    "for", "switch", "do", "break", "continue", "default", "static_assert", "enum", "class", "struct"};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string collapse(const std::string& s) {
    std::string out;
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += c;
    }
    return out;
}

bool isWord(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Splits at commas that aren't inside (), <>, [] or {}.
std::vector<std::string> splitTopLevel(const std::string& s, char sep) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if (c == '(' || c == '<' || c == '[' || c == '{') ++depth;
        if (c == ')' || c == '>' || c == ']' || c == '}') --depth;
        if (c == sep && depth == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!trim(cur).empty()) out.push_back(cur);
    return out;
}

// Index of the bracket matching the one at `open` (searching forward).
size_t matchForward(const std::string& s, size_t open) {
    char o = s[open], c = o == '(' ? ')' : o == '<' ? '>' : o == '[' ? ']' : '}';
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == o) ++depth;
        else if (s[i] == c && --depth == 0) return i;
    }
    return std::string::npos;
}

// "const std::vector<Node*>&" -> "vector"; "raylib::Vector2" -> "Vector2"
std::string baseName(std::string type) {
    type = trim(type);
    for (const char* word : {"const ", "volatile ", "struct ", "class ", "typename "})
        for (size_t p; (p = type.find(word)) != std::string::npos;) type.erase(p, std::string(word).size());
    if (size_t lt = type.find('<'); lt != std::string::npos) type = type.substr(0, lt);
    while (!type.empty() && (type.back() == '*' || type.back() == '&' || type.back() == ' ')) type.pop_back();
    if (size_t colon = type.rfind("::"); colon != std::string::npos) type = type.substr(colon + 2);
    return trim(type);
}

// The first template argument: "std::vector<Node*>" -> "Node*"
std::string templateArg(const std::string& type) {
    size_t lt = type.find('<');
    if (lt == std::string::npos) return "";
    size_t gt = matchForward(type, lt);
    if (gt == std::string::npos) return "";
    std::vector<std::string> args = splitTopLevel(type.substr(lt + 1, gt - lt - 1), ',');
    return args.empty() ? "" : trim(args[0]);
}

bool isPointerLike(const std::string& type) {
    std::string b = baseName(type);
    return b == "unique_ptr" || b == "shared_ptr" || b == "weak_ptr" || b == "optional";
}

// What `->` (or `*`) reaches: "std::unique_ptr<Node>" -> "Node", "Node*" -> "Node"
std::string pointee(const std::string& type) {
    if (isPointerLike(type)) return templateArg(type);
    std::string t = trim(type);
    while (!t.empty() && (t.back() == '*' || t.back() == '&' || t.back() == ' ')) t.pop_back();
    return t;
}

// The C++ parser behind everything: good enough for headers and scripts, not a compiler.
struct Parser {
    Parser(int source_, bool everything_, CompletionIndex::Symbols& out_)
        : source(source_), everything(everything_), out(out_) {}
    int source;
    bool everything;  // also private members and local variables (the file being edited)
    CompletionIndex::Symbols& out;

    struct Scope {
        enum Kind { Global, Namespace, Class, Enum, Function, Other } kind = Global;
        std::string name;
        bool open = true;   // public (classes) / transparent (namespaces)
        bool scoped = false;  // enum class
        int start = 0;
        int rangeIndex = -1;
    };
    std::vector<Scope> stack{Scope{}};
    std::string doc;
    std::vector<std::pair<std::vector<Completion>*, size_t>> lastDecls;
    int lastDeclLine = -1;
    int line = 0;

    bool inFunction() const {
        for (const Scope& s : stack)
            if (s.kind == Scope::Function) return true;
        return false;
    }
    // The class or namespace new names belong to ("" = global)
    std::string owner() const {
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            if (it->kind == Scope::Class) return it->name;
            if (it->kind == Scope::Namespace && it->name == "std") return "std";
        }
        return "";
    }
    bool hidden() const {  // inside a class's private part
        for (const Scope& s : stack)
            if (s.kind == Scope::Class && !s.open) return !everything;
        return false;
    }

    std::vector<Completion>& listFor(const std::string& owner) {
        return owner.empty() ? out.globals : out.members[owner];
    }
    void add(Completion c, const std::string& ownerName) {
        c.owner = ownerName;
        c.source = source;
        if (c.doc.empty()) c.doc = collapse(doc);
        std::vector<Completion>& list = listFor(ownerName);
        list.push_back(std::move(c));
        if (lastDeclLine != line) lastDecls.clear();
        lastDecls.push_back({&list, list.size() - 1});
        lastDeclLine = line;
    }

    // "template <class T> T* self() const" -> strips the template part, returns its parameter names
    static std::vector<std::string> stripTemplate(std::string& s) {
        std::vector<std::string> names;
        for (size_t p; (p = s.find("template")) != std::string::npos;) {
            size_t lt = s.find('<', p);
            if (lt == std::string::npos) break;
            size_t gt = matchForward(s, lt);
            if (gt == std::string::npos) break;
            for (const std::string& part : splitTopLevel(s.substr(lt + 1, gt - lt - 1), ',')) {
                std::string t = trim(part);
                if (size_t eq = t.find('='); eq != std::string::npos) t = trim(t.substr(0, eq));
                size_t i = t.size();
                while (i > 0 && isWord(t[i - 1])) --i;
                if (i < t.size()) names.push_back(t.substr(i));
            }
            s.erase(p, gt - p + 1);
        }
        return names;
    }
    static void stripWords(std::string& s) {
        static const std::regex attrs(R"(\[\[[^\]]*\]\])");
        s = std::regex_replace(s, attrs, " ");
        static const std::regex words(
            R"(\b(RLAPI|RMAPI|RLCPP_NODISCARD|RAYGUIAPI|virtual|static|inline|explicit|constexpr|consteval|friend|extern|mutable)\b)");
        s = collapse(std::regex_replace(s, words, " "));
    }

    static bool hasWord(const std::string& text, const std::string& word) {
        std::regex re("\\b" + word + "\\b");
        return std::regex_search(text, re);
    }

    // A function declaration or definition; returns its parameters (or nullopt if it isn't one)
    bool function(std::string s, const std::string& scopeOwner, std::string* paramsOut, std::string* ownerOut) {
        std::vector<std::string> tparams = stripTemplate(s);
        stripWords(s);
        // the first '(' that isn't inside <...>
        int angle = 0;
        size_t p = std::string::npos;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '<') ++angle;
            else if (s[i] == '>' && angle > 0) --angle;
            else if (s[i] == '(' && angle == 0) {
                p = i;
                break;
            }
        }
        if (p == std::string::npos) return false;
        size_t e = p;
        while (e > 0 && s[e - 1] == ' ') --e;
        size_t b = e;
        while (b > 0 && (isWord(s[b - 1]) || s[b - 1] == ':' || s[b - 1] == '~')) --b;
        std::string name = s.substr(b, e - b);
        std::string ret = trim(s.substr(0, b));
        if (name.empty() || ret.empty() || name.find("operator") != std::string::npos || name[0] == '~' ||
            kNotATypeName.count(ret) || kNotATypeName.count(name))
            return false;
        if (ret.find('(') != std::string::npos || ret.find('=') != std::string::npos) return false;
        size_t close = matchForward(s, p);
        if (close == std::string::npos) return false;
        std::string params = collapse(s.substr(p + 1, close - p - 1));
        std::string owner = scopeOwner;
        if (size_t colon = name.rfind("::"); colon != std::string::npos) {
            owner = baseName(name.substr(0, colon));
            name = name.substr(colon + 2);
            if (name == owner) return false;  // a constructor
        }
        if (!owner.empty() && name == owner) return false;
        Completion c;
        c.kind = Completion::Kind::Function;
        c.name = name;
        c.type = ret;
        c.params = !params.empty() && params != "void";
        // Needs <Type> when called if a template parameter only shows up in what it returns
        for (const std::string& t : tparams)
            if (hasWord(ret, t) && !hasWord(params, t)) c.templated = true;
        c.detail = ret + " " + name + (c.templated ? "<" + tparams[0] + ">" : "") + "(" + params + ")";
        if (paramsOut) *paramsOut = params;
        if (ownerOut) *ownerOut = owner;
        // An out-of-line definition (void Foo::bar() {...}) is already listed by Foo's declaration
        if (owner == scopeOwner || everything) add(c, owner);
        return true;
    }

    // "float speed = 260, jump = 3" -> variables
    void variables(const std::string& s, const std::string& ownerName, bool local) {
        static const std::regex first(
            R"(^\s*((?:(?:static|const|constexpr|inline|mutable|volatile|extern|thread_local)\s+)*)((?:[A-Za-z_]\w*::)*[A-Za-z_]\w*(?:\s*<.*>)?)\s*((?:[\*&]\s*)*)\s*([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*$)");
        static const std::regex more(R"(^\s*((?:[\*&]\s*)*)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*$)");
        std::vector<std::string> parts = splitTopLevel(s, ',');
        std::string baseType;
        for (size_t i = 0; i < parts.size(); ++i) {
            std::string decl = parts[i], init;
            // cut off "= value" or "{value}" (not inside brackets)
            int depth = 0;
            for (size_t k = 0; k < decl.size(); ++k) {
                char ch = decl[k];
                if (ch == '<' || ch == '(' || ch == '[') ++depth;
                if (ch == '>' || ch == ')' || ch == ']') --depth;
                if (depth == 0 && (ch == '=' || ch == '{')) {
                    init = trim(decl.substr(ch == '=' ? k + 1 : k));
                    decl = decl.substr(0, k);
                    break;
                }
            }
            std::smatch m;
            std::string type, name;
            if (i == 0) {
                if (!std::regex_match(decl, m, first)) return;
                std::string t = trim(m[2].str());
                if (kNotATypeName.count(t)) return;
                baseType = (hasWord(m[1].str(), "const") ? "const " : "") + t;
                type = baseType + trim(m[3].str());
                name = m[4].str();
            } else {
                if (!std::regex_match(decl, m, more)) return;
                type = baseType + trim(m[1].str());
                name = m[2].str();
            }
            if (kNotATypeName.count(name)) return;
            Completion c;
            c.kind = Completion::Kind::Variable;
            c.name = name;
            c.type = type;
            c.detail = type + " " + name;
            c.init = init;
            c.line = line;
            if (local) {
                c.source = source;
                out.locals.push_back(c);
            } else {
                add(c, ownerName);
            }
        }
    }

    void parameters(const std::string& params) {
        for (const std::string& part : splitTopLevel(params, ',')) {
            std::string p = part;
            int depth = 0;
            for (size_t k = 0; k < p.size(); ++k) {
                if (p[k] == '<' || p[k] == '(') ++depth;
                if (p[k] == '>' || p[k] == ')') --depth;
                if (depth == 0 && p[k] == '=') {
                    p = p.substr(0, k);
                    break;
                }
            }
            variables(p, "", true);
        }
    }

    void enumerator(const std::string& stmt) {
        std::string s = trim(stmt);
        size_t i = 0;
        while (i < s.size() && isWord(s[i])) ++i;
        if (i == 0) return;
        const Scope& e = stack.back();
        if (s.substr(0, i) == "true" || s.substr(0, i) == "false") return;  // raylib.h's C bool
        Completion c;
        c.kind = Completion::Kind::Constant;
        c.name = s.substr(0, i);
        c.type = e.name;
        c.detail = e.name.empty() ? c.name : e.name + "::" + c.name;
        if (e.scoped) {
            add(c, e.name);
        } else {
            add(c, owner());
            if (!e.name.empty()) {
                Completion copy = c;
                copy.owner = e.name;
                out.members[e.name].push_back(copy);
            }
        }
    }

    void define(const std::string& text) {
        static const std::regex re(R"(^\s*#\s*define\s+([A-Za-z_]\w*)\s+(.+)$)");
        std::smatch m;
        if (!std::regex_match(text, m, re)) return;
        std::string name = m[1].str(), value = m[2].str(), comment;
        if (name[0] == '_' || name.size() < 2) return;
        if (size_t c = value.find("//"); c != std::string::npos) {
            comment = trim(value.substr(c + 2));
            value = value.substr(0, c);
        }
        value = collapse(value);
        if (value.empty() || value.back() == '\\') return;
        Completion c;
        c.kind = Completion::Kind::Constant;
        c.name = name;
        c.detail = "#define " + name + " " + value;
        c.doc = comment;
        if (value.rfind("CLITERAL(Color)", 0) == 0) c.type = "Color";
        add(c, "");
    }

    void open(std::string stmt) {
        std::string s = collapse(stmt);
        Scope top = stack.back();
        Scope next;
        next.kind = Scope::Other;
        next.start = line;
        if (!inFunction() && top.kind != Scope::Other && top.kind != Scope::Enum) {
            std::string t = s;
            std::vector<std::string> tparams = stripTemplate(t);
            t = trim(t);
            static const std::regex cls(R"(^(?:typedef\s+)?(class|struct|union)\s*(?:[A-Z_]+\s+)?(\w*)\s*(?:final)?\s*(?::\s*(.*))?$)");
            static const std::regex enm(R"(^(?:typedef\s+)?enum(\s+class|\s+struct)?\s*(\w*)\s*(?::\s*[\w\s:]+)?$)");
            static const std::regex ns(R"(^(?:inline\s+)?namespace\s*([\w:]*)$)");
            std::smatch m;
            if (std::regex_match(t, m, cls)) {
                next.kind = Scope::Class;
                next.name = m[2].str();
                next.open = m[1].str() != "class";
                if (!next.name.empty() && !hidden()) {
                    Completion c;
                    c.kind = Completion::Kind::Type;
                    c.name = next.name;
                    c.detail = m[1].str() + " " + next.name;
                    add(c, owner() == "std" ? "std" : "");
                    if (m[3].matched)
                        for (std::string base : splitTopLevel(m[3].str(), ',')) {
                            for (const char* w : {"public", "private", "protected", "virtual"})
                                base = std::regex_replace(base, std::regex(std::string("\\b") + w + "\\b"), "");
                            std::string b = baseName(base);
                            if (!b.empty() && b != next.name) out.bases[next.name].push_back(b);
                        }
                    out.classes.push_back({next.name, line, 1 << 30});  // until its closing brace
                    next.rangeIndex = static_cast<int>(out.classes.size()) - 1;
                }
            } else if (std::regex_match(t, m, enm)) {
                next.kind = Scope::Enum;
                next.name = m[2].str();
                next.scoped = m[1].matched;
                if (!next.name.empty() && !hidden()) {
                    Completion c;
                    c.kind = Completion::Kind::Type;
                    c.name = next.name;
                    c.detail = "enum " + next.name;
                    add(c, owner());
                }
            } else if (std::regex_match(t, m, ns)) {
                next.kind = Scope::Namespace;
                next.name = m[1].str();
            } else if (t.rfind("extern", 0) == 0) {
                next.kind = Scope::Namespace;
            } else if (t.find('(') != std::string::npos && t.find('=') == std::string::npos) {
                std::string params, fnOwner;
                if (!hidden() || everything) {
                    if (function(t, owner(), &params, &fnOwner)) {
                        if (everything) {
                            parameters(params);
                            // void Foo::bar() { ... } is "inside" Foo for completion
                            if (!fnOwner.empty() && fnOwner != owner() && fnOwner != "std") {
                                out.classes.push_back({fnOwner, line, 1 << 30});
                                next.rangeIndex = static_cast<int>(out.classes.size()) - 1;
                            }
                        }
                    }
                }
                next.kind = Scope::Function;
            } else if (!t.empty() && !hidden()) {
                variables(t, owner(), false);  // Vector2 position{0, 0};
            }
        } else if (everything && inFunction()) {
            static const std::regex cond(R"(^(?:if|while|switch)\s*\(\s*(.*)\)\s*$)");
            static const std::regex forLoop(R"(^for\s*\((.*)\)\s*$)");
            static const std::regex lambda(R"(\]\s*\(([^()]*)\))");
            std::smatch m;
            if (std::regex_match(s, m, cond)) {
                std::string inner = m[1].str();
                if (inner.find(';') != std::string::npos) inner = inner.substr(0, inner.find(';'));
                if (inner.find('=') != std::string::npos) variables(inner, "", true);
            } else if (std::regex_match(s, m, forLoop)) {
                std::string inner = m[1].str();
                size_t semi = inner.find(';');
                if (semi != std::string::npos) {
                    variables(inner.substr(0, semi), "", true);
                } else if (size_t colon = inner.find(" : "); colon != std::string::npos) {
                    variables(inner.substr(0, colon), "", true);
                    if (!out.locals.empty()) out.locals.back().init = "*" + trim(inner.substr(colon + 3)) + "[0]";
                }
            }
            if (std::regex_search(s, m, lambda)) parameters(m[1].str());
        }
        stack.push_back(next);
    }

    void semicolon(const std::string& stmt) {
        std::string s = collapse(stmt);
        if (s.empty()) return;
        // EXPORT(float, speed, 250) / EXPORT_SCENE(nextLevel): a variable of the script
        static const std::regex exportVar(R"(^EXPORT(?:_STEP)?\s*\(\s*([^,]+?)\s*,\s*([A-Za-z_]\w*))");
        static const std::regex exportPath(R"(^EXPORT_(?:SCENE|TEXTURE)\s*\(\s*([A-Za-z_]\w*))");
        std::smatch m;
        if (!inFunction() && (std::regex_search(s, m, exportVar) || std::regex_search(s, m, exportPath))) {
            Completion c;
            c.kind = Completion::Kind::Variable;
            c.name = m.size() > 2 && m[2].matched ? m[2].str() : m[1].str();
            c.type = m.size() > 2 && m[2].matched ? m[1].str() : "std::string";
            c.detail = c.type + " " + c.name + "  (shown in the inspector)";
            add(c, owner());
            return;
        }
        Scope& top = stack.back();
        if (top.kind == Scope::Enum) return;
        if (inFunction()) {
            if (everything && s.find("return") != 0) variables(s, "", true);
            return;
        }
        if (top.kind == Scope::Other || hidden()) return;
        if (s.rfind("using", 0) == 0 || s.rfind("typedef", 0) == 0 || s.rfind("friend", 0) == 0 ||
            s.rfind("static_assert", 0) == 0 || (s.rfind("template", 0) == 0 && s.find('(') == std::string::npos))
            return;
        if (s.find('(') != std::string::npos && function(s, owner(), nullptr, nullptr)) return;
        if (s.find('(') == std::string::npos || s.find('=') != std::string::npos) variables(s, owner(), false);
    }

    void run(const std::string& text) {
        std::string stmt;
        int paren = 0;
        bool lineStart = true, blank = true;
        size_t n = text.size();
        for (size_t i = 0; i < n;) {
            char c = text[i];
            char next = i + 1 < n ? text[i + 1] : '\0';
            if (c == '\n') {
                ++line;
                if (blank) doc.clear();
                blank = lineStart = true;
                lastDecls.clear();
                stmt += ' ';
                ++i;
                continue;
            }
            if (lineStart && (c == ' ' || c == '\t' || c == '\r')) {
                ++i;
                continue;
            }
            if (lineStart) {
                lineStart = false;
                size_t eol = text.find('\n', i);
                if (eol == std::string::npos) eol = n;
                if (c == '#') {
                    // Preprocessor line (with \ continuations)
                    while (eol < n && eol > 0 && text[eol - 1] == '\\') {
                        eol = text.find('\n', eol + 1);
                        if (eol == std::string::npos) eol = n;
                    }
                    std::string pp = text.substr(i, eol - i);
                    for (char ch : pp) line += ch == '\n';
                    define(pp);
                    i = eol;
                    blank = false;
                    continue;
                }
                // A macro call on a line of its own: GETTERSETTER(float, X, x), REGISTER_SCRIPT(Foo)
                static const std::regex macroLine(R"(^[A-Z_][A-Z0-9_]*\s*\(.*\)\s*$)");
                std::string whole = text.substr(i, eol - i);
                if (paren == 0 && std::regex_match(whole, macroLine)) {
                    i = eol;
                    blank = false;
                    continue;
                }
            }
            if (c == '/' && next == '/') {
                size_t eol = text.find('\n', i);
                if (eol == std::string::npos) eol = n;
                std::string comment = trim(text.substr(i + 2, eol - i - 2));
                while (!comment.empty() && comment[0] == '/') comment = trim(comment.substr(1));
                if (lastDeclLine == line && !lastDecls.empty()) {
                    for (auto& [list, index] : lastDecls) (*list)[index].doc = comment;
                } else if (!comment.empty()) {
                    doc += (doc.empty() ? "" : " ") + comment;
                }
                blank = false;
                i = eol;
                continue;
            }
            if (c == '/' && next == '*') {
                size_t end = text.find("*/", i + 2);
                if (end == std::string::npos) end = n;
                for (size_t k = i; k < end; ++k) line += text[k] == '\n';
                i = std::min(n, end + 2);
                continue;
            }
            blank = false;
            if (c == '"' || c == '\'') {
                size_t j = i + 1;
                if (c == '"' && i > 0 && text[i - 1] == 'R') {  // raw string R"( ... )"
                    size_t end = text.find(")\"", j);
                    j = end == std::string::npos ? n : end + 2;
                } else {
                    while (j < n && text[j] != c && text[j] != '\n') j += text[j] == '\\' ? 2 : 1;
                    ++j;
                }
                for (size_t k = i; k < std::min(j, n); ++k) line += text[k] == '\n';
                stmt += c;
                stmt += c;
                i = std::min(j, n);
                continue;
            }
            if (paren > 0) {
                if (c == '(') ++paren;
                if (c == ')') --paren;
                stmt += c;
                ++i;
                continue;
            }
            if (c == '(') {
                ++paren;
                stmt += c;
            } else if (c == '{') {
                open(stmt);
                stmt.clear();
                doc.clear();
            } else if (c == '}') {
                if (stack.back().kind == Scope::Enum && !trim(stmt).empty()) enumerator(stmt);
                if (stack.size() > 1) {
                    if (stack.back().rangeIndex >= 0) out.classes[stack.back().rangeIndex].last = line;
                    stack.pop_back();
                }
                stmt.clear();
                doc.clear();
            } else if (c == ';') {
                semicolon(stmt);
                stmt.clear();
                doc.clear();
            } else if (c == ',' && stack.back().kind == Scope::Enum) {
                enumerator(stmt);
                stmt.clear();
                doc.clear();
            } else if (c == ':' && next != ':' && stack.back().kind == Scope::Class &&
                       (trim(stmt) == "public" || trim(stmt) == "private" || trim(stmt) == "protected")) {
                stack.back().open = trim(stmt) == "public";
                stmt.clear();
            } else if (c == ':' && next == ':') {
                stmt += "::";
                ++i;
            } else {
                stmt += c;
            }
            ++i;
        }
    }
};

bool startsWithNoCase(const std::string& s, const std::string& p) {
    if (p.size() > s.size()) return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(p[i]))) return false;
    return true;
}

bool subsequence(const std::string& s, const std::string& p) {
    size_t j = 0;
    for (size_t i = 0; i < s.size() && j < p.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(s[i])) == std::tolower(static_cast<unsigned char>(p[j]))) ++j;
    return j == p.size();
}

std::string readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The expression before `.`, `->` or `::` ending at `end` (exclusive), e.g.
// "self<Node2D>()->position" in "auto x = self<Node2D>()->position."
std::string receiverBefore(const std::string& s, int end) {
    int i = end - 1;
    while (i >= 0) {
        char c = s[i];
        if (isWord(c)) {
            --i;
        } else if (c == ')' || c == ']') {
            char open = c == ')' ? '(' : '[';
            int depth = 0;
            for (; i >= 0; --i) {
                if (s[i] == c) ++depth;
                else if (s[i] == open && --depth == 0) break;
            }
            --i;
        } else if (c == '>' && i > 0 && s[i - 1] == '-') {
            i -= 2;
        } else if (c == '>') {
            int depth = 0;
            for (; i >= 0; --i) {
                if (s[i] == '>') ++depth;
                else if (s[i] == '<' && --depth == 0) break;
            }
            --i;
        } else if (c == '.') {
            --i;
        } else if (c == ':' && i > 0 && s[i - 1] == ':') {
            i -= 2;
        } else {
            break;
        }
    }
    return trim(s.substr(i + 1, end - i - 1));
}

}  // namespace

void CompletionIndex::setFolders(const fs::path& engineSrc, const fs::path& depsDir, const fs::path& projectScripts) {
    engineSrc_ = engineSrc;
    depsDir_ = depsDir;
    projectScripts_ = projectScripts;
}

void CompletionIndex::parse(const std::string& text, int source, bool everything, Symbols& out) {
    Parser p(source, everything, out);
    p.run(text);
}

void CompletionIndex::merge(const Symbols& s) {
    all_.globals.insert(all_.globals.end(), s.globals.begin(), s.globals.end());
    for (auto& [k, v] : s.members) {
        auto& list = all_.members[k];
        list.insert(list.end(), v.begin(), v.end());
    }
    for (auto& [k, v] : s.bases) {
        auto& list = all_.bases[k];
        for (const std::string& b : v)
            if (std::find(list.begin(), list.end(), b) == list.end()) list.push_back(b);
    }
}

void CompletionIndex::load() {
    loaded_ = true;
    std::error_code ec;
    for (const char* name : {"script.hpp", "nodes.hpp", "scene_tree.hpp", "multiplayer.hpp"})
        if (fs::exists(engineSrc_ / name, ec)) parse(readFile(engineSrc_ / name), Engine, false, base_);
    for (const char* dir : {"raylib-src/src", "raylib_cpp-src/include", "raylib-cpp-src/include"})
        for (const char* name : {"raylib.h", "raymath.h", "Vector2.hpp", "Color.hpp", "Rectangle.hpp"})
            if (fs::exists(depsDir_ / dir / name, ec)) parse(readFile(depsDir_ / dir / name), Raylib, false, base_);
    parse(kStdHeader, Language, false, base_);
    for (const char* k : kKeywords) {
        Completion c;
        c.kind = std::string(k) == "std" ? Completion::Kind::Namespace : Completion::Kind::Keyword;
        c.name = k;
        c.source = Language;
        base_.globals.push_back(c);
    }
}

void CompletionIndex::refreshProject(const fs::path& current) {
    std::error_code ec;
    std::set<fs::path> seen;
    for (fs::directory_iterator it(projectScripts_, ec), end; !ec && it != end; it.increment(ec)) {
        fs::path p = it->path();
        std::string ext = p.extension().string();
        if (ext != ".cpp" && ext != ".hpp" && ext != ".h") continue;
        seen.insert(p);
        auto time = fs::last_write_time(p, ec);
        auto found = projectFiles_.find(p);
        if (found != projectFiles_.end() && found->second.first == time) continue;
        Symbols s;
        parse(readFile(p), Project, false, s);
        projectFiles_[p] = {time, std::move(s)};
        allDirty_ = true;
    }
    for (auto it = projectFiles_.begin(); it != projectFiles_.end();) {
        if (seen.count(it->first)) {
            ++it;
        } else {
            it = projectFiles_.erase(it);
            allDirty_ = true;
        }
    }
    if (!allDirty_ && allWithout_ == current) return;
    allDirty_ = false;
    allWithout_ = current;
    all_ = base_;
    for (auto& [path, entry] : projectFiles_)
        if (!fs::equivalent(path, current, ec)) merge(entry.second);
}

std::vector<Completion> CompletionIndex::membersOf(const std::string& type, bool arrow, int depth) const {
    std::vector<Completion> out;
    if (depth > 8) return out;
    std::string t = arrow ? pointee(type) : type;
    std::string name = baseName(t);
    if (name.empty()) return out;
    for (const Symbols* s : {&current_, &all_}) {
        auto it = s->members.find(name);
        if (it != s->members.end()) out.insert(out.end(), it->second.begin(), it->second.end());
    }
    for (const Symbols* s : {&current_, &all_}) {
        auto it = s->bases.find(name);
        if (it == s->bases.end()) continue;
        for (const std::string& b : it->second) {
            std::vector<Completion> inherited = membersOf(b, false, depth + 1);
            out.insert(out.end(), inherited.begin(), inherited.end());
        }
    }
    return out;
}

const Completion* CompletionIndex::findMember(const std::string& type, const std::string& name, int depth) const {
    if (depth > 8 || type.empty()) return nullptr;
    std::string base = baseName(type);
    for (const Symbols* s : {&current_, &all_}) {
        auto it = s->members.find(base);
        if (it != s->members.end())
            for (const Completion& c : it->second)
                if (c.name == name) return &c;
    }
    for (const Symbols* s : {&current_, &all_}) {
        auto it = s->bases.find(base);
        if (it == s->bases.end()) continue;
        for (const std::string& b : it->second)
            if (const Completion* c = findMember(b, name, depth + 1)) return c;
    }
    return nullptr;
}

std::string CompletionIndex::returnTypeOf(const std::string& owner, const std::string& name) const {
    if (!owner.empty()) {
        const Completion* c = findMember(owner, name);
        return c ? c->type : "";
    }
    for (const Symbols* s : {&current_, &all_})
        for (const Completion& c : s->globals)
            if (c.name == name && !c.type.empty()) return c.type;
    return "";
}

// What type an expression has, as far as these simple rules can tell ("" = no idea).
std::string CompletionIndex::resolve(const std::string& exprIn, int depth) const {
    std::string expr = trim(exprIn);
    if (expr.empty() || depth > 8) return "";
    if (expr == "this") return currentClass_ + "*";
    if (expr[0] == '*' || expr[0] == '&') return resolve(expr.substr(1), depth + 1);

    // Split at the last `.`, `->` or `::` outside brackets
    int level = 0;
    for (int i = static_cast<int>(expr.size()) - 1; i > 0; --i) {
        char c = expr[i];
        if (c == '>' && expr[i - 1] == '-' && level == 0) {
            std::string left = resolve(expr.substr(0, i - 1), depth + 1);
            if (left.empty()) return "";
            std::string member = expr.substr(i + 1);
            std::string owner = pointee(left);
            // atom on the right
            size_t paren = member.find('(');
            size_t lt = member.find('<');
            std::string name = member.substr(0, std::min(paren, lt));
            if (lt != std::string::npos && lt < paren) {
                size_t gt = matchForward(member, lt);
                if (gt != std::string::npos) return member.substr(lt + 1, gt - lt - 1) + "*";
            }
            const Completion* m = findMember(owner, trim(name));
            if (!m) return "";
            std::string t = m->type;
            if (member.find('[') != std::string::npos) t = templateArg(t);
            return t;
        }
        if (c == ')' || c == ']' || (c == '>' && expr[i - 1] != '-')) ++level;
        else if (c == '(' || c == '[' || c == '<') --level;
        else if (level == 0 && (c == '.' || (c == ':' && expr[i - 1] == ':'))) {
            bool scope = c == ':';
            std::string leftExpr = expr.substr(0, scope ? i - 1 : i);
            std::string left = scope ? leftExpr : resolve(leftExpr, depth + 1);
            if (left.empty()) return "";
            std::string member = expr.substr(i + 1);
            size_t paren = member.find('(');
            size_t lt = member.find('<');
            if (lt != std::string::npos && lt < paren) {
                size_t gt = matchForward(member, lt);
                if (gt != std::string::npos) return member.substr(lt + 1, gt - lt - 1) + "*";
            }
            std::string name = trim(member.substr(0, std::min(paren, member.find('['))));
            const Completion* m = findMember(left, name);
            if (!m) return "";
            std::string t = m->kind == Completion::Kind::Constant && m->type.empty() ? left : m->type;
            if (member.find('[') != std::string::npos) t = templateArg(t);
            return t;
        }
    }

    // A single name, call or index: name, name(...), name<T>(...), name[...]
    size_t paren = expr.find('('), lt = expr.find('<'), bracket = expr.find('[');
    std::string name = trim(expr.substr(0, std::min({paren, lt, bracket})));
    if (lt != std::string::npos && lt < paren && paren != std::string::npos) {
        size_t gt = matchForward(expr, lt);
        if (gt != std::string::npos) {
            std::string arg = expr.substr(lt + 1, gt - lt - 1);
            // make_unique<T>() and friends return a smart pointer; as<T>(), self<T>() a T*
            std::string ret = returnTypeOf(currentClass_, name);
            if (ret.empty()) ret = returnTypeOf("", name);
            if (isPointerLike(ret)) return baseName(ret) + "<" + arg + ">";
            return arg + "*";
        }
    }
    std::string type;
    if (paren != std::string::npos) {
        type = returnTypeOf(currentClass_, name);
        if (type.empty()) type = returnTypeOf("", name);
        if (type.empty() && (current_.members.count(name) || all_.members.count(name))) type = name;  // Vector2{...}
    } else {
        // the closest local declared before here
        const Completion* best = nullptr;
        for (const Completion& c : current_.locals)
            if (c.name == name && c.line <= cursorLine_ && (!best || c.line >= best->line)) best = &c;
        if (best) {
            type = best->type;
            if (baseName(type) == "auto") {
                std::string init = best->init;
                if (!init.empty() && init.front() == '{') init = init.substr(1, init.size() - 2);
                type = resolve(init, depth + 1);
            }
        } else if (const Completion* m = findMember(currentClass_, name)) {
            type = m->type;
        } else {
            for (const Symbols* s : {&current_, &all_})
                for (const Completion& c : s->globals)
                    if (c.name == name && type.empty())
                        type = c.kind == Completion::Kind::Type ? name : c.type;
        }
    }
    if (bracket != std::string::npos && bracket > std::min(paren, lt)) {
        std::string arg = templateArg(type);
        type = arg.empty() ? pointee(type) : arg;
    }
    return type;
}

std::vector<Completion> CompletionIndex::complete(const std::vector<std::string>& lines, int line, int col,
                                                  const fs::path& file, std::string& prefix, bool forced) {
    prefix.clear();
    if (line < 0 || line >= static_cast<int>(lines.size())) return {};
    const std::string& s = lines[line];
    col = std::clamp(col, 0, static_cast<int>(s.size()));
    std::string before = s.substr(0, col);

    // Not inside a comment, a string or a preprocessor line
    std::string firstWord = trim(before);
    if (!firstWord.empty() && firstWord[0] == '#') return {};
    bool inString = false;
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i] == '\\') {
            ++i;
            continue;
        }
        if (before[i] == '"') inString = !inString;
        if (!inString && before.compare(i, 2, "//") == 0) return {};
    }
    if (inString) return {};

    int start = col;
    while (start > 0 && isWord(before[start - 1])) --start;
    prefix = before.substr(start);
    if (!prefix.empty() && std::isdigit(static_cast<unsigned char>(prefix[0]))) return {};

    std::string op;
    if (start >= 1 && before[start - 1] == '.') op = ".";
    else if (start >= 2 && before.compare(start - 2, 2, "->") == 0) op = "->";
    else if (start >= 2 && before.compare(start - 2, 2, "::") == 0) op = "::";

    if (!loaded_) load();
    refreshProject(file);
    std::string text;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (static_cast<int>(i) == line) text += before + ";" + s.substr(col);  // a half-typed line shouldn't confuse the parser
        else text += lines[i];
        text += '\n';
    }
    current_ = Symbols{};
    parse(text, ThisFile, true, current_);
    cursorLine_ = line;
    currentClass_.clear();
    int bestSize = 1 << 30;
    for (const auto& r : current_.classes)
        if (r.first <= line && line <= r.last && r.last - r.first < bestSize) {
            bestSize = r.last - r.first;
            currentClass_ = r.name;
        }

    std::vector<Completion> candidates;
    if (!op.empty()) {
        std::string receiver = receiverBefore(before, start - static_cast<int>(op.size()));
        if (receiver.empty()) return {};
        if (std::isdigit(static_cast<unsigned char>(receiver[0]))) return {};  // 1.5
        if (op == "::") {
            if (receiver == "std") {
                candidates = all_.members["std"];
            } else if (receiver == "s2d" || receiver == "raylib") {
                for (const Completion& c : all_.globals)
                    if (c.source != Language) candidates.push_back(c);
            } else {
                candidates = membersOf(receiver, false);
            }
        } else {
            std::string type = resolve(receiver, 0);
            if (!type.empty()) candidates = membersOf(type, op == "->");
            if (candidates.empty()) {
                // Couldn't tell what it is: offer every engine class's members
                for (const Symbols* sym : {&current_, &all_})
                    for (auto& [cls, list] : sym->members)
                        if (cls != "std")
                            for (const Completion& c : list)
                                if (c.source <= Engine) candidates.push_back(c);
            }
        }
    } else {
        if (!forced && prefix.size() < 2) return {};
        // What came just before decides a little: after "new" or a type context, all names are fine.
        for (const Completion& c : current_.locals) candidates.push_back(c);
        if (!currentClass_.empty())
            for (Completion c : membersOf(currentClass_, false)) {
                c.source = ThisClass;
                candidates.push_back(c);
            }
        for (const Completion& c : current_.globals) candidates.push_back(c);
        for (const Completion& c : all_.globals) candidates.push_back(c);
    }

    // Filter and rank: starts with what's typed, then contains it, then has its letters in order.
    struct Ranked {
        int rank;
        const Completion* c;
        size_t index;  // members: the class's own come first, in the order they're declared
    };
    std::vector<Ranked> ranked;
    std::string lp = lower(prefix);
    for (const Completion& c : candidates) {
        if (c.name.empty() || (c.name == prefix && op.empty())) continue;
        int rank;
        if (prefix.empty()) rank = 0;
        else if (c.name.compare(0, prefix.size(), prefix) == 0) rank = 0;
        else if (startsWithNoCase(c.name, prefix)) rank = 1;
        else if (lower(c.name).find(lp) != std::string::npos) rank = 2;
        else if (prefix.size() >= 2 && startsWithNoCase(c.name, prefix.substr(0, 1)) && subsequence(c.name, prefix))
            rank = 3;  // the letters in order: "gnp" -> getNodePath
        else continue;
        ranked.push_back({rank, &c, ranked.size()});
    }
    bool members = !op.empty();
    std::stable_sort(ranked.begin(), ranked.end(), [members](const Ranked& a, const Ranked& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        if (members) return a.index < b.index;
        if (a.c->source != b.c->source) return a.c->source < b.c->source;
        if (a.c->name.size() != b.c->name.size()) return a.c->name.size() < b.c->name.size();
        return a.c->name < b.c->name;
    });
    // Loose matches only help when nothing matches properly
    if (!ranked.empty() && ranked.front().rank <= 1)
        ranked.erase(std::remove_if(ranked.begin(), ranked.end(), [](const Ranked& r) { return r.rank == 3; }),
                     ranked.end());
    std::vector<Completion> out;
    std::set<std::string> seen;
    for (const Ranked& r : ranked) {
        if (!seen.insert(r.c->name).second) continue;  // overloads: the first one
        out.push_back(*r.c);
        if (out.size() >= 60) break;
    }
    return out;
}

}  // namespace s2d
