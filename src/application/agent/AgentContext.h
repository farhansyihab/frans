// ~/AgenAI/frans/src/application/agent/AgentContext.h
#pragma once

#include "code_intelligence/parsers/CodeSnippet.h"

#include <string>
#include <vector>

namespace frans::application {

// Hasil satu langkah eksekusi (Stage 7c — ExecuteState).
//
// `is_success()` true HANYA kalau exit_code == 0 dan bukan timeout.
// Caller (ObserveState / FixState di Stage 7d) yang memutuskan langkah
// berikutnya berdasarkan isi stdout/stderr.
struct ExecutionOutcome {
    int         exit_code    = -1;      // 0 = sukses; 128+N = mati oleh signal N
    bool        timed_out    = false;   // true kalau SIGKILL karena timeout
    double      duration_sec = 0.0;
    std::string stdout_text;
    std::string stderr_text;

    bool is_success() const { return exit_code == 0 && !timed_out; }

    // Drained / cleared — panggil sebelum run supaya tidak baca hasil
    // state sebelumnya.
    void reset() { *this = ExecutionOutcome{}; }
};

// State yang dibawa lintas state agent (Generate → Execute → Observe → Fix).
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
    // untuk audit dan untuk dipakai state berikutnya (mis. FixState
    // menambahkan error message tapi tetap butuh snippet konteks).
    std::vector<frans::code_intelligence::CodeSnippet> contextSnippets;

    // Kode yang baru saja digenerate (isi fenced block, tanpa fence-nya).
    std::string lastGeneratedCode;

    // ---- hasil ExecuteState (Stage 7c) ----

    // stdout/stderr/exit_code/duration dari eksekusi terakhir.
    ExecutionOutcome lastExecution;

    // ---- status lintas state ----

    // Pesan error terakhir kalau success == false. Human-readable, siap
    // ditampilkan ke user.
    std::string lastError;

    // Hasil langkah terakhir. Setiap state WAJIB set ini.
    bool success = false;
};

} // namespace frans::application
