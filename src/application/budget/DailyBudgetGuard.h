// ~/AgenAI/frans/src/application/budget/DailyBudgetGuard.h
#pragma once

#include <mutex>
#include <string>

namespace frans::application {

// Snapshot status budget untuk UI/log. Immutable copy — caller boleh
// pegang melewati mutex guard.
//
// Stage 12 tambah: counter free-tier (Gemini) untuk audit. Nilai-nilai
// ini TIDAK mempengaruhi canCallPaidAPI / remaining_usd — Gemini gratis.
struct BudgetStatus {
    std::string date;           // "YYYY-MM-DD", local time
    double      budget_usd    = 0.0;
    double      spent_usd     = 0.0;
    double      remaining_usd = 0.0;

    // Audit free-tier (Gemini) — kumulatif hari ini.
    long long   free_prompt_tokens     = 0;
    long long   free_completion_tokens = 0;
};

// Blueprint §5.3 — DailyBudgetGuard.
//
// Membatasi pengeluaran harian API berbayar (DeepSeek). Dipanggil dari
// Agent loop SEBELUM memanggil provider berbayar (canCallPaidAPI) dan
// SESUDAH generation selesai (recordUsage).
//
// Stage 12: PaidLight (Gemini) GRATIS — tidak dikenai budget check.
// Namun usage Gemini tetap di-track via recordFreeTierUsage() untuk
// audit (blueprint §5.3: "Gemini TIDAK dihitung di budget, tapi tetap
// di-track untuk audit").
//
// State dipersist ke file JSON:
//   {"date": "YYYY-MM-DD", "spent_usd": 0.1234,
//    "free_prompt_tokens": 0, "free_completion_tokens": 0}
//
// Rollover harian: tiap pemanggilan public method mengecek tanggal
// lokal. Kalau berganti, semua counter direset ke 0 SEBELUM logika
// lain dijalankan.
//
// Tanggal ditentukan dari LOCAL time (localtime_r).
//
// Semua operasi thread-safe (mutex internal). Tidak throw: error I/O
// ditelan, fallback ke default (spent=0), log ke stderr prefix
// "[frans][budget]".
class DailyBudgetGuard {
public:
    DailyBudgetGuard(double daily_budget_usd, std::string state_file_path);

    DailyBudgetGuard(const DailyBudgetGuard&)            = delete;
    DailyBudgetGuard& operator=(const DailyBudgetGuard&) = delete;

    // True kalau estimated_cost_usd masih muat di sisa budget hari ini:
    //   (spent_usd + estimated_cost_usd) <= budget_usd
    //
    // Selalu false kalau budget_usd <= 0 (budget dimatikan = tidak
    // pernah boleh panggil API berbayar).
    bool canCallPaidAPI(double estimated_cost_usd);

    // Catat biaya AKTUAL setelah panggilan PaidHeavy selesai. Menambah
    // spent_usd hari ini dan menulis ulang state_file_path.
    void recordUsage(double actual_cost_usd);

    // Stage 12 — audit free-tier (Gemini). TIDAK mempengaruhi budget.
    // Kumulatif per hari, ikut dipersist.
    void recordFreeTierUsage(int prompt_tokens, int completion_tokens);

    // Sisa budget hari ini. 0.0 kalau sudah habis / budget dimatikan.
    double remainingBudgetToday();

    // Snapshot status (date, budget, spent, remaining, free-tier audit).
    BudgetStatus status();

private:
    void loadStateLocked();
    void saveStateLocked();
    void checkRolloverLocked();

    mutable std::mutex mtx_;
    double             budget_usd_      = 0.0;
    std::string        state_file_path_;
    std::string        current_date_;   // "YYYY-MM-DD", local time
    double             spent_usd_       = 0.0;

    // Stage 12 — audit free-tier (Gemini).
    long long          free_prompt_tokens_     = 0;
    long long          free_completion_tokens_ = 0;
};

} // namespace frans::application