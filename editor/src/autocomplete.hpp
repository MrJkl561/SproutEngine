// Autocomplete for the Script tab. No compiler involved: it reads the engine's
// own headers (script.hpp, nodes.hpp...), raylib.h and a few raylib-cpp
// classes, the project's scripts and the file being edited, and knows the
// common parts of the standard library. It follows simple chains like
// self<CharacterBody2D>()->velocity. to suggest the right members.
#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace s2d {

struct Completion {
    enum class Kind { Keyword, Type, Function, Variable, Constant, Namespace };
    Kind kind = Kind::Variable;
    std::string name;    // what gets typed
    std::string detail;  // e.g. "void play(const std::string& name)"
    std::string doc;     // the comment that goes with it
    std::string type;    // variables: their type; functions: what they return
    std::string owner;   // the class it's a member of ("" = not a member)
    int source = 0;      // where it came from; lower is listed first (see Source)
    bool params = false;     // a function that takes arguments
    bool templated = false;  // needs <Type> when called, like self<Node2D>()
    std::string init;  // local variables: what they're set to (to work out what `auto` is)
    int line = 0;      // local variables: where they're declared
};

class CompletionIndex {
public:
    enum Source { ThisFile = 0, ThisClass = 1, Project = 2, Engine = 3, Language = 4, Raylib = 5 };

    // Where to find things. Nothing is read until the first completion.
    void setFolders(const std::filesystem::path& engineSrc, const std::filesystem::path& depsDir,
                    const std::filesystem::path& projectScripts);

    // What could be typed at (line, col) of the given text (lines of a C++
    // file). prefix gets the part of the word already typed. Returns nothing
    // if there's nothing worth suggesting.
    std::vector<Completion> complete(const std::vector<std::string>& lines, int line, int col,
                                     const std::filesystem::path& file, std::string& prefix, bool forced);

    // Everything a piece of C++ declares. Used for every file the index reads.
    struct Symbols {
        std::vector<Completion> globals;                         // types, free functions, constants
        std::map<std::string, std::vector<Completion>> members;  // class -> its members
        std::map<std::string, std::vector<std::string>> bases;   // class -> the classes it derives from
        std::vector<Completion> locals;  // variables inside functions (only for the file being edited)
        struct ClassRange {
            std::string name;
            int first = 0, last = 0;  // lines
        };
        std::vector<ClassRange> classes;  // where each class (or out-of-line member function) is
    };
    static void parse(const std::string& text, int source, bool everything, Symbols& out);

private:
    void load();  // the engine, raylib and std (once)
    void refreshProject(const std::filesystem::path& current);
    void merge(const Symbols& s);
    std::vector<Completion> membersOf(const std::string& type, bool arrow, int depth = 0) const;
    std::string resolve(const std::string& expr, int depth) const;
    std::string returnTypeOf(const std::string& owner, const std::string& name) const;
    const Completion* findMember(const std::string& type, const std::string& name, int depth = 0) const;

    std::filesystem::path engineSrc_, depsDir_, projectScripts_;
    bool loaded_ = false;
    Symbols base_;  // engine + raylib + std
    std::map<std::filesystem::path, std::pair<std::filesystem::file_time_type, Symbols>> projectFiles_;
    bool allDirty_ = true;                 // all_ needs rebuilding (a project file changed)
    std::filesystem::path allWithout_;     // the file all_ leaves out (the one being edited)

    // During one complete() call: everything merged, plus the file being edited
    Symbols all_;
    Symbols current_;
    std::string currentClass_;
    int cursorLine_ = 0;
};

}  // namespace s2d
