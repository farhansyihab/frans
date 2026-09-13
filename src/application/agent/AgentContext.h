// ~/AgenAI/frans/src/application/agent/AgentContext.h
#pragma once

#include "code_intelligence/parsers/CodeSnippet.h"

#include <string>
#include <vector>

namespace frans::application {

// Blueprint §5.3 — klasifikasi kompleksitas task.
//
// Diisi PlanState (Stage 7d), dipakai oleh ProviderRouter (Stage 9) untuk
// memutuskan local vs API berbayar. Untuk Stage 7d, hanya mempengaruhi
// maxAttempts.
enum class TaskComplexity {
    Trivial,
    Moderate,
    Complex,
    RequiresDeepReasoning,
};

// Hasil satu langkah eksekusi (Stage 7c — ExecuteState).
//
// `ran` true HANYA kalau runCommand benar-benar dieksekusi (bukan
// dipotong oleh danger check, file tidak ada, dsb.). Dipakai oleh
// ObserveState untuk membedakan "tidak jalan" dari "jalan tapi gagal".
struct ExecutionOutcome {
    bool        ran          = false;   // false = tidak pernah dijalankan
    int         exit_code    = -1;      // 0 = sukses; 128+N = mati oleh signal N
    bool        timed_out    = false;   // true kalau SIGKILL karena timeout
    double      duration_sec = 0.0;
    std::string stdout_text;
    std::string stderr_text;

    bool is_success() const { return ran && exit_code == 0 && !timed_out; }

    // Drained / cleared — panggil sebelum run supaya tidak baca hasil
    // state sebelumnya.
    void reset() { *this = ExecutionOutcome{}; }
};

// Satu catatan attempt di attemptHistory (Stage 7d).
//
// Dipakai untuk audit trail + (Stage 9) EscalationPolicy menghitung
// berapa kali lokal berturut-turut gagal.
struct AttemptRecord {
    int         attempt     = 0;
    int         exit_code   = -1;
    bool        timed_out   = false;
    std::string observation;    // human-readable, dari ObserveState
};

// State yang dibawa lintas state agent (Plan → Generate → Execute →
// Observe → Fix → ... → Done).
//
// Konvensi: setiap state WAJIB mengisi ctx.success dengan benar sebelum
// return, dan mengosongkan ctx.lastError saat sukses.
struct AgentContext {
    // ---- input ----

    // Instruksi user, mentah (natural language).
    std::string taskDescription;

    // Path relatif terhadap sandbox root, mis. "generated/solve.py".
    // Bukan path absolut — SandboxedFileSystem yang resolve.
    std::string targetRelativePath;

    // ---- hasil GenerateState (Stage 7b) ----

    // Snippet yang dipilih ContextSelector. Diisi GenerateState, disimpan
    // untuk audit dan untuk dipakai state berikutnya (FixState menambahkan
    // error message tapi tetap butuh snippet konteks).
    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;

    // Kode yang baru saja digenerate / diperbaiki (isi fenced block,
    // tanpa fence-nya). Di-overwrite oleh FixState.
    std::string lastGeneratedCode;

    // ---- hasil ExecuteState (Stage 7c) ----

    // stdout/stderr/exit_code/duration dari eksekusi terakhir.
    ExecutionOutcome lastExecution;

    // ---- hasil PlanState + ObserveState (Stage 7d) ----

    // Hasil klasifikasi task oleh PlanState.
    TaskComplexity plannedComplexity = TaskComplexity::Moderate;

    // Analisis terstruktur dari ObserveState — biasanya berisi jenis
    // exception + pesan + line number kalau eksekusi gagal. Human-readable,
    // disuapkan langsung ke FixState sebagai bagian prompt.
    std::string lastObservation;

    // Nomor attempt saat ini. 0 = initial generate; >=1 = fix.
    int attempt     = 0;
    int maxAttempts = 3;

    // Riwayat semua attempt (termasuk yang sukses). Dipakai untuk audit
    // dan (Stage 9) EscalationPolicy.
    std::vector<AttemptRecord> attemptHistory;

    // ---- status lintas state ----

    // Pesan error terakhir kalau success == false. Human-readable, siap
    // ditampilkan ke user.
    std::string lastError;

    // Hasil langkah terakhir. Setiap state WAJIB set ini.
    bool success = false;
};

} // namespace frans::application
