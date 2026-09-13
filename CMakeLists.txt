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
# Stage 5 — Code Intelligence Layer (tree-sitter, opsional)
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
    # Suppress warnings dari third-party code (jangan diubah).
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

    # ---- frans_code_intel (library statis berisi kode kita) ----
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

    # ---- Smoke test executable (Stage 5 verification) ----
    add_executable(frans_graph_test tests/graph_smoke_test.cpp)
    target_link_libraries(frans_graph_test PRIVATE frans_code_intel)
    target_compile_definitions(frans_graph_test PRIVATE
        FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/python"
    )

    set(FRANS_CODE_INTEL_ENABLED ON)
else()
    message(WARNING
        "tree-sitter submodules tidak ditemukan.\n"
        "  Jalankan:\n"
        "    cd third_party && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter.git && \\\n"
        "    git clone --depth=1 https://github.com/tree-sitter/tree-sitter-python.git\n"
        "  Skip frans_code_intel + frans_graph_test.")
    set(FRANS_CODE_INTEL_ENABLED OFF)
endif()

# ===========================================================================
# Main executable (UI)
#
# Setelah refactor main.cpp:
#   - main.cpp          : bootstrap tipis (load config + run FLTK)
#   - AppContext.h      : state/services non-UI (header-only)
#   - Worker.{h,cpp}    : background task runner
#   - MainWindow.{h,cpp}: layout + timer + wiring
#   - MaintenancePanel  : RAM/Swap + tombol maintenance
#   - LlamaPanel        : pilih model + start/stop + status bar
#   - ChatView.{h,cpp}  : streaming chat (tidak berubah)
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
)

target_compile_definitions(frans_ai PRIVATE
    CPPHTTPLIB_THREAD_POOL_COUNT=2
)

target_compile_options(frans_ai PRIVATE -Wall -Wextra -Wpedantic)

# Stage 6 akan menautkan frans_code_intel ke frans_ai. Stage 5 cukup
# disiapkan sebagai library terpisah supaya UI build tetap tidak
# terpengaruh kalau tree-sitter belum di-clone.
if(FRANS_CODE_INTEL_ENABLED)
    target_link_libraries(frans_ai PRIVATE frans_code_intel)
endif()
