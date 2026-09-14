// ~/AgenAI/frans/tests/daily_budget_guard_smoke_test.cpp
//
// Smoke test Stage 9a: DailyBudgetGuard.
//
// Murni unit test — tidak butuh llama-server, tidak butuh API key,
// tidak butuh network. Menggunakan file state di /tmp (unik per run
// via getpid()) supaya test case tidak saling mengotori state.
//
// Skenario:
//   1. Fresh guard → canCallPaidAPI + remaining.
//   2. recordUsage mengurangi remaining.
//   3. Over-budget → canCallPaidAPI return false.
//   4. Restart aplikasi (guard baru, file sama, hari sama) → spent
//      ter-load kembali, bukan reset.
//   5. Rollover hari baru → spent direset ke 0.
//   6. budget = 0 → canCallPaidAPI selalu false.
//   7. File belum pernah dibuat → construct tanpa crash, spent=0.
//   8. Parent directory nested dibuat otomatis saat recordUsage.
//   9. recordUsage menulis file JSON yang bisa dibaca kembali.

#include "application/budget/DailyBudgetGuard.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using frans::application::BudgetStatus;
using frans::application::DailyBudgetGuard;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

void banner(const std::string& s) {
    std::printf("\n=== %s ===\n", s.c_str());
}

bool approx(double a, double b, double eps = 1e-9) {
    const double d = a - b;
    return (d < 0 ? -d : d) < eps;
}

// Path unik per test + per PID → tidak ada tabrakan antar run paralel.
std::string uniqueStatePath(const std::string& tag) {
    return "/tmp/frans_budget_test_" + tag + "_"
         + std::to_string(::getpid()) + ".json";
}

void cleanup(const std::string& path) {
    std::error_code ec;
    fs::remove(path, ec);
}

// Tulis state file JSON langsung (untuk simulasi "kemarin").
void writeRawState(const std::string& path,
                   const std::string& date,
                   double             spent_usd)
{
    // Buat parent kalau belum ada.
    const fs::path p(path);
    const fs::path parent = p.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
    }

    std::ofstream f(path, std::ios::trunc);
    f << "{\n"
      << "  \"date\": \"" << date << "\",\n"
      << "  \"spent_usd\": " << spent_usd << "\n"
      << "}\n";
}

} // namespace

int main() {
    std::printf("=== frans_ai daily budget guard smoke test (Stage 9a) ===\n");

    // =====================================================================
    // Test 1 — fresh guard: budget $1.00
    // =====================================================================
    banner("Fresh guard (budget $1.00)");
    {
        const std::string path = uniqueStatePath("fresh");
        cleanup(path);

        DailyBudgetGuard g(1.00, path);

        check(g.canCallPaidAPI(0.5), "canCallPaidAPI(0.5) == true");
        check(g.canCallPaidAPI(1.0), "canCallPaidAPI(1.0) == true (pas)");
        check(!g.canCallPaidAPI(1.01), "canCallPaidAPI(1.01) == false");

        const auto s = g.status();
        check(s.date.size() == 10, "date format YYYY-MM-DD (10 char)");
        check(s.date[4] == '-' && s.date[7] == '-', "date separator '-'");
        check(approx(s.budget_usd, 1.00), "status.budget == 1.00");
        check(approx(s.spent_usd, 0.0),   "status.spent == 0");
        check(approx(s.remaining_usd, 1.0), "status.remaining == 1.0");
    }

    // =====================================================================
    // Test 2 — recordUsage mengurangi remaining
    // =====================================================================
    banner("recordUsage mengurangi remaining");
    {
        const std::string path = uniqueStatePath("record");
        cleanup(path);

        DailyBudgetGuard g(1.00, path);
        g.recordUsage(0.30);
        check(approx(g.remainingBudgetToday(), 0.70),
              "after recordUsage(0.30): remaining == 0.70");

        g.recordUsage(0.20);
        check(approx(g.remainingBudgetToday(), 0.50),
              "after recordUsage(0.20): remaining == 0.50");

        check(g.canCallPaidAPI(0.49),  "canCall(0.49) == true");
        check(g.canCallPaidAPI(0.50),  "canCall(0.50) == true (pas)");
        check(!g.canCallPaidAPI(0.51), "canCall(0.51) == false");

        const auto s = g.status();
        check(approx(s.spent_usd, 0.50), "status.spent == 0.50");
    }

    // =====================================================================
    // Test 3 — over-budget
    // =====================================================================
    banner("Over-budget → canCall false");
    {
        const std::string path = uniqueStatePath("over");
        cleanup(path);

        DailyBudgetGuard g(1.00, path);
        g.recordUsage(0.5);
        g.recordUsage(0.6);   // total 1.1 > 1.0

        check(!g.canCallPaidAPI(0.0),
              "over-budget: canCall(0.0) == false");
        check(approx(g.remainingBudgetToday(), 0.0),
              "over-budget: remaining di-clamp ke 0");

        const auto s = g.status();
        check(approx(s.spent_usd, 1.1), "status.spent == 1.1 (bukan di-clamp)");
        check(approx(s.remaining_usd, 0.0), "status.remaining == 0.0");
    }

    // =====================================================================
    // Test 4 — restart aplikasi (hari yang sama) → spent ter-load
    // =====================================================================
    banner("Restart aplikasi (hari sama) → spent ter-load");
    {
        const std::string path = uniqueStatePath("restart");
        cleanup(path);

        {
            DailyBudgetGuard g1(1.00, path);
            g1.recordUsage(0.42);
            check(approx(g1.remainingBudgetToday(), 0.58),
                  "guard-1: remaining == 0.58");
        }   // g1 destructed, tapi state file tetap ada.

        DailyBudgetGuard g2(1.00, path);
        check(approx(g2.status().spent_usd, 0.42),
              "guard-2: spent ter-load dari file (0.42)");
        check(approx(g2.remainingBudgetToday(), 0.58),
              "guard-2: remaining == 0.58 (bukan 1.00)");
        check(!g2.canCallPaidAPI(0.59),
              "guard-2: canCall(0.59) == false");
        check(g2.canCallPaidAPI(0.58),
              "guard-2: canCall(0.58) == true (pas)");

        cleanup(path);
    }

    // =====================================================================
    // Test 5 — rollover hari baru
    // =====================================================================
    banner("Rollover hari baru → spent direset");
    {
        const std::string path = uniqueStatePath("rollover");
        cleanup(path);

        // Tulis manual file dengan date kemarin & spent tinggi.
        writeRawState(path, "2020-01-01", 999.0);

        DailyBudgetGuard g(1.00, path);

        check(approx(g.status().spent_usd, 0.0),
              "rollover: spent == 0 (bukan 999)");
        check(approx(g.remainingBudgetToday(), 1.0),
              "rollover: remaining == 1.0");
        check(g.canCallPaidAPI(0.5),
              "rollover: canCall(0.5) == true");

        // Tanggal di status harus hari ini, bukan "2020-01-01".
        const auto s = g.status();
        check(s.date != "2020-01-01",
              "rollover: status.date != '2020-01-01'");

        cleanup(path);
    }

    // =====================================================================
    // Test 6 — budget = 0 → selalu false
    // =====================================================================
    banner("Budget dimatikan (budget = 0) → canCall selalu false");
    {
        const std::string path = uniqueStatePath("zero");
        cleanup(path);

        DailyBudgetGuard g(0.0, path);
        check(!g.canCallPaidAPI(0.0),
              "budget=0: canCall(0.0) == false");
        check(!g.canCallPaidAPI(0.000001),
              "budget=0: canCall(0.000001) == false");
        check(!g.canCallPaidAPI(1.0),
              "budget=0: canCall(1.0) == false");

        check(approx(g.remainingBudgetToday(), 0.0),
              "budget=0: remaining == 0");

        // recordUsage tetap boleh (defensive — caller salah), tapi tidak
        // akan pernah bisa call paid API lagi karena budget=0.
        g.recordUsage(0.5);
        check(!g.canCallPaidAPI(0.0),
              "budget=0 + record: canCall(0.0) tetap false");
    }

    // =====================================================================
    // Test 7 — file tidak ada sama sekali
    // =====================================================================
    banner("File state belum pernah dibuat → construct tanpa crash");
    {
        const std::string path = uniqueStatePath("missing");
        cleanup(path);
        // Pastikan benar-benar tidak ada.
        check(!fs::exists(path), "pre: file benar-benar tidak ada");

        DailyBudgetGuard g(1.00, path);
        check(approx(g.status().spent_usd, 0.0),
              "missing file: spent == 0");
        check(g.canCallPaidAPI(0.5),
              "missing file: canCall(0.5) == true");

        // recordUsage tetap berhasil membuat file.
        g.recordUsage(0.25);
        check(fs::exists(path),
              "missing file: file dibuat setelah recordUsage");

        cleanup(path);
    }

    // =====================================================================
    // Test 8 — parent directory nested dibuat otomatis
    // =====================================================================
    banner("Parent directory nested dibuat otomatis");
    {
        const std::string dir =
            "/tmp/frans_budget_deep_" + std::to_string(::getpid());
        const std::string path = dir + "/sub1/sub2/budget_state.json";

        std::error_code ec;
        fs::remove_all(dir, ec);

        DailyBudgetGuard g(1.00, path);
        g.recordUsage(0.1);

        check(fs::exists(path, ec),
              "nested parent dirs dibuat + file tertulis");
        check(fs::is_directory(dir + "/sub1/sub2", ec),
              "sub1/sub2 benar-benar directory");

        cleanup(path);
        fs::remove_all(dir, ec);
    }

    // =====================================================================
    // Test 9 — file JSON yang ditulis bisa dibaca kembali
    // =====================================================================
    banner("State file round-trip (JSON format)");
    {
        const std::string path = uniqueStatePath("roundtrip");
        cleanup(path);

        {
            DailyBudgetGuard g(2.50, path);
            g.recordUsage(0.777);
        }

        // Baca file mentah, verifikasi isinya.
        std::ifstream f(path);
        check(static_cast<bool>(f), "file bisa dibuka untuk read");

        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        check(content.find("\"date\"")      != std::string::npos,
              "file mengandung key 'date'");
        check(content.find("\"spent_usd\"") != std::string::npos,
              "file mengandung key 'spent_usd'");
        check(content.find("0.777")         != std::string::npos,
              "file mengandung nilai 0.777");

        // Guard baru dari file yang sama → spent == 0.777.
        DailyBudgetGuard g2(2.50, path);
        check(approx(g2.status().spent_usd, 0.777),
              "reload: spent == 0.777");

        cleanup(path);
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
