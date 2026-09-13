// ~/AgenAI/frans/src/application/context_budget/ContextBudgetManager.cpp
#include "application/context_budget/ContextBudgetManager.h"

namespace frans::application {

Allocation ContextBudgetManager::allocate(int total_context_window) const {
    Allocation a;
    if (total_context_window <= 0) return a;

    if (total_context_window <= kMinWindowForDefaults) {
        // Window kecil — alokasi proporsional supaya tetap masuk.
        a.system_prompt_tokens         = total_context_window * 25 / 100;
        a.reserved_for_response_tokens = total_context_window * 25 / 100;
        a.code_context_tokens          = total_context_window * 50 / 100;
        // Sisa pembulatan masuk ke history supaya total() persis sama.
        a.conversation_history_tokens  = total_context_window
                                       - a.system_prompt_tokens
                                       - a.reserved_for_response_tokens
                                       - a.code_context_tokens;
        return a;
    }

    a.system_prompt_tokens         = kSystemPromptTokens;
    a.reserved_for_response_tokens = kReservedResponseTokens;

    const int remaining = total_context_window
                        - a.system_prompt_tokens
                        - a.reserved_for_response_tokens;

    a.code_context_tokens         = remaining * 3 / 4;
    a.conversation_history_tokens = remaining - a.code_context_tokens;

    return a;
}

} // namespace frans::application
