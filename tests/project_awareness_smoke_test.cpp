// ~/AgenAI/frans/tests/project_awareness_smoke_test.cpp
//
// Smoke test Stage 7f: ProjectGraphCache + TargetScopeResolver +
// GenerateState/FixState disk integration.
//
// Tidak butuh llama-server (MockProvider).
//
// Skenario:
//   1. Graph cache — build & hit.
//   2. Graph cache — ganti root (single-slot behavior).
//   3. Graph cache — file dihapus terdeteksi.
//   4. TargetScopeResolver — explicit user target.
//   5. TargetScopeResolver — auto-detect dari prompt.
//   6. FixState reads disk (spy FS meng-inject modifikasi pada read #2).
//   7. GenerateState snapshot capture.
//   8. In-scope files dilaporkan ke system prompt.

#include "application/agent/Agent.h"
#include "application/agent/AgentContext.h"
#include "application/agent/TargetScopeResolver.h"
#include "application/project/ProjectGraphCache.h"
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/ILlmProvider.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

using namespace frans::application;
using frans::code_intelligence::DependencyGraph;
using frans::code_intelligence::TreeSitterGraphBuilder;
using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::domain::IFileSystemPort;
using frans::domain::ILlmProvider;
using frans::domain::TokenCallback;
using frans::infrastructure::SandboxedFileSystem;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

void banner(const std::string& s) {
    std::printf("\n=== %s ===\n", s.c_str());
}

std::string makeTempDir(const std::string& tag) {
    const std::string p = (fs::temp_directory_path()
        / ("frans_7f_" + tag + "_" + std::to_string(::getpid()))).string();
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p;
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string fenced(const std::string& code) {
    return "```python\n" + code + "\n```\n";
}

// ---------------------------------------------------------------------------
// MockProvider — canned response + rekam messages.
// ---------------------------------------------------------------------------
class MockProvider : public ILlmProvider {
public:
    std::vector<std::string> responses;
    std::function<void(int)> before_return;

    int call_count = 0;
    std::vector<std::string>             received_user_messages;
    std::vector<std::vector<ChatMessage>> received_messages;

    std::string name() const override { return "Mock"; }
    bool isAvailable() const override { return true; }

    GenResult generate(const std::vector<ChatMessage>& messages,
                       const GenOptions&) override
    {
        GenResult r;
        r.success           = true;
        r.elapsed_sec       = 0.001;
        r.prompt_tokens     = 100;
        r.completion_tokens = 50;

        received_messages.push_back(messages);
        for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            if (it->role == "user") {
                received_user_messages.push_back(it->content);
                break;
            }
        }

        const int this_call = call_count;
        ++call_count;

        if (before_return) before_return(this_call);

        if (this_call < static_cast<int>(responses.size())) {
            r.text = responses[this_call];
        } else {
            r.text = fenced("# mock exhausted");
        }
        return r;
    }

    GenResult streamGenerate(const std::vector<ChatMessage>&,
                             const GenOptions&,
                             TokenCallback,
                             const std::atomic<bool>*) override
    {
        GenResult r;
        r.error = "MockProvider does not implement streaming";
        return r;
    }

    CostEstimate estimateCost(const std::vector<ChatMessage>&) const override {
        return {};
    }
};

// ---------------------------------------------------------------------------
// SpyFS — wrap SandboxedFileSystem, hitung read, inject konten pada
// read ke-N untuk path target.
// ---------------------------------------------------------------------------
class SpyFS : public IFileSystemPort {
public:
    explicit SpyFS(const std::string& root) : inner_(root) {}

    bool writeFile(const std::string& path, const std::string& content) override {
        return inner_.writeFile(path, content);
    }

    std::string readFile(const std::string& path) override {
        ++read_counts_[path];
        std::string content = inner_.readFile(path);
        if (path == inject_path_ && read_counts_[path] >= inject_on_or_after_) {
            content = inject_content_ + content;
        }
        return content;
    }

    bool createDirectory(const std::string& path) override {
        return inner_.createDirectory(path);
    }
    bool exists(const std::string& path) override {
        return inner_.exists(path);
    }
    bool deleteFile(const std::string& path) override {
        return inner_.deleteFile(path);
    }

    SandboxedFileSystem         inner_;
    std::map<std::string, int>  read_counts_;
    std::string                 inject_path_;
    std::string                 inject_content_;
    int                         inject_on_or_after_ = 2;
};

} // namespace

int main() {
    std::printf("=== frans_ai project awareness smoke test (Stage 7f) ===\n");
    std::printf("Mock provider — llama-server TIDAK diperlukan.\n");

    // =====================================================================
    // Test 1 — Graph cache: build & hit
    // =====================================================================
    banner("ProjectGraphCache — build & hit");
    {
        const std::string root = makeTempDir("cache_hit");
        writeFile(root + "/a.py", "def f(): pass\n");
        writeFile(root + "/b.py", "class C: pass\n");

        ProjectGraphCache cache;

        const auto& g1 = cache.getOrBuild(root);
        check(g1.fileCount() == 2, "first build: 2 files");
        check(cache.status().build_count == 1, "build_count == 1");

        // Second call, no changes → hit
        const auto& g2 = cache.getOrBuild(root);
        check(cache.status().build_count == 1, "second call: build_count still 1");
        check(&g1 == &g2, "same graph reference (cache hit)");

        // Edit one file → rebuild
        writeFile(root + "/a.py", "def f(): return 42\n");
        // FS mtime resolution bisa 1s; sentuh dengan mtime manual
        std::error_code ec;
        fs::last_write_time(root + "/a.py",
                            fs::file_time_type::clock::now(), ec);

        (void)cache.getOrBuild(root);
        check(cache.status().build_count == 2,
              "after edit: build_count == 2 (rebuild)");
    }

    // =====================================================================
    // Test 2 — Graph cache: ganti root (single-slot)
    // =====================================================================
    banner("ProjectGraphCache — ganti root (single-slot)");
    {
        const std::string root_a = makeTempDir("cache_root_a");
        const std::string root_b = makeTempDir("cache_root_b");
        writeFile(root_a + "/a.py", "def f(): pass\n");
        writeFile(root_b + "/b.py", "def g(): pass\n");

        ProjectGraphCache cache;

        (void)cache.getOrBuild(root_a);
        check(cache.status().build_count == 1, "root_A: build_count == 1");
        check(cache.status().root == root_a, "status.root == root_A");

        (void)cache.getOrBuild(root_b);
        check(cache.status().build_count == 2, "root_B: build_count == 2");
        check(cache.status().root == root_b, "status.root == root_B");

        // Kembali ke root_A → bukan cache hit, karena single-slot
        (void)cache.getOrBuild(root_a);
        check(cache.status().build_count == 3,
              "kembali ke root_A: build_count == 3 (bukan cache hit)");
    }

    // =====================================================================
    // Test 3 — Graph cache: file dihapus terdeteksi
    // =====================================================================
    banner("ProjectGraphCache — file dihapus terdeteksi");
    {
        const std::string root = makeTempDir("cache_del");
        writeFile(root + "/a.py", "def f(): pass\n");
        writeFile(root + "/b.py", "def g(): pass\n");
        writeFile(root + "/c.py", "def h(): pass\n");

        ProjectGraphCache cache;
        (void)cache.getOrBuild(root);
        check(cache.status().build_count == 1, "first build");
        check(cache.status().file_count == 3, "3 files");

        std::error_code ec;
        fs::remove(root + "/b.py", ec);
        check(!ec, "hapus b.py");

        (void)cache.getOrBuild(root);
        check(cache.status().build_count == 2,
              "rebuild setelah hapus file (build_count == 2)");
        check(cache.status().file_count == 2,
              "file_count jadi 2");
    }

    // =====================================================================
    // Build a fixture project for resolver tests.
    // =====================================================================
    const std::string fixture_root = makeTempDir("resolver");
    writeFile(fixture_root + "/geometry.py",
        "import math\n"
        "\n"
        "class Shape:\n"
        "    def area(self):\n"
        "        return 0.0\n"
        "\n"
        "class Circle(Shape):\n"
        "    def __init__(self, radius):\n"
        "        self.radius = radius\n"
        "\n"
        "    def area(self):\n"
        "        return math.pi * self.radius ** 2\n"
        "\n"
        "def describe(shape):\n"
        "    return f\"area={shape.area():.2f}\"\n");
    writeFile(fixture_root + "/main.py",
        "from geometry import Circle, describe\n"
        "\n"
        "def run():\n"
        "    c = Circle(2.0)\n"
        "    return describe(c)\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    print(run())\n");

    TreeSitterGraphBuilder builder;
    builder.build(fixture_root);
    const auto& fixture_graph = builder.graph();
    std::printf("\n[fixture] files=%zu symbols=%zu edges=%zu\n",
                fixture_graph.fileCount(), fixture_graph.symbolCount(),
                fixture_graph.edgeCount());

    // =====================================================================
    // Test 4 — TargetScopeResolver: explicit user target
    // =====================================================================
    banner("TargetScopeResolver — explicit user target");
    {
        TargetScopeResolver resolver;
        const auto scope = resolver.resolve(
            "irrelevant prompt that matches nothing",
            "custom.py",
            fixture_root,
            fixture_graph,
            2000);

        check(scope.primary_entry == "custom.py",
              "primary_entry == 'custom.py'");
        check(scope.entry_from_user,
              "entry_from_user == true");
    }

    // =====================================================================
    // Test 5 — TargetScopeResolver: auto-detect dari prompt
    // =====================================================================
    banner("TargetScopeResolver — auto-detect");
    {
        TargetScopeResolver resolver;
        const auto scope = resolver.resolve(
            "add method circumference to Circle",
            "",
            fixture_root,
            fixture_graph,
            2000);

        std::printf("  primary_entry = '%s'\n", scope.primary_entry.c_str());
        std::printf("  in_scope_files:\n");
        for (const auto& f : scope.in_scope_files) {
            std::printf("    - %s\n", f.c_str());
        }
        std::printf("  seed_symbols:");
        for (const auto& s : scope.seed_symbols) {
            std::printf(" %s", s.c_str());
        }
        std::printf("\n");

        check(scope.primary_entry == "geometry.py",
              "primary_entry == 'geometry.py'");
        check(!scope.entry_from_user,
              "entry_from_user == false");
        bool has_geometry = false;
        for (const auto& f : scope.in_scope_files) {
            if (f == "geometry.py") has_geometry = true;
        }
        check(has_geometry, "in_scope_files mengandung 'geometry.py'");
    }

    // =====================================================================
    // Test 6 — FixState reads disk (spy FS inject modifikasi pada read #2)
    // =====================================================================
    banner("FixState reads disk");
    {
        const std::string sandbox = makeTempDir("fix_disk");
        SpyFS spy(sandbox);
        // Inject "# edited_externally\n" pada read ke-2 (dan seterusnya)
        // untuk existing.py. Read #1 = ExecuteState danger check.
        // Read #2 = FixState. Kalau FixState baca memory (ctx.generatedFiles),
        // ia tidak akan lihat injection ini.
        spy.inject_path_ = "existing.py";
        spy.inject_content_ = "# edited_externally\n";
        spy.inject_on_or_after_ = 2;

        MockProvider provider;
        provider.responses = {
            // Attempt 0: buggy code
            "File: existing.py\n"
            "```python\n"
            "raise ValueError('intentional')\n"
            "```\n",
            // Attempt 1: fixed code
            "File: existing.py\n"
            "```python\n"
            "print('fixed')\n"
            "```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &spy;
        adeps.sandbox_root        = spy.inner_.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "print fixed";
        actx.targetRelativePath = "existing.py";
        actx.inScopeFiles       = {"existing.py"};

        Agent agent(adeps);
        const bool ok = agent.run(actx);

        std::printf("  provider calls = %d\n", provider.call_count);
        std::printf("  final success = %d\n", ok ? 1 : 0);

        check(ok, "agent.run return true");
        check(provider.call_count == 2, "2 LLM calls");

        // Verifikasi: prompt untuk FixState (call 2, index 1) mengandung
        // "# edited_externally" — hanya mungkin kalau FixState baca DISK
        // via spy FS yang meng-inject.
        check(provider.received_user_messages.size() >= 2,
              "received at least 2 user messages");
        const std::string& fix_prompt = provider.received_user_messages[1];
        check(fix_prompt.find("# edited_externally") != std::string::npos,
              "fix prompt mengandung '# edited_externally' (FixState baca disk)");
    }

    // =====================================================================
    // Test 7 — GenerateState snapshot capture
    // =====================================================================
    banner("GenerateState snapshot capture");
    {
        const std::string sandbox = makeTempDir("snapshot");
        // Pre-create existing.py dengan konten lama.
        writeFile(sandbox + "/existing.py", "print('old')\n");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            "File: existing.py\n"
            "```python\n"
            "print('new')\n"
            "```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "replace existing.py";
        actx.targetRelativePath = "existing.py";

        Agent agent(adeps);
        (void)agent.run(actx);

        check(actx.fileSnapshots.count("existing.py") == 1,
              "snapshot untuk existing.py di-capture");
        check(actx.fileSnapshots["existing.py"] == "print('old')\n",
              "snapshot berisi konten SEBELUM write");
    }

    // =====================================================================
    // Test 8 — In-scope files dilaporkan ke system prompt
    // =====================================================================
    banner("In-scope files → system prompt");
    {
        const std::string sandbox = makeTempDir("prompt_scope");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            "File: entry.py\n"
            "```python\n"
            "print('hi')\n"
            "```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "make an entry file";
        actx.targetRelativePath = "entry.py";
        actx.inScopeFiles       = {"entry.py", "helper.py"};

        Agent agent(adeps);
        (void)agent.run(actx);

        check(!provider.received_messages.empty(), "received messages");
        const auto& first_msgs = provider.received_messages[0];
        check(!first_msgs.empty(), "first message present");
        check(first_msgs[0].role == "system", "first msg is system");
        const std::string& sys = first_msgs[0].content;
        check(sys.find("Files currently in scope") != std::string::npos,
              "system prompt contains 'Files currently in scope'");
        check(sys.find("entry.py") != std::string::npos,
              "system prompt mentions entry.py");
        check(sys.find("helper.py") != std::string::npos,
              "system prompt mentions helper.py");
        check(sys.find("[ENTRY POINT]") != std::string::npos,
              "system prompt marks entry point");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
