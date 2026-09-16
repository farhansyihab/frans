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
    bool        ran               = false;
    int         exit_code         = -1;
    bool        timed_out         = false;
    bool        cancelled_by_user = false;   // Stage 7g
    double      duration_sec      = 0.0;
    std::string stdout_text;
    std::string stderr_text;

    bool is_success() const {
        return ran && exit_code == 0 && !timed_out && !cancelled_by_user;
    }
    void reset() { *this = ExecutionOutcome{}; }
};

// Blueprint Stage 7f.5 — hasil eksekusi test file.
struct TestOutcome {
    bool        ran          = false;
    std::string test_file;
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

// State yang dibawa lintas state agent.
struct AgentContext {
    // ---- input ----

    std::string taskDescription;
    std::string targetRelativePath;
    std::vector<std::string> inScopeFiles;
    std::map<std::string, std::string> fileSnapshots;

    // ---- hasil GenerateState / FixState ----

    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;
    std::vector<GeneratedFile> generatedFiles;
    std::string lastGeneratedCode;

    // Stage 9c — biaya AKTUAL panggilan LLM terakhir (dari
    // GenResult::cost_usd). Diisi GenerateState & FixState setelah
    // provider_.generate() return. 0.0 untuk provider lokal / gratis.
    double lastCallCostUsd = 0.0;

    // Stage 12d — true kalau panggilan LLM terakhir gagal karena rate
    // limit / capacity (HTTP 429 / 503). Diisi dari GenResult::rate_limited.
    // Dipakai Agent loop untuk memicu cooldown fallback ke Local,
    // BUKAN naik ke paid tier berikutnya.
    bool lastCallRateLimited = false;

    // Stage 12d — token usage dari panggilan LLM terakhir. Dipakai
    // DailyBudgetGuard::recordFreeTierUsage() untuk audit free-tier
    // (Gemini). Provider lokal juga boleh mengisi (untuk audit saja).
    int lastPromptTokens     = 0;
    int lastCompletionTokens = 0;

    // ---- hasil ExecuteState (Stage 7c) ----

    ExecutionOutcome lastExecution;
    TestOutcome      lastTest;

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
inline void snapshotBeforeWrite(AgentContext& ctx,
                                frans::domain::IFileSystemPort& fs,
                                const std::string& path)
{
    if (ctx.fileSnapshots.count(path)) return;
    ctx.fileSnapshots[path] = fs.exists(path) ? fs.readFile(path)
                                               : std::string{};
}

} // namespace frans::application