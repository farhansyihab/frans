# ~/AgenAI/frans/cmake/TreeSitter.cmake
#
# Deteksi submodule tree-sitter + grammar, buat static library untuk
# masing-masing grammar. Grammar yang tidak di-clone → di-skip dengan
# warning (tidak fatal).
#
# Set variabel global:
#   FRANS_HAS_CPP_PARSER       — ON/OFF
#   FRANS_HAS_TS_PARSER        — ON/OFF
#   FRANS_HAS_JS_PARSER        — ON/OFF
#   FRANS_HAS_VUE_PARSER       — ON/OFF (butuh TS + JS)
#   FRANS_CODE_INTEL_ENABLED   — ON/OFF (butuh core + python minimal)
#
# Membuat target: tree_sitter, tree_sitter_python, tree_sitter_cpp,
#                 tree_sitter_javascript, tree_sitter_typescript,
#                 tree_sitter_tsx.
# TIDAK membuat frans_code_intel — itu di Libraries.cmake.
include_guard(GLOBAL)

set(FRANS_CODE_INTEL_ENABLED OFF)

set(TS_CORE       "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src/lib.c")
set(TS_CORE_HDR   "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include/tree_sitter/api.h")
set(TS_PY_PARSER  "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/parser.c")
set(TS_PY_SCANNER "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/scanner.c")

if(NOT (EXISTS "${TS_CORE}" AND EXISTS "${TS_CORE_HDR}" AND EXISTS "${TS_PY_PARSER}"))
    message(WARNING
        "tree-sitter submodules tidak ditemukan.\n"
        "  Skip frans_code_intel + frans_context + frans_project + agent tests.\n"
        "  Untuk mengaktifkan: git submodule update --init --recursive")
    return()
endif()

message(STATUS "tree-sitter submodules ditemukan — build code_intelligence layer")
set(FRANS_CODE_INTEL_ENABLED ON)

# ---------------------------------------------------------------------------
# tree-sitter core (C)
# ---------------------------------------------------------------------------
add_library(tree_sitter STATIC "${TS_CORE}")
target_include_directories(tree_sitter PUBLIC
    "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include"
    "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src"
)
target_compile_options(tree_sitter PRIVATE -w)

# ---------------------------------------------------------------------------
# tree-sitter-python (selalu tersedia — sudah divalidasi di atas)
# ---------------------------------------------------------------------------
set(TS_PY_SOURCES "${TS_PY_PARSER}")
if(EXISTS "${TS_PY_SCANNER}")
    list(APPEND TS_PY_SOURCES "${TS_PY_SCANNER}")
endif()

add_library(tree_sitter_python STATIC ${TS_PY_SOURCES})
target_include_directories(tree_sitter_python PRIVATE
    "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src"
)
target_compile_options(tree_sitter_python PRIVATE -w)

# ---------------------------------------------------------------------------
# tree-sitter-cpp — Stage 10a
# ---------------------------------------------------------------------------
set(TS_CPP_DIR     "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-cpp/src")
set(TS_CPP_PARSER  "${TS_CPP_DIR}/parser.c")
set(TS_CPP_SCAN_CC "${TS_CPP_DIR}/scanner.cc")
set(TS_CPP_SCAN_C  "${TS_CPP_DIR}/scanner.c")
set(FRANS_HAS_CPP_PARSER OFF)

if(EXISTS "${TS_CPP_PARSER}")
    message(STATUS "tree-sitter-cpp ditemukan — C++ parser enabled")

    set(TS_CPP_SOURCES "${TS_CPP_PARSER}")

    if(EXISTS "${TS_CPP_SCAN_CC}")
        message(STATUS "  scanner: scanner.cc (C++)")
        list(APPEND TS_CPP_SOURCES "${TS_CPP_SCAN_CC}")
    elseif(EXISTS "${TS_CPP_SCAN_C}")
        message(STATUS "  scanner: scanner.c (C)")
        list(APPEND TS_CPP_SOURCES "${TS_CPP_SCAN_C}")
    else()
        message(WARNING
            "tree-sitter-cpp: scanner TIDAK ditemukan di ${TS_CPP_DIR}.\n"
            "  Dicek: scanner.cc dan scanner.c.\n"
            "  Link akan gagal karena parser.c mereferensikan\n"
            "  tree_sitter_cpp_external_scanner_*.")
    endif()

    add_library(tree_sitter_cpp STATIC ${TS_CPP_SOURCES})
    target_include_directories(tree_sitter_cpp PRIVATE "${TS_CPP_DIR}")
    target_compile_options(tree_sitter_cpp PRIVATE -w)
    set(FRANS_HAS_CPP_PARSER ON)
else()
    message(WARNING
        "tree-sitter-cpp TIDAK ditemukan — skip C++ parser.\n"
        "  Target frans_cpp_parser_test tidak akan di-build.")
endif()

# ---------------------------------------------------------------------------
# tree-sitter-javascript — Stage 10b
# ---------------------------------------------------------------------------
set(TS_JS_DIR     "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-javascript/src")
set(TS_JS_PARSER  "${TS_JS_DIR}/parser.c")
set(TS_JS_SCANNER "${TS_JS_DIR}/scanner.c")
set(FRANS_HAS_JS_PARSER OFF)

if(EXISTS "${TS_JS_PARSER}")
    message(STATUS "tree-sitter-javascript ditemukan — JavaScript parser enabled")

    set(TS_JS_SOURCES "${TS_JS_PARSER}")
    if(EXISTS "${TS_JS_SCANNER}")
        message(STATUS "  scanner: scanner.c")
        list(APPEND TS_JS_SOURCES "${TS_JS_SCANNER}")
    endif()

    add_library(tree_sitter_javascript STATIC ${TS_JS_SOURCES})
    target_include_directories(tree_sitter_javascript PRIVATE "${TS_JS_DIR}")
    target_compile_options(tree_sitter_javascript PRIVATE -w)
    set(FRANS_HAS_JS_PARSER ON)
else()
    message(WARNING
        "tree-sitter-javascript TIDAK ditemukan — skip JavaScript parser.")
endif()

# ---------------------------------------------------------------------------
# tree-sitter-typescript + tree-sitter-tsx — Stage 10b
# ---------------------------------------------------------------------------
set(TS_TS_DIR  "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-typescript/typescript/src")
set(TS_TSX_DIR "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-typescript/tsx/src")
set(FRANS_HAS_TS_PARSER OFF)

if(EXISTS "${TS_TS_DIR}/parser.c" AND EXISTS "${TS_TSX_DIR}/parser.c")
    message(STATUS "tree-sitter-typescript ditemukan — TS + TSX parser enabled")

    set(TS_TS_SOURCES "${TS_TS_DIR}/parser.c")
    if(EXISTS "${TS_TS_DIR}/scanner.c")
        list(APPEND TS_TS_SOURCES "${TS_TS_DIR}/scanner.c")
    endif()
    add_library(tree_sitter_typescript STATIC ${TS_TS_SOURCES})
    target_include_directories(tree_sitter_typescript PRIVATE "${TS_TS_DIR}")
    target_compile_options(tree_sitter_typescript PRIVATE -w)

    set(TS_TSX_SOURCES "${TS_TSX_DIR}/parser.c")
    if(EXISTS "${TS_TSX_DIR}/scanner.c")
        list(APPEND TS_TSX_SOURCES "${TS_TSX_DIR}/scanner.c")
    endif()
    add_library(tree_sitter_tsx STATIC ${TS_TSX_SOURCES})
    target_include_directories(tree_sitter_tsx PRIVATE "${TS_TSX_DIR}")
    target_compile_options(tree_sitter_tsx PRIVATE -w)

    set(FRANS_HAS_TS_PARSER ON)
else()
    message(WARNING
        "tree-sitter-typescript TIDAK ditemukan — skip TypeScript parser.")
endif()

# ---------------------------------------------------------------------------
# Vue SFC parser — Stage 10c
# ---------------------------------------------------------------------------
# Butuh BAIK TypeScript maupun JavaScript parser (delegasi dua arah).
set(FRANS_HAS_VUE_PARSER OFF)
if(FRANS_HAS_TS_PARSER AND FRANS_HAS_JS_PARSER)
    message(STATUS "Vue SFC parser enabled (delegasi ke TS + JS parser)")
    set(FRANS_HAS_VUE_PARSER ON)
else()
    message(WARNING
        "Vue parser SKIP — butuh BAIK TypeScript maupun JavaScript parser.\n"
        "  Status: TS=${FRANS_HAS_TS_PARSER} JS=${FRANS_HAS_JS_PARSER}")
endif()