// ~/AgenAI/frans/src/application/agent/GenerateState.cpp
#include "application/agent/GenerateState.h"

#include "application/agent/AgentContext.h"
#include "application/context_budget/ContextBudgetManager.h"
#include "application/context_selector/ContextSelector.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/GenOptions.h"
#include "domain/llm/ILlmProvider.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace frans::application {

using frans::domain::ChatMessage;
using frans::domain::GenOptions;
using frans::domain::GenResult;

namespace {

// Ekstrak isi fenced code block pertama (``` ... ```).
//
// Kontrak:
//   - Return false kalau tidak ada "```" pembuka, atau tidak ada newline
//     setelah fence pembuka, atau tidak ada "```" penutup.
//   - Trailing newline di dalam block dipangkas (trim dari belakang).
//   - Bahasa (```python) di fence pembuka diabaikan — yang diambil isi
//     setelah newline pertama.
//
// Dipakai HANYA oleh GenerateState; sengaja tidak diekspos di header.
bool extractFencedCodeBlock(const std::string& modelOutput,
                            std::string& outCode)
{
    const std::size_t start = modelOutput.find("```");
    if (start == std::string::npos) return false;

    const std::size_t lineEnd = modelOutput.find('\n', start);
    if (lineEnd == std::string::npos) return false;

    const std::size_t contentStart = lineEnd + 1;
    const std::size_t end = modelOutput.find("```", contentStart);
    if (end == std::string::npos) return false;

    outCode = modelOutput.substr(contentStart, end - contentStart);

    // Trim trailing newlines (biar file tidak diakhiri blank line berlebih).
    while (!outCode.empty() && outCode.back() == '\n') {
        outCode.pop_back();
    }
    return true;
}

// System prompt statis (Stage 7b). Sengaja konservatif: minta output
// PERSIS satu fenced block, tanpa basa-basi.
const char* kSystemPromptBase =
    "You are frans_ai, a local coding assistant. You write complete, correct "
    "code based on the user's request and the provided code context. Output "
    "ONLY a single fenced code block containing the full file content. Do not "
    "include any explanation before or after the code block. Do not include "
    "multiple code blocks. If context snippets are provided below, use them "
    "to stay consistent with existing code style and function signatures.";

} // namespace

GenerateState::GenerateState(
    const frans::code_intelligence::DependencyGraph& graph,
    frans::domain::ILlmProvider&                     provider,
    frans::domain::IFileSystemPort&                  fs)
    : graph_(graph), provider_(provider), fs_(fs) {}

void GenerateState::run(AgentContext& ctx) {
    // Selalu reset status supaya caller tidak membaca hasil run sebelumnya.
    ctx.success = false;
    ctx.lastError.clear();
    ctx.lastGeneratedCode.clear();
    ctx.contextSnippets.clear();

    // ---- 1. Token budget untuk code context ----
    ContextBudgetManager budgetMgr;
    const auto alloc = budgetMgr.allocate(4096);
    const int codeBudget = alloc.code_context_tokens;

    // ---- 2. Context selection ----
    ContextSelector selector;
    const auto selection =
        selector.selectContext(ctx.taskDescription, graph_, codeBudget);
    ctx.contextSnippets = selection.snippets;

    // ---- 3. System prompt + context appendix ----
    std::string systemPrompt = kSystemPromptBase;
    if (!selection.snippets.empty()) {
        systemPrompt += "\n\nExisting related code from the project:\n\n";
        systemPrompt += formatContextForPrompt(selection.snippets);
    } else {
        systemPrompt +=
            "\n\nNo existing project context available for this request.";
    }

    // ---- 4. Messages ----
    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   ctx.taskDescription},
    };

    // ---- 5. Generation options ----
    GenOptions opts;
    opts.max_tokens  = 1024;
    opts.temperature = 0.2;
    opts.top_p       = 0.9;

    // ---- 6. Generate (non-streaming) ----
    const GenResult result = provider_.generate(messages, opts);

    // ---- 7. Gagal di level provider ----
    if (!result.success) {
        ctx.success   = false;
        ctx.lastError = "LLM generate gagal: " + result.error;
        return;
    }

    // ---- 8. Ekstrak fenced code block ----
    std::string extracted;
    if (!extractFencedCodeBlock(result.text, extracted)) {
        ctx.success   = false;
        ctx.lastError = "Tidak ditemukan fenced code block di response model.";
        return;
    }

    // ---- 10. Parent directory (idempotent, aman dari Stage 7a) ----
    const std::filesystem::path target(ctx.targetRelativePath);
    const std::filesystem::path parent = target.parent_path();
    if (!parent.empty()) {
        (void)fs_.createDirectory(parent.string());
    }

    // ---- 11. Tulis file ----
    if (!fs_.writeFile(ctx.targetRelativePath, extracted)) {
        ctx.success   = false;
        ctx.lastError = "writeFile gagal (ditolak sandbox atau I/O error).";
        return;
    }

    ctx.success           = true;
    ctx.lastGeneratedCode = std::move(extracted);
    ctx.lastError.clear();
}

} // namespace frans::application
