// ~/AgenAI/frans/src/application/agent/ObserveState.h
#pragma once

#include "application/agent/IAgentState.h"

namespace frans::application {

// Blueprint §5.7 — ObserveState (Stage 7d).
//
// Satu langkah: menganalisis ctx.lastExecution dan menghasilkan
// ctx.lastObservation yang terstruktur. Tujuan utamanya adalah
// meng-ekstrak sinyal error dari stderr supaya FixState bisa mengoreksi
// secara spesifik, bukan sekadar "generate ulang random".
//
// Format observasi (Python):
//   Execution failed with exit code 1.
//   Exception: ValueError
//   Message: oops
//   Line: 1 (in <module>)
//   stderr (tail, last 500 chars):
//   Traceback (most recent call last):
//     File "boom.py", line 1, in <module>
//       raise ValueError('oops')
//   ValueError: oops
//
// Kategori yang dikenali:
//   - Success   : is_success() → observation singkat "Execution succeeded".
//   - Timeout   : lastExecution.timed_out → "TIMEOUT: ...".
//   - SyntaxError: parsing stderr untuk "SyntaxError: ..." (muncul
//                  SEBELUM traceback).
//   - Python traceback: "Traceback (most recent call last):" + exception
//                       terakhir + line number.
//   - Unknown   : fallback — stderr tail mentah.
//
// Kalau ctx.lastExecution.ran == false (dipotong sebelum runCommand —
// misal di-block danger check), observation menyebut alasan dari
// ctx.lastError sebelumnya. Ini mencegah ObserveState menelan informasi
// penting.
class ObserveState : public IAgentState {
public:
    std::string name() const override { return "Observe"; }
    void run(AgentContext& ctx) override;
};

} // namespace frans::application
