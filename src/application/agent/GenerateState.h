// ~/AgenAI/frans/src/application/agent/GenerateState.h
#pragma once

#include "application/agent/IAgentState.h"

#include <string>

namespace frans::code_intelligence {
class DependencyGraph;
}

namespace frans::domain {
class ILlmProvider;
class IFileSystemPort;
}

namespace frans::application {

// Blueprint §5.7 — GenerateState (Stage 7b).
//
// Satu langkah: task description (NL) → kode → tulis ke sandbox.
//
// Alur:
//   1. ContextBudgetManager::allocate(4096) → code budget.
//   2. ContextSelector::selectContext(taskDescription, graph, codeBudget)
//      → snippet relevan dari project.
//   3. Susun system prompt + user message.
//   4. provider_.generate(messages, opts) — non-streaming.
//   5. Ekstrak fenced code block dari response.
//   6. createDirectory(parent) lalu writeFile(target, code).
//
// TIDAK ada loop, TIDAK ada evaluasi hasil, TIDAK ada retry di sini.
// Retry / eskalasi provider adalah tanggung jawab state FixState di Stage 7d.
class GenerateState : public IAgentState {
public:
    GenerateState(const frans::code_intelligence::DependencyGraph& graph,
                  frans::domain::ILlmProvider&                    provider,
                  frans::domain::IFileSystemPort&                 fs);

    std::string name() const override { return "Generate"; }
    void run(AgentContext& ctx) override;

private:
    const frans::code_intelligence::DependencyGraph& graph_;
    frans::domain::ILlmProvider&                     provider_;
    frans::domain::IFileSystemPort&                  fs_;
};

} // namespace frans::application
