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
// konvensi "File: <path>" + fenced block. Konvensi ini dipakai
// aider dan model ini sudah familiar dengannya.
//
// Stage 7f: tambah "Files currently in scope" header supaya model
// tahu file apa saja yang dianggap relevan — membantu model
// menghindari menulis file yang tidak perlu, dan menyelaraskan
// pemahaman dengan ContextSelector.
//
// Stage 7g+1 (patch): perkuat instruksi supaya model TIDAK rewrite file
// di luar scope. Sebelumnya baris "- You MAY output additional files
// (tests, helpers)." secara literal meng-invite model menulis file apa
// pun yang dia lihat di "in scope" — perilaku observed di real run
// dengan 3B dan 7B: model rewrite test.py padahal task tidak minta.
// Perbaikan:
//   1. Buang "MAY output additional files" — ganti aturan eksplisit
//      "output ONLY entry point + file yang disebut eksplisit di task".
//   2. Tambah instruksi "preserve existing functions, edit surgically".
//   3. Anotasi per-file: [ENTRY POINT] vs [READ-ONLY].
std::string buildSystemPrompt(const std::string& target_path,
                              const std::string& context_block,
                              const std::vector<std::string>& in_scope_files)
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
         "- Output ONLY the following files:\n"
         "  · the ENTRY POINT file, AND\n"
         "  · any NEW file (path not yet in the project) that the task "
         "explicitly asks you to create.\n"
         "- Do NOT output any other file, even if it appears in the "
         "\"in scope\" list below. Files marked [READ-ONLY] are shown only "
         "as context so you understand the surrounding code — outputting "
         "them will break the user's project and the user did NOT ask for "
         "changes there.\n"
         "- When the ENTRY POINT is an EXISTING file, you MUST preserve "
         "ALL existing functions/classes in it unless the task explicitly "
         "asks to remove them. Edit surgically — do not rewrite the file "
         "from scratch.\n"
         "- Each file must be COMPLETE and self-contained (no fragments).\n"
         "- Do NOT write any text before the first \"File:\" line, between "
         "blocks, or after the last block.\n"
         "- The entry point, when run, must exit with code 0 on success "
         "and non-zero on failure.\n";

    // ---- Stage 7f: files in scope ----
    if (!in_scope_files.empty()) {
        p += "\nFiles currently in scope (relative to project root):\n";
        for (const auto& f : in_scope_files) {
            p += "  - ";
            p += f;
            if (f == target_path) {
                p += "   [ENTRY POINT]";
            } else {
                p += "   [READ-ONLY — do NOT include in your output]";
            }
            p += "\n";
        }
        p += "\nReminder: files marked [READ-ONLY] are context only. "
             "Your output MUST contain ONLY the entry point (and any new "
             "file the task explicitly asks to create).\n";
    }

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
        buildSystemPrompt(ctx.targetRelativePath, context_block,
                          ctx.inScopeFiles);

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
    // Stage 7f: snapshotBeforeWrite dipanggil SEBELUM fs_.writeFile untuk
    // SETIAP file yang akan ditulis — bukan hanya yang di inScopeFiles,
    // karena model bisa saja menghasilkan file baru di luar scope awal.
    for (const auto& f : parsed.files) {
        const std::filesystem::path p(f.relative_path);
        const std::filesystem::path parent = p.parent_path();
        if (!parent.empty()) {
            (void)fs_.createDirectory(parent.string());
        }

        snapshotBeforeWrite(ctx, fs_, f.relative_path);

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
