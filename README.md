frans/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── .gitignore
│
├── cmake/
│   ├── ProjectOptions.cmake
│   ├── CompilerWarnings.cmake
│   ├── Dependencies.cmake
│   ├── FLTK.cmake
│   ├── Platform.cmake
│   ├── PlatformWindows.cmake
│   ├── PlatformLinux.cmake
│   ├── Sources.cmake
│   ├── Tests.cmake
│   └── Install.cmake
│
├── config/
│   └── app.json
│
├── src/
│   ├── main.cpp
│   │
│   ├── ui/
│   │   ├── MainWindow.cpp
│   │   ├── MainWindow.h
│   │   ├── ManualImportDialog.cpp
│   │   └── ManualImportDialog.h
│   │
│   ├── application/
│   │   ├── import/
│   │   │   ├── MultiFileParser.cpp
│   │   │   └── MultiFileParser.h
│   │   │
│   │   ├── manual_import/
│   │   │   ├── ManualImporter.cpp
│   │   │   └── ManualImporter.h
│   │   │
│   │   ├── safety/
│   │   │   ├── DangerCheckHandler.cpp
│   │   │   └── DangerCheckHandler.h
│   │   │
│   │   └── build_runner/
│   │       ├── BuildRunner.cpp
│   │       ├── BuildRunner.h
│   │       ├── ProjectDetector.cpp
│   │       ├── ProjectDetector.h
│   │       ├── BuildReport.h
│   │       ├── ProjectInfo.h
│   │       ├── IBuildSystem.h
│   │       └── BuildSystems/
│   │           ├── CMakeBuildSystem.cpp
│   │           ├── CMakeBuildSystem.h
│   │           ├── MakeBuildSystem.cpp
│   │           ├── MakeBuildSystem.h
│   │           ├── MesonBuildSystem.cpp
│   │           ├── MesonBuildSystem.h
│   │           ├── NpmBuildSystem.cpp
│   │           ├── NpmBuildSystem.h
│   │           ├── PoetryBuildSystem.cpp
│   │           ├── PoetryBuildSystem.h
│   │           ├── StandaloneBuildSystem.cpp
│   │           └── StandaloneBuildSystem.h
│   │
│   ├── domain/
│   │   ├── fs/
│   │   │   └── IFileSystemPort.h
│   │   └── import/
│   │       ├── GeneratedFile.h
│   │       └── ImportResult.h
│   │
│   ├── infrastructure/
│   │   ├── fs/
│   │   │   ├── SandboxedFileSystem.cpp
│   │   │   └── SandboxedFileSystem.h
│   │   └── system/
│   │       ├── ProcessExecutor.cpp
│   │       ├── ProcessExecutor.h
│   │       ├── ProcessExecutor_posix.cpp
│   │       ├── ProcessExecutor_win32.cpp
│   │       ├── DiffEngine.cpp
│   │       └── DiffEngine.h
│   │
│   └── config/
│       ├── AppConfig.cpp
│       └── AppConfig.h
│
└── tests/
    ├── test_multifile_parser.cpp
    ├── test_manual_importer.cpp
    ├── test_sandbox.cpp
    ├── test_danger_check.cpp
    ├── test_project_detector.cpp
    ├── test_build_runner.cpp
    ├── test_process_executor.cpp
    └── test_app_config.cpp
