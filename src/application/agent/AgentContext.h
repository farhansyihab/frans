// ~/AgenAI/frans/src/application/agent/AgentContext.h
#pragma once

#include "application/agent/MultiFileParser.h"   // GeneratedFile
#include "code_intelligence/parsers/CodeSnippet.h"

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

    // Path relatif terhadap sandbox root untuk ENTRY POINT file. Meskipun
    // model menghasilkan multi-file, hanya satu file yang dijalankan
    // sebagai entry point.
    std::string targetRelativePath;

    // ---- hasil GenerateState / FixState ----

    // Snippet yang dipilih ContextSelector (kosong kalau graph null).
    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;

    // SEMUA file yang dihasilkan attempt terakhir. Selalu berisi minimal
    // satu file — entry point di `targetRelativePath`. Di-stage 7e,
    // GenerateState/FixState menggunakan MultiFileParser.
    std::vector<GeneratedFile> generatedFiles;

    // Convenience: isi file entry point. Di-set otomatis dari
    // generatedFiles untuk backward-compat dengan pembaca Stage 7b/7d.
    // JANGAN dimodifikasi langsung — pakai generatedFiles.
    std::string lastGeneratedCode;

    // ---- hasil ExecuteState (Stage 7c) ----

    ExecutionOutcome lastExecution;

    // ---- hasil PlanState + ObserveState (Stage 7d) ----

    TaskComplexity plannedComplexity = TaskComplexity::Moderate;
    std::string    lastObservation;

    // Nomor attempt saat ini. 0 = initial generate; >=1 = fix.
    int attempt     = 0;
    int maxAttempts = 3;

    std::vector<AttemptRecord> attemptHistory;

    // ---- status lintas state ----

    std::string lastError;
    bool        success = false;

    // Helper: cari file entry di generatedFiles.
    // Return nullptr kalau targetRelativePath kosong atau tidak ketemu.
    const GeneratedFile* findEntryFile() const {
        if (targetRelativePath.empty()) return nullptr;
        for (const auto& f : generatedFiles) {
            if (f.relative_path == targetRelativePath) return &f;
        }
        return nullptr;
    }
};

} // namespace frans::application
