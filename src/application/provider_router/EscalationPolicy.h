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
// satu run (sticky) kecuali lewat rollback manual oleh caller (jarang).
enum class RoutingTier {
    Local,
    PaidLight,
    PaidHeavy,
};

// State eskalasi SATU run Agent (dibuat fresh oleh Agent::run(), TIDAK
// disimpan di AgentContext supaya tidak bocor lintas-run).
//
// Perubahan Stage 12 dari versi 2-tier:
//   - `escalated` (bool)  ->  `max_tier_reached` (RoutingTier, sticky)
//   - `paid_light_fail_streak` baru (untuk naik PaidLight → PaidHeavy)
//
// Caller (Agent loop) yang meng-update `local_fail_streak` /
// `paid_light_fail_streak` setelah tahu hasil attempt. Router yang
// meng-update `max_tier_reached` setelah routing berhasil.
struct EscalationContext {
    int         local_fail_streak      = 0;   // fail berturut di Local
    int         paid_light_fail_streak = 0;   // fail berturut di Gemini

    // Tier tertinggi yang PERNAH berhasil di-route pada run ini (sticky).
    // Dipakai policy sebagai "don't downgrade" — sekali naik ke
    // PaidLight, attempt berikutnya minimal PaidLight (kecuali caller
    // sengaja reset). Sekali naik ke PaidHeavy, tetap PaidHeavy sampai
    // run selesai.
    RoutingTier max_tier_reached       = RoutingTier::Local;
};

// Blueprint §5.3 — EscalationPolicy (3-tier).
//
// Stateless, pure decision function. Tidak ada state internal; semua
// informasi keputusan datang dari parameter. Memudahkan unit test
// (no setup/teardown) dan menghindari side-effect tersembunyi.
//
// Aturan (urutan cek, berhenti di match pertama):
//   1. RequiresDeepReasoning  → PaidHeavy (langsung, skip local+light).
//   2. Complex                → PaidLight (minimal). Kalau sudah pernah
//                               sampai PaidHeavy (sticky), tetap PaidHeavy.
//   3. max_tier_reached == PaidHeavy → PaidHeavy (sticky).
//   4. max_tier_reached == PaidLight:
//         - paid_light_fail_streak >= 2 → PaidHeavy
//         - selain itu                  → PaidLight
//   5. local_fail_streak >= 2 → PaidLight
//   6. Default → Local
//
// CATATAN: policy TIDAK mempertimbangkan ketersediaan provider / budget.
// Itu tanggung jawab ProviderRouter (yang bisa override keputusan policy
// jadi fallback kalau provider tidak available / budget habis). Pemisahan
// ini disengaja: policy = keputusan IDEAL, router = realita.
class EscalationPolicy {
public:
    // Return tier IDEAL untuk attempt berikutnya.
    //
    // TIDAK menyentuh esc_ctx — murni baca. Mutasi `max_tier_reached`
    // dilakukan oleh ProviderRouter::selectProvider() setelah routing
    // benar-benar terjadi (supaya sticky hanya aktif kalau eskalasi
    // BENAR terjadi).
    RoutingTier decide(const EscalationContext& esc_ctx,
                       TaskComplexity complexity) const;
};

} // namespace frans::application