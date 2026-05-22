cmake_minimum_required(VERSION 3.25)
project(whis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure Ninja handles dependency scanning cleanly
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define target platform mode flags
if(EMSCRIPTEN)
    message(STATUS "Whis Core: Target platform identified as WebAssembly / Emscripten")
    set(WHIS_BUILD_WASM ON)
else()
    message(STATUS "Whis Core: Target platform identified as Native Desktop (${CMAKE_SYSTEM_PROCESSOR})")
    set(WHIS_BUILD_WASM OFF)
endif()

# =========================================================================
# Dependency Resolution (PEGTL Header-Only Integration)
# =========================================================================
include(FetchContent)

FetchContent_Declare(
    pegtl
    GIT_REPOSITORY https://github.com/taocpp/PEGTL.git
    GIT_TAG        3.2.7 # Clean stable branch matched to standard C++20 setups
)
FetchContent_MakeAvailable(pegtl)

# =========================================================================
# System Subdirectories Setup
# =========================================================================
add_subdirectory(core)

if(NOT WHIS_BUILD_WASM)
    # Enable comprehensive test suites natively. We exclude these from the 
    # Emscripten pass to keep the WebAssembly asset bundle compact.
    enable_testing()
    add_subdirectory(tests)
endif()