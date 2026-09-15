// ~/AgenAI/frans/src/application/provider_router/EscalationPolicy.h
#pragma once

#include "application/agent/AgentContext.h"   // TaskComplexity

namespace frans::application {

// State eskalasi SATU run Agent (dibuat fresh oleh Agent::run(), TIDAK
// disimpan di AgentContext supaya tidak lintas-run tanpa sengaja).
//
// Kenapa bukan di AgentContext:
//   - AgentContext dipakai lintas-run oleh TaskRunner. Menyimpan
//     local_fail_streak di sana membuka peluang bocor dari run sebelumnya
//     kalau field tidak di-reset dengan benar di PlanState.
//   - Dengan struct terpisah, siklus hidup escalation_state terikat ke
//     satu panggilan Agent::run() — sekali run selesai, state dibuang.
struct EscalationContext {
    int  local_fail_streak = 0;      // jumlah attempt LOKAL berturut-turut
                                     // yang gagal (0 = belum ada / habis reset)
    bool escalated         = false;  // sekali true, tetap true sampai run
                                     // selesai (sticky)
};

// Blueprint §5.3 — EscalationPolicy.
//
// Stateless, pure decision function. Tidak ada state internal; semua
// informasi keputusan datang dari parameter. Ini memudahkan unit test
// (no setup / teardown) dan memastikan tidak ada side-effect tersembunyi
// dari pemanggilan berulang.
//
// Aturan (urutan cek, berhenti di match pertama):
//   1. Task terlalu kompleks untuk local (Complex / RequiresDeepReasoning)
//      → langsung ke paid. Local dilewati TOTAL, bukan "coba dulu".
//   2. Sudah pernah eskalasi di run ini (sticky) → tetap paid.
//      Sekali naik tidak turun lagi sampai run selesai — mencegah
//      oscillasi local↔paid yang bikin biaya naik tak terduga.
//   3. Local sudah gagal 2x berturut-turut → naik ke paid.
//   4. Default: tetap local.
//
// CATATAN: policy ini TIDAK mempertimbangkan ketersediaan paid provider
// atau budget. Itu tanggung jawab ProviderRouter (yang bisa override
// keputusan policy jadi fallback-local kalau paid tidak available atau
// budget habis). Pemisahan ini disengaja supaya policy murni keputusan
// "idealnya routing ke mana", sedangkan router yang menangani realita.
class EscalationPolicy {
public:
    // Return true kalau attempt berikutnya HARUS pakai paid API.
    // Return false kalau tetap pakai local.
    //
    // TIDAK menyentuh esc_ctx — murni baca. Mutasi `esc_ctx.escalated`
    // dilakukan oleh ProviderRouter::selectProvider() setelah budget
    // check lolos (supaya sticky hanya aktif kalau eskalasi BENAR terjadi).
    bool shouldEscalate(const EscalationContext& esc_ctx,
                        TaskComplexity complexity) const;
};

} // namespace frans::application
