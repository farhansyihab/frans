# ~/AgenAI/frans/cmake/Libraries.cmake
#
# Semua static library internal. Urutan deklarasi mengikuti dependency:
#   frans_fs → frans_manual_import → frans_code_intel → frans_context
#   → frans_project → frans_build_runner
#
# frans_routing & frans_diff independen, taruh paling atas.
#
# Conditional: frans_code_intel + frans_context + frans_project +
# frans_build_runner hanya di-build kalau FRANS_CODE_INTEL_ENABLED.
include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Stage 7a — frans_fs
# ---------------------------------------------------------------------------
add_library(frans_fs STATIC
    ${CMAKE_SOURCE_DIR}/src/infrastructure/fs/SandboxedFileSystem.cpp
)
target_include_directories(frans_fs PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_fs PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Stage 7g — frans_diff (standalone, tidak butuh deps lain)
# ---------------------------------------------------------------------------
add_library(frans_diff STATIC
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/DiffEngine.cpp
)
target_include_directories(frans_diff PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_diff PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Stage 11a — frans_manual_import
# ---------------------------------------------------------------------------
add_library(frans_manual_import STATIC
    ${CMAKE_SOURCE_DIR}/src/application/manual_import/ManualImporter.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
)
target_include_directories(frans_manual_import PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_manual_import PUBLIC frans_fs)
target_compile_options(frans_manual_import PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Stage 9c + 12 — frans_routing
# ---------------------------------------------------------------------------
add_library(frans_routing STATIC
    ${CMAKE_SOURCE_DIR}/src/application/provider_router/ProviderRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/application/provider_router/EscalationPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/application/budget/DailyBudgetGuard.cpp
)
target_include_directories(frans_routing PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_routing PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_compile_options(frans_routing PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Conditional — semua yang butuh tree-sitter
# ---------------------------------------------------------------------------
if(NOT FRANS_CODE_INTEL_ENABLED)
    return()
endif()

# ---------------------------------------------------------------------------
# frans_code_intel
# ---------------------------------------------------------------------------
set(_code_intel_sources
    ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/PythonParser.cpp
    ${CMAKE_SOURCE_DIR}/src/code_intelligence/visitor/PythonAstVisitor.cpp
    ${CMAKE_SOURCE_DIR}/src/code_intelligence/graph/DependencyGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/code_intelligence/builder/TreeSitterGraphBuilder.cpp
)
set(_code_intel_libs
    tree_sitter
    tree_sitter_python
)

if(FRANS_HAS_CPP_PARSER)
    list(APPEND _code_intel_sources
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/CppParser.cpp
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/visitor/CppAstVisitor.cpp
    )
    list(APPEND _code_intel_libs tree_sitter_cpp)
endif()

if(FRANS_HAS_TS_PARSER OR FRANS_HAS_JS_PARSER)
    list(APPEND _code_intel_sources
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/TsJsParserBase.cpp
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/visitor/TsJsAstVisitor.cpp
    )
endif()

if(FRANS_HAS_TS_PARSER)
    list(APPEND _code_intel_sources
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/TypeScriptParser.cpp
    )
    list(APPEND _code_intel_libs
        tree_sitter_typescript
        tree_sitter_tsx
    )
endif()

if(FRANS_HAS_JS_PARSER)
    list(APPEND _code_intel_sources
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/JavaScriptParser.cpp
    )
    list(APPEND _code_intel_libs tree_sitter_javascript)
endif()

if(FRANS_HAS_VUE_PARSER)
    list(APPEND _code_intel_sources
        ${CMAKE_SOURCE_DIR}/src/code_intelligence/parsers/VueParser.cpp
    )
endif()

add_library(frans_code_intel STATIC ${_code_intel_sources})
target_include_directories(frans_code_intel PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_code_intel PUBLIC ${_code_intel_libs})
target_compile_options(frans_code_intel PRIVATE -Wall -Wextra -Wpedantic)

if(FRANS_HAS_CPP_PARSER)
    target_compile_definitions(frans_code_intel PRIVATE FRANS_HAS_CPP_PARSER=1)
endif()
if(FRANS_HAS_TS_PARSER)
    target_compile_definitions(frans_code_intel PRIVATE FRANS_HAS_TS_PARSER=1)
endif()
if(FRANS_HAS_JS_PARSER)
    target_compile_definitions(frans_code_intel PRIVATE FRANS_HAS_JS_PARSER=1)
endif()
if(FRANS_HAS_VUE_PARSER)
    target_compile_definitions(frans_code_intel PRIVATE FRANS_HAS_VUE_PARSER=1)
endif()

# ---------------------------------------------------------------------------
# Stage 6 — frans_context
# ---------------------------------------------------------------------------
add_library(frans_context STATIC
    ${CMAKE_SOURCE_DIR}/src/application/context_budget/ContextBudgetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/application/context_selector/ContextSelector.cpp
)
target_include_directories(frans_context PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_context PUBLIC frans_code_intel)
target_compile_options(frans_context PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Stage 7f — frans_project
# ---------------------------------------------------------------------------
add_library(frans_project STATIC
    ${CMAKE_SOURCE_DIR}/src/application/project/ProjectGraphCache.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/TargetScopeResolver.cpp
)
target_include_directories(frans_project PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_project PUBLIC frans_context)
target_compile_options(frans_project PRIVATE -Wall -Wextra -Wpedantic)

# ---------------------------------------------------------------------------
# Stage 11d-revisi — frans_build_runner
# ---------------------------------------------------------------------------
# ProcessExecutor.cpp disertakan sebagai sumber di sini (tidak ada
# library frans_process terpisah). frans_ai juga menyertakan file yang
# sama di source list-nya — linker menyelesaikan symbol dari salah satu
# karena static library hanya pull .o saat dibutuhkan.
add_library(frans_build_runner STATIC
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/ProjectDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/CMakeBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/MakeBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/MesonBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/NpmBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/PoetryBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
)
target_include_directories(frans_build_runner PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(frans_build_runner PUBLIC
    frans_manual_import
    frans_project
)
target_compile_options(frans_build_runner PRIVATE -Wall -Wextra -Wpedantic)