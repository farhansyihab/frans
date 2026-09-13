// ~/AgenAI/frans/src/application/context_budget/ContextBudgetManager.h
#pragma once

namespace frans::application {

// Alokasi token untuk satu request ke LLM. Total() harus == context window
// yang di-request (tidak ada token yang "menguap").
struct Allocation {
    int system_prompt_tokens         = 0;
    int code_context_tokens          = 0;
    int conversation_history_tokens  = 0;
    int reserved_for_response_tokens = 0;

    int total() const {
        return system_prompt_tokens
             + code_context_tokens
             + conversation_history_tokens
             + reserved_for_response_tokens;
    }
};

// Blueprint §5.5 — Context Budget Manager.
//
// Murni perhitungan, tanpa state. Konstanta dibuat `static constexpr` supaya
// mudah di-tune; nilai default mengikuti rekomendasi blueprint untuk model
// 7B dengan window 4096.
//
// Strategi default:
//   window <= 1000  : alokasi proporsional (25% system, 25% response,
//                     50% code, 0% history)
//   window >  1000  : system=300, response=800, sisa dibagi 3:1 antara
//                     code context (prioritas) dan conversation history
//
// Contoh: window 4096 → system=300, response=800, code=2247, history=749.
class ContextBudgetManager {
public:
    static constexpr int kSystemPromptTokens     = 300;
    static constexpr int kReservedResponseTokens = 800;
    static constexpr int kMinWindowForDefaults   = 1000;

    // Return Alloc untuk window `total_context_window` token.
    // window <= 0 → semua nol.
    Allocation allocate(int total_context_window) const;
};

} // namespace frans::application
