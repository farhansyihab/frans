// ~/AgenAI/frans/src/application/agent/FixState.h
#pragma once

#include "application/agent/IAgentState.h"

namespace frans::domain {
class ILlmProvider;
class IFileSystemPort;
}

namespace frans::application {

// Blueprint §5.7 — FixState (Stage 7d).
//
// Satu langkah: perbaiki kode yang gagal berdasarkan error observation.
//
// Beda dengan GenerateState:
//   - Prompt BERISI kode lama + observation error.
//   - Meminta model MEMPERBAIKI, bukan menulis dari nol.
//   - Memakai contextSnippets yang sudah dipilih GenerateState — TIDAK
//     memilih ulang (biar konsisten & hemat komputasi).
//
// Alur:
//   1. Validasi: lastGeneratedCode & lastObservation harus terisi.
//   2. Bangun prompt: task + kode lama + observation + instruksi fix.
//   3. provider_.generate() — non-streaming.
//   4. Ekstrak fenced block.
//   5. Overwrite file di sandbox.
//
// Preconditions (dicek di awal run()):
//   - ctx.targetRelativePath  tidak kosong
//   - ctx.lastGeneratedCode   tidak kosong (ada yang diperbaiki)
//   - ctx.lastObservation     tidak kosong (ada error yang dibaca)
//
// Kalau salah satu tidak terpenuhi → fail dengan lastError jelas.
class FixState : public IAgentState {
public:
    FixState(frans::domain::ILlmProvider&    provider,
             frans::domain::IFileSystemPort& fs);

    std::string name() const override { return "Fix"; }
    void run(AgentContext& ctx) override;

private:
    frans::domain::ILlmProvider&    provider_;
    frans::domain::IFileSystemPort& fs_;
};

} // namespace frans::application
