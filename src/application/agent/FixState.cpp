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
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace frans::application {

using frans::domain::ChatMessage;
using frans::domain::GenOptions;
using frans::domain::GenResult;

namespace {

// Stage 7g+1 (patch): perkuat instruksi serupa GenerateState.
// "Output the FULL corrected file(s)" terlalu ambigu — model bisa
// menginterpretasikan sebagai "semua file yang kamu lihat". Sekarang
// eksplisit: hanya file yang ada di "Your previous attempt" di
// user message.
const char* kFixSystemPrompt =
    "You are frans_ai, a local coding assistant. Your previous attempt at "
    "the task FAILED. Analyze the error carefully and produce a corrected "
    "version.\n"
    "\n"
    "Use EXACTLY this format for EACH file you output:\n"
    "\n"
    "File: <relative_path>\n"
    "```<language>\n"
    "<file content>\n"
    "```\n"
    "\n"
    "Rules:\n"
    "- Fix the SPECIFIC problem described in the error observation.\n"
    "- Output ONLY files that appeared in \"Your previous attempt\" below, "
    "  OR new files (paths not present anywhere in the project) that the "
    "  fix explicitly requires.\n"
    "- Do NOT output files shown as project context if they were NOT part "
    "  of your previous attempt. Those files were correct as-is — the user "
    "  did NOT ask for changes there.\n"
    "- Preserve ALL existing functions/classes in files you output, unless "
    "  the fix explicitly requires changing them. Edit surgically — do not "
    "  rewrite files from scratch.\n"
    "- Do NOT invent unrelated changes.\n"
    "- Preserve the original intent and any existing style.\n"
    "- Output the FULL corrected file(s), not fragments.\n"
    "- Do NOT write any text before the first \"File:\" line or after "
    "the last block.\n";

// Blueprint Stage 7f — baca file dari DISK, bukan dari memory
// (ctx.generatedFiles). Ini mencegah bug di mana IDE auto-format file
// antara attempt 0 dan attempt 1, sehingga memory dan disk tidak sinkron.
//
// Kumpulkan union dari inScopeFiles (prioritas — entry + file relevan
// lain) dan generatedFiles. Dedup lewat std::set.
std::vector<std::string> collectFilesToRead(const AgentContext& ctx) {
    std::vector<std::string> files;
    std::set<std::string> seen;

    for (const auto& f : ctx.inScopeFiles) {
        if (seen.insert(f).second) files.push_back(f);
    }
    for (const auto& f : ctx.generatedFiles) {
        if (seen.insert(f.relative_path).second) {
            files.push_back(f.relative_path);
        }
    }
    return files;
}

// Render konten file yang dibaca dari disk dengan format File: + fenced
// block. File yang tidak ada di disk (mis. user hapus manual) ditulis
// sebagai blok kosong — model bisa generate ulang.
std::string formatFilesFromDisk(const std::vector<std::string>& paths,
                                frans::domain::IFileSystemPort& fs) {
    std::string out;
    if (paths.empty()) {
        out += "(no files known)\n";
        return out;
    }
    for (const auto& path : paths) {
        const std::string content = fs.readFile(path);
        out += "File: " + path + "\n";
        out += "```\n";
        out += content;
        if (content.empty() || content.back() != '\n') out += '\n';
        out += "```\n\n";
    }
    return out;
}

std::string buildUserMessage(const AgentContext& ctx,
                             frans::domain::IFileSystemPort& fs) {
    std::string msg;
    msg += "=== Original task ===\n";
    msg += ctx.taskDescription;
    msg += "\n\n";

    msg += "=== Your previous attempt (the file(s) currently on disk) ===\n";
    msg += formatFilesFromDisk(collectFilesToRead(ctx), fs);
    msg += "\n";

    msg += "=== Error observation from running that code ===\n";
    msg += ctx.lastObservation;
    if (ctx.lastObservation.empty() || ctx.lastObservation.back() != '\n') {
        msg += '\n';
    }
    msg += "\n";

    msg += "=== Your task ===\n";
    msg += "Produce the full corrected file(s) using the SAME format. "
           "Fix the specific problem described above. Output ONLY files "
           "that were part of \"Your previous attempt\" above — do not "
           "touch files that appeared only as project context.";

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
    if (ctx.generatedFiles.empty() && ctx.lastGeneratedCode.empty()
        && ctx.inScopeFiles.empty()) {
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
            "\nProject context (for style/signature consistency, "
            "READ-ONLY — do NOT output these files):\n\n";
        systemPrompt += formatContextForPrompt(ctx.contextSnippets);
    }

    // ---- 3. Messages ----
    // Stage 7f: buildUserMessage baca DISK (via fs), bukan memory.
    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   buildUserMessage(ctx, fs_)},
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
    // Snapshot pakai fungsi YANG SAMA dengan GenerateState — tidak
    // reimplementasi logic terpisah. Idempotent: kalau sudah tercatat
    // dari GenerateState, tidak overwrite.
    for (const auto& f : parsed.files) {
        const std::filesystem::path p(f.relative_path);
        const std::filesystem::path parent = p.parent_path();
        if (!parent.empty()) {
            (void)fs_.createDirectory(parent.string());
        }

        snapshotBeforeWrite(ctx, fs_, f.relative_path);

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
