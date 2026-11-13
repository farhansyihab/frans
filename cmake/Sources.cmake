include_guard(GLOBAL)

set(FRANS_SOURCES
  src/main.cpp

  # UI
  src/ui/MainWindow.cpp
  src/ui/ManualImportDialog.cpp
  src/ui/PromptDialog.cpp
  src/ui/IngestDialog.cpp

  # Application -- import
  src/application/import/MultiFileParser.cpp

  # Application -- manual_import
  src/application/manual_import/ManualImporter.cpp

  # Application -- safety
  src/application/safety/DangerCheckHandler.cpp

  # Application -- prompt (Fase A #41)
  src/application/prompt/PromptGenerator.cpp

  # Application -- ai (Fase C #54)
  src/application/ai/ApiKeyProvider.cpp

  # Application -- build_runner
  src/application/build_runner/BuildRunner.cpp
  src/application/build_runner/ProjectDetector.cpp
  # Fase A #44: ringkasan error untuk SessionState (murni, tanpa FLTK)
  src/application/build_runner/BuildErrorSummary.cpp
  src/application/build_runner/BuildSystems/CMakeBuildSystem.cpp
  src/application/build_runner/BuildSystems/MakeBuildSystem.cpp
  src/application/build_runner/BuildSystems/MesonBuildSystem.cpp
  src/application/build_runner/BuildSystems/NpmBuildSystem.cpp
  src/application/build_runner/BuildSystems/PoetryBuildSystem.cpp
  src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp

  # Application -- ingest (Fase B #46, #48, #50)
  src/application/ingest/FileCollector.cpp
  src/application/ingest/MarkdownRenderer.cpp
  src/application/ingest/RepoIngester.cpp

  # Infrastructure
  src/infrastructure/fs/SandboxedFileSystem.cpp
  src/infrastructure/system/DiffEngine.cpp

  # Config
  src/config/AppConfig.cpp
  src/config/AppPaths.cpp
)

if(WIN32)
  list(APPEND FRANS_SOURCES
    src/infrastructure/system/ProcessExecutor_win32.cpp)
else()
  list(APPEND FRANS_SOURCES
    src/infrastructure/system/ProcessExecutor_posix.cpp)
endif()

add_executable(frans ${FRANS_SOURCES})

target_include_directories(frans PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(frans PRIVATE
  FLTK::FLTK
  Threads::Threads
  nlohmann_json::nlohmann_json
  ${FRANS_PLATFORM_LIBS}
  frans_warnings
)

if(FRANS_PLATFORM_DEFS)
  target_compile_definitions(frans PRIVATE ${FRANS_PLATFORM_DEFS})
endif()

if(WIN32)
  set_target_properties(frans PROPERTIES WIN32_EXECUTABLE TRUE)
endif()