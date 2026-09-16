// ~/AgenAI/frans/src/application/provider_router/EscalationPolicy.cpp
#include "application/provider_router/EscalationPolicy.h"

namespace frans::application {

RoutingTier EscalationPolicy::decide(const EscalationContext& esc_ctx,
                                      TaskComplexity complexity) const
{
    // ---- Hitung tier normal dulu, lalu terapkan override rate-limit ----
    RoutingTier tier;

    // ---- 1. Task terlalu kompleks → PaidHeavy langsung ----
    if (complexity == TaskComplexity::RequiresDeepReasoning) {
        tier = RoutingTier::PaidHeavy;
    }
    // ---- 2. Complex → minimal PaidLight ----
    else if (complexity == TaskComplexity::Complex) {
        tier = (esc_ctx.max_tier_reached == RoutingTier::PaidHeavy)
            ? RoutingTier::PaidHeavy
            : RoutingTier::PaidLight;
    }
    // ---- 3. Sticky di PaidHeavy ----
    else if (esc_ctx.max_tier_reached == RoutingTier::PaidHeavy) {
        tier = RoutingTier::PaidHeavy;
    }
    // ---- 4. Sticky di PaidLight ----
    else if (esc_ctx.max_tier_reached == RoutingTier::PaidLight) {
        tier = (esc_ctx.paid_light_fail_streak >= 2)
            ? RoutingTier::PaidHeavy
            : RoutingTier::PaidLight;
    }
    // ---- 5. Di Local, cek fail streak untuk naik ke PaidLight ----
    else if (esc_ctx.local_fail_streak >= 2) {
        tier = RoutingTier::PaidLight;
    }
    // ---- 6. Default: Local ----
    else {
        tier = RoutingTier::Local;
    }

    // ---- Stage 12d — rate-limit cooldown override ----
    // Kalau tier yang diinginkan adalah PaidLight tapi Gemini baru saja
    // merespons 429/503, downgrade ke Local. Blueprint §10: rate limit
    // tidak boleh memicu kenaikan ke DeepSeek (biar DeepSeek hemat untuk
    // task berat).
    //
    // PaidHeavy TIDAK terpengaruh: kalau tier sudah PaidHeavy (baik via
    // complexity RequiresDeepReasoning maupun sticky max_tier_reached),
    // kita tetap izinkan.
    if (tier == RoutingTier::PaidLight
        && esc_ctx.paid_light_rate_limit_cooldown > 0) {
        tier = RoutingTier::Local;
    }

    return tier;
}

} // namespace frans::application