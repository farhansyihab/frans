// ~/AgenAI/frans/src/application/agent/FixState.cpp
#include "application/agent/FixState.h"

#include "application/agent/AgentContext.h"
#include "application/agent/CodeExtraction.h"
#include "application/context_selector/ContextSelector.h"   // formatContextForPrompt
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

const char* kFixSystemPrompt =
    "You are frans_ai, a local coding assistant. Your previous attempt at "
    "the task FAILED. Analyze the error carefully and produce a corrected "
    "version of the ENTIRE file. Preserve the original intent and any "
    "existing style shown in the context. Fix the SPECIFIC problem "
    "described in the error observation — do not invent unrelated changes. "
    "Output ONLY a single fenced code block containing the full corrected "
    "file content — no explanation before or after, no multiple blocks.";

// User message: task + kode lama + observation + instruksi.
//
// Struktur sengaja eksplisit dengan header "=== ... ===" supaya model
// mudah membedakan bagian-bagian, dan konsisten antar-run.
std::string buildUserMessage(const AgentContext& ctx) {
    std::string msg;
    msg.reserve(2048 + ctx.lastGeneratedCode.size()
                     + ctx.lastObservation.size());

    msg += "=== Original task ===\n";
    msg += ctx.taskDescription;
    msg += "\n\n";

    msg += "=== Your previous attempt (the code currently in the file) ===\n";
    msg += ctx.lastGeneratedCode;
    if (ctx.lastGeneratedCode.empty()
        || ctx.lastGeneratedCode.back() != '\n') {
        msg += '\n';
    }
    msg += "\n";

    msg += "=== Error observation from running that code ===\n";
    msg += ctx.lastObservation;
    if (ctx.lastObservation.empty()
        || ctx.lastObservation.back() != '\n') {
        msg += '\n';
    }
    msg += "\n";

    msg += "=== Your task ===\n";
    msg += "Produce the full corrected file. Fix the specific problem "
           "described above. Do NOT invent unrelated changes.";

    return msg;
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
    if (ctx.lastGeneratedCode.empty()) {
        ctx.lastError = "FixState: tidak ada kode sebelumnya "
                        "(lastGeneratedCode kosong).";
        return;
    }
    if (ctx.lastObservation.empty()) {
        ctx.lastError = "FixState: tidak ada observasi error "
                        "(lastObservation kosong).";
        return;
    }

    // ---- 2. System prompt + context appendix ----
    // Reuse contextSnippets dari GenerateState — TIDAK memilih ulang.
    std::string systemPrompt = kFixSystemPrompt;
    if (!ctx.contextSnippets.empty()) {
        systemPrompt +=
            "\n\nProject context (for style/signature consistency):\n\n";
        systemPrompt += formatContextForPrompt(ctx.contextSnippets);
    }

    // ---- 3. Messages ----
    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   buildUserMessage(ctx)},
    };

    // ---- 4. Generation options ----
    GenOptions opts;
    opts.max_tokens  = 1024;
    opts.temperature = 0.2;   // fix deterministik lebih penting daripada generate
    opts.top_p       = 0.9;

    // ---- 5. Generate ----
    const GenResult result = provider_.generate(messages, opts);

    if (!result.success) {
        ctx.lastError = "FixState: LLM generate gagal: " + result.error;
        return;
    }

    // ---- 6. Ekstrak fenced block ----
    std::string extracted;
    if (!extractFencedCodeBlock(result.text, extracted)) {
        ctx.lastError = "FixState: tidak ditemukan fenced code block di "
                        "response model.";
        return;
    }

    // ---- 7. Overwrite file di sandbox ----
    const std::filesystem::path target(ctx.targetRelativePath);
    const std::filesystem::path parent = target.parent_path();
    if (!parent.empty()) {
        (void)fs_.createDirectory(parent.string());
    }

    if (!fs_.writeFile(ctx.targetRelativePath, extracted)) {
        ctx.lastError = "FixState: writeFile gagal (sandbox atau I/O error).";
        return;
    }

    ctx.lastGeneratedCode = std::move(extracted);
    ctx.success = true;
    ctx.lastError.clear();
}

} // namespace frans::application
