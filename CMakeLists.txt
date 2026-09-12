# ~/AgenAI/frans/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(frans_ai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Dependencies ----
find_package(FLTK REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# ---- Target ----
add_executable(frans_ai
    src/main.cpp
    src/config/AppConfig.cpp
    src/config/LlamaServerConfig.cpp
    src/domain/command/MacroCommand.cpp
    src/application/resource_guard/ResourceGuard.cpp
    src/application/resource_guard/IdleUnloadTimer.cpp
    src/infrastructure/http/HttpProbe.cpp
    src/infrastructure/llama/ModelRegistry.cpp
    src/infrastructure/llama/LlamaServerProcessController.cpp
    src/infrastructure/system/ProcessExecutor.cpp
    src/infrastructure/system/MemoryInfo.cpp
    src/infrastructure/system/SystemMaintenanceFacade.cpp
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
