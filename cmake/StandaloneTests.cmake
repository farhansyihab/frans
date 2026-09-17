# ~/AgenAI/frans/cmake/StandaloneTests.cmake
#
# Test executable yang TIDAK butuh tree-sitter, TIDAK butuh LLM,
# TIDAK butuh network. Semua offline & cepat.
#
# Test ini selalu di-build & selalu terdaftar di CTest tanpa label.
include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Stage 7a — SandboxedFileSystem
# ---------------------------------------------------------------------------
add_executable(frans_sandbox_test tests/sandbox_smoke_test.cpp)
target_link_libraries(frans_sandbox_test PRIVATE frans_fs)
target_compile_options(frans_sandbox_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_sandbox_test COMMAND $<TARGET_FILE:frans_sandbox_test>)
set_tests_properties(frans_sandbox_test PROPERTIES TIMEOUT 30)

# ---------------------------------------------------------------------------
# Stage 11a — ManualImporter (application layer)
# ---------------------------------------------------------------------------
add_executable(frans_manual_import_test tests/manual_importer_smoke_test.cpp)
target_link_libraries(frans_manual_import_test PRIVATE frans_manual_import)
target_include_directories(frans_manual_import_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_manual_import_test PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME frans_manual_import_test COMMAND $<TARGET_FILE:frans_manual_import_test>)
set_tests_properties(frans_manual_import_test PROPERTIES TIMEOUT 30)

# ---------------------------------------------------------------------------
# Stage 7c + 7g — ExecuteState + DangerCheckHandler
# ---------------------------------------------------------------------------
add_executable(frans_execute_test
    tests/execute_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/DiffEngine.cpp
)
target_link_libraries(frans_execute_test PRIVATE frans_fs)
target_include_directories(frans_execute_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_execute_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_execute_test COMMAND $<TARGET_FILE:frans_execute_test>)
set_tests_properties(frans_execute_test PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------------
# Stage 7g — DiffEngine
# ---------------------------------------------------------------------------
add_executable(frans_diff_engine_test tests/diff_engine_smoke_test.cpp)
target_link_libraries(frans_diff_engine_test PRIVATE frans_diff)
target_include_directories(frans_diff_engine_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(frans_diff_engine_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_diff_engine_test COMMAND $<TARGET_FILE:frans_diff_engine_test>)
set_tests_properties(frans_diff_engine_test PROPERTIES TIMEOUT 30)

# ---------------------------------------------------------------------------
# Stage 9b — ProviderRouter (unit-level, mock providers)
# ---------------------------------------------------------------------------
add_executable(frans_provider_router_test
    tests/provider_router_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/provider_router/ProviderRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/application/provider_router/EscalationPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/application/budget/DailyBudgetGuard.cpp
)
target_link_libraries(frans_provider_router_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_include_directories(frans_provider_router_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_provider_router_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_provider_router_test COMMAND $<TARGET_FILE:frans_provider_router_test>)
set_tests_properties(frans_provider_router_test PROPERTIES TIMEOUT 30)

# ---------------------------------------------------------------------------
# Stage 9a — DailyBudgetGuard (unit-level)
# ---------------------------------------------------------------------------
add_executable(frans_daily_budget_guard_test
    tests/daily_budget_guard_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/budget/DailyBudgetGuard.cpp
)
target_link_libraries(frans_daily_budget_guard_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_include_directories(frans_daily_budget_guard_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_daily_budget_guard_test PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME frans_daily_budget_guard_test COMMAND $<TARGET_FILE:frans_daily_budget_guard_test>)
set_tests_properties(frans_daily_budget_guard_test PROPERTIES TIMEOUT 30)

# ---------------------------------------------------------------------------
# Stage 9b — EscalationPolicy (unit-level)
# ---------------------------------------------------------------------------
add_executable(frans_escalation_policy_test
    tests/escalation_policy_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/provider_router/EscalationPolicy.cpp
)
target_include_directories(frans_escalation_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_escalation_policy_test PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME frans_escalation_policy_test COMMAND $<TARGET_FILE:frans_escalation_policy_test>)
set_tests_properties(frans_escalation_policy_test PROPERTIES TIMEOUT 30)