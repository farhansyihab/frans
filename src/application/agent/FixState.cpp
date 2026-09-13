// ~/AgenAI/frans/src/application/agent/FixState.cpp
#include "application/agent/FixState.h"

#include "application/agent/AgentContext.h"
#include "application/agent/MultiFileParser.h"
#include "application/context_selector/ContextSelector.h"   // formatContextForPrompt
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

const char* kFixSystemPrompt =
    "You are frans_ai, a local coding assistant. Your previous attempt at "
    "the task FAILED. Analyze the error carefully and produce a corrected "
    "version.\n"
    "\n"
    "Use EXACTLY this format for EACH file:\n"
    "\n"
    "File: <relative_path>\n"
    "```<language>\n"
    "<file content>\n"
    "```\n"
    "\n"
    "Rules:\n"
    "- Fix the SPECIFIC problem described in the error observation.\n"
    "- Do NOT invent unrelated changes.\n"
    "- Preserve the original intent and any existing style.\n"
    "- Output the FULL corrected file(s), not fragments.\n"
    "- Do NOT write any text before the first \"File:\" line or after "
    "the last block.\n";

// Render semua generatedFiles jadi blok teks dengan format File:.
std::string formatGeneratedFiles(const std::vector<GeneratedFile>& files) {
    std::string out;
    for (const auto& f : files) {
        out += "File: " + f.relative_path + "\n";
        out += "```\n";
        out += f.content;
        if (f.content.empty() || f.content.back() != '\n') out += '\n';
        out += "```\n\n";
    }
    return out;
}

std::string buildUserMessage(const AgentContext& ctx) {
    std::string msg;
    msg += "=== Original task ===\n";
    msg += ctx.taskDescription;
    msg += "\n\n";

    msg += "=== Your previous attempt (the file(s) currently on disk) ===\n";
    if (!ctx.generatedFiles.empty()) {
        msg += formatGeneratedFiles(ctx.generatedFiles);
    } else if (!ctx.lastGeneratedCode.empty()) {
        msg += "File: " + ctx.targetRelativePath + "\n";
        msg += "```\n";
        msg += ctx.lastGeneratedCode;
        if (ctx.lastGeneratedCode.back() != '\n') msg += '\n';
        msg += "```\n\n";
    }
    msg += "\n";

    msg += "=== Error observation from running that code ===\n";
    msg += ctx.lastObservation;
    if (ctx.lastObservation.empty() || ctx.lastObservation.back() != '\n') {
        msg += '\n';
    }
    msg += "\n";

    msg += "=== Your task ===\n";
    msg += "Produce the full corrected file(s) using the SAME format. "
           "Fix the specific problem described above.";

    return msg;
}

void syncLastGeneratedCode(AgentContext& ctx) {
    ctx.lastGeneratedCode.clear();
    if (const auto* entry = ctx.findEntryFile()) {
        ctx.lastGeneratedCode = entry->content;
    } else if (!ctx.generatedFiles.empty()) {
        ctx.lastGeneratedCode = ctx.generatedFiles.front().content;
    }
}

} // namespace

FixState::FixState(frans::domain::ILlmProvider&    provider,
                   frans::domain::IFileSystemPort& fs)
    : provider_(provider), fs_(fs) {}

void FixState::run(AgentContext& ctx) {
    ctx.success = false;
    ctx.lastError.clear();

    // ---- 1. Preconditions ----
    if (ctx.targetRelativePath.empty()) {
        ctx.lastError = "FixState: targetRelativePath kosong.";
        return;
    }
    if (ctx.generatedFiles.empty() && ctx.lastGeneratedCode.empty()) {
        ctx.lastError = "FixState: tidak ada kode sebelumnya.";
        return;
    }
    if (ctx.lastObservation.empty()) {
        ctx.lastError = "FixState: tidak ada observasi error.";
        return;
    }

    // ---- 2. System prompt ----
    std::string systemPrompt = kFixSystemPrompt;
    if (!ctx.contextSnippets.empty()) {
        systemPrompt +=
            "\nProject context (for style/signature consistency):\n\n";
        systemPrompt += formatContextForPrompt(ctx.contextSnippets);
    }

    // ---- 3. Messages ----
    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   buildUserMessage(ctx)},
    };

    GenOptions opts;
    opts.max_tokens  = 2048;
    opts.temperature = 0.2;
    opts.top_p       = 0.9;

    // ---- 4. Generate ----
    const GenResult result = provider_.generate(messages, opts);

    if (!result.success) {
        ctx.lastError = "FixState: LLM generate gagal: " + result.error;
        return;
    }

    // ---- 5. Parse ----
    auto parsed = parseModelOutput(result.text, ctx.targetRelativePath);

    if (!parsed.error.empty() && parsed.files.empty()) {
        ctx.lastError = "FixState: parse gagal: " + parsed.error;
        return;
    }
    if (parsed.files.empty()) {
        ctx.lastError = "FixState: model tidak menghasilkan file apapun.";
        return;
    }

    std::fprintf(stderr,
        "[frans][fix] parsed %zu file (multi_file=%d)\n",
        parsed.files.size(), parsed.multi_file ? 1 : 0);
    std::fflush(stderr);

    // ---- 6. Tulis semua file (overwrite) ----
    for (const auto& f : parsed.files) {
        const std::filesystem::path p(f.relative_path);
        const std::filesystem::path parent = p.parent_path();
        if (!parent.empty()) {
            (void)fs_.createDirectory(parent.string());
        }
        if (!fs_.writeFile(f.relative_path, f.content)) {
            ctx.lastError = "FixState: writeFile gagal untuk: " + f.relative_path;
            return;
        }
        std::fprintf(stderr,
            "[frans][fix]   wrote %s (%zu bytes)\n",
            f.relative_path.c_str(), f.content.size());
        std::fflush(stderr);
    }

    ctx.generatedFiles = std::move(parsed.files);
    syncLastGeneratedCode(ctx);

    if (ctx.findEntryFile() == nullptr) {
        ctx.lastError = "FixState: entry point '" + ctx.targetRelativePath
                      + "' tidak ada di antara file yang dihasilkan model.";
        return;
    }

    ctx.success = true;
    ctx.lastError.clear();
}

} // namespace frans::application
