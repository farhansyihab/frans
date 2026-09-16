// ~/AgenAI/frans/src/application/build_runner/IBuildStrategy.h
#pragma once

#include "application/agent/MultiFileParser.h"    // GeneratedFile
#include "application/build_runner/BuildReport.h"
#include "domain/fs/IFileSystemPort.h"

#include <string>
#include <vector>

namespace frans::application {

// Blueprint Stage 11d §13.1 — Strategy untuk build/compile satu bahasa.
//
// Implementasi konkret:
//   - PythonBuildStrategy      (.py)
//   - CppBuildStrategy         (.cpp, .cc, .cxx)
//   - TypeScriptBuildStrategy  (.ts, .tsx)
//   - JavaScriptBuildStrategy  (.js, .jsx, .mjs, .cjs)
//
// Vue SFC TIDAK punya strategy — BuildRunner menanganinya sebagai kasus
// khusus (return error informatif).
//
// Kontrak:
//   - detectEntryPoint() TIDAK boleh throw. Return "" kalau tidak bisa.
//   - buildCommand()     TIDAK boleh throw. Return argv siap dijalankan.
//   - parseOutput()      TIDAK boleh throw. Return BuildReport terisi.
class IBuildStrategy {
public:
    virtual ~IBuildStrategy() = default;

    virtual std::string languageName() const = 0;

    // Dari list file yang ditulis, tentukan entry point (file yang akan
    // di-compile/run). Return relative path terhadap sandbox root, atau
    // "" kalau tidak bisa deteksi.
    virtual std::string detectEntryPoint(
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs,
        const std::string& sandbox_root) = 0;

    // Bangun argv untuk build/run. argv[0] = executable.
    //
    // `sandbox_root` disediakan untuk strategy yang butuh menambah
    // include path atau argumen lain berbasis lokasi project. Strategy
    // yang tidak butuh boleh mengabaikan parameter ini.
    //
    // Return empty vector kalau tidak bisa bangun command.
    virtual std::vector<std::string> buildCommand(
        const std::string& entry_point,
        const std::vector<GeneratedFile>& files,
        const std::string& sandbox_root) = 0;

    // Parse stdout/stderr + exit_code menjadi BuildReport.
    // `sandbox_root` untuk normalisasi path absolut di output.
    // Field `related_files` di sini hanya berisi PRIMARY_ERROR.
    virtual BuildReport parseOutput(
        int exit_code,
        bool timed_out,
        const std::string& stdout_text,
        const std::string& stderr_text,
        double duration_sec,
        const std::string& sandbox_root) = 0;
};

} // namespace frans::application