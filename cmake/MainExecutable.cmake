# ~/AgenAI/frans/cmake/MainExecutable.cmake
#
# Target frans_ai (UI executable).
#
# Catatan duplikasi source: ProcessExecutor.cpp, MultiFileParser.cpp,
# dst. disertakan langsung di sini DAN di static library (frans_build_runner,
# frans_manual_import). Linker menyelesaikan symbol dari salah satu —
# static library hanya pull .o yang benar-benar dibutuhkan. Ini pola
# yang sudah ada sejak versi monolith, tidak diubah.
include_guard(GLOBAL)

set(FRANS_AI_SOURCES
    # ---- bootstrap ----
    ${CMAKE_SOURCE_DIR}/src/main.cpp

    # ---- application ----
    ${CMAKE_SOURCE_DIR}/src/application/Worker.cpp
    ${CMAKE_SOURCE_DIR}/src/application/resource_guard/ResourceGuard.cpp
    ${CMAKE_SOURCE_DIR}/src/application/resource_guard/IdleUnloadTimer.cpp

    # ---- config ----
    ${CMAKE_SOURCE_DIR}/src/config/AppConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/config/LlamaServerConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/config/DeepSeekConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/config/GeminiConfig.cpp

    # ---- domain ----
    ${CMAKE_SOURCE_DIR}/src/domain/command/MacroCommand.cpp

    # ---- infrastructure ----
    ${CMAKE_SOURCE_DIR}/src/infrastructure/http/HttpProbe.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/llama/ModelRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/llama/LlamaServerProcessController.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/llama/LlamaCppProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/api_providers/DeepSeekProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/api_providers/GeminiProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/ProcessExecutor.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/MemoryInfo.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/SystemMaintenanceFacade.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/system/DiffEngine.cpp

    # ---- ui ----
    ${CMAKE_SOURCE_DIR}/src/ui/ChatView.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/MainWindow.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/MaintenancePanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LlamaPanel.cpp
)

if(FRANS_CODE_INTEL_ENABLED)
    list(APPEND FRANS_AI_SOURCES
        # ---- agent ----
        ${CMAKE_SOURCE_DIR}/src/application/agent/PlanState.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/GenerateState.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/ExecuteState.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/ObserveState.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/FixState.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/Agent.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/MultiFileParser.cpp
        ${CMAKE_SOURCE_DIR}/src/application/agent/TargetScopeResolver.cpp
        ${CMAKE_SOURCE_DIR}/src/application/project/ProjectGraphCache.cpp
        ${CMAKE_SOURCE_DIR}/src/application/safety/DangerCheckHandler.cpp
        ${CMAKE_SOURCE_DIR}/src/application/task_runner/TaskRunner.cpp

        # ---- manual import (application) ----
        ${CMAKE_SOURCE_DIR}/src/application/manual_import/ManualImporter.cpp

        # ---- agent UI ----
        ${CMAKE_SOURCE_DIR}/src/ui/AgentPanel.cpp

        # ---- manual import UI ----
        ${CMAKE_SOURCE_DIR}/src/ui/ManualImportDialog.cpp
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
        frans_build_runner
    )
    target_compile_definitions(frans_ai PRIVATE FRANS_HAS_AGENT_PANEL=1)
endif()