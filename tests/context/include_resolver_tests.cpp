// tests/context/include_resolver_tests.cpp
//
// Fase E #74 -- test IncludeResolver (local include closure).
//
// Cakupan:
//   - parseQuotedIncludes : kutip vs <>, komentar, spasi, CRLF, include_next
//   - normalizeRelPath    : ".", "..", backslash, absolut, keluar root
//   - isIncludeScannable  : ekstensi C/C++
//   - closure (in-memory) : kedalaman, siklus, budget, not_found, ambiguous,
//                           skipped, disabled, cancel, seed non-C++
//   - closure (disk)      : lewat SandboxedFileSystem + temp dir
//   - mergeIncludeClosure : Entry lengkap & Entry minimal (path + selected)
//
// Tanpa FLTK, tanpa jaringan, tanpa subprocess, tanpa git.
#include "application/context/IncludeResolver.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace frans::application;

namespace {

int g_checks   = 0;
int g_failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        ++g_checks;                                                           \
        if (!(cond)) {                                                        \
            ++g_failures;                                                     \
            std::fprintf(stderr, "[FAIL] %s:%d  %s\n", __FILE__, __LINE__,    \
                         #cond);                                              \
        }                                                                     \
    } while (0)

using Strings = std::vector<std::string>;

// --- Fixture in-memory ------------------------------------------------------

struct MemRepo {
    std::map<std::string, std::string> files;
    std::vector<CollectedFile> all;

    void add(const std::string& path, const std::string& content) {
        files[path] = content;
    }
    void addSkipped(const std::string& path, const std::string& reason) {
        CollectedFile f;
        f.relative_path = path;
        f.skipped       = true;
        f.skip_reason   = reason;
        f.size_bytes    = 999999;
        extra.push_back(f);
    }
    void finalize() {
        all.clear();
        for (const auto& [p, c] : files) {
            CollectedFile f;
            f.relative_path = p;
            f.size_bytes    = c.size();
            all.push_back(f);
        }
        for (const auto& f : extra) all.push_back(f);
    }
    IncludeFileReader reader() const {
        return [this](const std::string& rel, std::string& out) {
            const auto it = files.find(rel);
            if (it == files.end()) return false;
            out = it->second;
            return true;
        };
    }
    std::vector<CollectedFile> extra;
};

Strings paths(const IncludeClosureResult& r) {
    Strings p;
    for (const auto& a : r.additions) p.push_back(a.path);
    return p;
}

IncludeClosureOptions enabledOpt() {
    IncludeClosureOptions o;
    o.enabled = true;
    return o;
}

// --- Temp dir RAII ----------------------------------------------------------

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("frans_include_test_" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    void write(const std::string& rel, const std::string& content) const {
        const auto full = path / rel;
        std::filesystem::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << content;
    }
};

// --- Entry untuk test merge -------------------------------------------------

struct FullEntry {
    std::string path;
    std::string priority;
    std::string reason;
    std::size_t size_bytes = 0;
    bool        selected   = true;
};

struct MinimalEntry {
    std::string path;
    bool        selected = true;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testParse() {
    CHECK(parseQuotedIncludes("#include \"a.h\"\n") == Strings({"a.h"}));
    CHECK(parseQuotedIncludes("#include <vector>\n").empty());
    CHECK(parseQuotedIncludes("#  include   \"sp.h\"\n") == Strings({"sp.h"}));
    CHECK(parseQuotedIncludes("#include\"nospace.h\"\n") == Strings({"nospace.h"}));
    CHECK(parseQuotedIncludes("   \t#include \"indented.h\"\n") == Strings({"indented.h"}));
    CHECK(parseQuotedIncludes("#include \"a.h\" // komentar\n") == Strings({"a.h"}));
    CHECK(parseQuotedIncludes("// #include \"hidden.h\"\n").empty());
    CHECK(parseQuotedIncludes("#include_next \"n.h\"\n").empty());
    CHECK(parseQuotedIncludes("#include MACRO_NAME\n").empty());
    CHECK(parseQuotedIncludes("#define X \"a.h\"\n").empty());
    CHECK(parseQuotedIncludes("#include \"\"\n").empty());
    CHECK(parseQuotedIncludes("#include \"unterminated.h\n").empty());

    // CRLF
    CHECK(parseQuotedIncludes("#include \"a.h\"\r\n#include \"b.h\"\r\n") ==
          Strings({"a.h", "b.h"}));

    // Komentar blok multi-baris + include setelah komentar satu baris.
    const std::string src =
        "/*\n"
        "#include \"hidden.h\"\n"
        "*/\n"
        "#include \"shown.h\" /* c */\n"
        "/* x */ #include \"after_comment.h\"\n";
    CHECK(parseQuotedIncludes(src) == Strings({"shown.h", "after_comment.h"}));

    // Urutan kemunculan dipertahankan, include tanpa newline akhir tetap terbaca.
    CHECK(parseQuotedIncludes("#include \"z.h\"\n#include <x>\n#include \"y.h\"") ==
          Strings({"z.h", "y.h"}));

    // "/*" di dalam string tidak membuka komentar.
    CHECK(parseQuotedIncludes("const char* s = \"/*\";\n#include \"ok.h\"\n") ==
          Strings({"ok.h"}));
}

void testNormalize() {
    auto r = normalizeRelPath("a/./b/../c");
    CHECK(r.has_value() && *r == "a/c");

    r = normalizeRelPath("a\\b\\c.h");
    CHECK(r.has_value() && *r == "a/b/c.h");

    r = normalizeRelPath("a//b");
    CHECK(r.has_value() && *r == "a/b");

    r = normalizeRelPath(".");
    CHECK(r.has_value() && r->empty());

    CHECK(!normalizeRelPath("").has_value());
    CHECK(!normalizeRelPath("../x").has_value());
    CHECK(!normalizeRelPath("a/../../x").has_value());
    CHECK(!normalizeRelPath("/abs/x").has_value());
    CHECK(!normalizeRelPath("C:\\x\\y").has_value());
}

void testScannable() {
    CHECK(isIncludeScannable("src/a.cpp"));
    CHECK(isIncludeScannable("src/a.H"));
    CHECK(isIncludeScannable("a.hpp"));
    CHECK(isIncludeScannable("a.inl"));
    CHECK(!isIncludeScannable("CMakeLists.txt"));
    CHECK(!isIncludeScannable("README.md"));
    CHECK(!isIncludeScannable("Makefile"));
    CHECK(!isIncludeScannable("dir.with.dot/Makefile"));
}

MemRepo makeChainRepo() {
    MemRepo m;
    m.add("src/ui/a.cpp",
          "#include \"a.h\"\n#include \"application/x.h\"\n#include <vector>\n#include \"missing.h\"\n");
    m.add("src/ui/a.h", "#pragma once\n#include \"../util/b.h\"\n");
    m.add("src/util/b.h", "#include \"c.h\"\n#include \"a.h\"\n");   // a.h -> siklus
    m.add("src/util/c.h", "// leaf\n");
    m.add("src/application/x.h", "#include \"deep1.h\"\n");
    m.add("src/application/deep1.h", "#include \"deep2.h\"\n");
    m.add("src/application/deep2.h", "// leaf\n");
    m.add("README.md", "#include \"ignored.h\"\n");
    m.finalize();
    return m;
}

void testClosureDepth() {
    MemRepo m = makeChainRepo();
    const Strings seed = {"src/ui/a.cpp"};

    IncludeClosureOptions o = enabledOpt();   // depth 3
    auto r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(r.ok);
    CHECK(paths(r) == Strings({"src/ui/a.h", "src/application/x.h", "src/util/b.h",
                               "src/application/deep1.h", "src/util/c.h",
                               "src/application/deep2.h"}));
    CHECK(r.additions.size() == 6);
    CHECK(r.additions[0].depth == 1);
    CHECK(r.additions[0].included_by == "src/ui/a.cpp");
    CHECK(r.additions[0].directive == "a.h");
    CHECK(r.additions[2].depth == 2);
    CHECK(r.additions[5].depth == 3);

    // Siklus (b.h -> a.h) tidak menambah entri ganda; missing.h dilaporkan.
    CHECK(r.unresolved.size() == 1);
    CHECK(r.unresolved[0].directive == "missing.h");
    CHECK(r.unresolved[0].reason == "not_found");
    CHECK(r.dropped_budget.empty());

    o.max_depth = 1;
    r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(paths(r) == Strings({"src/ui/a.h", "src/application/x.h"}));

    o.max_depth = 2;
    r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(r.additions.size() == 4);

    o.max_depth = 0;
    r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(r.ok && r.additions.empty());
}

void testClosureSeedHandling() {
    MemRepo m = makeChainRepo();

    // Seed non-C++ tidak dipindai (README.md berisi #include palsu).
    auto r = resolveIncludeClosureWith(m.reader(), {"README.md"}, m.all, enabledOpt());
    CHECK(r.ok && r.additions.empty());

    // Seed yang sudah terpilih tidak ditambahkan lagi.
    r = resolveIncludeClosureWith(m.reader(), {"src/ui/a.cpp", "src/ui/a.h"},
                                  m.all, enabledOpt());
    CHECK(r.ok);
    for (const auto& a : r.additions) CHECK(a.path != "src/ui/a.h");

    // Seed tidak ada di reader / path jahat: tidak crash.
    r = resolveIncludeClosureWith(m.reader(), {"nope.cpp", "../evil.cpp", "/abs.cpp", ""},
                                  m.all, enabledOpt());
    CHECK(r.ok && r.additions.empty());
}

void testClosureBudget() {
    MemRepo m = makeChainRepo();
    const Strings seed = {"src/ui/a.cpp"};

    IncludeClosureOptions o = enabledOpt();
    o.max_added_files = 2;
    auto r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(r.ok);
    CHECK(paths(r) == Strings({"src/ui/a.h", "src/application/x.h"}));
    CHECK(r.dropped_budget == Strings({"src/util/b.h", "src/application/deep1.h"}));

    o = enabledOpt();
    o.max_added_bytes = 1;   // semua file > 1 byte
    r = resolveIncludeClosureWith(m.reader(), seed, m.all, o);
    CHECK(r.ok && r.additions.empty());
    CHECK(r.dropped_budget.size() == 2);   // a.h dan x.h
}

void testClosureResolutionOrder() {
    MemRepo m;
    // 1. relatif ke folder includer menang atas search dir.
    m.add("src/ui/main.cpp", "#include \"util.h\"\n");
    m.add("src/ui/util.h", "// ui\n");
    m.add("src/util.h", "// root-src\n");
    // 2. search dir "src": include gaya proyek ("application/...").
    m.add("src/app/y.cpp", "#include \"ui/util.h\"\n#include \"gen/g.h\"\n");
    // 3. suffix match unik di luar search dir.
    m.add("third_party/gen/g.h", "// g\n");
    m.finalize();

    auto r = resolveIncludeClosureWith(m.reader(), {"src/ui/main.cpp"}, m.all, enabledOpt());
    CHECK(paths(r) == Strings({"src/ui/util.h"}));

    r = resolveIncludeClosureWith(m.reader(), {"src/app/y.cpp"}, m.all, enabledOpt());
    CHECK(paths(r) == Strings({"src/ui/util.h", "third_party/gen/g.h"}));

    // Suffix match dimatikan -> g.h tidak ditemukan.
    IncludeClosureOptions o = enabledOpt();
    o.allow_suffix_match = false;
    r = resolveIncludeClosureWith(m.reader(), {"src/app/y.cpp"}, m.all, o);
    CHECK(paths(r) == Strings({"src/ui/util.h"}));
    CHECK(r.unresolved.size() == 1 && r.unresolved[0].directive == "gen/g.h");
}

void testClosureAmbiguousAndSkipped() {
    MemRepo m;
    m.add("main.cpp", "#include \"dup.h\"\n#include \"big.h\"\n#include \"ok.h\"\n");
    m.add("lib1/dup.h", "// 1\n");
    m.add("lib2/dup.h", "// 2\n");
    m.add("ok.h", "// ok\n");
    m.addSkipped("big.h", "too_large");
    m.finalize();

    auto r = resolveIncludeClosureWith(m.reader(), {"main.cpp"}, m.all, enabledOpt());
    CHECK(r.ok);
    CHECK(paths(r) == Strings({"ok.h"}));
    CHECK(r.unresolved.size() == 2);
    bool saw_ambiguous = false, saw_skipped = false;
    for (const auto& u : r.unresolved) {
        if (u.directive == "dup.h" && u.reason == "ambiguous (2 kandidat)") saw_ambiguous = true;
        if (u.directive == "big.h" && u.reason == "skipped:too_large")       saw_skipped = true;
    }
    CHECK(saw_ambiguous);
    CHECK(saw_skipped);
}

void testClosureSecretNeverAdded() {
    MemRepo m;
    m.add("main.cpp", "#include \"secrets.h\"\n");
    m.addSkipped("secrets.h", "secret");
    m.finalize();

    auto r = resolveIncludeClosureWith(m.reader(), {"main.cpp"}, m.all, enabledOpt());
    CHECK(r.ok && r.additions.empty());
    CHECK(r.unresolved.size() == 1 && r.unresolved[0].reason == "skipped:secret");
}

void testClosureDisabledAndCancel() {
    MemRepo m = makeChainRepo();

    IncludeClosureOptions off;   // enabled = false (default)
    auto r = resolveIncludeClosureWith(m.reader(), {"src/ui/a.cpp"}, m.all, off);
    CHECK(r.ok && r.additions.empty());

    r = resolveIncludeClosure("", {"src/ui/a.cpp"}, m.all, off);
    CHECK(r.ok && r.additions.empty());

    std::atomic<bool> cancel{true};
    r = resolveIncludeClosureWith(m.reader(), {"src/ui/a.cpp"}, m.all, enabledOpt(), &cancel);
    CHECK(!r.ok);
    CHECK(r.error == "dibatalkan");

    r = resolveIncludeClosure("", {"src/ui/a.cpp"}, m.all, enabledOpt());
    CHECK(!r.ok && !r.error.empty());
}

void testClosureDisk() {
    TempDir tmp;
    tmp.write("src/ui/Dialog.cpp", "#include \"ui/Dialog.h\"\n#include \"../../../../etc/passwd\"\n");
    tmp.write("src/ui/Dialog.h", "#pragma once\n#include \"core/Model.h\"\n");
    tmp.write("src/core/Model.h", "// model\n");
    tmp.write("src/core/Unrelated.h", "// tidak di-include\n");

    std::vector<CollectedFile> all;
    for (const char* p : {"src/ui/Dialog.cpp", "src/ui/Dialog.h",
                          "src/core/Model.h", "src/core/Unrelated.h"}) {
        CollectedFile f;
        f.relative_path = p;
        f.size_bytes = 10;
        all.push_back(f);
    }

    auto r = resolveIncludeClosure(tmp.path.string(), {"src/ui/Dialog.cpp"}, all, enabledOpt());
    CHECK(r.ok);
    CHECK(paths(r) == Strings({"src/ui/Dialog.h", "src/core/Model.h"}));
    CHECK(r.unresolved.size() == 1);   // path keluar root -> tidak pernah dibaca

    // applyIncludeClosure: seed hanya dari entri `selected`.
    std::vector<FullEntry> entries(2);
    entries[0].path = "src/ui/Dialog.cpp";
    entries[1].path = "src/core/Unrelated.h";
    entries[1].selected = false;   // tidak jadi seed

    const auto r2 = applyIncludeClosure(entries, tmp.path.string(), all, enabledOpt());
    CHECK(r2.ok);
    CHECK(entries.size() == 4);
    CHECK(entries[2].path == "src/ui/Dialog.h");
    CHECK(entries[3].path == "src/core/Model.h");
    CHECK(summarizeIncludeClosure(r2).find("+2 file") != std::string::npos);
}

void testMerge() {
    IncludeClosureResult res;
    res.ok = true;
    IncludeAddition a;
    a.path = "src/x.h"; a.included_by = "src/x.cpp"; a.directive = "x.h"; a.size_bytes = 42;
    IncludeAddition dup;
    dup.path = "src/user_unchecked.h"; dup.included_by = "src/x.cpp"; dup.directive = "user_unchecked.h";
    res.additions = {a, dup};

    std::vector<FullEntry> full(1);
    full[0].path = "src/user_unchecked.h";
    full[0].selected = false;   // pengguna sengaja uncheck -> tidak dicentang ulang

    CHECK(mergeIncludeClosure(full, res) == 1);
    CHECK(full.size() == 2);
    CHECK(full[0].selected == false);
    CHECK(full[1].path == "src/x.h");
    CHECK(full[1].selected);
    CHECK(full[1].priority == "helpful");
    CHECK(full[1].size_bytes == 42);
    CHECK(full[1].reason == "#include \"x.h\" dari src/x.cpp");

    // Idempoten.
    CHECK(mergeIncludeClosure(full, res) == 0);

    // Entry minimal (hanya path + selected) harus tetap kompilasi dan jalan.
    std::vector<MinimalEntry> minimal;
    CHECK(mergeIncludeClosure(minimal, res) == 2);
    CHECK(minimal.size() == 2 && minimal[0].selected);
}

void testSummary() {
    IncludeClosureResult r;
    r.ok = true;
    CHECK(summarizeIncludeClosure(r) == "Include closure: +0 file (0.0 KB)");

    r.added_bytes = 2048;
    r.additions.resize(3);
    r.unresolved.resize(1);
    r.dropped_budget.resize(2);
    CHECK(summarizeIncludeClosure(r) ==
          "Include closure: +3 file (2.0 KB), 1 include tidak terselesaikan, 2 dibuang (batas)");

    IncludeClosureResult bad;
    bad.error = "dibatalkan";
    CHECK(summarizeIncludeClosure(bad) == "Include closure gagal: dibatalkan");
}

} // namespace

int main() {
    testParse();
    testNormalize();
    testScannable();
    testClosureDepth();
    testClosureSeedHandling();
    testClosureBudget();
    testClosureResolutionOrder();
    testClosureAmbiguousAndSkipped();
    testClosureSecretNeverAdded();
    testClosureDisabledAndCancel();
    testClosureDisk();
    testMerge();
    testSummary();

    if (g_failures == 0) {
        std::printf("[PASS] include_resolver_tests: %d check\n", g_checks);
        return 0;
    }
    std::fprintf(stderr, "[FAIL] include_resolver_tests: %d dari %d check gagal\n",
                 g_failures, g_checks);
    return 1;
}