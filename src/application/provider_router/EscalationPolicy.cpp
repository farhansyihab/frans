// ~/AgenAI/frans/src/application/provider_router/EscalationPolicy.cpp
#include "application/provider_router/EscalationPolicy.h"

namespace frans::application {

RoutingTier EscalationPolicy::decide(const EscalationContext& esc_ctx,
                                      TaskComplexity complexity) const
{
    // ---- 1. Task terlalu kompleks → PaidHeavy langsung ----
    // RequiresDeepReasoning dari PlanState berarti local & Gemini
    // kemungkinan besar tidak cukup. Percobaan di tier bawah hanya
    // membuang waktu + kuota Gemini (rate-limited).
    if (complexity == TaskComplexity::RequiresDeepReasoning) {
        return RoutingTier::PaidHeavy;
    }

    // ---- 2. Task kompleks → minimal PaidLight ----
    // Kalau sudah pernah sampai PaidHeavy di run ini (mis. attempt
    // sebelumnya adalah RequiresDeepReasoning), tetap di PaidHeavy.
    if (complexity == TaskComplexity::Complex) {
        if (esc_ctx.max_tier_reached == RoutingTier::PaidHeavy) {
            return RoutingTier::PaidHeavy;
        }
        return RoutingTier::PaidLight;
    }

    // ---- 3. Sticky: sudah pernah di PaidHeavy, jangan turun ----
    if (esc_ctx.max_tier_reached == RoutingTier::PaidHeavy) {
        return RoutingTier::PaidHeavy;
    }

    // ---- 4. Sticky: sudah pernah di PaidLight ----
    // Bisa naik ke PaidHeavy kalau PaidLight gagal 2x berturut-turut.
    if (esc_ctx.max_tier_reached == RoutingTier::PaidLight) {
        if (esc_ctx.paid_light_fail_streak >= 2) {
            return RoutingTier::PaidHeavy;
        }
        return RoutingTier::PaidLight;
    }

    // ---- 5. Di Local: cek fail streak untuk naik ke PaidLight ----
    if (esc_ctx.local_fail_streak >= 2) {
        return RoutingTier::PaidLight;
    }

    // ---- 6. Default: Local ----
    return RoutingTier::Local;
}

} // namespace frans::application