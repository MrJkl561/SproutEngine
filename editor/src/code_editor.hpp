// Opening scripts and scene files in a code editor of the user's choice
// (VS Code, Visual Studio, CLion, Sublime Text...). The choice is kept per
// user in the editor's settings folder (code_editor.json).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace s2d {

struct CodeEditor {
    std::string name = "System default";
    // The command line to run. {file} becomes the file and {project} the
    // project folder (both already quoted). "" = the system's default program
    // for that kind of file.
    std::string command;
};

// The editors this computer has, starting with "System default".
const std::vector<CodeEditor>& installedCodeEditors();

// The user's choice (System default until they pick one).
CodeEditor loadCodeEditor();
void saveCodeEditor(const CodeEditor& editor);
bool codeEditorChosen();  // false until the user has picked one

// Opens the file. False (with a reason in error) if it couldn't start.
bool openInCodeEditor(const CodeEditor& editor, const std::filesystem::path& file,
                      const std::filesystem::path& project, std::string& error);

}  // namespace s2d
