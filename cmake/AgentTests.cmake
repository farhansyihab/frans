# ~/AgenAI/frans/cmake/AgentTests.cmake
#
# Test executable untuk Agent loop, TaskRunner, GenerateState, dan
# integration test yang membutuhkan tree-sitter + ProjectGraphCache.
#
# Label:
#   requires-llama — butuh llama-server hidup di 127.0.0.1:8080.
#                    Kalau tidak, test akan fail (connection refused).
#   network        — butuh API key + internet (via ProviderRouter).
#
# Skip di CI ringan:
#   ctest -LE "network|requires-llama"
include_guard(GLOBAL)

if(NOT FRANS_CODE_INTEL_ENABLED)
    message(STATUS "Agent tests SKIP — tree-sitter tidak tersedia")
    return()
endif()

# ---------------------------------------------------------------------------
# Stage 7b — GenerateState
# ---------------------------------------------------------------------------
add_executable(frans_generate_test
    tests/generate_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/llama/LlamaCppProvider.cpp
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
add_test(NAME frans_generate_test COMMAND $<TARGET_FILE:frans_generate_test>)
set_tests_properties(frans_generate_test PROPERTIES
    LABELS "requires-llama"
    TIMEOUT 300
)

# ---------------------------------------------------------------------------
# Stage 7d — Agent loop end-to-end
# ---------------------------------------------------------------------------
add_executable(frans_agent_loop_test
    tests/agent_loop_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/PlanState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ObserveState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/FixState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/Agent.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
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
add_test(NAME frans_agent_loop_test COMMAND $<TARGET_FILE:frans_agent_loop_test>)
set_tests_properties(frans_agent_loop_test PROPERTIES
    LABELS "requires-llama"
    TIMEOUT 600
)

# ---------------------------------------------------------------------------
# Stage 7 — TaskRunner
# ---------------------------------------------------------------------------
add_executable(frans_task_runner_test
    tests/task_runner_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/PlanState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ObserveState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/FixState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/Agent.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/application/task_runner/TaskRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/DiffEngine.cpp
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
add_test(NAME frans_task_runner_test COMMAND $<TARGET_FILE:frans_task_runner_test>)
set_tests_properties(frans_task_runner_test PROPERTIES
    LABELS "requires-llama"
    TIMEOUT 600
)

# ---------------------------------------------------------------------------
# Stage 7f — Project awareness
# ---------------------------------------------------------------------------
add_executable(frans_project_awareness_test
    tests/project_awareness_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/PlanState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ObserveState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/FixState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/Agent.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
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
add_test(NAME frans_project_awareness_test COMMAND $<TARGET_FILE:frans_project_awareness_test>)
set_tests_properties(frans_project_awareness_test PROPERTIES
    LABELS "requires-llama"
    TIMEOUT 600
)

# ---------------------------------------------------------------------------
# Stage 11c — Manual Import E2E (paste → write → graph rebuild)
# ---------------------------------------------------------------------------
# Tidak butuh LLM — pure application-layer test.
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
add_test(NAME frans_manual_import_e2e_test COMMAND $<TARGET_FILE:frans_manual_import_e2e_test>)
set_tests_properties(frans_manual_import_e2e_test PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------------
# Stage 11d-revisi — BuildRunner
# ---------------------------------------------------------------------------
# Tidak butuh LLM — pure subprocess (cmake/g++/python3/node).
# Test ini sendiri yang akan memanggil toolchain dari dalam test.
add_executable(frans_build_runner_test
    tests/build_runner_smoke_test.cpp
)
target_link_libraries(frans_build_runner_test PRIVATE
    frans_build_runner
)
target_include_directories(frans_build_runner_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_build_runner_test PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME frans_build_runner_test COMMAND $<TARGET_FILE:frans_build_runner_test>)
set_tests_properties(frans_build_runner_test PROPERTIES TIMEOUT 180)

# ---------------------------------------------------------------------------
# Stage 9c — ProviderRouter integration via Agent loop
# ---------------------------------------------------------------------------
# Butuh network (via ProviderRouter) + llama-server.
add_executable(frans_provider_router_integration_test
    tests/provider_router_integration_test.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/PlanState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/ObserveState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/FixState.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/Agent.cpp
    ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
    ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
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
add_test(NAME frans_provider_router_integration_test
    COMMAND $<TARGET_FILE:frans_provider_router_integration_test>)
set_tests_properties(frans_provider_router_integration_test PROPERTIES
    LABELS "network;requires-llama"
    TIMEOUT 900
)