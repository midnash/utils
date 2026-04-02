cmake_minimum_required(VERSION 3.16)
project(utils VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Keep build artifacts out of the source root and avoid name clashes (e.g. ./ff directory).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

function(add_utils_tool target source)
    add_executable(${target} ${source})
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
        $<$<CONFIG:Debug>:-g -O0>
    )
endfunction()

add_utils_tool(ff ff/src/main.cpp)
add_utils_tool(rn rn/src/main.cpp)
add_utils_tool(lip lip/src/main.cpp)
add_utils_tool(pfd pfd/src/main.cpp)
add_utils_tool(git-summary git-summary/src/main.cpp)
add_utils_tool(git-size git-size/src/main.cpp)
add_utils_tool(hex hex/src/main.cpp)
add_utils_tool(magic magic/src/main.cpp)

set(UTILS_INSTALL_TARGETS
    ff
    rn
    lip
    pfd
    git-summary
    git-size
    hex
    magic
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET
        libavformat
        libavcodec
        libavutil
        libswscale
    )

    if(FFMPEG_FOUND)
        add_utils_tool(thumbgen thumbgen/src/main.cpp)
        target_link_libraries(thumbgen PRIVATE PkgConfig::FFMPEG)
        list(APPEND UTILS_INSTALL_TARGETS thumbgen)
    else()
        message(STATUS "FFmpeg development libraries not found: skipping thumbgen target")
    endif()
else()
    message(STATUS "pkg-config not found: skipping thumbgen target")
endif()

# Install to /usr/local/bin by default
install(TARGETS ${UTILS_INSTALL_TARGETS} DESTINATION bin)