# SourceCode.md: frans
Dibuat oleh Frans | 2026-11-15 08:22 | 16 file disertakan | 75.5 KB
Permintaan: - baca `BluePrint-v9-Addendum.md` dan `SourceCode.md`
- `SourceCode.md` adalah code yang paling terakhir, paling update
- PERHATIKAN root app adalah `/home/farhan/Proyek/AgenAI/proyek/frans/`
- buatka
Tanda "*" pada struktur di bawah = isi file disertakan setelah bagian ini.

Directory structure:
└── frans/
    ├── .gitignore
    ├── CMakeLists.txt *
    ├── CMakePresets.json
    ├── README.md
    ├── cmake/
    │   ├── CompilerWarnings.cmake *
    │   ├── Dependencies.cmake *
    │   ├── FLTK.cmake *
    │   ├── GeminiTests.cmake *
    │   ├── Install.cmake *
    │   ├── NlohmannJson.cmake *
    │   ├── Platform.cmake *
    │   ├── PlatformLinux.cmake *
    │   ├── PlatformWindows.cmake *
    │   ├── ProjectOptions.cmake *
    │   ├── Sources.cmake *
    │   └── Tests.cmake *
    ├── config/
    │   ├── app.json
    │   ├── gemini_selector_prompt.txt
    │   └── prompt_template.txt
    ├── include/
    │   └── frans/
    │       └── gemini/
    │           └── ApiKeyProvider.h
    ├── packaging/
    │   └── linux/
    │       ├── frans.desktop.in
    │       └── frans.svg
    ├── prompt_template.txt
    ├── scripts/
    │   ├── README.md
    │   ├── create-shortcut.bat
    │   ├── create-shortcut.ps1
    │   └── install-desktop-user.sh
    ├── src/
    │   ├── application/
    │   │   ├── ai/
    │   │   │   ├── ApiKeyProvider.cpp
    │   │   │   ├── ApiKeyProvider.h
    │   │   │   ├── GeminiClient.cpp
    │   │   │   ├── GeminiClient.h
    │   │   │   ├── GeminiSchema.cpp
    │   │   │   └── GeminiSchema.h
    │   │   ├── build_runner/
    │   │   │   ├── BuildErrorSummary.cpp
    │   │   │   ├── BuildErrorSummary.h
    │   │   │   ├── BuildReport.h
    │   │   │   ├── BuildRunner.cpp
    │   │   │   ├── BuildRunner.h
    │   │   │   ├── BuildSystems/
    │   │   │   │   ├── CMakeBuildSystem.cpp
    │   │   │   │   ├── CMakeBuildSystem.h
    │   │   │   │   ├── MakeBuildSystem.cpp
    │   │   │   │   ├── MakeBuildSystem.h
    │   │   │   │   ├── MesonBuildSystem.cpp
    │   │   │   │   ├── MesonBuildSystem.h
    │   │   │   │   ├── NpmBuildSystem.cpp
    │   │   │   │   ├── NpmBuildSystem.h
    │   │   │   │   ├── PoetryBuildSystem.cpp
    │   │   │   │   ├── PoetryBuildSystem.h
    │   │   │   │   ├── StandaloneBuildSystem.cpp
    │   │   │   │   └── StandaloneBuildSystem.h
    │   │   │   ├── IBuildSystem.h
    │   │   │   ├── ProjectDetector.cpp
    │   │   │   ├── ProjectDetector.h
    │   │   │   └── ProjectInfo.h
    │   │   ├── context/
    │   │   │   ├── ContextSelector.cpp
    │   │   │   ├── ContextSelector.h
    │   │   │   ├── SourceCodeBuilder.cpp
    │   │   │   └── SourceCodeBuilder.h
    │   │   ├── git/
    │   │   │   ├── GitFacade.cpp
    │   │   │   └── GitFacade.h
    │   │   ├── import/
    │   │   │   ├── MultiFileParser.cpp
    │   │   │   └── MultiFileParser.h
    │   │   ├── ingest/
    │   │   │   ├── FileCollector.cpp
    │   │   │   ├── FileCollector.h
    │   │   │   ├── MarkdownRenderer.cpp
    │   │   │   ├── MarkdownRenderer.h
    │   │   │   ├── RepoIngester.cpp
    │   │   │   └── RepoIngester.h
    │   │   ├── manual_import/
    │   │   │   ├── ManualImporter.cpp
    │   │   │   └── ManualImporter.h
    │   │   ├── prompt/
    │   │   │   ├── PromptGenerator.cpp
    │   │   │   └── PromptGenerator.h
    │   │   └── safety/
    │   │       ├── DangerCheckHandler.cpp *
    │   │       └── DangerCheckHandler.h *
    │   ├── config/
    │   │   ├── AppConfig.cpp
    │   │   ├── AppConfig.h
    │   │   ├── AppPaths.cpp
    │   │   └── AppPaths.h
    │   ├── domain/
    │   │   ├── fs/
    │   │   │   └── IFileSystemPort.h
    │   │   └── import/
    │   │       ├── GeneratedFile.h
    │   │       └── ImportResult.h
    │   ├── infrastructure/
    │   │   ├── fs/
    │   │   │   ├── SandboxedFileSystem.cpp
    │   │   │   └── SandboxedFileSystem.h
    │   │   ├── http/
    │   │   │   ├── CurlTransport.cpp
    │   │   │   ├── CurlTransport.h
    │   │   │   └── IHttpTransport.h
    │   │   └── system/
    │   │       ├── DiffEngine.cpp
    │   │       ├── DiffEngine.h
    │   │       ├── ProcessExecutor.cpp
    │   │       ├── ProcessExecutor.h
    │   │       ├── ProcessExecutor_posix.cpp
    │   │       ├── ProcessExecutor_win32.cpp
    │   │       └── SystemMaintenanceFacade.h
    │   ├── ingest/
    │   │   ├── IngestDialog.cpp
    │   │   ├── IngestDialog.h
    │   │   ├── RepoIngester.cpp
    │   │   └── RepoIngester.h
    │   ├── main.cpp
    │   └── ui/
    │       ├── IngestDialog.cpp
    │       ├── IngestDialog.h
    │       ├── MainWindow.cpp
    │       ├── MainWindow.h
    │       ├── ManualImportDialog.cpp
    │       ├── ManualImportDialog.h
    │       ├── PromptDialog.cpp
    │       ├── PromptDialog.h
    │       └── SessionState.h
    ├── test_env_extra.cpp
    ├── tests/
    │   ├── ApiKeyProviderTest.cpp
    │   ├── GeminiSchemaTest.cpp
    │   ├── README-FaseA-44.md
    │   ├── README.md
    │   ├── ScenarioTest.cpp *
    │   ├── SessionStateTest.cpp
    │   ├── ai/
    │   │   ├── FakeHttpTransport.h
    │   │   └── gemini_client_tests.cpp
    │   ├── context/
    │   │   └── source_code_builder_tests.cpp
    │   ├── git/
    │   │   └── GitFacadeTest.cpp
    │   └── manual/
    │       ├── fixtures-generated/
    │       │   ├── 01-cmake-fail.md
    │       │   ├── 02-cmake-fix.md
    │       │   ├── 03-python-fail.md
    │       │   ├── 04-sleeper.md
    │       │   └── 05-python-ok.md
    │       └── make-fasea44-fixtures.sh
    └── ~/
        └── frans-out/
            ├── SourceCode.md
            └── SourceCode.md.manifest.json

================================================
FILE: CMakeLists.txt
================================================
# Root CMakeLists.txt — tipis.
# Detail dipecah ke cmake/*.cmake yang di-include berurutan.
cmake_minimum_required(VERSION 3.16)
project(frans VERSION 1.0.0 LANGUAGES CXX)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Platform.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Sources.cmake)
include(cmake/Tests.cmake)
include(cmake/Install.cmake)
include(cmake/GeminiTests.cmake)
================================================
FILE: cmake/CompilerWarnings.cmake
================================================
include_guard(GLOBAL)

add_library(frans_warnings INTERFACE)

if(MSVC)
    target_compile_options(frans_warnings INTERFACE
        /W4
        /permissive-
    )
else()
    target_compile_options(frans_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
endif()
================================================
FILE: cmake/Dependencies.cmake
================================================
include_guard(GLOBAL)
include(cmake/FLTK.cmake)
include(cmake/NlohmannJson.cmake)
find_package(Threads REQUIRED)
================================================
FILE: cmake/FLTK.cmake
================================================
include_guard(GLOBAL)

find_package(FLTK REQUIRED)

if(NOT TARGET FLTK::FLTK)
    add_library(FLTK::FLTK INTERFACE IMPORTED)
    target_include_directories(FLTK::FLTK INTERFACE ${FLTK_INCLUDE_DIRS})
    target_link_libraries(FLTK::FLTK INTERFACE ${FLTK_LIBRARIES})
endif()
================================================
FILE: cmake/GeminiTests.cmake
================================================
# Fase C #56 -- target test offline untuk GeminiClient (transport palsu).
# Aktifkan dengan menambahkan satu baris di CMakeLists.txt root:
#   include(cmake/GeminiTests.cmake)

find_package(Threads REQUIRED)

# --- Cari nlohmann/json.hpp secara mandiri ---
#
# Urutan prioritas:
#   1. Override manual:  -DFRANS_GEMINI_JSON_INCLUDE_DIR=/path/ke/include-root
#   2. Target CMake dari NlohmannJson.cmake (nlohmann_json::nlohmann_json)
#   3. Pencarian rekursif di source tree + build tree (struktur folder bebas)
#   4. Lokasi sistem (/usr/include, /usr/local/include)

set(_frans_json_include_dirs "")
set(_frans_json_found "")

# (2) Jika NlohmannJson.cmake membuat target, ambil include dir-nya
foreach(_tgt nlohmann_json::nlohmann_json nlohmann_json)
  if(TARGET ${_tgt})
    get_target_property(_tgt_inc ${_tgt} INTERFACE_INCLUDE_DIRECTORIES)
    if(_tgt_inc)
      foreach(_i IN LISTS _tgt_inc)
        # Abaikan generator expression ($<BUILD_INTERFACE:...>) yang tak bisa di-resolve di sini
        if(NOT _i MATCHES "^\\$<" AND EXISTS "${_i}/nlohmann/json.hpp")
          list(APPEND _frans_json_include_dirs "${_i}")
        endif()
      endforeach()
    endif()
  endif()
endforeach()

# (3) Pencarian rekursif: cari SEMUA json.hpp lalu saring yang berada di folder "nlohmann"
file(GLOB_RECURSE _frans_all_json_hpp
  "${CMAKE_SOURCE_DIR}/json.hpp"
  "${CMAKE_BINARY_DIR}/json.hpp"
)
# GLOB_RECURSE hanya mencocokkan nama file di semua subfolder, jadi pola di atas
# menjangkau seluruh pohon. Saring hanya yang berada di .../nlohmann/json.hpp
foreach(_h IN LISTS _frans_all_json_hpp)
  if(_h MATCHES "/nlohmann/json\\.hpp$")
    list(APPEND _frans_json_found "${_h}")
    get_filename_component(_d "${_h}" DIRECTORY)   # .../nlohmann
    get_filename_component(_d "${_d}" DIRECTORY)   # include root
    list(APPEND _frans_json_include_dirs "${_d}")
  endif()
endforeach()

if(_frans_json_include_dirs)
  list(REMOVE_DUPLICATES _frans_json_include_dirs)
endif()

# (1) dan (4): find_path menghormati cache variable FRANS_GEMINI_JSON_INCLUDE_DIR
# jika sudah di-set manual lewat -D.
find_path(FRANS_GEMINI_JSON_INCLUDE_DIR
  NAMES nlohmann/json.hpp
  HINTS ${_frans_json_include_dirs}
  PATHS /usr/include /usr/local/include
)

if(NOT FRANS_GEMINI_JSON_INCLUDE_DIR)
  message(STATUS "[frans] Kandidat json.hpp yang ditemukan:")
  foreach(_h IN LISTS _frans_json_found)
    message(STATUS "  ${_h}")
  endforeach()

  message(FATAL_ERROR
    "[frans] nlohmann/json.hpp tidak ditemukan untuk frans_gemini_tests\n"
    "Hints yang dicoba: ${_frans_json_include_dirs}\n"
    "Solusi: letakkan json.hpp di <folder>/nlohmann/json.hpp di dalam proyek, atau jalankan:\n"
    "  cmake -S . -B build -DFRANS_GEMINI_JSON_INCLUDE_DIR=/path/ke/folder-induk-nlohmann")
endif()

message(STATUS "[frans] frans_gemini_tests: json.hpp di ${FRANS_GEMINI_JSON_INCLUDE_DIR}")

add_executable(frans_gemini_tests
  tests/ai/gemini_client_tests.cpp
  src/application/ai/GeminiClient.cpp
)

target_include_directories(frans_gemini_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}"
  "${FRANS_GEMINI_JSON_INCLUDE_DIR}"
)

target_compile_features(frans_gemini_tests PRIVATE cxx_std_17)
target_link_libraries(frans_gemini_tests PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(frans_gemini_tests PRIVATE /W4)
else()
  target_compile_options(frans_gemini_tests PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME gemini_client COMMAND frans_gemini_tests)
set_tests_properties(gemini_client PROPERTIES TIMEOUT 60)

message(STATUS "[frans] frans_gemini_tests aktif")
================================================
FILE: cmake/Install.cmake
================================================
include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Binary + config inti.
# ---------------------------------------------------------------------------

install(TARGETS frans RUNTIME DESTINATION bin)
install(FILES config/app.json DESTINATION share/frans)

# ---------------------------------------------------------------------------
# Linux: .desktop + icon supaya muncul di menu aplikasi.
#
# CATATAN: `Exec=` di-resolve pada CONFIGURE time memakai
# ${CMAKE_INSTALL_PREFIX}. Kalau install pakai prefix lain, set saat
# configure:
#     cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/opt/frans
# Bukan lewat `cmake --install --prefix` (itu akan bikin Exec salah).
#
# Kalau user pakai install user-level (tanpa sudo), pakai script
# `scripts/install-desktop-user.sh` — script itu menulis Exec dengan
# path absolut binary hasil build.
# ---------------------------------------------------------------------------

if(UNIX AND NOT APPLE)
    set(FRANS_INSTALL_EXEC "${CMAKE_INSTALL_PREFIX}/bin/frans")

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/frans.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/frans.desktop"
        @ONLY)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/frans.desktop"
            DESTINATION share/applications)

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/frans.svg"
            DESTINATION share/icons/hicolor/scalable/apps
            RENAME frans.svg)
endif()

# ---------------------------------------------------------------------------
# Windows: tidak ada installer / shortcut otomatis.
# User jalankan `scripts/create-shortcut.ps1` manual setelah build.
# Alasan: personal use (§9.5), git clone + build workflow.
# ---------------------------------------------------------------------------
================================================
FILE: cmake/NlohmannJson.cmake
================================================
# cmake/NlohmannJson.cmake
#
# Fase C #53 -- vendor nlohmann/json (single header).
#
# Pola disamakan dengan cmake/FLTK.cmake:
#   - Buat INTERFACE IMPORTED target `nlohmann_json::nlohmann_json`.
#   - Include directory = <repo>/third_party (bukan third_party/nlohmann),
#     supaya kode bisa tulis `#include <nlohmann/json.hpp>`.
#   - Warning saat configure kalau file belum di-download.
#   - Tidak ada dependensi sistem, jalan offline setelah file ada.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Lokasi vendor header.
# ---------------------------------------------------------------------------
set(NLOHMANN_JSON_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(NLOHMANN_JSON_HEADER     "${NLOHMANN_JSON_VENDOR_DIR}/nlohmann/json.hpp")

# ---------------------------------------------------------------------------
# Cek keberadaan file. Warning (bukan error) saat configure supaya
# CMakePresets / IDE tetap bisa load project. Compile akan gagal lewat
# #error di placeholder kalau file belum di-download.
# ---------------------------------------------------------------------------
if(NOT EXISTS "${NLOHMANN_JSON_HEADER}")
    message(WARNING
        "[frans] nlohmann/json.hpp TIDAK DITEMUKAN di:\n"
        "    ${NLOHMANN_JSON_HEADER}\n"
        "    Build akan GAGAL sampai file di-download.\n"
        "    Lihat: third_party/README.md\n"
        "    Quick fix:\n"
        "      curl -L -o third_party/nlohmann/json.hpp \\\n"
        "        https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp")
else()
    # Cek ukuran kasar -- placeholder < 5 KB, file asli ~800 KB.
    file(SIZE "${NLOHMANN_JSON_HEADER}" _nj_size)
    if(_nj_size LESS 10000)
        message(WARNING
            "[frans] nlohmann/json.hpp terdeteksi sebagai PLACEHOLDER "
            "(${_nj_size} bytes). File asli ~800 KB.\n"
            "    Download versi asli dari: "
            "https://github.com/nlohmann/json/releases")
    else()
        message(STATUS
            "[frans] nlohmann/json.hpp OK (${_nj_size} bytes)")
    endif()
    unset(_nj_size)
endif()

# ---------------------------------------------------------------------------
# Definisikan target INTERFACE IMPORTED.
# Idempoten: kalau target sudah ada (mis. dari parent scope), skip.
# ---------------------------------------------------------------------------
if(NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)

    # Include path = third_party/ (bukan third_party/nlohmann/),
    # supaya #include <nlohmann/json.hpp> bekerja.
    target_include_directories(nlohmann_json::nlohmann_json
        INTERFACE
            "${NLOHMANN_JSON_VENDOR_DIR}"
    )

    # nlohmann/json butuh C++17. Proyek ini sudah set C++20 di
    # ProjectOptions.cmake, tapi kita tegaskan di target supaya
    # kalau nanti di- reuse di tempat lain, requirement-nya eksplisit.
    target_compile_features(nlohmann_json::nlohmann_json
        INTERFACE
            cxx_std_17
    )
endif()
================================================
FILE: cmake/Platform.cmake
================================================
include_guard(GLOBAL)

if(WIN32)
    include(cmake/PlatformWindows.cmake)
else()
    include(cmake/PlatformLinux.cmake)
endif()
================================================
FILE: cmake/PlatformLinux.cmake
================================================
include_guard(GLOBAL)

set(FRANS_PLATFORM_LIBS pthread)
set(FRANS_PLATFORM_DEFS)
================================================
FILE: cmake/PlatformWindows.cmake
================================================
include_guard(GLOBAL)

set(FRANS_PLATFORM_LIBS ws2_32)
set(FRANS_PLATFORM_DEFS
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN)
================================================
FILE: cmake/ProjectOptions.cmake
================================================
include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(FRANS_VERSION="${PROJECT_VERSION}")
================================================
FILE: cmake/Sources.cmake
================================================
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

    # Application -- ai (Fase C #54, #56, #57, #58)
    src/application/ai/ApiKeyProvider.cpp
    src/application/ai/GeminiClient.cpp
    src/application/ai/GeminiSchema.cpp

    # Application -- context (Fase C #59, #60)
    src/application/context/ContextSelector.cpp
    src/application/context/SourceCodeBuilder.cpp

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

    # Application -- git (Fase D #67)
    src/application/git/GitFacade.cpp

    # Infrastructure
    src/infrastructure/fs/SandboxedFileSystem.cpp
    src/infrastructure/system/DiffEngine.cpp

    # Infrastructure -- http (Fase C #55)
    src/infrastructure/http/CurlTransport.cpp

    # Config
    src/config/AppConfig.cpp
    src/config/AppPaths.cpp
)

if(WIN32)
    list(APPEND FRANS_SOURCES
        src/infrastructure/system/ProcessExecutor_win32.cpp
    )
else()
    list(APPEND FRANS_SOURCES
        src/infrastructure/system/ProcessExecutor_posix.cpp
    )
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
================================================
FILE: cmake/Tests.cmake
================================================
include_guard(GLOBAL)
option(FRANS_BUILD_TESTS "Build tests" OFF)
if(FRANS_BUILD_TESTS)
enable_testing()
#------------------------------------------------------------------
 # frans_scenarios
 # Runner test scenario untuk Manual Import + Danger Check + Sandbox
 # + error parser (traceback frame picking) + PromptGenerator (#42)
 # + FileCollector (#46) + MarkdownRenderer (#48).
 #
 # frans_scenarios                -> jalankan semua scenario
 # frans_scenarios --list         -> daftar nama scenario
 # frans_scenarios --dump NAME    -> print scenario (paste ke GUI)
 #------------------------------------------------------------------
 set(FRANS_SCENARIO_SOURCES
     tests/ScenarioTest.cpp
     src/application/import/MultiFileParser.cpp
     src/application/safety/DangerCheckHandler.cpp
     src/infrastructure/fs/SandboxedFileSystem.cpp
     src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
     src/application/prompt/PromptGenerator.cpp
     # Fase B #46: FileCollector + ProcessExecutor (untuk git ls-files fallback)
     src/application/ingest/FileCollector.cpp
     # Fase B #48: MarkdownRenderer (murni, tanpa I/O)
     src/application/ingest/MarkdownRenderer.cpp
 )
 if(WIN32)
     list(APPEND FRANS_SCENARIO_SOURCES
         src/infrastructure/system/ProcessExecutor_win32.cpp)
 else()
     list(APPEND FRANS_SCENARIO_SOURCES
         src/infrastructure/system/ProcessExecutor_posix.cpp)
 endif()
 add_executable(frans_scenarios ${FRANS_SCENARIO_SOURCES})
 target_include_directories(frans_scenarios PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
 )
 target_link_libraries(frans_scenarios PRIVATE
     Threads::Threads
     ${FRANS_PLATFORM_LIBS}
     frans_warnings
 )
 if(FRANS_PLATFORM_DEFS)
     target_compile_definitions(frans_scenarios PRIVATE ${FRANS_PLATFORM_DEFS})
 endif()
 add_test(NAME scenario_tests COMMAND frans_scenarios)
 #------------------------------------------------------------------
 # frans_session_tests  (Fase A #44 + #45)
 # Test ringan, TANPA FLTK dan TANPA ProsesExecutor:
 # - summarizeBuildError()  : isi ringkasan error per phase
 # - wasCancelledByUser()   : deteksi cancel
 # - SessionState           : aturan publish / clear / skip-cancel
 # - integrasi ke PromptGenerator (blok ERROR)
 #
 # Hanya 3 file .cpp -> compile cepat di mesin lama.
 #------------------------------------------------------------------
 add_executable(frans_session_tests
     tests/SessionStateTest.cpp
     src/application/build_runner/BuildErrorSummary.cpp
     src/application/prompt/PromptGenerator.cpp
 )
 target_include_directories(frans_session_tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
 )
 target_link_libraries(frans_session_tests PRIVATE
     frans_warnings
 )
 add_test(NAME session_state_tests COMMAND frans_session_tests)
 #------------------------------------------------------------------
 # frans_apikey_tests  (Fase C #54)
 # ApiKeyProvider: env -> file kunci -> default ~/.gemini/api_key,
 # trim/BOM/CRLF, izin file longgar, masking 4 karakter terakhir,
 # dan pastikan diagnostik tidak membocorkan key.
 #
 # Test memakai HOME palsu di direktori temp; file kunci asli pengguna
 # tidak disentuh. Tanpa FLTK, tanpa jaringan, tanpa subprocess.
 #------------------------------------------------------------------
 add_executable(frans_apikey_tests
     tests/ApiKeyProviderTest.cpp
     src/application/ai/ApiKeyProvider.cpp
 )
 target_include_directories(frans_apikey_tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
 )
 target_link_libraries(frans_apikey_tests PRIVATE
     frans_warnings
 )
 add_test(NAME apikey_provider_tests COMMAND frans_apikey_tests)
 #------------------------------------------------------------------
 # frans_gemini_schema_tests  (Fase C #57)
 # GeminiSchema: verifikasi struktur JSON schema untuk response Gemini
 # dan file prompt gemini_selector_prompt.txt ada serta valid.
 #
 # Tanpa FLTK, tanpa jaringan, tanpa subprocess.
 #------------------------------------------------------------------
 add_executable(frans_gemini_schema_tests
     tests/GeminiSchemaTest.cpp
     src/application/ai/GeminiSchema.cpp
 )
 target_include_directories(frans_gemini_schema_tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
     ${CMAKE_CURRENT_SOURCE_DIR}/third_party
 )
 target_link_libraries(frans_gemini_schema_tests PRIVATE
     frans_warnings
 )
 # Copy config files ke build directory agar test bisa akses
 add_custom_command(TARGET frans_gemini_schema_tests POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory
         ${CMAKE_CURRENT_SOURCE_DIR}/config
         ${CMAKE_CURRENT_BINARY_DIR}/config
     COMMENT "Copying config files for gemini schema tests"
 )
 add_test(NAME gemini_schema_tests COMMAND frans_gemini_schema_tests)
 #------------------------------------------------------------------
 # frans_context_tests  (Fase C #60)
 # SourceCodeBuilder:
 # - fnv1a_64_hex         : panjang, lowercase, konsistensi, vektor known
 # - buildManifestJson    : struktur JSON, escape karakter, files kosong
 # - buildSourceCode      : happy path, unselected excluded, content
 #                          identik dengan disk, empty selection,
 #                          error input kosong, cancel flag
 #
 # Tanpa FLTK, tanpa jaringan, tanpa subprocess, tanpa git.
 # Memakai temp directory sebagai fixture (RAII, auto-cleanup).
 #------------------------------------------------------------------
 set(FRANS_CONTEXT_TEST_SOURCES
     tests/context/source_code_builder_tests.cpp
     src/application/context/SourceCodeBuilder.cpp
     src/application/context/ContextSelector.cpp
     src/application/ingest/MarkdownRenderer.cpp
     src/application/ingest/FileCollector.cpp
     src/application/ai/GeminiClient.cpp
     src/application/ai/ApiKeyProvider.cpp
     src/application/ai/GeminiSchema.cpp
     src/infrastructure/fs/SandboxedFileSystem.cpp
     src/infrastructure/http/CurlTransport.cpp
 )
 if(WIN32)
     list(APPEND FRANS_CONTEXT_TEST_SOURCES
         src/infrastructure/system/ProcessExecutor_win32.cpp)
 else()
     list(APPEND FRANS_CONTEXT_TEST_SOURCES
         src/infrastructure/system/ProcessExecutor_posix.cpp)
 endif()
 add_executable(frans_context_tests ${FRANS_CONTEXT_TEST_SOURCES})
 target_include_directories(frans_context_tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
     ${CMAKE_CURRENT_SOURCE_DIR}/third_party
 )
 target_link_libraries(frans_context_tests PRIVATE
     Threads::Threads
     ${FRANS_PLATFORM_LIBS}
     frans_warnings
 )
 if(FRANS_PLATFORM_DEFS)
     target_compile_definitions(frans_context_tests PRIVATE ${FRANS_PLATFORM_DEFS})
 endif()
 add_test(NAME source_code_builder_tests COMMAND frans_context_tests)
 #------------------------------------------------------------------
 # frans_git_facade_tests  (Fase D #67 + #68)
 # GitFacade:
 # - parseStatusPorcelain   : parse output git status --porcelain=v1 -b
 # - suggestCommitMessage   : generate commit message suggestion
 # - status/addAll/commit/push : operasi git via ProcessExecutor
 #
 # Tanpa FLTK, tanpa jaringan, tanpa git binary (untuk pure tests).
 # Operasi git memerlukan ProcessExecutor (untuk runCommand).
 #------------------------------------------------------------------
 set(FRANS_GIT_FACADE_TEST_SOURCES
     tests/git/GitFacadeTest.cpp
     src/application/git/GitFacade.cpp
 )
 # Fase D #68: GitFacade memanggil runCommand() dari ProcessExecutor,
 # jadi kita perlu link implementasi ProcessExecutor agar tidak error linker.
 if(WIN32)
     list(APPEND FRANS_GIT_FACADE_TEST_SOURCES
         src/infrastructure/system/ProcessExecutor_win32.cpp)
 else()
     list(APPEND FRANS_GIT_FACADE_TEST_SOURCES
         src/infrastructure/system/ProcessExecutor_posix.cpp)
 endif()
 add_executable(frans_git_facade_tests ${FRANS_GIT_FACADE_TEST_SOURCES})
 target_include_directories(frans_git_facade_tests PRIVATE
     ${CMAKE_CURRENT_SOURCE_DIR}/src
 )
 target_link_libraries(frans_git_facade_tests PRIVATE
     Threads::Threads
     ${FRANS_PLATFORM_LIBS}
     frans_warnings
 )
 if(FRANS_PLATFORM_DEFS)
     target_compile_definitions(frans_git_facade_tests PRIVATE ${FRANS_PLATFORM_DEFS})
 endif()
 add_test(NAME git_facade_tests COMMAND frans_git_facade_tests)
endif()
================================================
FILE: src/application/safety/DangerCheckHandler.cpp
================================================
#include "application/safety/DangerCheckHandler.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace frans::application {

namespace {

const std::unordered_map<std::string, DangerVerdict>& pythonPatterns() {
    static const std::unordered_map<std::string, DangerVerdict> kPatterns = {
        // Shell & subprocess
        {"os.system",                DangerVerdict::NeedsConfirm},
        {"os.popen",                 DangerVerdict::NeedsConfirm},
        {"os.spawn",                 DangerVerdict::NeedsConfirm},
        {"os.exec",                  DangerVerdict::NeedsConfirm},
        {"os.fork",                  DangerVerdict::NeedsConfirm},
        {"subprocess.",              DangerVerdict::NeedsConfirm},

        // Dynamic code
        {"eval(",                    DangerVerdict::NeedsConfirm},
        {"exec(",                    DangerVerdict::NeedsConfirm},
        {"__import__(",              DangerVerdict::NeedsConfirm},
        {"pickle.loads",             DangerVerdict::NeedsConfirm},
        {"marshal.loads",            DangerVerdict::NeedsConfirm},

        // Filesystem destruction
        {"shutil.rmtree",            DangerVerdict::NeedsConfirm},
        {"os.removedirs",            DangerVerdict::NeedsConfirm},
        {"os.remove(",               DangerVerdict::NeedsConfirm},
        {"os.unlink(",               DangerVerdict::NeedsConfirm},
        {"os.rmdir(",                DangerVerdict::NeedsConfirm},
        {"os.truncate(",             DangerVerdict::NeedsConfirm},

        // Network
        {"socket.socket",            DangerVerdict::NeedsConfirm},
        {"socket.create_connection", DangerVerdict::NeedsConfirm},
        {"urllib.request",           DangerVerdict::NeedsConfirm},
        {"requests.get",             DangerVerdict::NeedsConfirm},
        {"requests.post",            DangerVerdict::NeedsConfirm},
        {"http.client",              DangerVerdict::NeedsConfirm},
        {"ftplib.",                  DangerVerdict::NeedsConfirm},
        {"smtplib.",                 DangerVerdict::NeedsConfirm},
    };
    return kPatterns;
}

// Path sistem Linux kritis -> Blocked.
const std::vector<std::string>& blockedLinuxPaths() {
    static const std::vector<std::string> kPaths = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/sudoers",
        "/etc/sudoers.d/",
    };
    return kPaths;
}

// Path sistem Windows kritis -> Blocked.
const std::vector<std::string>& blockedWindowsPaths() {
    static const std::vector<std::string> kPaths = {
        "C:/Windows/System32/config/SAM",
        "C:/Windows/System32/config/SECURITY",
        "C:/Windows/System32/config/SYSTEM",
        "C:/Windows/System32/drivers/etc/hosts",
        "AppData/Local/Microsoft/Credentials",
    };
    return kPaths;
}

// Prefix path yang mencurigakan -> NeedsConfirm.
const std::vector<std::string>& suspiciousPathPrefixes() {
    static const std::vector<std::string> kPrefixes = {
        "/etc/",
        "/root/",
        "/boot/",
        "/sys/kernel/",
        "/proc/sys/",
    };
    return kPrefixes;
}

// Base command berbahaya (Linux + Windows).
const std::unordered_set<std::string>& dangerousBaseCommands() {
    static const std::unordered_set<std::string> kCmds = {
        // Linux
        "rm", "dd", "mkfs", "mkfs.ext4", "mkfs.xfs", "mkfs.btrfs",
        "shred", "wipefs", "fdisk", "parted", "mkswap", "swapoff",
        "swapon", "reboot", "shutdown", "poweroff", "halt", "init",
        "chown",
        // Windows
        "format", "diskpart", "reg", "shutdown.exe", "taskkill",
    };
    return kCmds;
}

std::string basename(const std::string& path) {
    const auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string joinPatterns(const std::vector<std::string>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += "'" + v[i] + "'";
    }
    return s;
}

} // namespace

DangerCheckResult DangerCheckHandler::checkCode(
    const std::string& code,
    const std::string& language) const
{
    DangerCheckResult r;

    // Saat ini pattern hanya untuk Python.
    if (language != "python") return r;

    bool any_blocked = false;
    bool any_confirm = false;

    // Linux blocked paths.
    for (const auto& needle : blockedLinuxPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    // Windows blocked paths.
    for (const auto& needle : blockedWindowsPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    // Suspicious path prefixes.
    for (const auto& pfx : suspiciousPathPrefixes()) {
        const std::string dq = "\"" + pfx;
        const std::string sq = "'"  + pfx;
        if (code.find(dq) != std::string::npos) {
            r.matched_patterns.push_back(dq + "...");
            any_confirm = true;
        }
        if (code.find(sq) != std::string::npos) {
            r.matched_patterns.push_back(sq + "...");
            any_confirm = true;
        }
    }

    // Pattern API.
    for (const auto& [needle, verdict] : pythonPatterns()) {
        if (code.find(needle) == std::string::npos) continue;
        r.matched_patterns.push_back(needle);
        if (verdict == DangerVerdict::Blocked)                any_blocked = true;
        else if (verdict == DangerVerdict::NeedsConfirm)      any_confirm = true;
    }

    std::sort(r.matched_patterns.begin(), r.matched_patterns.end());
    r.matched_patterns.erase(
        std::unique(r.matched_patterns.begin(), r.matched_patterns.end()),
        r.matched_patterns.end());

    if (r.matched_patterns.empty()) {
        r.verdict = DangerVerdict::Safe;
        return r;
    }

    if (any_blocked) {
        r.verdict = DangerVerdict::Blocked;
        r.explanation = "Kode mengandung referensi ke path sistem kritis: "
                      + joinPatterns(r.matched_patterns);
    } else if (any_confirm) {
        r.verdict = DangerVerdict::NeedsConfirm;
        r.explanation = "Kode mengandung pola berisiko: "
                      + joinPatterns(r.matched_patterns);
    }
    return r;
}

DangerCheckResult DangerCheckHandler::checkCommand(
    const std::vector<std::string>& argv) const
{
    DangerCheckResult r;
    if (argv.empty()) return r;

    const std::string base = basename(argv[0]);

    if (dangerousBaseCommands().count(base) > 0) {
        r.matched_patterns.push_back("cmd:" + base);
        r.verdict = DangerVerdict::NeedsConfirm;
    }

    for (const auto& a : argv) {
        if ((a == "-rf" || a == "-fr" || a == "--no-preserve-root")
            && base == "rm")
        {
            r.matched_patterns.push_back("flag:" + a);
            r.verdict = DangerVerdict::NeedsConfirm;
        }

        for (const auto& blocked : blockedLinuxPaths()) {
            if (a.find(blocked) != std::string::npos) {
                r.matched_patterns.push_back("arg:" + a);
                r.verdict = DangerVerdict::Blocked;
                r.explanation = "Argumen menyentuh path sistem kritis: '"
                              + a + "'";
                return r;
            }
        }
        for (const auto& blocked : blockedWindowsPaths()) {
            if (a.find(blocked) != std::string::npos) {
                r.matched_patterns.push_back("arg:" + a);
                r.verdict = DangerVerdict::Blocked;
                r.explanation = "Argumen menyentuh path sistem kritis: '"
                              + a + "'";
                return r;
            }
        }
    }

    if (!r.matched_patterns.empty()
        && r.verdict == DangerVerdict::NeedsConfirm)
    {
        r.explanation = "Perintah berisiko: "
                      + joinPatterns(r.matched_patterns);
    }
    return r;
}

} // namespace frans::application
================================================
FILE: src/application/safety/DangerCheckHandler.h
================================================
#pragma once

#include <string>
#include <vector>

namespace frans::application {

enum class DangerVerdict {
    Safe,
    NeedsConfirm,
    Blocked,
};

struct DangerCheckResult {
    DangerVerdict             verdict = DangerVerdict::Safe;
    std::vector<std::string>  matched_patterns;
    std::string               explanation;

    bool is_safe()       const { return verdict == DangerVerdict::Safe; }
    bool needs_confirm() const { return verdict == DangerVerdict::NeedsConfirm; }
    bool is_blocked()    const { return verdict == DangerVerdict::Blocked; }
};

// Deteksi pola berbahaya di source code dan argv command.
//
// Konservatif: pola umum (open, print, import os) TIDAK ditandai.
// Pola berisiko (subprocess, socket) -> NeedsConfirm.
// Path sistem kritis -> Blocked.
//
// Pattern list multi-bahasa: Python + Windows (SAM, hosts, reg, dll).
class DangerCheckHandler {
public:
    DangerCheckResult checkCode(const std::string& code,
                                const std::string& language) const;

    DangerCheckResult checkCommand(const std::vector<std::string>& argv) const;
};

} // namespace frans::application
================================================
FILE: tests/ScenarioTest.cpp
================================================
// tests/ScenarioTest.cpp
//
// Runner test scenario untuk Manual Import + Danger Check + Sandbox
// + error parser (traceback frame picking) + PromptGenerator (Fase A #42)
// + FileCollector (Fase B #46) + MarkdownRenderer (Fase B #48).
//
// Mode:
//   frans_scenarios              -> jalankan semua scenario, print PASS/FAIL
//   frans_scenarios --list       -> daftar nama scenario
//   frans_scenarios --dump NAME  -> print isi scenario (paste ke GUI)
//
// Scenario yang di-cover (sesuai Blueprint §9.2 + §9.3 + Fase B #46/#48):
//   §9.2:
//     01 python-standalone        -> #16
//     02 cmake-multifile          -> regression
//     03 path-escape              -> #18 (parser)
//     04 danger-python            -> #19
//     05 ai-browser-realistic     -> #17
//     06 cancel-sleep             -> #23 (manual via GUI)
//   §9.3:
//     07 npm-basic                -> #22
//     08 npm-typescript           -> #22
//     09 standalone-js            -> #22
//   plus:
//     sandbox-path-safety         -> #18 (sandbox)
//     traceback-frame-picking     -> regression (anomali 1)
//     prompt-generator            -> #42 (Fase A)
//     prompt-format-rules         -> verifikasi aturan format output
//     file-collector-basic        -> #46 (Fase B)
//     markdown-renderer           -> #48 (Fase B)
#include "application/import/MultiFileParser.h"
#include "application/safety/DangerCheckHandler.h"
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/ProjectInfo.h"
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ingest/FileCollector.h"
#include "application/ingest/MarkdownRenderer.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using frans::application::parseModelOutput;
using frans::application::BuildPhase;
using frans::application::BuildSystemKind;
using frans::application::CommandResult;
using frans::application::DangerCheckHandler;
using frans::application::DangerVerdict;
using frans::application::PhaseResult;
using frans::application::ProjectInfo;
using frans::application::StandaloneBuildSystem;
using frans::application::PromptInput;
using frans::application::renderPrompt;
using frans::application::defaultPromptTemplate;
using frans::application::CollectedFile;
using frans::application::CollectOptions;
using frans::application::CollectResult;
using frans::application::collectFiles;
using frans::application::isSecretBasename;
using frans::application::isExcludedPath;
using frans::application::renderTree;
using frans::application::renderFileBlock;
using frans::application::renderMarkdown;
using frans::application::RenderInput;
using frans::application::RenderOptions;
using frans::application::detectLanguageFromExtension;
using frans::application::formatBytesHuman;
using frans::domain::GeneratedFile;

// Hindari literal triple-backtick di source (bisa mengganggu tooling).
// Placeholder "###F###" di-expand menjadi tiga backtick oleh expandFences().
const std::string kFence            = "`" "`" "`";
const std::string kFencePlaceholder = "###F###";

std::string expandFences(const std::string& raw) {
    std::string result;
    result.reserve(raw.size() + 32);
    std::size_t pos = 0;
    while (pos < raw.size()) {
        const std::size_t hit = raw.find(kFencePlaceholder, pos);
        if (hit == std::string::npos) {
            result.append(raw, pos, std::string::npos);
            break;
        }
        result.append(raw, pos, hit - pos);
        result += kFence;
        pos = hit + kFencePlaceholder.size();
    }
    return result;
}

enum class DangerExpect {
    Any,
    Safe,
    NeedsConfirm,
    Blocked,
};

struct FileExpect {
    std::string  relative_path;
    DangerExpect danger = DangerExpect::Any;
};

struct Scenario {
    std::string             name;
    std::string             description;
    std::string             raw_input;          // pakai ###F### untuk fence
    std::size_t             expected_file_count = 0;
    bool                    expect_parse_error  = false;
    std::vector<FileExpect> file_expects;
};

std::vector<Scenario> buildScenarios() {
    std::vector<Scenario> v;

    // ---- 01: Python standalone ----
    {
        Scenario s;
        s.name = "python-standalone";
        s.description = "Skrip Python tanpa marker build system.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: hello.py
###F###python
#!/usr/bin/env python3
"""Contoh skrip Python standalone."""
def greet(name: str) -> str:
    return f"Halo, {name}!"
def main() -> None:
    print(greet("Dunia"))
if __name__ == "__main__":
    main()
###F###
)RAW";
        s.file_expects.push_back({"hello.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 02: CMake multi-file (regression §9.1 #12) ----
    {
        Scenario s;
        s.name = "cmake-multifile";
        s.description = "CMakeLists.txt + src/main.cpp (2 file).";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(File: CMakeLists.txt
###F###cmake
cmake_minimum_required(VERSION 3.16)
project(demo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(demo src/main.cpp)
enable_testing()
add_test(NAME demo_run COMMAND demo)
###F###
File: src/main.cpp
###F###cpp
#include <iostream>
int main() {
    std::cout << "Halo dari demo\n";
    return 0;
}
###F###
)RAW";
        s.file_expects.push_back({"CMakeLists.txt", DangerExpect::Safe});
        s.file_expects.push_back({"src/main.cpp",   DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 03: Path escape (parser accept, sandbox reject) ----
    {
        Scenario s;
        s.name = "path-escape";
        s.description = "File dengan '../' — parser accept, sandbox harus "
                        "BLOCKED (lihat sandbox-path-safety).";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: ../evil.py
###F###python
print("coba keluar sandbox")
###F###
)RAW";
        s.file_expects.push_back({"../evil.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 04: Danger pattern Python ----
    {
        Scenario s;
        s.name = "danger-python";
        s.description = "os.system + shutil.rmtree — harus NeedsConfirm.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: cleanup.py
###F###python
import os
import shutil
def wipe(path):
    os.system("rm -rf " + path)
    shutil.rmtree(path)
if __name__ == "__main__":
    wipe("/tmp/scratch")
###F###
)RAW";
        s.file_expects.push_back({"cleanup.py", DangerExpect::NeedsConfirm});
        v.push_back(std::move(s));
    }

    // ---- 05: Output AI browser realistis ----
    {
        Scenario s;
        s.name = "ai-browser-realistic";
        s.description = "Output dengan prose + multi-file, seperti DeepSeek.";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(Berikut adalah implementasi yang kamu minta.
Saya buat dua file: satu module utility, satu entry point.
File: utils.py
###F###python
"""Utility sederhana."""
def tambah(a, b):
    return a + b
def kurang(a, b):
    return a - b
###F###
File: app.py
###F###python
from utils import tambah, kurang
def main():
    print(tambah(2, 3))
    print(kurang(10, 4))
if __name__ == "__main__":
    main()
###F###
Semoga membantu! Kalau ada error, kabari saya.
)RAW";
        s.file_expects.push_back({"utils.py", DangerExpect::Safe});
        s.file_expects.push_back({"app.py",   DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 06: Cancel test (§9.3 #23) — manual via GUI ----
    {
        Scenario s;
        s.name = "cancel-sleep";
        s.description = "Script Python sleep 60 detik. Untuk test Cancel "
                        "button: paste, Write, Build, klik Cancel dalam "
                        "2 detik, verify status CANCELLED.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: sleeper.py
###F###python
import time
def main():
    print("Mulai tidur 60 detik...", flush=True)
    time.sleep(60)
    print("Selesai tidur", flush=True)
if __name__ == "__main__":
    main()
###F###
)RAW";
        s.file_expects.push_back({"sleeper.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 07: npm basic (§9.3 #22) ----
    {
        Scenario s;
        s.name = "npm-basic";
        s.description = "Project npm sederhana: package.json + src/index.js. "
                        "Uji: npm install -> npm run build -> npm test.";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(File: package.json
###F###json
{
  "name": "demo-npm-basic",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "build": "node --check src/index.js",
    "test": "node --check src/index.js"
  }
}
###F###
File: src/index.js
###F###javascript
function greet(name) {
    return "Halo, " + name + "!";
}
console.log(greet("npm"));
###F###
)RAW";
        s.file_expects.push_back({"package.json", DangerExpect::Safe});
        s.file_expects.push_back({"src/index.js", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 08: npm TypeScript (§9.3 #22) ----
    {
        Scenario s;
        s.name = "npm-typescript";
        s.description = "Project npm dengan TypeScript. Uji: npm install "
                        "(tarik tsc) -> tsc --noEmit via npm run build.";
        s.expected_file_count = 3;
        s.raw_input = R"RAW(File: package.json
###F###json
{
  "name": "demo-npm-typescript",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "build": "tsc --noEmit",
    "test": "tsc --noEmit"
  },
  "devDependencies": {
    "typescript": "^5.4.0"
  }
}
###F###
File: tsconfig.json
###F###json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "strict": true,
    "noEmit": true,
    "esModuleInterop": true,
    "skipLibCheck": true
  },
  "include": ["src/**/*.ts"]
}
###F###
File: src/index.ts
###F###typescript
function greet(name: string): string {
    return "Halo, " + name + "!";
}
const msg: string = greet("TypeScript");
console.log(msg);
###F###
)RAW";
        s.file_expects.push_back({"package.json",  DangerExpect::Safe});
        s.file_expects.push_back({"tsconfig.json", DangerExpect::Safe});
        s.file_expects.push_back({"src/index.ts",  DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 09: Standalone JavaScript (§9.3 #22) ----
    {
        Scenario s;
        s.name = "standalone-js";
        s.description = "JS tanpa package.json. Uji: node --check "
                        "(build) + node hello.js (run).";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: hello.js
###F###javascript
function greet(name) {
    return "Halo, " + name + "!";
}
console.log(greet("Dunia"));
###F###
)RAW";
        s.file_expects.push_back({"hello.js", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    return v;
}

const char* dangerName(DangerVerdict v) {
    switch (v) {
        case DangerVerdict::Safe:         return "Safe";
        case DangerVerdict::NeedsConfirm: return "NeedsConfirm";
        case DangerVerdict::Blocked:      return "Blocked";
    }
    return "?";
}

bool dangerMatches(DangerVerdict actual, DangerExpect expected) {
    if (expected == DangerExpect::Any) return true;
    if (expected == DangerExpect::Safe         && actual == DangerVerdict::Safe)         return true;
    if (expected == DangerExpect::NeedsConfirm && actual == DangerVerdict::NeedsConfirm) return true;
    if (expected == DangerExpect::Blocked      && actual == DangerVerdict::Blocked)      return true;
    return false;
}

std::string languageFromPath(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    const std::string ext = path.substr(dot + 1);
    if (ext == "py")  return "python";
    if (ext == "cpp" || ext == "cc" || ext == "cxx"
        || ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "h") return "cpp";
    if (ext == "ts"  || ext == "tsx") return "typescript";
    if (ext == "js" || ext == "jsx" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "vue") return "vue";
    return {};
}

bool runScenario(const Scenario& s) {
    std::printf("\n=== %s ===\n%s\n",
                s.name.c_str(), s.description.c_str());
    const std::string input = expandFences(s.raw_input);
    const auto parse = parseModelOutput(input, "");
    bool ok = true;

    if (s.expect_parse_error) {
        if (parse.error.empty()) {
            std::printf("  [FAIL] expected parse error, tapi parse sukses\n");
            ok = false;
        } else {
            std::printf("  [ok] parse error seperti diharapkan: %s\n",
                        parse.error.c_str());
        }
    } else if (!parse.error.empty()) {
        std::printf("  [FAIL] parse error: %s\n", parse.error.c_str());
        ok = false;
    }

    if (s.expected_file_count > 0
        && parse.files.size() != s.expected_file_count) {
        std::printf("  [FAIL] expected %zu file, got %zu\n",
                    s.expected_file_count, parse.files.size());
        ok = false;
    } else if (s.expected_file_count > 0) {
        std::printf("  [ok] %zu file di-parse\n", parse.files.size());
    }

    DangerCheckHandler danger;
    for (std::size_t i = 0; i < parse.files.size(); ++i) {
        const auto& f = parse.files[i];
        const std::string lang = languageFromPath(f.relative_path);
        const auto dc = danger.checkCode(f.content, lang);
        std::printf("    file[%zu] %-24s lang=%-10s danger=%s\n",
                    i, f.relative_path.c_str(),
                    lang.empty() ? "?" : lang.c_str(),
                    dangerName(dc.verdict));
        if (i < s.file_expects.size()) {
            const auto& exp = s.file_expects[i];
            if (!exp.relative_path.empty()
                && exp.relative_path != f.relative_path) {
                std::printf("      [FAIL] path expected '%s', got '%s'\n",
                            exp.relative_path.c_str(),
                            f.relative_path.c_str());
                ok = false;
            }
            if (!dangerMatches(dc.verdict, exp.danger)) {
                std::printf("      [FAIL] danger expected != actual\n");
                ok = false;
            }
        }
    }

    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test sandbox (path containment) — terpisah dari scenario ----
bool runSandboxTest() {
    std::printf("\n=== sandbox-path-safety ===\n");
    std::printf("  Cek SandboxedFileSystem::isPathSafe() — path escape "
                "harus ditolak.\n");
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec) {
        std::printf("  [SKIP] temp_directory_path gagal: %s\n",
                    ec.message().c_str());
        return true;
    }
    tmp /= "frans_test_sandbox_v8";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    if (ec) {
        std::printf("  [SKIP] create_directories gagal: %s\n",
                    ec.message().c_str());
        return true;
    }

    frans::infrastructure::SandboxedFileSystem sb(tmp.string());
    int fails = 0;
    auto check = [&](const char* p, bool expected_safe) {
        const bool actual = sb.isPathSafe(p);
        const bool pass = (actual == expected_safe);
        if (!pass) ++fails;
        std::printf("  %s isPathSafe(%-28s) = %s (harus %s)\n",
                    pass ? "[ok]  " : "[FAIL]",
                    p,
                    actual ? "true" : "false",
                    expected_safe ? "true" : "false");
    };

    check("hello.py",          true);
    check("src/main.cpp",      true);
    check("a/b/c/d.py",        true);
    check("../evil.py",        false);
    check("../../etc/passwd",  false);
    check("/etc/passwd",       false);
    check("src/../../evil.py", false);

    fs::remove_all(tmp, ec);
    const bool ok = (fails == 0);
    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test parseOutput: traceback frame picking (anomali 1) ----
bool runTracebackPickingTest() {
    std::printf("\n=== traceback-frame-picking ===\n");
    std::printf("  Cek StandaloneBuildSystem::parseOutput memilih frame "
                "PERTAMA di dalam project_root (file user), bukan frame "
                "stdlib di akhir traceback.\n");

    PhaseResult pr;
    pr.phase   = BuildPhase::Run;
    pr.skipped = false;
    pr.success = false;

    CommandResult cr;
    cr.label     = "run";
    cr.command   = "python3 cleanup.py";
    cr.ran       = true;
    cr.success   = false;
    cr.exit_code = 1;
    cr.stderr_tail =
        "Traceback (most recent call last):\n"
        "  File \"/tmp/frans_pytest/cleanup.py\", line 9, in <module>\n"
        "    wipe(\"/tmp/scratch\")\n"
        "  File \"/tmp/frans_pytest/cleanup.py\", line 6, in wipe\n"
        "    shutil.rmtree(path)\n"
        "  File \"/usr/lib/python3.12/shutil.py\", line 775, in rmtree\n"
        "    onexc(os.lstat, path, err)\n"
        "  File \"/usr/lib/python3.12/shutil.py\", line 773, in rmtree\n"
        "    orig_st = os.lstat(path, dir_fd=dir_fd)\n"
        "FileNotFoundError: [Errno 2] No such file or directory: "
        "'/tmp/scratch'\n";
    pr.commands.push_back(std::move(cr));

    ProjectInfo info;
    info.project_root     = "/tmp/frans_pytest";
    info.sandbox_root     = "/tmp/frans_pytest";
    info.kind             = BuildSystemKind::Standalone;
    info.primary_language = "Python";

    StandaloneBuildSystem sys;
    sys.parseOutput(pr, info);

    bool ok = true;
    if (pr.related_files.empty()) {
        std::printf("  [FAIL] related_files kosong\n");
        ok = false;
    } else {
        const auto& rf = pr.related_files.front();
        std::printf("  frame dipilih: %s:%d\n", rf.path.c_str(), rf.line);
        if (rf.path != "cleanup.py") {
            std::printf("  [FAIL] expected path 'cleanup.py', got '%s'\n",
                        rf.path.c_str());
            ok = false;
        }
        if (rf.line != 9) {
            std::printf("  [FAIL] expected line 9 (frame pertama user), "
                        "got %d\n", rf.line);
            ok = false;
        }
    }

    if (pr.error_message.find("FileNotFoundError") == std::string::npos) {
        std::printf("  [FAIL] error_message tidak mengandung "
                    "'FileNotFoundError': '%s'\n",
                    pr.error_message.c_str());
        ok = false;
    } else {
        std::printf("  error_message: %s\n", pr.error_message.c_str());
    }

    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test PromptGenerator (Fase A #42) ----
bool runPromptGeneratorTest() {
    std::printf("\n=== prompt-generator ===\n");
    std::printf("  Cek PromptGenerator::renderPrompt — placeholder, "
                "blok kondisional, template kosong, placeholder tak "
                "dikenal, blok tanpa penutup, tail 8 KB.\n");

    int fails = 0;
    auto check = [&](const char* name, const std::string& actual,
                     const std::string& expected_substr, bool should_contain) {
        const bool has = actual.find(expected_substr) != std::string::npos;
        const bool pass = (has == should_contain);
        if (!pass) ++fails;
        std::printf("  %s %-40s %s '%s'\n",
                    pass ? "[ok]  " : "[FAIL]",
                    name,
                    should_contain ? "contains" : "NOT contains",
                    expected_substr.c_str());
        if (!pass) {
            std::printf("        actual: '%s'\n", actual.c_str());
        }
    };

    // ---- Test 1: semua blok aktif ----
    {
        PromptInput in;
        in.tmpl = "REQ={{REQUEST}} CTX={{CONTEXT_FILE_NAME}} "
                  "{{#ERROR}}ERR={{ERROR_OUTPUT}}{{/ERROR}} "
                  "{{#CONTEXT}}HAS_CTX{{/CONTEXT}} "
                  "{{#NEWFILES}}NEW={{NEW_FILES_HINT}}{{/NEWFILES}}";
        in.request           = "buat fungsi foo";
        in.error_output      = "Traceback...";
        in.context_file_name = "SourceCode.md";
        in.new_files_hint    = "src/new.py";

        const std::string out = renderPrompt(in);
        check("all-blocks: request",     out, "REQ=buat fungsi foo", true);
        check("all-blocks: ctx name",    out, "CTX=SourceCode.md",   true);
        check("all-blocks: error on",    out, "ERR=Traceback...",    true);
        check("all-blocks: context on",  out, "HAS_CTX",             true);
        check("all-blocks: newfiles on", out, "NEW=src/new.py",      true);
    }

    // ---- Test 2: tanpa error_output -> blok ERROR tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "REQ={{REQUEST}} {{#ERROR}}ERR={{ERROR_OUTPUT}}{{/ERROR}} END";
        in.request      = "foo";
        in.error_output = "";

        const std::string out = renderPrompt(in);
        check("no-error: request", out, "REQ=foo", true);
        check("no-error: no ERR",  out, "ERR=",    false);
        check("no-error: END",     out, "END",     true);
    }

    // ---- Test 3: tanpa context_file_name -> blok CONTEXT tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "{{#CONTEXT}}HAS_CTX{{/CONTEXT}} DONE";
        in.context_file_name = "";

        const std::string out = renderPrompt(in);
        check("no-ctx: no HAS_CTX", out, "HAS_CTX", false);
        check("no-ctx: DONE",       out, "DONE",    true);
    }

    // ---- Test 4: tanpa new_files_hint -> blok NEWFILES tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "{{#NEWFILES}}NEW={{NEW_FILES_HINT}}{{/NEWFILES}} OK";
        in.new_files_hint = "";

        const std::string out = renderPrompt(in);
        check("no-newfiles: no NEW", out, "NEW=", false);
        check("no-newfiles: OK",     out, "OK",   true);
    }

    // ---- Test 5: placeholder tak dikenal -> dibiarkan apa adanya ----
    {
        PromptInput in;
        in.tmpl = "A={{REQUEST}} B={{UNKNOWN}} C={{CONTEXT_FILE_NAME}}";
        in.request           = "req";
        in.context_file_name = "ctx";

        const std::string out = renderPrompt(in);
        check("unknown-ph: A=req",  out, "A=req",         true);
        check("unknown-ph: B kept", out, "B={{UNKNOWN}}", true);
        check("unknown-ph: C=ctx",  out, "C=ctx",         true);
    }

    // ---- Test 6: template kosong -> pakai default ----
    {
        PromptInput in;
        in.tmpl              = "";
        in.request           = "test request";
        in.context_file_name = "SourceCode.md";

        const std::string out = renderPrompt(in);
        check("empty-tmpl: has default marker",
              out, "Full file content, not diff", true);
        check("empty-tmpl: has request",
              out, "test request", true);
        check("empty-tmpl: has context block",
              out, "SourceCode.md", true);
        check("empty-tmpl: no error block (error_output kosong)",
              out, "--- error output ---", false);
    }

    // ---- Test 7: blok tanpa penutup -> dirender apa adanya ----
    {
        PromptInput in;
        in.tmpl         = "BEFORE {{#ERROR}}no-closer END";
        in.error_output = "err";

        const std::string out = renderPrompt(in);
        check("no-closer: BEFORE",    out, "BEFORE",        true);
        check("no-closer: tag kept",  out, "{{#ERROR}}",    true);
        check("no-closer: rest kept", out, "no-closer END", true);
    }

    // ---- Test 8: error_output di-tail +/- 8 KB ----
    {
        PromptInput in;
        in.tmpl = "{{ERROR_OUTPUT}}";
        std::string big(20 * 1024, 'x');
        in.error_output = big;

        const std::string out = renderPrompt(in);
        const bool truncated = out.size() < big.size();
        const bool has_prefix = out.find("truncated") != std::string::npos;
        const bool pass = truncated && has_prefix;
        if (!pass) ++fails;
        std::printf("  %s %-40s truncated=%s has_prefix=%s (out=%zu, in=%zu)\n",
                    pass ? "[ok]  " : "[FAIL]",
                    "error-tail-8kb",
                    truncated  ? "true" : "false",
                    has_prefix ? "true" : "false",
                    out.size(), big.size());
    }

    // ---- Test 9: default template valid ----
    {
        const std::string& d = defaultPromptTemplate();
        const bool not_empty     = !d.empty();
        const bool has_request   = d.find("{{REQUEST}}") != std::string::npos;
        const bool has_ctx_block = d.find("{{#CONTEXT}}") != std::string::npos;
        const bool ok = not_empty && has_request && has_ctx_block;
        if (!ok) ++fails;
        std::printf("  %s %-40s not_empty=%d has_REQUEST=%d has_CTX=%d\n",
                    ok ? "[ok]  " : "[FAIL]",
                    "default-template-valid",
                    (int)not_empty, (int)has_request, (int)has_ctx_block);
    }

    // ---- Test 10: placeholder di dalam blok kondisional ikut di-substitute ----
    {
        PromptInput in;
        in.tmpl              = "{{#CONTEXT}}CTX={{CONTEXT_FILE_NAME}}{{/CONTEXT}}";
        in.context_file_name = "SourceCode.md";

        const std::string out = renderPrompt(in);
        check("inner-ph: substituted", out, "CTX=SourceCode.md", true);
    }

    // ---- Test 11: "{{" tanpa "}}" di akhir template ----
    {
        PromptInput in;
        in.tmpl    = "BEFORE {{REQUEST}} DAN {{";
        in.request = "req";

        const std::string out = renderPrompt(in);
        check("unclosed-open: BEFORE", out, "BEFORE req DAN {{", true);
    }

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
}

// ---- Test aturan format output (sinkronisasi dengan format user) ----
bool runPromptFormatRulesTest() {
    std::printf("\n=== prompt-format-rules ===\n");
    std::printf("  Verifikasi default template mengandung semua aturan format "
                "output yang disinkronkan.\n");

    const std::string& d = defaultPromptTemplate();
    int fails = 0;
    auto check = [&](const char* name, const std::string& needle) {
        const bool has = d.find(needle) != std::string::npos;
        if (!has) ++fails;
        std::printf("  %s %-50s contains '%s'\n",
                    has ? "[ok]  " : "[FAIL]",
                    name,
                    needle.c_str());
    };

    check("rule-2: File: <path>",          "File: <path>");
    check("rule-2: fenced code block",     "fenced code block");
    check("rule-3: Path relatif",          "Path relatif");
    check("rule-4: awal baris",            "awal baris");
    check("rule-4: tidak ada indentasi",   "tidak ada indentasi");
    check("rule-5: SEBELUM memulai File:", "SEBELUM memulai File:");
    check("rule-6: tiga backtick",         "tiga backtick");
    check("rule-6: merusak parser",        "merusak parser");
    check("rule-7: tidak berubah",         "tidak berubah");
    check("rule-8: file binary",           "file binary");
    check("rule-9: Full file content",     "Full file content");
    check("rule-9: bukan patch",           "bukan patch");

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
}

// ---- Test FileCollector (Fase B #46) ----
bool runFileCollectorTest() {
    std::printf("\n=== file-collector-basic ===\n");
    std::printf("  Cek FileCollector::collectFiles — secret, exclude, "
                "binary, size, output-dir.\n");

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec) {
        std::printf("  [SKIP] temp_directory_path gagal: %s\n",
                    ec.message().c_str());
        return true;
    }
    tmp /= "frans_test_collector_v1";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    if (ec) {
        std::printf("  [SKIP] create_directories gagal: %s\n",
                    ec.message().c_str());
        return true;
    }

    auto write = [&](const std::string& rel, const std::string& content) {
        fs::path p = tmp / rel;
        std::error_code ec2;
        fs::create_directories(p.parent_path(), ec2);
        std::ofstream f(p, std::ios::binary);
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
    };

    write("hello.py",     "print('hi')\n");
    write("src/main.cpp", "#include <iostream>\nint main(){}\n");
    write("a/b/c.txt",    "hello\n");
    write(".env",                    "SECRET=xxx\n");
    write("config/credentials.json", "{}\n");
    write("secrets.prod.yaml",       "api_key: xxx\n");
    write("node_modules/x.js", "module.exports={};\n");
    write("build/out.o",       "binary-object\n");

    {
        std::ofstream f(tmp / "big.bin", std::ios::binary);
        std::string big(300 * 1024, 'x');
        f.write(big.data(), static_cast<std::streamsize>(big.size()));
    }
    {
        std::ofstream f(tmp / "data.bin", std::ios::binary);
        const char payload[] = {'a', 'b', 'c', '\0', 'd', 'e', 'f'};
        f.write(payload, sizeof(payload));
    }

    const fs::path out_dir = tmp / "out";
    fs::create_directories(out_dir, ec);
    write("out/SourceCode.md", "# harus di-exclude\n");

    CollectOptions opt;
    opt.max_file_bytes  = 256 * 1024;
    opt.exclude_secrets = true;
    opt.output_dir      = out_dir.string();

    CollectResult result = collectFiles(tmp.string(), opt, nullptr);

    if (result.used_git) {
        std::printf("  [SKIP] temp dir terdeteksi sebagai git repo — "
                    "test ini dirancang untuk fallback walker.\n");
        fs::remove_all(tmp, ec);
        return true;
    }

    bool ok = result.ok;
    if (!ok) {
        std::printf("  [FAIL] collectFiles gagal: %s\n", result.error.c_str());
    }

    auto findFile = [&](const std::string& rel) -> const CollectedFile* {
        for (const auto& f : result.files) {
            if (f.relative_path == rel) return &f;
        }
        return nullptr;
    };

    auto checkExist = [&](const char* rel, bool should_exist) {
        const CollectedFile* f = findFile(rel);
        const bool found = (f != nullptr);
        const bool pass = (found == should_exist);
        if (!pass) ok = false;
        std::printf("  %s exist %-32s found=%d expected=%d\n",
                    pass ? "[ok]  " : "[FAIL]",
                    rel, found ? 1 : 0, should_exist ? 1 : 0);
    };

    auto checkSkip = [&](const char* rel, const char* expected_reason) {
        const CollectedFile* f = findFile(rel);
        if (!f) {
            ok = false;
            std::printf("  [FAIL] skip  %-32s NOT FOUND\n", rel);
            return;
        }
        const bool pass = f->skipped && f->skip_reason == expected_reason;
        if (!pass) ok = false;
        std::printf("  %s skip   %-32s reason='%s' expected='%s'\n",
                    pass ? "[ok]  " : "[FAIL]",
                    rel,
                    f->skip_reason.c_str(),
                    expected_reason);
    };

    checkExist("hello.py",     true);
    checkExist("src/main.cpp", true);
    checkExist("a/b/c.txt",    true);
    checkSkip (".env",                   "secret");
    checkSkip ("config/credentials.json", "secret");
    checkSkip ("secrets.prod.yaml",      "secret");
    checkExist("node_modules/x.js", false);
    checkExist("build/out.o",       false);
    checkSkip ("big.bin",  "too_large");
    checkSkip ("data.bin", "binary");
    checkExist("out/SourceCode.md", false);

    auto helperCheck = [&](const char* label, bool cond) {
        if (!cond) ok = false;
        std::printf("  %s helper %-40s\n",
                    cond ? "[ok]  " : "[FAIL]", label);
    };

    helperCheck("isSecretBasename('.env')",
                isSecretBasename(".env"));
    helperCheck("isSecretBasename('id_rsa')",
                isSecretBasename("id_rsa"));
    helperCheck("isSecretBasename('foo.pem')",
                isSecretBasename("foo.pem"));
    helperCheck("isSecretBasename('secrets.prod.yaml')",
                isSecretBasename("secrets.prod.yaml"));
    helperCheck("!isSecretBasename('main.cpp')",
                !isSecretBasename("main.cpp"));
    helperCheck("!isSecretBasename('.envrc')",
                !isSecretBasename(".envrc"));
    helperCheck("isExcludedPath('node_modules/x.js', {})",
                isExcludedPath("node_modules/x.js", {}));
    helperCheck("isExcludedPath('build/out.o', {})",
                isExcludedPath("build/out.o", {}));
    helperCheck("isExcludedPath('a/__pycache__/x.pyc', {})",
                isExcludedPath("a/__pycache__/x.pyc", {}));
    helperCheck("!isExcludedPath('src/main.cpp', {})",
                !isExcludedPath("src/main.cpp", {}));
    helperCheck("isExcludedPath('foo.log', {'*.log'})",
                isExcludedPath("foo.log", {"*.log"}));
    helperCheck("!isExcludedPath('foo.txt', {'*.log'})",
                !isExcludedPath("foo.txt", {"*.log"}));

    if (result.files.size() >= 2) {
        bool sorted = true;
        for (std::size_t i = 1; i < result.files.size(); ++i) {
            if (result.files[i - 1].relative_path
                > result.files[i].relative_path)
            {
                sorted = false;
                break;
            }
        }
        helperCheck("files terurut leksikografis", sorted);
    }

    fs::remove_all(tmp, ec);
    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test MarkdownRenderer (Fase B #48) ----
//
// Cek:
//   - renderTree: struktur tree, penanda "*", sorting alphabetical,
//     intermediate dirs dibuat otomatis.
//   - renderFileBlock: separator + FILE: path + separator + konten.
//   - renderMarkdown: header + tree + file blocks lengkap.
//   - detectLanguageFromExtension: pemetaan ekstensi -> bahasa.
//   - formatBytesHuman: B / KB / MB.
bool runMarkdownRendererTest() {
    std::printf("\n=== markdown-renderer ===\n");
    std::printf("  Cek MarkdownRenderer — tree, file block, render lengkap, "
                "language detection, format bytes.\n");

    int fails = 0;
    auto checkContains = [&](const char* label, const std::string& hay,
                             const std::string& needle) {
        const bool ok = hay.find(needle) != std::string::npos;
        if (!ok) ++fails;
        std::printf("  %s %-44s contains '%s'\n",
                    ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
        if (!ok) std::printf("        actual: '%s'\n", hay.c_str());
    };
    auto checkNotContains = [&](const char* label, const std::string& hay,
                                const std::string& needle) {
        const bool ok = hay.find(needle) == std::string::npos;
        if (!ok) ++fails;
        std::printf("  %s %-44s NOT contains '%s'\n",
                    ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
    };

    // ---- Test 1: renderTree dasar ----
    {
        std::vector<CollectedFile> files = {
            {"README.md",       100, true, false, ""},
            {"src/main.cpp",    200, true, false, ""},
            {"src/util.h",       50, true, false, ""},
            {"tests/test.cpp",  150, true, false, ""},
        };
        std::vector<std::string> starred = {"src/main.cpp", "README.md"};

        const std::string tree = renderTree(files, "demo-repo", starred);

        checkContains("tree: header",        tree, "Directory structure:");
        checkContains("tree: root",          tree, "└── demo-repo/");
        checkContains("tree: README starred", tree, "README.md *");
        checkContains("tree: src dir",       tree, "src/");
        checkContains("tree: main.cpp starred", tree, "main.cpp *");
        checkContains("tree: util.h no star", tree, "util.h");
        checkNotContains("tree: util.h no *", tree, "util.h *");
        checkContains("tree: tests dir",     tree, "tests/");
        checkContains("tree: test.cpp",      tree, "test.cpp");
        // util.h TIDAK boleh berbintang
        checkNotContains("tree: util.h clean", tree, "util.h *");
    }

    // ---- Test 2: sorting alphabetical (file & dir bercampur) ----
    {
        std::vector<CollectedFile> files = {
            {"z.txt",      10, true, false, ""},
            {"a.txt",      10, true, false, ""},
            {"m/b.txt",    10, true, false, ""},
            {"m/a.txt",    10, true, false, ""},
        };
        const std::string tree = renderTree(files, "repo", {});

        // Urutan yang diharapkan:
        //   a.txt
        //   m/
        //     a.txt
        //     b.txt
        //   z.txt
        const auto pos_a = tree.find("a.txt");
        const auto pos_m = tree.find("m/");
        const auto pos_z = tree.find("z.txt");
        const bool order_ok = (pos_a < pos_m) && (pos_m < pos_z);
        if (!order_ok) ++fails;
        std::printf("  %s %-44s alphabetical order\n",
                    order_ok ? "[ok]  " : "[FAIL]", "tree-sort");
    }

    // ---- Test 3: intermediate dirs dibuat otomatis ----
    {
        std::vector<CollectedFile> files = {
            {"a/b/c/d.txt", 10, true, false, ""},
        };
        const std::string tree = renderTree(files, "repo", {});

        checkContains("tree: a/",  tree, "a/");
        checkContains("tree: b/",  tree, "b/");
        checkContains("tree: c/",  tree, "c/");
        checkContains("tree: d.txt", tree, "d.txt");
    }

    // ---- Test 4: file yang di-skip tidak muncul di tree ----
    {
        std::vector<CollectedFile> files = {
            {"visible.txt", 10, true, false, ""},
            {"secret.env",  10, true, true, "secret"},
        };
        const std::string tree = renderTree(files, "repo", {});

        checkContains("tree: visible ada",  tree, "visible.txt");
        checkNotContains("tree: secret skip", tree, "secret.env");
    }

    // ---- Test 5: renderFileBlock ----
    {
        const std::string block =
            renderFileBlock("src/main.cpp", "#include <iostream>\nint main() {}\n");

        checkContains("block: separator atas", block,
                      "================================================");
        checkContains("block: FILE: path",     block, "FILE: src/main.cpp");
        checkContains("block: konten",         block, "#include <iostream>");
        checkContains("block: akhir newline",  block, "int main() {}\n");
    }

    // ---- Test 6: renderFileBlock tanpa trailing newline ----
    {
        const std::string block =
            renderFileBlock("a.txt", "no newline at end");

        checkContains("block: auto-newline", block, "no newline at end\n");
    }

    // ---- Test 7: renderMarkdown lengkap ----
    {
        RenderInput in;
        in.all_files = {
            {"README.md",    100, true, false, ""},
            {"src/main.cpp", 200, true, false, ""},
            {"src/util.h",    50, true, false, ""},
        };
        in.starred_paths = {"src/main.cpp", "README.md"};
        in.file_contents = {
            {"README.md",    "# Demo\n"},
            {"src/main.cpp", "#include <iostream>\n"},
        };
        in.options.repo_name            = "demo-repo";
        in.options.header_title         = "SourceCode.md";
        in.options.created_at           = "2026-11-13 14:05";
        in.options.request_summary      = "buat fitur X";
        in.options.total_files_included = 2;
        in.options.total_content_bytes  = 38 * 1024 + 200;  // 38.2 KB

        const std::string out = renderMarkdown(in);

        checkContains("md: header title",  out, "# SourceCode.md: demo-repo");
        checkContains("md: timestamp",     out, "2026-11-13 14:05");
        checkContains("md: file count",    out, "2 file disertakan");
        checkContains("md: size",          out, "38.2 KB");
        checkContains("md: permintaan",    out, "Permintaan: buat fitur X");
        checkContains("md: marker legend", out,
                      "Tanda \"*\" pada struktur di bawah");
        checkContains("md: tree root",     out, "└── demo-repo/");
        checkContains("md: README starred", out, "README.md *");
        checkContains("md: util no star",  out, "util.h");
        checkNotContains("md: util clean", out, "util.h *");
        checkContains("md: block README",  out, "FILE: README.md");
        checkContains("md: block main",    out, "FILE: src/main.cpp");
        checkNotContains("md: util no block", out, "FILE: src/util.h");
        checkContains("md: konten README", out, "# Demo");
        checkContains("md: konten main",   out, "#include <iostream>");
    }

    // ---- Test 8: renderMarkdown tanpa permintaan ----
    {
        RenderInput in;
        in.all_files = {{"a.txt", 10, true, false, ""}};
        in.starred_paths = {"a.txt"};
        in.file_contents = {{"a.txt", "hello\n"}};
        in.options.repo_name            = "r";
        in.options.header_title         = "Ingest";
        in.options.total_files_included = 1;
        in.options.total_content_bytes  = 6;

        const std::string out = renderMarkdown(in);
        checkNotContains("md: no permintaan line", out, "Permintaan:");
        checkContains("md: header tetap ada", out, "# Ingest: r");
    }

    // ---- Test 9: starred path tidak ada di file_contents -> warning + skip ----
    {
        RenderInput in;
        in.all_files = {{"a.txt", 10, true, false, ""}};
        in.starred_paths = {"a.txt", "missing.txt"};
        in.file_contents = {{"a.txt", "hello\n"}};
        in.options.repo_name = "r";

        const std::string out = renderMarkdown(in);
        checkContains("md: a.txt block ada", out, "FILE: a.txt");
        checkNotContains("md: missing.txt skip", out, "FILE: missing.txt");
    }

    // ---- Test 10: detectLanguageFromExtension ----
    {
        auto checkLang = [&](const char* path, const char* expected) {
            const std::string actual = detectLanguageFromExtension(path);
            const bool ok = (actual == expected);
            if (!ok) ++fails;
            std::printf("  %s lang(%-24s) = '%s' (expected '%s')\n",
                        ok ? "[ok]  " : "[FAIL]",
                        path, actual.c_str(), expected);
        };
        checkLang("main.cpp",          "cpp");
        checkLang("header.hpp",        "cpp");
        checkLang("script.py",         "python");
        checkLang("app.js",            "javascript");
        checkLang("app.ts",            "typescript");
        checkLang("data.json",         "json");
        checkLang("config.yaml",       "yaml");
        checkLang("CMakeLists.txt",    "cmake");
        checkLang("Makefile",          "makefile");
        checkLang("Dockerfile",        "dockerfile");
        checkLang("readme.md",         "markdown");
        checkLang("noext",             "");
        checkLang("unknown.xyz",       "");
    }

    // ---- Test 11: formatBytesHuman ----
    {
        auto checkFmt = [&](std::size_t bytes, const char* expected) {
            const std::string actual = formatBytesHuman(bytes);
            const bool ok = (actual == expected);
            if (!ok) ++fails;
            std::printf("  %s fmt(%zu) = '%s' (expected '%s')\n",
                        ok ? "[ok]  " : "[FAIL]",
                        bytes, actual.c_str(), expected);
        };
        checkFmt(0,          "0 B");
        checkFmt(512,        "512 B");
        checkFmt(1024,       "1.0 KB");
        checkFmt(1536,       "1.5 KB");
        checkFmt(38 * 1024 + 200, "38.2 KB");
        checkFmt(1024 * 1024, "1.0 MB");
        checkFmt(5 * 1024 * 1024 + 123 * 1024, "5.1 MB");
    }

    // ---- Test 12: tree kosong (tidak ada file) ----
    {
        std::vector<CollectedFile> files;
        const std::string tree = renderTree(files, "empty-repo", {});
        checkContains("tree-empty: header", tree, "Directory structure:");
        checkContains("tree-empty: root",   tree, "└── empty-repo/");
        // Tidak ada anak
        checkNotContains("tree-empty: no children", tree, "├──");
        checkNotContains("tree-empty: no children2", tree, "└── ");
    }

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
}

void listScenarios(const std::vector<Scenario>& scenarios) {
    std::printf("Scenario tersedia:\n");
    for (const auto& s : scenarios) {
        std::printf("  %-24s %s\n", s.name.c_str(), s.description.c_str());
    }
    std::printf("  %-24s Cek SandboxedFileSystem::isPathSafe()\n",
                "sandbox-path-safety");
    std::printf("  %-24s Cek parseOutput pilih frame Python user\n",
                "traceback-frame-picking");
    std::printf("  %-24s Cek PromptGenerator::renderPrompt (Fase A #42)\n",
                "prompt-generator");
    std::printf("  %-24s Verifikasi aturan format output\n",
                "prompt-format-rules");
    std::printf("  %-24s Cek FileCollector (Fase B #46)\n",
                "file-collector-basic");
    std::printf("  %-24s Cek MarkdownRenderer (Fase B #48)\n",
                "markdown-renderer");
}

void dumpScenario(const Scenario& s) {
    const std::string input = expandFences(s.raw_input);
    std::fwrite(input.data(), 1, input.size(), stdout);
    if (input.empty() || input.back() != '\n') std::putchar('\n');
}

} // namespace

int main(int argc, char** argv) {
    const auto scenarios = buildScenarios();

    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        listScenarios(scenarios);
        return 0;
    }
    if (argc >= 3 && std::strcmp(argv[1], "--dump") == 0) {
        for (const auto& s : scenarios) {
            if (s.name == argv[2]) {
                dumpScenario(s);
                return 0;
            }
        }
        std::fprintf(stderr, "scenario '%s' tidak ada\n", argv[2]);
        return 2;
    }

    int pass = 0;
    int fail = 0;

    for (const auto& s : scenarios) {
        if (runScenario(s)) ++pass;
        else                ++fail;
    }
    if (runSandboxTest())           ++pass;
    else                            ++fail;
    if (runTracebackPickingTest())  ++pass;
    else                            ++fail;
    if (runPromptGeneratorTest())   ++pass;
    else                            ++fail;
    if (runPromptFormatRulesTest()) ++pass;
    else                            ++fail;
    if (runFileCollectorTest())     ++pass;
    else                            ++fail;
    if (runMarkdownRendererTest())  ++pass;
    else                            ++fail;

    std::printf("\n=== Summary: %d passed, %d failed, %d total ===\n",
                pass, fail, pass + fail);
    return fail == 0 ? 0 : 1;
}
