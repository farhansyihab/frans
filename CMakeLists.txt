# ~/AgenAI/frans/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
# Stage 5 — tambah C untuk compile tree-sitter core + grammar Python.
# Stage 8 — tambah OpenSSL (opsional) untuk HTTPS (DeepSeek).
# Stage 9c — tambah frans_routing (ProviderRouter + EscalationPolicy +
#            DailyBudgetGuard) sebagai static library bersama.
# Stage 10a — tambah tree_sitter_cpp + CppParser + CppAstVisitor
#             + frans_cpp_parser_test.
# Stage 10b — tambah tree_sitter_javascript + tree_sitter_typescript
#             + tree_sitter_tsx + TsJsParserBase + TypeScriptParser
#             + JavaScriptParser + TsJsAstVisitor + frans_ts_parser_test
#             + frans_js_parser_test.
# Stage 10c — tambah VueParser (SFC extraction + delegasi ke TS/JS parser)
#             + frans_vue_parser_test.
# Stage 10d — multi-language integration.
# Stage 11a — ManualImporter (application layer) + smoke test.
# Stage 11b — ManualImportDialog (UI) + tombol [Import Paste] di AgentPanel.
# Stage 11c — manual import E2E test (paste → write → graph rebuild).
#             Guard FRANS_CODE_INTEL_ENABLED karena butuh ProjectGraphCache
#             (transitively tree-sitter).
project(frans_ai LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Dependencies ----
find_package(FLTK REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# ===========================================================================
# Stage 8 — OpenSSL (opsional). Diperlukan untuk DeepSeekProvider (HTTPS).
# ===========================================================================
find_package(OpenSSL QUIET)
set(FRANS_HTTPLIB_DEFS "")
set(FRANS_OPENSSL_LIBS "")
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL ditemukan — HTTPS (DeepSeekProvider) enabled")
    list(APPEND FRANS_HTTPLIB_DEFS CPPHTTPLIB_OPENSSL_SUPPORT)
    list(APPEND FRANS_OPENSSL_LIBS OpenSSL::SSL OpenSSL::Crypto)
else()
    message(WARNING
        "OpenSSL TIDAK ditemukan — DeepSeekProvider (HTTPS) akan fail runtime.\n"
        "  Install: sudo apt install libssl-dev\n"
        "  Lalu reconfigure & rebuild.")
endif()

# ===========================================================================
# Stage 7a — FileSystemPort + SandboxedFileSystem
# ===========================================================================
add_library(frans_fs STATIC
    src/infrastructure/fs/SandboxedFileSystem.cpp
)
target_include_directories(frans_fs PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_fs PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 7g — DiffEngine (standalone, tidak butuh deps lain)
# ===========================================================================
add_library(frans_diff STATIC
    src/infrastructure/system/DiffEngine.cpp
)
target_include_directories(frans_diff PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_diff PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 11a — ManualImporter (application layer, standalone).
#
# Butuh: IFileSystemPort (frans_fs) + MultiFileParser + DangerCheckHandler.
# TIDAK butuh tree-sitter → sengaja dipisah supaya bisa dibuild di mesin
# tanpa grammar. Callback invalidation (std::function) memutus ketergantungan
# ke ProjectGraphCache (blueprint §3 — Dependency Inversion).
#
# CATATAN Stage 11b: ManualImportDialog (UI) TIDAK masuk library ini —
# lib ini bebas FLTK, dipakai oleh test CLI (frans_manual_import_test).
# Dialog masuk langsung ke frans_ai exe (butuh FLTK).
# ===========================================================================
add_library(frans_manual_import STATIC
    src/application/manual_import/ManualImporter.cpp
    src/application/agent/MultiFileParser.cpp
    src/application/safety/DangerCheckHandler.cpp
)
target_include_directories(frans_manual_import PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(frans_manual_import PUBLIC frans_fs)
target_compile_options(frans_manual_import PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 9c — frans_routing: ProviderRouter + EscalationPolicy +
#            DailyBudgetGuard.
# ===========================================================================
add_library(frans_routing STATIC
    src/application/provider_router/ProviderRouter.cpp
    src/application/provider_router/EscalationPolicy.cpp
    src/application/budget/DailyBudgetGuard.cpp
)
target_include_directories(frans_routing PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(frans_routing PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_compile_options(frans_routing PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 5 + 6 — tree-sitter & context (opsional)
# Stage 10a — C++ parser (opsional, conditional)
# Stage 10b — TypeScript + JavaScript parser (opsional, conditional)
# Stage 10c — Vue parser (opsional, conditional — butuh 10b)
# Stage 10d — multi-language registry (conditional on 10a/10b/10c)
# ===========================================================================
set(TS_CORE "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src/lib.c")
set(TS_CORE_HDR "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include/tree_sitter/api.h")
set(TS_PY_PARSER "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/parser.c")
set(TS_PY_SCANNER "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/scanner.c")

if(EXISTS "${TS_CORE}" AND EXISTS "${TS_CORE_HDR}" AND EXISTS "${TS_PY_PARSER}")
    message(STATUS "tree-sitter submodules ditemukan — build code_intelligence layer")

    add_library(tree_sitter STATIC "${TS_CORE}")
    target_include_directories(tree_sitter PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include"
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src"
    )
    target_compile_options(tree_sitter PRIVATE -w)

    set(TS_PY_SOURCES "${TS_PY_PARSER}")
    if(EXISTS "${TS_PY_SCANNER}")
        list(APPEND TS_PY_SOURCES "${TS_PY_SCANNER}")
    endif()

    add_library(tree_sitter_python STATIC ${TS_PY_SOURCES})
    target_include_directories(tree_sitter_python PRIVATE
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src"
    )
    target_compile_options(tree_sitter_python PRIVATE -w)

    # -----------------------------------------------------------------------
    # Stage 10a — tree-sitter-cpp (opsional)
    # -----------------------------------------------------------------------
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

    # -----------------------------------------------------------------------
    # Stage 10b — tree-sitter-javascript (opsional)
    # -----------------------------------------------------------------------
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

    # -----------------------------------------------------------------------
    # Stage 10b — tree-sitter-typescript + tree-sitter-tsx (opsional)
    # -----------------------------------------------------------------------
    set(TS_TS_DIR  "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-typescript/typescript/src")
    set(TS_TSX_DIR "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-typescript/tsx/src")
    set(FRANS_HAS_TS_PARSER OFF)

    if(EXISTS "${TS_TS_DIR}/parser.c" AND EXISTS "${TS_TSX_DIR}/parser.c")
        message(STATUS "tree-sitter-typescript ditemukan — TS + TSX parser enabled")

        # TypeScript grammar
        set(TS_TS_SOURCES "${TS_TS_DIR}/parser.c")
        if(EXISTS "${TS_TS_DIR}/scanner.c")
            list(APPEND TS_TS_SOURCES "${TS_TS_DIR}/scanner.c")
        endif()
        add_library(tree_sitter_typescript STATIC ${TS_TS_SOURCES})
        target_include_directories(tree_sitter_typescript PRIVATE "${TS_TS_DIR}")
        target_compile_options(tree_sitter_typescript PRIVATE -w)

        # TSX grammar
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

    # -----------------------------------------------------------------------
    # frans_code_intel — library bersama.
    # -----------------------------------------------------------------------
    set(FRANS_CODE_INTEL_SOURCES
        src/code_intelligence/parsers/PythonParser.cpp
        src/code_intelligence/visitor/PythonAstVisitor.cpp
        src/code_intelligence/graph/DependencyGraph.cpp
        src/code_intelligence/builder/TreeSitterGraphBuilder.cpp
    )
    set(FRANS_CODE_INTEL_LIBS
        tree_sitter
        tree_sitter_python
    )

    if(FRANS_HAS_CPP_PARSER)
        list(APPEND FRANS_CODE_INTEL_SOURCES
            src/code_intelligence/parsers/CppParser.cpp
            src/code_intelligence/visitor/CppAstVisitor.cpp
        )
        list(APPEND FRANS_CODE_INTEL_LIBS tree_sitter_cpp)
    endif()

    if(FRANS_HAS_TS_PARSER OR FRANS_HAS_JS_PARSER)
        list(APPEND FRANS_CODE_INTEL_SOURCES
            src/code_intelligence/parsers/TsJsParserBase.cpp
            src/code_intelligence/visitor/TsJsAstVisitor.cpp
        )
    endif()

    if(FRANS_HAS_TS_PARSER)
        list(APPEND FRANS_CODE_INTEL_SOURCES
            src/code_intelligence/parsers/TypeScriptParser.cpp
        )
        list(APPEND FRANS_CODE_INTEL_LIBS
            tree_sitter_typescript
            tree_sitter_tsx
        )
    endif()

    if(FRANS_HAS_JS_PARSER)
        list(APPEND FRANS_CODE_INTEL_SOURCES
            src/code_intelligence/parsers/JavaScriptParser.cpp
        )
        list(APPEND FRANS_CODE_INTEL_LIBS tree_sitter_javascript)
    endif()

    set(FRANS_HAS_VUE_PARSER OFF)
    if(FRANS_HAS_TS_PARSER AND FRANS_HAS_JS_PARSER)
        message(STATUS "Vue SFC parser enabled (delegasi ke TS + JS parser)")
        list(APPEND FRANS_CODE_INTEL_SOURCES
            src/code_intelligence/parsers/VueParser.cpp
        )
        set(FRANS_HAS_VUE_PARSER ON)
    else()
        message(WARNING
            "Vue parser SKIP — butuh BAIK TypeScript maupun JavaScript parser.")
    endif()

    add_library(frans_code_intel STATIC ${FRANS_CODE_INTEL_SOURCES})
    target_include_directories(frans_code_intel PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(frans_code_intel PUBLIC ${FRANS_CODE_INTEL_LIBS})
    target_compile_options(frans_code_intel PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 10d — macros untuk conditional registry di
    # TreeSitterGraphBuilder.cpp. PRIVATE karena hanya .cpp builder yang
    # butuh (supportedExtensions() meng-expose hasilnya via runtime).
    # -----------------------------------------------------------------------
    if(FRANS_HAS_CPP_PARSER)
        target_compile_definitions(frans_code_intel PRIVATE
            FRANS_HAS_CPP_PARSER=1)
    endif()
    if(FRANS_HAS_TS_PARSER)
        target_compile_definitions(frans_code_intel PRIVATE
            FRANS_HAS_TS_PARSER=1)
    endif()
    if(FRANS_HAS_JS_PARSER)
        target_compile_definitions(frans_code_intel PRIVATE
            FRANS_HAS_JS_PARSER=1)
    endif()
    if(FRANS_HAS_VUE_PARSER)
        target_compile_definitions(frans_code_intel PRIVATE
            FRANS_HAS_VUE_PARSER=1)
    endif()

    add_executable(frans_graph_test tests/graph_smoke_test.cpp)
    target_link_libraries(frans_graph_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_graph_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

    # -----------------------------------------------------------------------
    # Stage 10a — C++ parser smoke test.
    # -----------------------------------------------------------------------
    if(FRANS_HAS_CPP_PARSER)
        add_executable(frans_cpp_parser_test tests/cpp_parser_smoke_test.cpp)
        target_link_libraries(frans_cpp_parser_test PRIVATE frans_code_intel)
        target_compile_definitions(frans_cpp_parser_test PRIVATE
            FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/cpp"
        )
        target_compile_options(frans_cpp_parser_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    # -----------------------------------------------------------------------
    # Stage 10b — TypeScript parser smoke test.
    # -----------------------------------------------------------------------
    if(FRANS_HAS_TS_PARSER)
        add_executable(frans_ts_parser_test tests/ts_parser_smoke_test.cpp)
        target_link_libraries(frans_ts_parser_test PRIVATE frans_code_intel)
        target_compile_definitions(frans_ts_parser_test PRIVATE
            FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/typescript"
        )
        target_compile_options(frans_ts_parser_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    # -----------------------------------------------------------------------
    # Stage 10b — JavaScript parser smoke test.
    # -----------------------------------------------------------------------
    if(FRANS_HAS_JS_PARSER)
        add_executable(frans_js_parser_test tests/js_parser_smoke_test.cpp)
        target_link_libraries(frans_js_parser_test PRIVATE frans_code_intel)
        target_compile_definitions(frans_js_parser_test PRIVATE
            FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/javascript"
        )
        target_compile_options(frans_js_parser_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    # -----------------------------------------------------------------------
    # Stage 10c — Vue parser smoke test.
    # -----------------------------------------------------------------------
    if(FRANS_HAS_VUE_PARSER)
        add_executable(frans_vue_parser_test tests/vue_parser_smoke_test.cpp)
        target_link_libraries(frans_vue_parser_test PRIVATE frans_code_intel)
        target_compile_definitions(frans_vue_parser_test PRIVATE
            FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/vue"
        )
        target_compile_options(frans_vue_parser_test PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()

    add_library(frans_context STATIC
        src/application/context_budget/ContextBudgetManager.cpp
        src/application/context_selector/ContextSelector.cpp
    )
    target_include_directories(frans_context PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(frans_context PUBLIC frans_code_intel)
    target_compile_options(frans_context PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(frans_context_test tests/context_smoke_test.cpp)
    target_link_libraries(frans_context_test PRIVATE frans_context)
    target_compile_definitions(frans_context_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

    # -----------------------------------------------------------------------
    # Stage 7f — ProjectGraphCache + TargetScopeResolver
    # -----------------------------------------------------------------------
    add_library(frans_project STATIC
        src/application/project/ProjectGraphCache.cpp
        src/application/agent/TargetScopeResolver.cpp
    )
    target_include_directories(frans_project PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(frans_project PUBLIC frans_context)
    target_compile_options(frans_project PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 10d — multi-language integration test.
    # -----------------------------------------------------------------------
    if(FRANS_HAS_CPP_PARSER AND FRANS_HAS_VUE_PARSER)
        add_executable(frans_multilang_test tests/multilang_smoke_test.cpp)
        target_link_libraries(frans_multilang_test PRIVATE
            frans_project
        )
        target_compile_definitions(frans_multilang_test PRIVATE
            FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/mixed"
        )
        target_compile_options(frans_multilang_test PRIVATE
            -Wall -Wextra -Wpedantic)
    else()
        message(WARNING
            "frans_multilang_test SKIP — butuh C++ + Vue parser.\n"
            "  Status: CPP=${FRANS_HAS_CPP_PARSER} VUE=${FRANS_HAS_VUE_PARSER}")
    endif()

    # -----------------------------------------------------------------------
    # Stage 7b — GenerateState smoke test
    # -----------------------------------------------------------------------
    add_executable(frans_generate_test
        tests/generate_smoke_test.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/MultiFileParser.cpp
        src/infrastructure/llama/LlamaCppProvider.cpp
    )
    target_link_libraries(frans_generate_test PRIVATE
        frans_context
        frans_project
        frans_fs
        nlohmann_json::nlohmann_json
        Threads::Threads
        ${FRANS_OPENSSL_LIBS}
    )
    target_include_directories(frans_generate_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_definitions(frans_generate_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
        ${FRANS_HTTPLIB_DEFS}
    )
    target_compile_options(frans_generate_test PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 7d — Full agent loop smoke test
    # -----------------------------------------------------------------------
    add_executable(frans_agent_loop_test
        tests/agent_loop_smoke_test.cpp
        src/application/agent/PlanState.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/ExecuteState.cpp
        src/application/agent/ObserveState.cpp
        src/application/agent/FixState.cpp
        src/application/agent/Agent.cpp
        src/application/agent/MultiFileParser.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/infrastructure/system/ProcessExecutor.cpp
    )
    target_link_libraries(frans_agent_loop_test PRIVATE
        frans_context
        frans_project
        frans_fs
        frans_routing
    )
    target_include_directories(frans_agent_loop_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_options(frans_agent_loop_test PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 7e + 7g — TaskRunner smoke test
    # -----------------------------------------------------------------------
    add_executable(frans_task_runner_test
        tests/task_runner_smoke_test.cpp
        src/application/agent/PlanState.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/ExecuteState.cpp
        src/application/agent/ObserveState.cpp
        src/application/agent/FixState.cpp
        src/application/agent/Agent.cpp
        src/application/agent/MultiFileParser.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/application/task_runner/TaskRunner.cpp
        src/infrastructure/system/ProcessExecutor.cpp
        src/infrastructure/system/DiffEngine.cpp
    )
    target_link_libraries(frans_task_runner_test PRIVATE
        frans_context
        frans_project
        frans_fs
        frans_routing
    )
    target_include_directories(frans_task_runner_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_options(frans_task_runner_test PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 7f — Project awareness smoke test
    # -----------------------------------------------------------------------
    add_executable(frans_project_awareness_test
        tests/project_awareness_smoke_test.cpp
        src/application/agent/PlanState.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/ExecuteState.cpp
        src/application/agent/ObserveState.cpp
        src/application/agent/FixState.cpp
        src/application/agent/Agent.cpp
        src/application/agent/MultiFileParser.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/infrastructure/system/ProcessExecutor.cpp
    )
    target_link_libraries(frans_project_awareness_test PRIVATE
        frans_context
        frans_project
        frans_fs
        frans_routing
    )
    target_include_directories(frans_project_awareness_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_options(frans_project_awareness_test PRIVATE -Wall -Wextra -Wpedantic)

    # -----------------------------------------------------------------------
    # Stage 11c — Manual Import end-to-end test.
    #
    # Butuh tree-sitter (via frans_project → frans_context → frans_code_intel)
    # karena menguji kontrak ManualImporter ↔ ProjectGraphCache (invalidate
    # → rebuild). Kalau tree-sitter tidak ada, target ini di-skip — Stage 11a
    # test (frans_manual_import_test) tetap bisa dijalankan standalone.
    # -----------------------------------------------------------------------
    add_executable(frans_manual_import_e2e_test
        tests/manual_import_e2e_test.cpp
    )
    target_link_libraries(frans_manual_import_e2e_test PRIVATE
        frans_manual_import
        frans_project
    )
    target_include_directories(frans_manual_import_e2e_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_options(frans_manual_import_e2e_test PRIVATE
        -Wall -Wextra -Wpedantic)

    set(FRANS_CODE_INTEL_ENABLED ON)
else()
    message(WARNING
        "tree-sitter submodules tidak ditemukan.\n"
        "  Skip frans_code_intel + frans_context + frans_project + agent tests.")
    set(FRANS_CODE_INTEL_ENABLED OFF)
endif()

# ===========================================================================
# Stage 8 — DeepSeekProvider smoke test (independen dari tree-sitter)
# ===========================================================================
add_executable(frans_deepseek_test
    tests/deepseek_provider_smoke_test.cpp
    src/config/DeepSeekConfig.cpp
    src/infrastructure/api_providers/DeepSeekProvider.cpp
)
target_link_libraries(frans_deepseek_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${FRANS_OPENSSL_LIBS}
)
target_include_directories(frans_deepseek_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_definitions(frans_deepseek_test PRIVATE
    ${FRANS_HTTPLIB_DEFS}
)
target_compile_options(frans_deepseek_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Main executable (UI)
# ===========================================================================
set(FRANS_AI_SOURCES
    # ---- bootstrap ----
    src/main.cpp

    # ---- application ----
    src/application/Worker.cpp
    src/application/resource_guard/ResourceGuard.cpp
    src/application/resource_guard/IdleUnloadTimer.cpp

    # ---- config ----
    src/config/AppConfig.cpp
    src/config/LlamaServerConfig.cpp
    src/config/DeepSeekConfig.cpp

    # ---- domain ----
    src/domain/command/MacroCommand.cpp

    # ---- infrastructure ----
    src/infrastructure/http/HttpProbe.cpp
    src/infrastructure/llama/ModelRegistry.cpp
    src/infrastructure/llama/LlamaServerProcessController.cpp
    src/infrastructure/llama/LlamaCppProvider.cpp
    src/infrastructure/api_providers/DeepSeekProvider.cpp
    src/infrastructure/system/ProcessExecutor.cpp
    src/infrastructure/system/MemoryInfo.cpp
    src/infrastructure/system/SystemMaintenanceFacade.cpp
    src/infrastructure/system/DiffEngine.cpp

    # ---- ui ----
    src/ui/ChatView.cpp
    src/ui/MainWindow.cpp
    src/ui/MaintenancePanel.cpp
    src/ui/LlamaPanel.cpp
)

if(FRANS_CODE_INTEL_ENABLED)
    list(APPEND FRANS_AI_SOURCES
        # ---- agent ----
        src/application/agent/PlanState.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/ExecuteState.cpp
        src/application/agent/ObserveState.cpp
        src/application/agent/FixState.cpp
        src/application/agent/Agent.cpp
        src/application/agent/MultiFileParser.cpp
        src/application/agent/TargetScopeResolver.cpp
        src/application/project/ProjectGraphCache.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/application/task_runner/TaskRunner.cpp

        # ---- manual import (Stage 11a application) ----
        # ManualImporter.cpp sudah di frans_manual_import lib — WAJIB
        # ditambahkan di sini juga karena static lib tidak transitively
        # di-link ke executable. Sumber di-share, object file di-compile
        # dua kali (sekali untuk lib, sekali untuk exe) — trade-off kecil,
        # build time OK.
        src/application/manual_import/ManualImporter.cpp

        # ---- agent UI ----
        src/ui/AgentPanel.cpp

        # ---- manual import UI (Stage 11b) ----
        src/ui/ManualImportDialog.cpp
    )
endif()

add_executable(frans_ai ${FRANS_AI_SOURCES})

target_include_directories(frans_ai PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${FLTK_INCLUDE_DIRS}
)

target_link_libraries(frans_ai PRIVATE
    ${FLTK_LIBRARIES}
    nlohmann_json::nlohmann_json
    Threads::Threads
    frans_fs
    ${FRANS_OPENSSL_LIBS}
)

target_compile_definitions(frans_ai PRIVATE
    CPPHTTPLIB_THREAD_POOL_COUNT=2
    ${FRANS_HTTPLIB_DEFS}
)

target_compile_options(frans_ai PRIVATE -Wall -Wextra -Wpedantic)

if(FRANS_CODE_INTEL_ENABLED)
    target_link_libraries(frans_ai PRIVATE
        frans_code_intel
        frans_context
        frans_routing
    )
    target_compile_definitions(frans_ai PRIVATE FRANS_HAS_AGENT_PANEL=1)
endif()

# ===========================================================================
# Stage 7a — SandboxedFileSystem smoke test (tidak butuh tree-sitter)
# ===========================================================================
add_executable(frans_sandbox_test tests/sandbox_smoke_test.cpp)
target_link_libraries(frans_sandbox_test PRIVATE frans_fs)
target_compile_options(frans_sandbox_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 11a — ManualImporter smoke test (tidak butuh tree-sitter).
# ===========================================================================
add_executable(frans_manual_import_test tests/manual_importer_smoke_test.cpp)
target_link_libraries(frans_manual_import_test PRIVATE frans_manual_import)
target_include_directories(frans_manual_import_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_manual_import_test PRIVATE
    -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 7c + 7g — ExecuteState + DangerCheckHandler smoke test
# ===========================================================================
add_executable(frans_execute_test
    tests/execute_smoke_test.cpp
    src/application/agent/ExecuteState.cpp
    src/application/agent/MultiFileParser.cpp
    src/application/safety/DangerCheckHandler.cpp
    src/infrastructure/system/ProcessExecutor.cpp
    src/infrastructure/system/DiffEngine.cpp
)
target_link_libraries(frans_execute_test PRIVATE frans_fs)
target_include_directories(frans_execute_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_execute_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 7g — DiffEngine smoke test (standalone)
# ===========================================================================
add_executable(frans_diff_engine_test tests/diff_engine_smoke_test.cpp)
target_link_libraries(frans_diff_engine_test PRIVATE frans_diff)
target_include_directories(frans_diff_engine_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_diff_engine_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 9b — ProviderRouter + EscalationPolicy smoke test (unit-level)
# ===========================================================================
add_executable(frans_provider_router_test
    tests/provider_router_smoke_test.cpp
    src/application/provider_router/ProviderRouter.cpp
    src/application/provider_router/EscalationPolicy.cpp
    src/application/budget/DailyBudgetGuard.cpp
)
target_link_libraries(frans_provider_router_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_include_directories(frans_provider_router_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_provider_router_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 9a — DailyBudgetGuard smoke test (unit-level)
# ===========================================================================
add_executable(frans_daily_budget_guard_test
    tests/daily_budget_guard_smoke_test.cpp
    src/application/budget/DailyBudgetGuard.cpp
)
target_link_libraries(frans_daily_budget_guard_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_include_directories(frans_daily_budget_guard_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_daily_budget_guard_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 9b — EscalationPolicy smoke test (unit-level)
# ===========================================================================
add_executable(frans_escalation_policy_test
    tests/escalation_policy_smoke_test.cpp
    src/application/provider_router/EscalationPolicy.cpp
)
target_include_directories(frans_escalation_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_escalation_policy_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 9c — ProviderRouter + DailyBudgetGuard integration via Agent loop
# ===========================================================================
add_executable(frans_provider_router_integration_test
    tests/provider_router_integration_test.cpp
    src/application/agent/PlanState.cpp
    src/application/agent/GenerateState.cpp
    src/application/agent/ExecuteState.cpp
    src/application/agent/ObserveState.cpp
    src/application/agent/FixState.cpp
    src/application/agent/Agent.cpp
    src/application/agent/MultiFileParser.cpp
    src/application/safety/DangerCheckHandler.cpp
    src/infrastructure/system/ProcessExecutor.cpp
)
target_link_libraries(frans_provider_router_integration_test PRIVATE
    frans_context
    frans_project
    frans_fs
    frans_routing
)
target_include_directories(frans_provider_router_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_options(frans_provider_router_integration_test PRIVATE
    -Wall -Wextra -Wpedantic)
