// ~/AgenAI/frans/src/application/provider_router/ProviderRouter.h
#pragma once

#include "application/agent/AgentContext.h"           // TaskComplexity
#include "application/provider_router/EscalationPolicy.h"

#include <string>

namespace frans::domain {
class ILlmProvider;
}

namespace frans::application {

class DailyBudgetGuard;

// Hasil keputusan routing satu request.
//
// `provider` direferensikan (bukan di-own) — caller TIDAK boleh menyimpan
// referensi ini melewati lifetime ProviderRouter atau provider itu sendiri.
// Semua provider di-own oleh caller (AgentPanel / Agent / test harness).
//
// Aggregate init didukung (no user-declared constructors): caller boleh
// menulis `RouteResult{local_, false, false}`.
struct RouteResult {
    frans::domain::ILlmProvider& provider;

    // True kalau provider yang dipilih adalah paid (DeepSeek/Gemini).
    // Caller pakai flag ini untuk:
    //   - track biaya (via DailyBudgetGuard::recordUsage setelah generation)
    //   - log / telemetry
    //   - UI feedback ("escalated to paid API")
    bool is_paid = false;

    // True kalau SEHARUSNYA paid (policy bilang ya) tapi budget habis
    // sehingga fallback ke local. Caller WAJIB melihat ini dan memberi
    // tahu user (bukan silently degrade). Stderr log sudah otomatis di
    // ProviderRouter, tapi UI juga harus menampilkan supaya user sadar
    // kualitas output mungkin turun karena batas biaya.
    bool budget_blocked = false;
};

// Blueprint §5.3 — ProviderRouter (hybrid local + paid API).
//
// Tugas: pilih provider yang tepat untuk satu request berdasarkan
// kompleksitas + riwayat attempt + budget + ketersediaan paid API.
//
// Keputusan arsitektur (blueprint §5.3 diskusi):
//   - Hanya SATU provider lokal aktif dalam satu waktu. llama-server
//     cuma bisa menjalankan satu model per proses — jadi 3B vs 7B adalah
//     urusan LlamaPanel / user, BUKAN urusan router.
//   - ProviderRouter TIDAK membedakan 3B vs 7B: ia hanya router antara
//     "local" (satu ILlmProvider&) vs "paid" (satu ILlmProvider*, nullable).
//   - paid boleh nullptr: user mungkin belum konfigurasi API key DeepSeek
//     → router tetap berfungsi (selalu local).
//   - budget_guard boleh nullptr: budget tracking bisa dimatikan
//     (mis. testing / user memilih "unlimited"). Dalam kasus itu,
//     paid selalu diizinkan (kalau tersedia).
//
// Router TIDAK menyimpan pointer ke EscalationContext — caller memegang
// state-nya. Ini menjaga EscalationContext tetap satu-run dan mencegah
// state bocor antar-run.
class ProviderRouter {
public:
    // local        : WAJIB. Referensi, caller menjamin hidup > router.
    // paid         : boleh nullptr.
    // budget_guard : boleh nullptr.
    // policy       : default-constructed kalau tidak diisi.
    ProviderRouter(frans::domain::ILlmProvider& local,
                   frans::domain::ILlmProvider*  paid,
                   DailyBudgetGuard*             budget_guard,
                   EscalationPolicy              policy = EscalationPolicy{});

    ProviderRouter(const ProviderRouter&)            = delete;
    ProviderRouter& operator=(const ProviderRouter&) = delete;

    // Estimasi kasar biaya SEBELUM generate, dari task_description saja.
    // Bukan full prompt — cukup untuk keputusan budget kasar; precision
    // tinggi tidak dibutuhkan di titik ini (kalau cost sesungguhnya lebih
    // besar, DailyBudgetGuard::recordUsage akan meng-update spent yang
    // akan memblokir panggilan berikutnya).
    //
    // Delegasi ke paid_->estimateCost({user: task_description}) dan
    // mengembalikan total_usd(). Return 0.0 kalau paid_ == nullptr.
    double estimatePaidCost(const std::string& task_description) const;

    // Pilih provider untuk request berikutnya.
    //
    // `esc_ctx` di-mutate: `escalated` di-set true KALAU dan HANYA KALAU
    // routing benar-benar ke paid. Caller (Agent loop) yang meng-update
    // `local_fail_streak` setelah tahu hasil attempt — router TIDAK
    // menyentuh field itu sama sekali.
    //
    // Logika (urutan):
    //   want_paid = policy_.shouldEscalate(esc_ctx, complexity)
    //   if want_paid && paid_ != nullptr && paid_->isAvailable():
    //       budget_ok = (budget_guard_ == nullptr) ? true
    //                   : budget_guard_->canCallPaidAPI(estimated_cost_usd)
    //       if budget_ok:
    //           esc_ctx.escalated = true
    //           return { *paid_, true, false }
    //       else:
    //           [log warning ke stderr]
    //           return { local_, false, true }   // budget_blocked
    //   else:
    //       return { local_, false, false }
    //
    // Catatan: kalau paid_ == nullptr ATAU !paid_->isAvailable(), kita
    // TIDAK set budget_blocked=true — itu bukan soal budget, tapi soal
    // provider tidak tersedia. Caller bisa bedakan dari flag ini.
    RouteResult selectProvider(TaskComplexity    complexity,
                               EscalationContext& esc_ctx,
                               double             estimated_cost_usd);

    // Akses read-only untuk UI / debugging.
    bool                    hasPaidProvider() const { return paid_ != nullptr; }
    const EscalationPolicy& policy()          const { return policy_; }

private:
    frans::domain::ILlmProvider& local_;
    frans::domain::ILlmProvider* paid_;
    DailyBudgetGuard*            budget_guard_;
    EscalationPolicy             policy_;
};

} // namespace frans::application
