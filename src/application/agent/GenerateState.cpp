// ~/AgenAI/frans/src/application/agent/GenerateState.cpp
#include "application/agent/GenerateState.h"

#include "application/agent/AgentContext.h"
#include "application/agent/CodeExtraction.h"
#include "application/agent/MultiFileParser.h"
#include "application/context_budget/ContextBudgetManager.h"
#include "application/context_selector/ContextSelector.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/GenOptions.h"
#include "domain/llm/ILlmProvider.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace frans::application {

using frans::domain::ChatMessage;
using frans::domain::GenOptions;
using frans::domain::GenResult;

namespace {

// System prompt untuk multi-file response (Stage 7e).
//
// Instruksi format SANGAT eksplisit supaya qwen2.5-coder-7b mengikuti
// konvensi "File: <path>" + fenced block. Konvensi ini dipakai aider
// dan model ini sudah familiar dengannya.
std::string buildSystemPrompt(const std::string& target_path,
                              const std::string& context_block)
{
    std::string p;
    p +=
        "You are frans_ai, a local coding assistant. Produce the file(s) "
        "needed to complete the user's task.\n"
        "\n"
        "Use EXACTLY this format for EACH file you output:\n"
        "\n"
        "File: <relative_path>\n"
        "```<language>\n"
        "<file content>\n"
        "```\n"
        "\n"
        "Rules:\n"
        "- Paths are relative to the project root.\n"
        "- The ENTRY POINT file MUST have path: ";
    p += target_path;
    p += "\n"
         "- You MAY output additional files (tests, helpers).\n"
         "- Each file must be COMPLETE and self-contained (no fragments).\n"
         "- Do NOT write any text before the first \"File:\" line, between "
         "blocks, or after the last block.\n"
         "- The entry point, when run, must exit with code 0 on success "
         "and non-zero on failure.\n";

    if (!context_block.empty()) {
        p += "\nExisting related code from the project (for style/signature "
             "consistency):\n\n";
        p += context_block;
    } else {
        p += "\nNo existing project context available for this request.\n";
    }
    return p;
}

// Set `lastGeneratedCode` dari generatedFiles (backward compat).
void syncLastGeneratedCode(AgentContext& ctx) {
    ctx.lastGeneratedCode.clear();
    if (const auto* entry = ctx.findEntryFile()) {
        ctx.lastGeneratedCode = entry->content;
    } else if (!ctx.generatedFiles.empty()) {
        // Fallback kalau entry tidak ketemu: pakai file pertama.
        ctx.lastGeneratedCode = ctx.generatedFiles.front().content;
    }
}

} // namespace

GenerateState::GenerateState(
    const frans::code_intelligence::DependencyGraph& graph,
    frans::domain::ILlmProvider&                     provider,
    frans::domain::IFileSystemPort&                  fs)
    : graph_(graph), provider_(provider), fs_(fs) {}

void GenerateState::run(AgentContext& ctx) {
    ctx.success = false;
    ctx.lastError.clear();
    ctx.generatedFiles.clear();
    ctx.lastGeneratedCode.clear();
    ctx.contextSnippets.clear();

    // ---- 1. Token budget ----
    ContextBudgetManager budgetMgr;
    const auto alloc = budgetMgr.allocate(4096);
    const int codeBudget = alloc.code_context_tokens;

    // ---- 2. Context selection ----
    ContextSelector selector;
    const auto selection =
        selector.selectContext(ctx.taskDescription, graph_, codeBudget);
    ctx.contextSnippets = selection.snippets;

    std::string context_block;
    if (!selection.snippets.empty()) {
        context_block = formatContextForPrompt(selection.snippets);
    }

    // ---- 3. System prompt + messages ----
    const std::string systemPrompt =
        buildSystemPrompt(ctx.targetRelativePath, context_block);

    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   ctx.taskDescription},
    };

    GenOptions opts;
    opts.max_tokens  = 2048;   // naik dari 1024 → ruang untuk multi-file
    opts.temperature = 0.2;
    opts.top_p       = 0.9;

    // ---- 4. Generate ----
    const GenResult result = provider_.generate(messages, opts);

    if (!result.success) {
        ctx.success   = false;
        ctx.lastError = "LLM generate gagal: " + result.error;
        return;
    }

    // ---- 5. Parse (multi-file, fallback single-file) ----
    auto parsed = parseModelOutput(result.text, ctx.targetRelativePath);

    if (!parsed.error.empty() && parsed.files.empty()) {
        ctx.success   = false;
        ctx.lastError = "Parse gagal: " + parsed.error;
        return;
    }
    if (parsed.files.empty()) {
        ctx.success   = false;
        ctx.lastError = "Model tidak menghasilkan file apapun.";
        return;
    }

    std::fprintf(stderr,
        "[frans][generate] parsed %zu file (multi_file=%d)\n",
        parsed.files.size(), parsed.multi_file ? 1 : 0);
    std::fflush(stderr);

    // ---- 6. Tulis semua file ----
    for (const auto& f : parsed.files) {
        const std::filesystem::path p(f.relative_path);
        const std::filesystem::path parent = p.parent_path();
        if (!parent.empty()) {
            (void)fs_.createDirectory(parent.string());
        }
        if (!fs_.writeFile(f.relative_path, f.content)) {
            ctx.success   = false;
            ctx.lastError = "writeFile gagal untuk: " + f.relative_path;
            return;
        }
        std::fprintf(stderr,
            "[frans][generate]   wrote %s (%zu bytes)\n",
            f.relative_path.c_str(), f.content.size());
        std::fflush(stderr);
    }

    ctx.generatedFiles = std::move(parsed.files);
    syncLastGeneratedCode(ctx);

    // ---- 7. Verifikasi entry point ada di antara file yang ditulis ----
    if (ctx.findEntryFile() == nullptr) {
        ctx.success   = false;
        ctx.lastError = "Entry point '" + ctx.targetRelativePath
                      + "' tidak ada di antara file yang dihasilkan model.";
        return;
    }

    ctx.success = true;
    ctx.lastError.clear();
}

} // namespace frans::application
