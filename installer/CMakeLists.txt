cmake_minimum_required(VERSION 3.22)

# CMP0091 must match the root project setting for consistent runtime library handling
cmake_policy(SET CMP0091 NEW)

project(MasterBlasterInstaller VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-only: the installer is a Win32 application
if(NOT WIN32)
    message(FATAL_ERROR "The installer is Windows-only.")
endif()

# ── Static CRT: installer.exe has zero runtime DLL dependencies ───────────────
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_executable(MasterBlasterSetup WIN32
    src/installer.cpp
    src/installer.rc
)

target_compile_definitions(MasterBlasterSetup PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_compile_options(MasterBlasterSetup PRIVATE
    /W3
    /O2
    /GL     # whole-program optimisation → smaller, faster exe
    /MT     # static CRT — belt-and-braces, matches CMAKE_MSVC_RUNTIME_LIBRARY above
)

target_link_options(MasterBlasterSetup PRIVATE
    /LTCG
    /SUBSYSTEM:WINDOWS
)

# Output directly into installer/dist/ (relative to the installer/ source dir)
set_target_properties(MasterBlasterSetup PROPERTIES
    OUTPUT_NAME "MasterBlaster-1.1.0-Setup"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_CURRENT_SOURCE_DIR}/dist"
)
