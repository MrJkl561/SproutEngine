# =============================================================================
#  ENet: reliable and unreliable messages over UDP, for the Sprout Editor's
#  multiplayer (the same library Godot uses for its ENetMultiplayerPeer).
#  MIT license. Downloaded once, like the other libraries.
#
#  Makes the target sprout_enet. Not for the web: browsers can't use UDP.
# =============================================================================
include(FetchContent)

set(SPROUT_ENET_VERSION "v1.3.18" CACHE STRING "ENet git tag to download")

# Only the sources are used (SOURCE_SUBDIR points nowhere): ENet's own
# CMakeLists sets things for the whole folder, so it's built here instead.
FetchContent_Declare(enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        ${SPROUT_ENET_VERSION}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  sprout-no-cmake
)
FetchContent_MakeAvailable(enet)

if(NOT TARGET sprout_enet)
    add_library(sprout_enet STATIC
        "${enet_SOURCE_DIR}/callbacks.c"
        "${enet_SOURCE_DIR}/compress.c"
        "${enet_SOURCE_DIR}/host.c"
        "${enet_SOURCE_DIR}/list.c"
        "${enet_SOURCE_DIR}/packet.c"
        "${enet_SOURCE_DIR}/peer.c"
        "${enet_SOURCE_DIR}/protocol.c"
        "${enet_SOURCE_DIR}/unix.c"
        "${enet_SOURCE_DIR}/win32.c")
    target_include_directories(sprout_enet PUBLIC "${enet_SOURCE_DIR}/include")
    set_target_properties(sprout_enet PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(WIN32)
        target_link_libraries(sprout_enet PUBLIC ws2_32 winmm)
        if(MSVC)
            target_compile_definitions(sprout_enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
        endif()
    else()
        # Everything ENet can use is there on Linux, macOS and the BSDs.
        target_compile_definitions(sprout_enet PRIVATE HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1
            HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
    endif()
endif()
