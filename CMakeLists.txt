# ~/AgenAI/frans/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
# Stage 5 — tambah C untuk compile tree-sitter core + grammar Python.
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
# Stage 7a — FileSystemPort + SandboxedFileSystem
#
# Diletakkan SEBELUM add_executable(frans_ai ...) karena frans_ai
# mereferensikan frans_fs di target_link_libraries. Forward-reference
# sebenarnya resolve di CMake 3.16+, tapi ordering eksplisit lebih
# aman & lebih mudah dibaca.
# ===========================================================================
add_library(frans_fs STATIC
    src/infrastructure/fs/SandboxedFileSystem.cpp
)
target_include_directories(frans_fs PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_fs PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 5 — Code Intelligence Layer (tree-sitter, opsional)
# Stage 6 — Context Selector + Budget Manager (butuh Code Intelligence)
# ===========================================================================
set(TS_CORE "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src/lib.c")
set(TS_CORE_HDR "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include/tree_sitter/api.h")
set(TS_PY_PARSER "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/parser.c")
set(TS_PY_SCANNER "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src/scanner.c")

if(EXISTS "${TS_CORE}" AND EXISTS "${TS_CORE_HDR}" AND EXISTS "${TS_PY_PARSER}")
    message(STATUS "tree-sitter submodules ditemukan — build code_intelligence layer")

    # ---- tree-sitter core (amalgamated lib.c) ----
    add_library(tree_sitter STATIC "${TS_CORE}")
    target_include_directories(tree_sitter PUBLIC
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/include"
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter/lib/src"
    )
    target_compile_options(tree_sitter PRIVATE -w)

    # ---- tree-sitter-python grammar ----
    set(TS_PY_SOURCES "${TS_PY_PARSER}")
    if(EXISTS "${TS_PY_SCANNER}")
        list(APPEND TS_PY_SOURCES "${TS_PY_SCANNER}")
    endif()

    add_library(tree_sitter_python STATIC ${TS_PY_SOURCES})
    target_include_directories(tree_sitter_python PRIVATE
        "${CMAKE_SOURCE_DIR}/third_party/tree-sitter-python/src"
    )
    target_compile_options(tree_sitter_python PRIVATE -w)

    # ---- frans_code_intel (Stage 5) ----
    add_library(frans_code_intel STATIC
        src/code_intelligence/parsers/PythonParser.cpp
        src/code_intelligence/visitor/PythonAstVisitor.cpp
        src/code_intelligence/graph/DependencyGraph.cpp
        src/code_intelligence/builder/TreeSitterGraphBuilder.cpp
    )
    target_include_directories(frans_code_intel PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(frans_code_intel PUBLIC
        tree_sitter
        tree_sitter_python
    )
    target_compile_options(frans_code_intel PRIVATE -Wall -Wextra -Wpedantic)

    # ---- Stage 5 smoke test ----
    add_executable(frans_graph_test tests/graph_smoke_test.cpp)
    target_link_libraries(frans_graph_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_graph_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

    # ---- frans_context (Stage 6) — ContextSelector + ContextBudgetManager ----
    add_library(frans_context STATIC
        src/application/context_budget/ContextBudgetManager.cpp
        src/application/context_selector/ContextSelector.cpp
    )
    target_include_directories(frans_context PUBLIC
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(frans_context PUBLIC frans_code_intel)
    target_compile_options(frans_context PRIVATE -Wall -Wextra -Wpedantic)

    # ---- Stage 6 smoke test ----
    add_executable(frans_context_test tests/context_smoke_test.cpp)
    target_link_libraries(frans_context_test PRIVATE frans_context)
    target_compile_definitions(frans_context_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

    # =======================================================================
    # Stage 7b — GenerateState smoke test
    # =======================================================================
    add_executable(frans_generate_test
        tests/generate_smoke_test.cpp
        src/application/agent/GenerateState.cpp
        src/infrastructure/llama/LlamaCppProvider.cpp
    )
    target_link_libraries(frans_generate_test PRIVATE
        frans_context
        frans_fs
        nlohmann_json::nlohmann_json
        Threads::Threads
    )
    target_include_directories(frans_generate_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_definitions(frans_generate_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )
    target_compile_options(frans_generate_test PRIVATE -Wall -Wextra -Wpedantic)

    # =======================================================================
    # Stage 7d — Full agent loop smoke test (mock provider, no llama-server)
    #
    # Butuh frans_context karena GenerateState.cpp memakai
    # formatContextForPrompt dari ContextSelector.h. FixState.cpp juga.
    # Karena itu target ini hanya di-build kalau tree-sitter tersedia.
    # =======================================================================
    add_executable(frans_agent_loop_test
        tests/agent_loop_smoke_test.cpp
        src/application/agent/PlanState.cpp
        src/application/agent/GenerateState.cpp
        src/application/agent/ExecuteState.cpp
        src/application/agent/ObserveState.cpp
        src/application/agent/FixState.cpp
        src/application/agent/Agent.cpp
        src/application/safety/DangerCheckHandler.cpp
        src/infrastructure/system/ProcessExecutor.cpp
    )
    target_link_libraries(frans_agent_loop_test PRIVATE
        frans_context
        frans_fs
    )
    target_include_directories(frans_agent_loop_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    target_compile_options(frans_agent_loop_test PRIVATE -Wall -Wextra -Wpedantic)

    set(FRANS_CODE_INTEL_ENABLED ON)
else()
    message(WARNING
        "tree-sitter submodules tidak ditemukan.\n"
        "  Jalankan:\n"
        "    cd third_party && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter.git && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter-python.git\n"
        "  Skip frans_code_intel + frans_graph_test + frans_context +\n"
        "       frans_agent_loop_test.")
    set(FRANS_CODE_INTEL_ENABLED OFF)
endif()

# ===========================================================================
# Main executable (UI)
# ===========================================================================
add_executable(frans_ai
    # ---- bootstrap ----
    src/main.cpp

    # ---- application ----
    src/application/Worker.cpp
    src/application/resource_guard/ResourceGuard.cpp
    src/application/resource_guard/IdleUnloadTimer.cpp

    # ---- config ----
    src/config/AppConfig.cpp
    src/config/LlamaServerConfig.cpp

    # ---- domain ----
    src/domain/command/MacroCommand.cpp

    # ---- infrastructure ----
    src/infrastructure/http/HttpProbe.cpp
    src/infrastructure/llama/ModelRegistry.cpp
    src/infrastructure/llama/LlamaServerProcessController.cpp
    src/infrastructure/llama/LlamaCppProvider.cpp
    src/infrastructure/system/ProcessExecutor.cpp
    src/infrastructure/system/MemoryInfo.cpp
    src/infrastructure/system/SystemMaintenanceFacade.cpp

    # ---- ui ----
    src/ui/ChatView.cpp
    src/ui/MainWindow.cpp
    src/ui/MaintenancePanel.cpp
    src/ui/LlamaPanel.cpp
)

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
)

target_compile_definitions(frans_ai PRIVATE
    CPPHTTPLIB_THREAD_POOL_COUNT=2
)

target_compile_options(frans_ai PRIVATE -Wall -Wextra -Wpedantic)

if(FRANS_CODE_INTEL_ENABLED)
    target_link_libraries(frans_ai PRIVATE frans_code_intel frans_context)
endif()

# ===========================================================================
# Stage 7a manual smoke test (tanpa tree-sitter)
# ===========================================================================
add_executable(frans_sandbox_test tests/sandbox_smoke_test.cpp)
target_link_libraries(frans_sandbox_test PRIVATE frans_fs)
target_compile_options(frans_sandbox_test PRIVATE -Wall -Wextra -Wpedantic)

# ===========================================================================
# Stage 7c manual smoke test — ExecuteState + DangerCheckHandler
#
# TIDAK bergantung tree-sitter → selalu di-build.
# ===========================================================================
add_executable(frans_execute_test
    tests/execute_smoke_test.cpp
    src/application/agent/ExecuteState.cpp
    src/application/safety/DangerCheckHandler.cpp
    src/infrastructure/system/ProcessExecutor.cpp
)
target_link_libraries(frans_execute_test PRIVATE frans_fs)
target_include_directories(frans_execute_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_execute_test PRIVATE -Wall -Wextra -Wpedantic)
