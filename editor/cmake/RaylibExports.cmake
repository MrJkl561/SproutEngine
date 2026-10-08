# sprout_editor_raylib_def(<raylib.h> <output .def>)
#
# Windows hot reloading: the scripts DLL calls raylib functions that live
# inside sprout_editor.exe. An .exe only shares what it exports, and
# WINDOWS_EXPORT_ALL_SYMBOLS only exports the editor's own code, not the
# static raylib library linked into it. This writes a module-definition file
# naming every function raylib.h declares (the RLAPI lines); CMake merges it
# into the editor's exports.
function(sprout_editor_raylib_def header output)
    file(READ "${header}" text)
    # One declaration per line; ';' is turned into a marker first because
    # CMake lists split on it.
    string(REPLACE ";" "<semicolon>" text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    set(names "")
    foreach(line IN LISTS lines)
        if(line MATCHES "^RLAPI[^(]*[ *]([A-Za-z_][A-Za-z0-9_]*)\\(")
            list(APPEND names "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES names)
    list(LENGTH names count)
    if(count LESS 100)
        message(FATAL_ERROR "Sprout editor: only found ${count} functions in ${header}; has raylib.h changed?")
    endif()
    list(JOIN names "\n    " body)
    set(content "EXPORTS\n    ${body}\n")
    if(EXISTS "${output}")
        file(READ "${output}" old)
    endif()
    if(NOT old STREQUAL content)  # don't touch it when nothing changed (avoids relinking)
        file(WRITE "${output}" "${content}")
    endif()
endfunction()
