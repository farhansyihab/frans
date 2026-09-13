// ~/AgenAI/frans/src/application/agent/AgentContext.h
#pragma once

#include "application/agent/MultiFileParser.h"   // GeneratedFile
#include "code_intelligence/parsers/CodeSnippet.h"
#include "domain/fs/IFileSystemPort.h"

#include <map>
#include <string>
#include <vector>

namespace frans::application {

// Blueprint §5.3 — klasifikasi kompleksitas task.
enum class TaskComplexity {
    Trivial,
    Moderate,
    Complex,
    RequiresDeepReasoning,
};

// Hasil satu langkah eksekusi (Stage 7c — ExecuteState).
struct ExecutionOutcome {
    bool        ran          = false;
    int         exit_code    = -1;
    bool        timed_out    = false;
    double      duration_sec = 0.0;
    std::string stdout_text;
    std::string stderr_text;

    bool is_success() const { return ran && exit_code == 0 && !timed_out; }
    void reset() { *this = ExecutionOutcome{}; }
};

// Blueprint Stage 7f.5 — hasil eksekusi test file.
//
// Diisi ExecuteState. Field ini TIDAK dibaca ObserveState — ObserveState
// hanya melihat lastExecution. Tujuan field ini: audit, UI (TaskProgress),
// dan debugging di terminal. Pemisahan disengaja supaya ObserveState
// tidak perlu tahu apakah eksekusi terakhir itu test atau entry point.
struct TestOutcome {
    bool        ran          = false;
    std::string test_file;              // relatif terhadap sandbox root
    int         exit_code    = -1;
    bool        timed_out    = false;
    double      duration_sec = 0.0;

    bool is_success() const { return ran && exit_code == 0 && !timed_out; }
    void reset() { *this = TestOutcome{}; }
};

// Satu catatan attempt di attemptHistory (Stage 7d).
struct AttemptRecord {
    int         attempt     = 0;
    int         exit_code   = -1;
    bool        timed_out   = false;
    std::string observation;
};

// State yang dibawa lintas state agent (Plan → Generate → Execute →
// Observe → Fix → ... → Done).
struct AgentContext {
    // ---- input ----

    // Instruksi user, mentah (natural language).
    std::string taskDescription;

    // Path relatif terhadap sandbox root untuk ENTRY POINT file.
    std::string targetRelativePath;

    // Blueprint Stage 7f — file in scope untuk run ini.
    std::vector<std::string> inScopeFiles;

    // Blueprint Stage 7f — snapshot isi file SEBELUM pertama kali ditulis.
    std::map<std::string, std::string> fileSnapshots;

    // ---- hasil GenerateState / FixState ----

    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;
    std::vector<GeneratedFile> generatedFiles;
    std::string lastGeneratedCode;

    // ---- hasil ExecuteState (Stage 7c) ----

    ExecutionOutcome lastExecution;

    // Blueprint Stage 7f.5 — hasil test terakhir (kalau test file
    // terdeteksi dan dijalankan).
    //
    // Kontrak:
    //   ran = false            → tidak ada test file, entry langsung jalan.
    //   ran = true, exit_code=0 → test pass; entry point kemudian dijalankan.
    //   ran = true, exit_code!=0 → test fail; entry point TIDAK dijalankan,
    //                              dan lastExecution berisi hasil test.
    TestOutcome lastTest;

    // ---- hasil PlanState + ObserveState (Stage 7d) ----

    TaskComplexity plannedComplexity = TaskComplexity::Moderate;
    std::string    lastObservation;

    int attempt     = 0;
    int maxAttempts = 3;

    std::vector<AttemptRecord> attemptHistory;

    // ---- status lintas state ----

    std::string lastError;
    bool        success = false;

    const GeneratedFile* findEntryFile() const {
        if (targetRelativePath.empty()) return nullptr;
        for (const auto& f : generatedFiles) {
            if (f.relative_path == targetRelativePath) return &f;
        }
        return nullptr;
    }
};

// Blueprint Stage 7f — snapshot capture sebelum write.
//
// Idempotent: kalau path sudah ada di ctx.fileSnapshots, tidak melakukan
// apapun — snapshot harus tetap versi PALING AWAL sebelum attempt 0
// menyentuhnya, tidak boleh tertimpa oleh attempt berikutnya.
inline void snapshotBeforeWrite(AgentContext& ctx,
                                frans::domain::IFileSystemPort& fs,
                                const std::string& path)
{
    if (ctx.fileSnapshots.count(path)) return;
    ctx.fileSnapshots[path] = fs.exists(path) ? fs.readFile(path)
                                               : std::string{};
}

} // namespace frans::application
