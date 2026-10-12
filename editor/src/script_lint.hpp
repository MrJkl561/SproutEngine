// Mistakes in scripts that compile fine but don't work, found by reading the
// script's text (shown as warnings in the Script tab, next to the compiler's).
//
//   - self / getNode / findNode / autoload in a member variable's "= ..." (or the
//     constructor): that runs when the script is made, before it's on its
//     node, so it always finds nothing. It belongs in ready().
#pragma once

#include <string>
#include <vector>

#include "text_editor.hpp"  // Diagnostic

namespace s2d {

std::vector<Diagnostic> lintScript(const std::string& text);

}  // namespace s2d
