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
// Field `tier` memberi tahu caller tier AKTUAL yang dipakai (bisa
// berbeda dari yang diminta policy kalau ada fallback karena provider
// tidak tersedia / budget blocked). `is_paid` tetap ada untuk backward
// compat dengan kode 2-tier lama: true untuk PaidLight ATAU PaidHeavy.
//
// Urutan field sengaja: provider, is_paid, budget_blocked, tier — supaya
// aggregate init lama `RouteResult{local_, false, false}` tetap valid
// (tier akan default ke RoutingTier::Local).
struct RouteResult {
    frans::domain::ILlmProvider& provider;

    // Backward compat: true kalau provider != local_ (baik PaidLight
    // maupun PaidHeavy). Pakai `tier` untuk diskriminasi lebih presisi.
    bool is_paid = false;

    // True kalau SEHARUSNYA escalation ke paid terjadi (policy +
    // complexity menghendaki), tapi budget harian PaidHeavy habis
    // sehingga fallback (ke PaidLight atau Local). Caller WAJIB
    // memberi tahu user supaya sadar kualitas output mungkin turun.
    bool budget_blocked = false;

    // Tier AKTUAL yang dipilih. Gunakan ini (bukan is_paid) untuk
    // keperluan telemetri / update EscalationContext.
    RoutingTier tier = RoutingTier::Local;
};

// Stage 12 — Blueprint §5.3 — ProviderRouter (3-tier hybrid).
//
// Tugas: pilih provider yang tepat untuk satu request berdasarkan
// kompleksitas + riwayat attempt + budget + ketersediaan paid API.
//
// Keputusan arsitektur:
//   - Hanya SATU provider lokal aktif (llama-server 1 model per proses).
//   - paid_light_ (Gemini) dan paid_heavy_ (DeepSeek) boleh nullptr.
//   - Kalau paid_light_ == nullptr (setup 2-tier), PaidLight request
//     akan fallback ke paid_heavy_ (kalau tersedia) — supaya setup
//     "local + DeepSeek only" tetap bisa escalate.
//   - Kalau paid_heavy_ budget-blocked, fallback ke paid_light_
//     (gratis) — bukan langsung local. Ini memaksimalkan kualitas
//     tanpa biaya tambahan.
//   - Kalau semua paid tidak tersedia → local (dengan flag
//     budget_blocked=true kalau alasan spesifiknya budget).
//
// Router TIDAK menyimpan pointer ke EscalationContext — caller yang
// memegang state-nya.
class ProviderRouter {
public:
    // Constructor 3-tier (Stage 12).
    //   local        : WAJIB. Caller menjamin hidup > router.
    //   paid_light   : Gemini (gratis), boleh nullptr.
    //   paid_heavy   : DeepSeek (berbayar), boleh nullptr.
    //   budget_guard : boleh nullptr (unlimited / testing).
    ProviderRouter(frans::domain::ILlmProvider& local,
                   frans::domain::ILlmProvider*  paid_light,
                   frans::domain::ILlmProvider*  paid_heavy,
                   DailyBudgetGuard*             budget_guard,
                   EscalationPolicy              policy = EscalationPolicy{});

    // Constructor 2-tier (backward compat). `paid` diperlakukan sebagai
    // paid_heavy. paid_light selalu nullptr. Berguna untuk setup yang
    // hanya punya local + DeepSeek.
    ProviderRouter(frans::domain::ILlmProvider& local,
                   frans::domain::ILlmProvider*  paid,
                   DailyBudgetGuard*             budget_guard,
                   EscalationPolicy              policy = EscalationPolicy{});

    ProviderRouter(const ProviderRouter&)            = delete;
    ProviderRouter& operator=(const ProviderRouter&) = delete;

    // Estimasi biaya SEBELUM generate. Pakai paid_heavy_->estimateCost().
    // Return 0.0 kalau paid_heavy_ == nullptr. Estimasi hanya dipakai
    // untuk budget check PaidHeavy (PaidLight gratis — tidak butuh
    // estimasi).
    double estimatePaidCost(const std::string& task_description) const;

    // Pilih provider untuk request berikutnya.
    //
    // `esc_ctx` di-mutate: `max_tier_reached` di-set ke tier yang
    // BENAR-BENAR dipakai, hanya kalau routing berhasil ke tier >= PaidLight.
    // Caller (Agent loop) yang meng-update `local_fail_streak` /
    // `paid_light_fail_streak` setelah tahu hasil attempt.
    RouteResult selectProvider(TaskComplexity    complexity,
                               EscalationContext& esc_ctx,
                               double             estimated_cost_usd);

    // Akses read-only untuk UI / debugging.
    bool                    hasPaidProvider()      const;
    bool                    hasPaidLightProvider() const;
    bool                    hasPaidHeavyProvider() const;
    const EscalationPolicy& policy()               const { return policy_; }

private:
    frans::domain::ILlmProvider& local_;
    frans::domain::ILlmProvider* paid_light_;
    frans::domain::ILlmProvider* paid_heavy_;
    DailyBudgetGuard*            budget_guard_;
    EscalationPolicy             policy_;
};

} // namespace frans::application