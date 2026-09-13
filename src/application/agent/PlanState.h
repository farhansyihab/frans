// ~/AgenAI/frans/src/application/agent/PlanState.h
#pragma once

#include "application/agent/IAgentState.h"

namespace frans::application {

// Blueprint §5.7 — PlanState (Stage 7d).
//
// Satu langkah: validasi input + klasifikasi task + reset state.
//
// Alur:
//   1. Pastikan taskDescription & targetRelativePath tidak kosong.
//   2. Klasifikasi TaskComplexity lewat heuristic (keyword matching).
//   3. Set ctx.maxAttempts berdasarkan kompleksitas.
//   4. Reset attempt counter, history, observation, execution state.
//
// Yang TIDAK dilakukan di sini:
//   - Pemilihan provider (Stage 9 — ProviderRouter).
//   - Dekomposisi task jadi sub-task (belum perlu untuk single-file 7d).
//
// Heuristik klasifikasi sengaja sederhana. Kalau ternyata under/over-
// classify, ganti dengan LLM-based classifier di Stage 9 — API-nya
// (TaskComplexity di AgentContext) tidak berubah.
class PlanState : public IAgentState {
public:
    std::string name() const override { return "Plan"; }
    void run(AgentContext& ctx) override;
};

} // namespace frans::application
