#include "script_lint.hpp"

#include <cctype>
#include <regex>

namespace s2d {

namespace {

// The text with comments and string/char literals blanked out (same length,
// newlines kept), so braces and names inside them don't count.
std::string codeOnly(const std::string& text) {
    std::string out = text;
    size_t i = 0, n = text.size();
    auto blank = [&](size_t from, size_t to) {
        for (size_t k = from; k < to && k < n; ++k)
            if (out[k] != '\n') out[k] = ' ';
    };
    while (i < n) {
        if (text.compare(i, 2, "//") == 0) {
            size_t end = text.find('\n', i);
            if (end == std::string::npos) end = n;
            blank(i, end);
            i = end;
        } else if (text.compare(i, 2, "/*") == 0) {
            size_t end = text.find("*/", i + 2);
            end = end == std::string::npos ? n : end + 2;
            blank(i, end);
            i = end;
        } else if (text[i] == '"' || text[i] == '\'') {
            char q = text[i];
            size_t k = i + 1;
            while (k < n && text[k] != q && text[k] != '\n') k += text[k] == '\\' ? 2 : 1;
            blank(i + 1, std::min(k, n));  // keep the quotes themselves
            i = k + 1;
        } else {
            ++i;
        }
    }
    return out;
}

// What a { opens
enum class Scope { Other, Class, Function, Init, Constructor };

}  // namespace

std::vector<Diagnostic> lintScript(const std::string& text) {
    std::vector<Diagnostic> out;
    const std::string code = codeOnly(text);
    static const std::regex classHead(R"((?:^|[^\w])(?:class|struct)\s+(\w+)[^()]*$)");
    static const std::regex call(R"(\b(self|getNode|findNode)\s*(?:<[^;(){}]*>)?\s*\()");

    auto where = [&](size_t offset, int& line, int& column) {
        line = 1;
        size_t lineStart = 0;
        for (size_t k = 0; k < offset && k < text.size(); ++k)
            if (text[k] == '\n') {
                ++line;
                lineStart = k + 1;
            }
        column = static_cast<int>(offset - lineStart) + 1;
    };
    // Each call found in code[from, to) gets a warning
    auto report = [&](size_t from, size_t to, bool constructor) {
        std::string part = code.substr(from, to - from);
        for (auto m = std::sregex_iterator(part.begin(), part.end(), call); m != std::sregex_iterator(); ++m) {
            Diagnostic d;
            d.error = false;
            where(from + m->position(1), d.line, d.column);
            d.endLine = d.line;
            d.endColumn = d.column + static_cast<int>(m->length(1));
            std::string fn = (*m)[1].str();
            d.message = fn + "() here runs when the script is made, before it's on its node" +
                        std::string(constructor ? " (in the constructor)" : "") +
                        ", so it always finds nothing (nullptr). Do it in ready(): declare the variable = nullptr "
                        "and set it there.";
            out.push_back(std::move(d));
        }
    };

    struct Frame {
        Scope scope;
        std::string className;
        size_t start;  // where its { is
    };
    std::vector<Frame> stack;
    size_t stmtStart = 0;  // where the current statement (in a class body) began
    auto top = [&] { return stack.empty() ? Scope::Other : stack.back().scope; };
    auto isLambdaOrFunction = [](std::string head) {
        while (!head.empty() && std::isspace(static_cast<unsigned char>(head.back()))) head.pop_back();
        return head.find(')') != std::string::npos || (!head.empty() && head.back() == ']');
    };

    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];
        if (c == '{') {
            std::string head = code.substr(stmtStart, i - stmtStart);
            std::smatch m;
            Scope s = Scope::Other;
            std::string name;
            Scope t = top();
            static const std::regex className(R"((?:^|[^\w])(?:class|struct)\s+(\w+))");
            if (std::regex_search(head, m, classHead) && head.find('=') == std::string::npos) {
                s = Scope::Class;
                // the last one: "template <class T> class Enemy" is Enemy
                for (auto k = std::sregex_iterator(head.begin(), head.end(), className); k != std::sregex_iterator(); ++k)
                    name = (*k)[1].str();
            } else if (t == Scope::Class) {
                const std::string& cls = stack.back().className;
                static const std::regex ctorStart(R"(^\s*(?:explicit\s+)?(\w+)\s*\()");
                // (after "public:" and the like)
                std::string h = head;
                for (const char* access : {"public:", "protected:", "private:"})
                    if (size_t p = h.rfind(access); p != std::string::npos) h = h.substr(p + std::string(access).size());
                if (std::regex_search(h, m, ctorStart) && m[1].str() == cls) {
                    s = Scope::Constructor;
                    report(stmtStart, i, true);  // its initializer list
                } else if (isLambdaOrFunction(head)) {
                    s = Scope::Function;
                } else {
                    s = Scope::Init;  // a member's {...}
                }
            } else if (t == Scope::Init) {
                s = isLambdaOrFunction(head) ? Scope::Function : Scope::Init;
            } else if (t == Scope::Constructor) {
                s = isLambdaOrFunction(head) && head.find(']') != std::string::npos ? Scope::Function : Scope::Constructor;
            }
            stack.push_back({s, name, i});
            if (s != Scope::Init) stmtStart = i + 1;  // (a member's {...} is part of its statement)
        } else if (c == '}') {
            if (stack.empty()) continue;
            Frame f = stack.back();
            stack.pop_back();
            if (f.scope == Scope::Constructor && top() == Scope::Class) report(f.start, i, true);
            if (f.scope != Scope::Init) stmtStart = i + 1;
        } else if (c == ';') {
            if (top() == Scope::Class) report(stmtStart, i, false);  // a member declaration
            if (top() != Scope::Init) stmtStart = i + 1;
        }
    }
    return out;
}

}  // namespace s2d
