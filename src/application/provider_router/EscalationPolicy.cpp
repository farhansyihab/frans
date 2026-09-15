// ~/AgenAI/frans/src/application/provider_router/EscalationPolicy.cpp
#include "application/provider_router/EscalationPolicy.h"

namespace frans::application {

bool EscalationPolicy::shouldEscalate(const EscalationContext& esc_ctx,
                                      TaskComplexity complexity) const
{
    // ---- 1. Task kompleks → langsung paid, skip local ----
    // Kompleksitas "Complex" dan "RequiresDeepReasoning" dari PlanState
    // mengimplikasikan bahwa local 7B kemungkinan besar tidak cukup, dan
    // mencoba local dulu hanya membuang waktu (retry + timeout) sebelum
    // akhirnya tetap naik ke paid. Jadi langsung skip.
    if (complexity == TaskComplexity::Complex
        || complexity == TaskComplexity::RequiresDeepReasoning) {
        return true;
    }

    // ---- 2. Sticky: kalau sudah pernah eskalasi di run ini, tetap paid ----
    // Mencegah oscillasi: kalau attempt sebelumnya sudah naik ke paid,
    // attempt berikutnya di run yang sama tidak turun kembali ke local.
    if (esc_ctx.escalated) {
        return true;
    }

    // ---- 3. Local sudah gagal 2x berturut-turut → naik ----
    // "Berturut-turut" di sini bermakna: caller (Agent) yang menghitung
    // local_fail_streak. Kalau attempt sebelumnya sukses, caller reset
    // ke 0. Kalau gagal, increment. Policy hanya threshold check.
    if (esc_ctx.local_fail_streak >= 2) {
        return true;
    }

    // ---- 4. Default: tetap local ----
    return false;
}

} // namespace frans::application
