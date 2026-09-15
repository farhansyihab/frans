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
// POSIX). std::put_time (bukan snprintf) untuk hindari false-positive
// -Wformat-truncation.
std::string todayLocalDate() {
    const std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm_buf{};
    if (::localtime_r(&t, &tm_buf) == nullptr) {
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

    current_date_ = today;
    spent_usd_    = 0.0;
    free_prompt_tokens_     = 0;
    free_completion_tokens_ = 0;

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
    long long   file_fpt   = 0;
    long long   file_fct   = 0;

    if (j.contains("date") && j["date"].is_string()) {
        file_date = j["date"].get<std::string>();
    }
    if (j.contains("spent_usd") && j["spent_usd"].is_number()) {
        file_spent = j["spent_usd"].get<double>();
    }
    if (j.contains("free_prompt_tokens")
        && j["free_prompt_tokens"].is_number_integer()) {
        file_fpt = j["free_prompt_tokens"].get<long long>();
    }
    if (j.contains("free_completion_tokens")
        && j["free_completion_tokens"].is_number_integer()) {
        file_fct = j["free_completion_tokens"].get<long long>();
    }

    if (file_date != today) {
        logBudget("rollover saat load: file date='%s', today='%s' "
                  "-> semua counter direset ke 0",
                  file_date.empty() ? "(kosong)" : file_date.c_str(),
                  today.c_str());
        return;
    }

    current_date_           = today;
    spent_usd_              = file_spent;
    free_prompt_tokens_     = file_fpt;
    free_completion_tokens_ = file_fct;

    logBudget("state di-load: date=%s spent=$%.6f budget=$%.2f "
              "free_tok=%lld/%lld",
              current_date_.c_str(), spent_usd_, budget_usd_,
              free_prompt_tokens_, free_completion_tokens_);
}

// ---------------------------------------------------------------------------
// Persist
// ---------------------------------------------------------------------------
void DailyBudgetGuard::saveStateLocked() {
    if (state_file_path_.empty()) return;

    std::error_code ec;
    const fs::path p(state_file_path_);
    const fs::path parent = p.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            logBudget("create_directories('%s') gagal: %s "
                      "(tetap coba tulis file)",
                      parent.string().c_str(), ec.message().c_str());
        }
    }

    nlohmann::json j;
    j["date"]                   = current_date_;
    j["spent_usd"]              = spent_usd_;
    j["free_prompt_tokens"]     = free_prompt_tokens_;
    j["free_completion_tokens"] = free_completion_tokens_;

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

    logBudget("rollover: '%s' -> '%s', spent=$%.6f direset ke 0 "
              "(free_tok lama: %lld/%lld)",
              current_date_.c_str(), today.c_str(), spent_usd_,
              free_prompt_tokens_, free_completion_tokens_);

    current_date_           = today;
    spent_usd_              = 0.0;
    free_prompt_tokens_     = 0;
    free_completion_tokens_ = 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
bool DailyBudgetGuard::canCallPaidAPI(double estimated_cost_usd) {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    if (budget_usd_ <= 0.0) {
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

void DailyBudgetGuard::recordFreeTierUsage(int prompt_tokens,
                                            int completion_tokens) {
    std::lock_guard<std::mutex> lk(mtx_);
    checkRolloverLocked();

    if (prompt_tokens > 0)
        free_prompt_tokens_ += static_cast<long long>(prompt_tokens);
    if (completion_tokens > 0)
        free_completion_tokens_ += static_cast<long long>(completion_tokens);

    logBudget("recordFreeTierUsage(prompt=%d, completion=%d) -> "
              "free_total=%lld/%lld",
              prompt_tokens, completion_tokens,
              free_prompt_tokens_, free_completion_tokens_);

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
    s.date                  = current_date_;
    s.budget_usd            = budget_usd_;
    s.spent_usd             = spent_usd_;
    s.free_prompt_tokens    = free_prompt_tokens_;
    s.free_completion_tokens = free_completion_tokens_;

    if (budget_usd_ <= 0.0) {
        s.remaining_usd = 0.0;
    } else {
        const double rem = budget_usd_ - spent_usd_;
        s.remaining_usd  = rem > 0.0 ? rem : 0.0;
    }
    return s;
}

} // namespace frans::application