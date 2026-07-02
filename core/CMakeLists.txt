# =========================================================================
# Sources & Architecture Target Configuration
# =========================================================================
set(CORE_HEADERS
    include/whis/grammar.hpp
    include/whis/ast.hpp
    include/whis/dimensions.hpp
)

set(CORE_SOURCES
    src/compiler.cpp
    src/ast_serialize.cpp
    src/dimensions.cpp
)

add_library(whis_core STATIC ${CORE_HEADERS} ${CORE_SOURCES})

# Establish structural modern search paths
target_include_directories(whis_core
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Bind the underlying library dependency requirements
target_link_libraries(whis_core PUBLIC taocpp::pegtl)

# Universal compilation definitions protecting across Arm64 Apple Clang 17
target_compile_options(whis_core PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow -O3>
)

# =========================================================================
# WebAssembly Deployment Target Construction Pass
# =========================================================================
if(WHIS_BUILD_WASM)
    # Create the WebAssembly output target module boundary
    add_executable(whis_wasm ../bindings/wasm/whis_wasm_bindings.cpp)
    target_link_libraries(whis_wasm PRIVATE whis_core)
    
    # Configure production emscripten compiler flags to expose parser boundaries
    target_link_options(whis_wasm PRIVATE
        "-sWASMTIME=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='WhisCompilerEngine'"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sDISABLE_EXCEPTION_CATCHING=0" # Required to pass PEGTL parse diagnostics cleanly
        "-O3"
    )
    
    set_target_properties(whis_wasm PROPERTIES
        OUTPUT_NAME "whis"
        SUFFIX ".js"
    )
endif()