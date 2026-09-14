// ~/AgenAI/frans/src/application/budget/DailyBudgetGuard.cpp
#include "application/budget/DailyBudgetGuard.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace frans::application {

namespace fs = std::filesystem;

namespace {

// Tanggal lokal hari ini, "YYYY-MM-DD". Pakai localtime_r (thread-safe
// POSIX) supaya tidak race dengan worker thread lain.
//
// Memakai std::put_time (bukan snprintf) untuk menghindari false-positive
// -Wformat-truncation: GCC tidak tahu tm_mon dibatasi 0..11 dan tm_mday
// dibatasi 1..31, jadi dia asumsikan %02d bisa sampai 11 karakter.
// std::put_time tidak lewat jalur analisis format-string itu.
std::string todayLocalDate() {
    const std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm_buf{};
    if (::localtime_r(&t, &tm_buf) == nullptr) {
        // Fallback deterministik (harus tidak pernah terjadi di Linux).
        return "1970-01-01";
    }

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d");
    return oss.str();
}

void logBudget(const char* fmt, ...) {
    std::fprintf(stderr, "[frans][budget] ");
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / state load
// ---------------------------------------------------------------------------
DailyBudgetGuard::DailyBudgetGuard(double daily_budget_usd,
                                   std::string state_file_path)
    : budget_usd_(daily_budget_usd),
      state_file_path_(std::move(state_file_path))
{
    std::lock_guard<std::mutex> lk(mtx_);
    loadStateLocked();
}

void DailyBudgetGuard::loadStateLocked() {
    const std::string today = todayLocalDate();

    // Default state — dipakai kalau file tidak ada / rusak.
    current_date_ = today;
    spent_usd_    = 0.0;

    if (state_file_path_.empty()) {
        logBudget("state_file_path kosong — budget hanya in-memory "
                  "(tidak survive restart)");
        return;
    }

    std::ifstream f(state_file_path_);
    if (!f) {
        logBudget("state file belum ada: %s (mulai dari spent=0)",
                  state_file_path_.c_str());
        return;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        logBudget("JSON rusak di %s: %s (mulai dari spent=0)",
                  state_file_path_.c_str(), e.what());
        return;
    }

    if (!j.is_object()) {
        logBudget("state file bukan JSON object (mulai dari spent=0)");
        return;
    }

    std::string file_date;
    double      file_spent = 0.0;

    if (j.contains("date") && j["date"].is_string()) {
        file_date = j["date"].get<std::string>();
    }
    if (j.contains("spent_usd") && j["spent_usd"].is_number()) {
        file_spent = j["spent_usd"].get<double>();
    }

    if (file_date != today) {
        // Rollover saat load — file dibuat di hari lain. Reset.
        logBudget("rollover saat load: file date='%s', today='%s' "
                  "-> spent direset ke 0",
                  file_date.empty() ? "(kosong)" : file_date.c_str(),
                  today.c_str());
        current_date_ = today;
        spent_usd_    = 0.0;
        return;
    }

    current_date_ = today;
    spent_usd_    = file_spent;
    logBudget("state di-load: date=%s spent=$%.6f budget=$%.2f",
              current_date_.c_str(), spent_usd_, budget_usd_);
}

// ---------------------------------------------------------------------------
// Persist
// ---------------------------------------------------------------------------
void DailyBudgetGuard::saveStateLocked() {
    if (state_file_path_.empty()) return;

    std::error_code ec;

    // Buat parent directory kalau belum ada (nested).
    const fs::path p(state_file_path_);
    const fs::path parent = p.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            logBudget("create_directories('%s') gagal: %s "
                      "(tetap coba tulis file)",
                      parent.string().c_str(), ec.message().c_str());
            // tidak return — coba tulis saja, mungkin parent sudah ada
        }
    }

    nlohmann::json j;
    j["date"]      = current_date_;
    j["spent_usd"] = spent_usd_;

    std::ofstream f(state_file_path_, std::ios::trunc);
    if (!f) {
        logBudget("tidak bisa membuka '%s' untuk write",
                  state_file_path_.c_str());
        return;
    }
    f << j.dump(2);
    if (!f) {
        logBudget("write error ke '%s'", state_file_path_.c_str());
    }
}

// ---------------------------------------------------------------------------
// Rollover check — WAJIB dipanggil dari tiap public method (dengan lock)
// ---------------------------------------------------------------------------
void DailyBudgetGuard::checkRolloverLocked() {
    const std::string today = todayLocalDate();
    if (today == current_date_) return;

    logBudget("rollover: '%s' -> '%s', spent=$%.6f direset ke 0",
              current_date_.c_str(), today.c_str(), spent_usd_);

    current_date_ = today;
    spent_usd_    = 0.0;

    // Tidak persist di sini — persist terjadi saat recordUsage berikutnya.
    // State in-memory sudah benar untuk sisa sesi ini, dan kalau aplikasi
    // restart tanpa recordUsage, loadStateLocked akan mendeteksi rollover
    // lagi dari file (idempotent). Ini menghindari write yang tidak perlu
    // saat canCallPaidAPI hanya untuk query.
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool DailyBudgetGuard::canCallPaidAPI(double estimated_cost_usd) {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    if (budget_usd_ <= 0.0) {
        // Budget dimatikan = tidak pernah boleh call paid API.
        return false;
    }
    return (spent_usd_ + estimated_cost_usd) <= budget_usd_;
}

void DailyBudgetGuard::recordUsage(double actual_cost_usd) {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    spent_usd_ += actual_cost_usd;

    logBudget("recordUsage($%.6f) -> spent=$%.6f / budget=$%.2f",
              actual_cost_usd, spent_usd_, budget_usd_);

    saveStateLocked();
}

double DailyBudgetGuard::remainingBudgetToday() {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    if (budget_usd_ <= 0.0) return 0.0;
    const double rem = budget_usd_ - spent_usd_;
    return rem > 0.0 ? rem : 0.0;
}

BudgetStatus DailyBudgetGuard::status() {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    BudgetStatus s;
    s.date       = current_date_;
    s.budget_usd = budget_usd_;
    s.spent_usd  = spent_usd_;

    if (budget_usd_ <= 0.0) {
        s.remaining_usd = 0.0;
    } else {
        const double rem = budget_usd_ - spent_usd_;
        s.remaining_usd  = rem > 0.0 ? rem : 0.0;
    }
    return s;
}

} // namespace frans::application
