// ~/AgenAI/frans/src/application/provider_router/EscalationPolicy.h
#pragma once

#include "application/agent/AgentContext.h"   // TaskComplexity

namespace frans::application {

// Stage 12 — 3-tier routing (blueprint §5.3).
//
//   Local      : LlamaCppProvider (3B/7B) — gratis, offline, tapi terbatas
//   PaidLight  : GeminiProvider            — gratis, online, rate-limited
//   PaidHeavy  : DeepSeekProvider          — berbayar, kualitas tertinggi
//
// Urutan naik: Local → PaidLight → PaidHeavy. Tidak pernah turun dalam
// satu run (sticky).
enum class RoutingTier {
    Local,
    PaidLight,
    PaidHeavy,
};

// State eskalasi SATU run Agent.
struct EscalationContext {
    int         local_fail_streak      = 0;
    int         paid_light_fail_streak = 0;
    RoutingTier max_tier_reached       = RoutingTier::Local;

    // Stage 12d — rate-limit cooldown untuk PaidLight.
    //
    // Di-set oleh Agent saat PaidLight (Gemini) merespons HTTP 429 / 503.
    // Selama > 0, EscalationPolicy::decide() men-DOWNGRADE permintaan
    // PaidLight menjadi Local. PaidHeavy TIDAK terpengaruh — kalau policy
    // dasarnya memang meminta PaidHeavy (task RequiresDeepReasoning, atau
    // sticky max_tier_reached=PaidHeavy), itu tetap boleh.
    //
    // Decrement dilakukan Agent setelah setiap attempt non-rate-limit.
    // Nilai 2 = dua attempt berikutnya akan dilewatkan dari PaidLight.
    int paid_light_rate_limit_cooldown = 0;
};

// Blueprint §5.3 — EscalationPolicy (3-tier).
//
// Stateless, pure decision function.
//
// Aturan (urutan cek, berhenti di match pertama):
//   0. Rate-limit cooldown aktif + hasil normal = PaidLight → downgrade
//      ke Local (Stage 12d — blueprint §10: rate limit JANGAN memicu
//      kenaikan ke DeepSeek).
//   1. RequiresDeepReasoning  → PaidHeavy (langsung).
//   2. Complex                → PaidLight (kecuali sticky sudah PaidHeavy).
//   3. max_tier_reached == PaidHeavy → PaidHeavy (sticky).
//   4. max_tier_reached == PaidLight:
//         - paid_light_fail_streak >= 2 → PaidHeavy
//         - selain itu                  → PaidLight
//   5. local_fail_streak >= 2 → PaidLight
//   6. Default → Local
//
// CATATAN: policy tidak mempertimbangkan ketersediaan provider / budget.
// Itu tanggung jawab ProviderRouter.
class EscalationPolicy {
public:
    RoutingTier decide(const EscalationContext& esc_ctx,
                       TaskComplexity complexity) const;
};

} // namespace frans::application