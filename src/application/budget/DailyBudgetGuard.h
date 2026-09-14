// ~/AgenAI/frans/src/application/budget/DailyBudgetGuard.h
#pragma once

#include <mutex>
#include <string>

namespace frans::application {

// Snapshot status budget untuk UI/log. Immutable copy — caller boleh
// pegang melewati mutex guard.
struct BudgetStatus {
    std::string date;           // "YYYY-MM-DD", local time
    double      budget_usd    = 0.0;
    double      spent_usd     = 0.0;
    double      remaining_usd = 0.0;
};

// Blueprint §5.3 — DailyBudgetGuard.
//
// Membatasi pengeluaran harian API berbayar (DeepSeek/Gemini). Dipanggil
// dari Agent loop (worker thread) SEBELUM memanggil provider berbayar
// (canCallPaidAPI) dan SESUDAH generation selesai (recordUsage).
//
// State dipersist ke file JSON supaya usage survive restart aplikasi di
// hari yang sama:
//   {"date": "YYYY-MM-DD", "spent_usd": 0.1234}
//
// Rollover harian: tiap pemanggilan canCallPaidAPI() / recordUsage()
// mengecek tanggal lokal. Kalau tanggal sudah berganti sejak panggilan
// terakhir, spent_usd direset ke 0 SEBELUM logika lain dijalankan.
//
// Tanggal ditentukan dari LOCAL time (std::chrono::system_clock +
// localtime_r), bukan UTC — user melihat "hari ini" menurut zona waktunya.
//
// Semua operasi thread-safe (mutex internal). Tidak throw: semua error
// I/O ditelan dan fallback ke default (spent=0), dengan log ke stderr
// ber-prefix "[frans][budget]".
class DailyBudgetGuard {
public:
    // `daily_budget_usd` : dari AppConfig::daily_api_budget_usd.
    //                      0 atau negatif = budget dimatikan (canCall
    //                      selalu false).
    // `state_file_path`  : file JSON untuk persist usage. Caller yang
    //                      menentukan path (biasanya
    //                      cfg.log_dir + "/budget_state.json"). Parent
    //                      directory dibuat otomatis saat pertama write.
    DailyBudgetGuard(double daily_budget_usd, std::string state_file_path);

    DailyBudgetGuard(const DailyBudgetGuard&)            = delete;
    DailyBudgetGuard& operator=(const DailyBudgetGuard&) = delete;

    // True kalau estimated_cost_usd masih muat di sisa budget hari ini:
    //   (spent_usd + estimated_cost_usd) <= budget_usd
    //
    // Selalu false kalau budget_usd <= 0 (budget dimatikan = tidak
    // pernah boleh panggil API berbayar).
    bool canCallPaidAPI(double estimated_cost_usd);

    // Catat biaya AKTUAL setelah panggilan API selesai. Menambah
    // spent_usd hari ini dan menulis ulang state_file_path.
    void recordUsage(double actual_cost_usd);

    // Sisa budget hari ini. 0.0 kalau sudah habis / budget dimatikan.
    double remainingBudgetToday();

    // Snapshot status (date, budget, spent, remaining). Copy aman.
    BudgetStatus status();

private:
    // WAJIB dipanggil dengan mtx_ held.
    void loadStateLocked();
    void saveStateLocked();
    void checkRolloverLocked();

    mutable std::mutex mtx_;
    double             budget_usd_      = 0.0;
    std::string        state_file_path_;
    std::string        current_date_;   // "YYYY-MM-DD", local time
    double             spent_usd_       = 0.0;
};

} // namespace frans::application
