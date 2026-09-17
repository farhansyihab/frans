# ~/AgenAI/frans/cmake/ParserTests.cmake
#
# Test executable untuk parser & context (Stage 5-10).
# Semua test ini OFFLINE — tidak butuh network, tidak butuh LLM.
#
# Setiap add_executable() diikuti add_test() supaya bisa dijalankan
# via ctest. Tidak ada label khusus (semua selalu dijalankan).
include_guard(GLOBAL)

if(NOT FRANS_CODE_INTEL_ENABLED)
    message(STATUS "Parser tests SKIP — tree-sitter tidak tersedia")
    return()
endif()

# ---------------------------------------------------------------------------
# Stage 5 — graph smoke test (Python only)
# ---------------------------------------------------------------------------
add_executable(frans_graph_test tests/graph_smoke_test.cpp)
target_link_libraries(frans_graph_test PRIVATE frans_code_intel)
target_compile_definitions(frans_graph_test PRIVATE
    FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
)
add_test(NAME frans_graph_test COMMAND $<TARGET_FILE:frans_graph_test>)
set_tests_properties(frans_graph_test PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------------
# Stage 10a — C++ parser
# ---------------------------------------------------------------------------
if(FRANS_HAS_CPP_PARSER)
    add_executable(frans_cpp_parser_test tests/cpp_parser_smoke_test.cpp)
    target_link_libraries(frans_cpp_parser_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_cpp_parser_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/cpp"
    )
    target_compile_options(frans_cpp_parser_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME frans_cpp_parser_test COMMAND $<TARGET_FILE:frans_cpp_parser_test>)
    set_tests_properties(frans_cpp_parser_test PROPERTIES TIMEOUT 60)
endif()

# ---------------------------------------------------------------------------
# Stage 10b — TypeScript parser
# ---------------------------------------------------------------------------
if(FRANS_HAS_TS_PARSER)
    add_executable(frans_ts_parser_test tests/ts_parser_smoke_test.cpp)
    target_link_libraries(frans_ts_parser_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_ts_parser_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/typescript"
    )
    target_compile_options(frans_ts_parser_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME frans_ts_parser_test COMMAND $<TARGET_FILE:frans_ts_parser_test>)
    set_tests_properties(frans_ts_parser_test PROPERTIES TIMEOUT 60)
endif()

# ---------------------------------------------------------------------------
# Stage 10b — JavaScript parser
# ---------------------------------------------------------------------------
if(FRANS_HAS_JS_PARSER)
    add_executable(frans_js_parser_test tests/js_parser_smoke_test.cpp)
    target_link_libraries(frans_js_parser_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_js_parser_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/javascript"
    )
    target_compile_options(frans_js_parser_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME frans_js_parser_test COMMAND $<TARGET_FILE:frans_js_parser_test>)
    set_tests_properties(frans_js_parser_test PROPERTIES TIMEOUT 60)
endif()

# ---------------------------------------------------------------------------
# Stage 10c — Vue parser
# ---------------------------------------------------------------------------
if(FRANS_HAS_VUE_PARSER)
    add_executable(frans_vue_parser_test tests/vue_parser_smoke_test.cpp)
    target_link_libraries(frans_vue_parser_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_vue_parser_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/vue"
    )
    target_compile_options(frans_vue_parser_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME frans_vue_parser_test COMMAND $<TARGET_FILE:frans_vue_parser_test>)
    set_tests_properties(frans_vue_parser_test PROPERTIES TIMEOUT 60)
endif()

# ---------------------------------------------------------------------------
# Stage 6 — context selector
# ---------------------------------------------------------------------------
add_executable(frans_context_test tests/context_smoke_test.cpp)
target_link_libraries(frans_context_test PRIVATE frans_context)
target_compile_definitions(frans_context_test PRIVATE
    FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
)
add_test(NAME frans_context_test COMMAND $<TARGET_FILE:frans_context_test>)
set_tests_properties(frans_context_test PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------------
# Stage 10d — multi-language integration
# ---------------------------------------------------------------------------
if(FRANS_HAS_CPP_PARSER AND FRANS_HAS_VUE_PARSER)
    add_executable(frans_multilang_test tests/multilang_smoke_test.cpp)
    target_link_libraries(frans_multilang_test PRIVATE frans_project)
    target_compile_definitions(frans_multilang_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/mixed"
    )
    target_compile_options(frans_multilang_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME frans_multilang_test COMMAND $<TARGET_FILE:frans_multilang_test>)
    set_tests_properties(frans_multilang_test PROPERTIES TIMEOUT 60)
else()
    message(WARNING
        "frans_multilang_test SKIP — butuh C++ + Vue parser.\n"
        "  Status: CPP=${FRANS_HAS_CPP_PARSER} VUE=${FRANS_HAS_VUE_PARSER}")
endif()