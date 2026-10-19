cmake_minimum_required(VERSION 3.21)

project(battlezone_enhanced VERSION 0.1.0 LANGUAGES C CXX)

# Builds: see BUILD.md.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BZ_BUILD_GAME "Build the game itself (needs the VectorPie SDK and SDL2)" ON)

# --- third party ------------------------------------------------------------

# miniz (MIT): reads the MAME ROM zip.
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)
target_compile_options(miniz PRIVATE -w)

# --- the game core: no SDK, no SDL, testable on its own ----------------------

add_library(bzcore STATIC
    src/core/machine.c
    src/core/emu.c
    src/cpu/m6502.c
    src/mbox/mathbox.c
    src/avg/avg.c
    src/rom/romset.c
    src/capture/capture.c
    src/game/mainline.c
    src/game/view.c
    src/game/play.c
    src/game/nmi.c
    src/game/vgut.c
    src/game/sound.c
    src/game/coin.c
    src/game/pace.c
    src/sound/pokey.c
    src/sound/sound.c
    src/render/smooth.c
    src/render/enhance.c
    src/render/ground.c
    src/sound/samples.c
    src/render/solid.c
    src/render/color.c
    src/render/hiscore.c
)
target_include_directories(bzcore PUBLIC src)
target_link_libraries(bzcore PUBLIC miniz)
target_compile_options(bzcore PRIVATE -Wall -Wextra)
if(UNIX)
    target_link_libraries(bzcore PUBLIC m)
endif()

# --- tools ---------------------------------------------------------------------

# Checks the game frame by frame against a MAME recording.
add_executable(bzcompare tools/bzcompare/main.c)
target_link_libraries(bzcompare PRIVATE bzcore)
target_compile_options(bzcompare PRIVATE -Wall -Wextra)

# Checks the interrupt routine in C against the original's on the 6502 core.
add_executable(bznmi tools/bznmi/main.c)
target_link_libraries(bznmi PRIVATE bzcore)
target_compile_options(bznmi PRIVATE -Wall -Wextra)

# The board's sounds, each as a WAV file (for the sound replacement).
add_executable(bzsound tools/bzsound/main.c)
target_link_libraries(bzsound PRIVATE bzcore)
target_compile_options(bzsound PRIVATE -Wall -Wextra)

# Runs the game without a screen: its picture as SVG, its sound as WAV.
add_executable(bzshot tools/bzshot/main.c)
target_link_libraries(bzshot PRIVATE bzcore)
target_compile_options(bzshot PRIVATE -Wall -Wextra)

# --- the game: on the VectorPie SDK (a window on a PC, the vector board on the cabinet) ---

if(BZ_BUILD_GAME)
    set(BZ_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/VectorPie/sdk"
        CACHE PATH "VectorPie SDK source directory")
    if(NOT EXISTS "${BZ_SDK_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "VectorPie SDK not found in ${BZ_SDK_DIR}: see BUILD.md "
                            "(or configure with -DBZ_BUILD_GAME=OFF for the tools only)")
    endif()
    if(BZ_PI_BUILD)
        # cmake/toolchain-pi-clang.cmake (the Pi build from Windows, without
        # WSL): the cabinet's own SDL2, by its file.
        add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
            IMPORTED_SONAME "libSDL2-2.0.so.0"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2")
        set(THREADS_PREFER_PTHREAD_FLAG ON)
    endif()
    set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${BZ_SDK_DIR}" vectorpie EXCLUDE_FROM_ALL)

    add_library(bzout STATIC
        src/render/config.c
        src/render/draw.c
        src/platform/app.c
        src/platform/controls.c
        src/platform/menu.c
        src/platform/vpbind.c
        src/platform/audio.c
    )
    target_link_libraries(bzout PUBLIC bzcore vectorpie)
    target_compile_options(bzout PRIVATE -Wall -Wextra)

    add_executable(battlezone src/main.c)
    target_link_libraries(battlezone PRIVATE bzout)
    vp_program(battlezone)

    # package files next to the program (vp_asset_path looks there)
    configure_file(package/battlezone.ini ${CMAKE_CURRENT_BINARY_DIR}/battlezone.ini COPYONLY)
    configure_file(package/marquee.png ${CMAKE_CURRENT_BINARY_DIR}/marquee.png COPYONLY)
    configure_file(package/game.ini ${CMAKE_CURRENT_BINARY_DIR}/game.ini COPYONLY)
    # the sound pack (tools/mksounds.py made it)
    file(COPY package/sounds DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# --- tests ------------------------------------------------------------------------

include(CTest)
if(BUILD_TESTING)
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE bzcore)
    add_test(NAME core COMMAND test_core)
endif()
