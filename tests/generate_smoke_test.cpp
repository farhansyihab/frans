// ~/AgenAI/frans/tests/generate_smoke_test.cpp
//
// Manual smoke test Stage 7b: GenerateState.
//
// PENTING: test ini memanggil llama-server lewat HTTP. Pastikan server
// sudah RUNNING di 127.0.0.1:8080 sebelum menjalankan binary ini.
//
// Cara paling gampang:
//   - Jalankan frans_ai UI, klik "Start Server" pada model 3B/7B, tunggu
//     status jadi RUNNING.
//   - Di terminal lain, jalankan frans_generate_test.
//
// Atau dari CLI kalau punya llama-server di PATH:
//   ~/AgenAI/llama.cpp/build/bin/llama-server -m <model.gguf>
//       --host 127.0.0.1 --port 8080 -c 4096 -t 4 -ngl 0
//
// Test TIDAK assert otomatis — output di-print untuk dibaca manual, karena
// hasil LLM non-deterministik.

#include "application/agent/AgentContext.h"
#include "application/agent/GenerateState.h"
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include "infrastructure/llama/LlamaCppProvider.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

#ifndef FIXTURE_DIR
#define FIXTURE_DIR "tests/fixtures/python"
#endif

using frans::application::AgentContext;
using frans::application::GenerateState;
using frans::code_intelligence::TreeSitterGraphBuilder;
using frans::infrastructure::LlamaCppProvider;
using frans::infrastructure::SandboxedFileSystem;

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    const std::string fixture_root = (argc > 1) ? argv[1] : FIXTURE_DIR;

    std::printf("=== frans_ai generate smoke test (Stage 7b) ===\n");
    std::printf("PENTING: pastikan llama-server sudah jalan di 127.0.0.1:8080 "
                "sebelum menjalankan test ini.\n\n");
    std::printf("fixture root: %s\n", fixture_root.c_str());

    // ---- Sandbox sementara (pola sama seperti sandbox_smoke_test.cpp) ----
    const std::string sandbox_root =
        (fs::temp_directory_path()
         / ("frans_generate_test_" + std::to_string(::getpid()))).string();

    std::error_code ec;
    fs::remove_all(sandbox_root, ec);
    std::printf("sandbox root : %s\n\n", sandbox_root.c_str());

    SandboxedFileSystem fs_port(sandbox_root);

    // ---- Build DependencyGraph dari fixture ----
    TreeSitterGraphBuilder builder;
    builder.build(fixture_root);
    const auto& graph = builder.graph();
    std::printf("graph: files=%zu  symbols=%zu  edges=%zu\n\n",
                graph.fileCount(), graph.symbolCount(), graph.edgeCount());

    if (!builder.errors().empty()) {
        std::printf("build errors:\n");
        for (const auto& e : builder.errors()) {
            std::printf("  %s\n", e.c_str());
        }
        std::printf("\n");
    }

    // ---- Provider (llama-server lokal) ----
    LlamaCppProvider::Config cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 8080;
    // sisanya default (connect_timeout 5s, read_timeout 600s)
    LlamaCppProvider provider(cfg);

    // ---- AgentContext ----
    AgentContext ctx;
    ctx.taskDescription =
        "Add a function named add_numbers(a, b) in Python that returns their "
        "sum, following the style of the existing utils.py file.";
    ctx.targetRelativePath = "generated/add_numbers.py";

    // ---- Run GenerateState ----
    GenerateState state(graph, provider, fs_port);
    std::printf("Running GenerateState...\n");
    state.run(ctx);

    // ---- Print hasil ----
    std::printf("\n--- result ---\n");
    std::printf("success            : %s\n", ctx.success ? "true" : "false");
    if (!ctx.lastError.empty()) {
        std::printf("lastError          : %s\n", ctx.lastError.c_str());
    }
    std::printf("contextSnippets    : %zu\n", ctx.contextSnippets.size());
    for (const auto& s : ctx.contextSnippets) {
        std::printf("   - %s:%d-%d  %s %s\n",
                    s.file_path.c_str(), s.start_line, s.end_line,
                    s.kind.c_str(), s.symbol_name.c_str());
    }
    std::printf("lastGeneratedCode  :\n---8<---\n%s\n--->8---\n",
                ctx.lastGeneratedCode.c_str());

    // ---- Verifikasi tambahan kalau sukses ----
    if (ctx.success) {
        const bool exists = fs_port.exists("generated/add_numbers.py");
        std::printf("\nfs.exists('generated/add_numbers.py') : %s\n",
                    exists ? "true" : "false");

        const std::string content = fs_port.readFile("generated/add_numbers.py");
        std::printf("file size : %zu bytes\n", content.size());

        if (content.find("add_numbers") != std::string::npos) {
            std::printf("PASS  : file mengandung 'add_numbers'\n");
        } else {
            std::printf("WARN  : file TIDAK mengandung 'add_numbers'\n");
        }
    }

    std::printf("\nsandbox tersisa di: %s\n", fs_port.root().string().c_str());
    std::printf("Cek manual dengan : cat %s/generated/add_numbers.py\n",
                fs_port.root().string().c_str());
    return 0;
}
