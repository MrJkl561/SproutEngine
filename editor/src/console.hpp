// Windows: an exported game is a windowed program, with no console. A
// dedicated server (--server) has no window either, so it borrows the console
// it was started from: its print()s show there and Ctrl+C stops it.
// (Its own file: <windows.h> can't be in a file with raylib.h.)
#pragma once

namespace s2d {

void useParentConsole();  // does nothing on other systems, or without a console to borrow

}  // namespace s2d
