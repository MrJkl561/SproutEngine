#include "console.hpp"

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#endif

namespace s2d {

void useParentConsole() {
#if defined(_WIN32)
    if (GetConsoleWindow()) return;  // has one already (a console program)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
    std::freopen("CONOUT$", "w", stdout);
    std::freopen("CONOUT$", "w", stderr);
#endif
}

}  // namespace s2d
