# ~/AgenAI/frans/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
# Stage 5 — tambah C untuk compile tree-sitter core + grammar Python.
# Stage 8 — tambah OpenSSL (opsional) untuk HTTPS (DeepSeek).
# Stage 9c — tambah frans_routing (ProviderRouter + EscalationPolicy +
#            DailyBudgetGuard) sebagai static library bersama. Agent.cpp
#            sekarang mereferensikan simbol-simbol ini, jadi SEMUA target
#            yang meng-link Agent.cpp WAJIB juga link frans_routing.
#            Dikumpulkan jadi satu library untuk menghindari duplikasi
#            source list yang riskan drift.
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
# Kalau tidak ditemukan, build tetap lanjut — DeepSeekProvider akan fail
# runtime dengan pesan jelas, dan LlamaCppProvider (HTTP lokal) tetap jalan.
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
# Stage 9c — frans_routing: ProviderRouter + EscalationPolicy +
#            DailyBudgetGuard.
#
# Static library bersama supaya Agent.cpp (yang mereferensikan ketiga
# komponen ini) bisa di-link oleh SEMUA target test maupun frans_ai tanpa
# perlu menduplikasi source list di setiap target.
#
# Dependency: nlohmann_json (DailyBudgetGuard.cpp pakai nlohmann::json
# untuk persist state budget). Dideklarasikan PUBLIC supaya consumer yang
# meng-link frans_routing otomatis mendapat nlohmann_json juga — mereka
# tidak perlu tahu detail internal DailyBudgetGuard.
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

    add_executable(frans_graph_test tests/graph_smoke_test.cpp)
    target_link_libraries(frans_graph_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_graph_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

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
    # Stage 7f — ProjectGraphCache + TargetScopeResolver (library bersama)
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
    # Stage 7b — GenerateState smoke test
    #
    # Catatan: TIDAK link frans_routing karena test ini tidak memakai
    # Agent.cpp (hanya GenerateState langsung). Kalau nanti test ini
    # mulai memakai Agent, tambahkan frans_routing ke target_link_libraries.
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
    #
    # Stage 9c: Agent.cpp sekarang referensi ProviderRouter + DailyBudgetGuard,
    # jadi WAJIB link frans_routing. Test file itu sendiri TIDAK diubah —
    # dia hanya set deps.provider (bukan deps.router), jadi runtime path
    # tetap mengikuti perilaku lama, tapi linker butuh simbol tetap ada
    # karena Agent.cpp di-compile dengan kedua branch.
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
    #
    # Stage 9c: sama seperti frans_agent_loop_test — butuh frans_routing
    # karena meng-link Agent.cpp.
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
    #
    # Stage 9c: butuh frans_routing karena meng-link Agent.cpp.
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

    set(FRANS_CODE_INTEL_ENABLED ON)
else()
    message(WARNING
        "tree-sitter submodules tidak ditemukan.\n"
        "  Jalankan:\n"
        "    cd third_party && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter.git && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter-python.git\n"
        "  Skip frans_code_intel + frans_context + frans_project + agent tests.\n"
        "  Stage 7f (AgentPanel) TIDAK akan di-build.")
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
#
# Stage 9c: AgentPanel sekarang membuat ProviderRouter (via provider_router
# header) + main.cpp membuat DailyBudgetGuard. Karena itu, frans_ai WAJIB
# link frans_routing. Di-link di dalam if(FRANS_CODE_INTEL_ENABLED) karena
# AgentPanel.cpp hanya di-compile kalau code_intel enabled.
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
    src/config/DeepSeekConfig.cpp          # Stage 8

    # ---- domain ----
    src/domain/command/MacroCommand.cpp

    # ---- infrastructure ----
    src/infrastructure/http/HttpProbe.cpp
    src/infrastructure/llama/ModelRegistry.cpp
    src/infrastructure/llama/LlamaServerProcessController.cpp
    src/infrastructure/llama/LlamaCppProvider.cpp
    src/infrastructure/api_providers/DeepSeekProvider.cpp   # Stage 8
    src/infrastructure/system/ProcessExecutor.cpp
    src/infrastructure/system/MemoryInfo.cpp
    src/infrastructure/system/SystemMaintenanceFacade.cpp
    src/infrastructure/system/DiffEngine.cpp       # Stage 7g

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

        # ---- agent UI ----
        src/ui/AgentPanel.cpp

        # NOTE Stage 9c: source file ProviderRouter.cpp, EscalationPolicy.cpp,
        # dan DailyBudgetGuard.cpp TIDAK dimasukkan ke sini — mereka masuk
        # via link ke frans_routing di bawah, supaya tidak terduplikasi
        # dengan target test lain.
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
        frans_routing      # Stage 9c — ProviderRouter + DailyBudgetGuard
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
#
# Link frans_routing sebagai ganti source file eksplisit, supaya konsisten
# dengan target lain. frans_routing sendiri sudah membawa nlohmann_json +
# Threads transitively (PUBLIC), jadi link tambahan tidak diperlukan.
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
