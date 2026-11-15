// tests/context/selection_cache_tests.cpp
//
// Fase E #73 -- test SelectionCache + helper hash.
// Tanpa FLTK, tanpa jaringan, tanpa git. (selectContextCached sendiri tidak
// dites di sini karena butuh GeminiClient dengan transport palsu.)
#include "application/context/SelectionCache.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace frans::application;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,     \
                         #cond);                                             \
        }                                                                    \
    } while (0)

CollectedFile mkFile(const std::string& path, std::size_t size)
{
    CollectedFile f;
    f.relative_path = path;
    f.size_bytes    = size;
    return f;
}

SelectionResult mkResult(const std::string& path)
{
    SelectionResult r;
    r.ok   = true;
    r.mode = "full";
    ContextFileEntry e;
    e.path     = path;
    e.priority = "must";
    r.files.push_back(e);
    return r;
}

ContextRequest mkRequest()
{
    ContextRequest r;
    r.project_root    = "/tmp/proj";
    r.request_text    = "tambah fitur X";
    r.gemini_model    = "gemini-2.5-flash";
    r.selector_prompt = "prompt";
    r.api_key         = "SECRET-1";
    return r;
}

void testStoreAndLookup()
{
    SelectionCache c(4);
    SelectionResult out;
    CHECK(!c.lookup("k1", out));

    c.store("k1", mkResult("a.cpp"));
    CHECK(c.lookup("k1", out));
    CHECK(out.ok);
    CHECK(out.files.size() == 1);
    CHECK(out.files[0].path == "a.cpp");

    SelectionCacheStats s = c.stats();
    CHECK(s.hits == 1);
    CHECK(s.misses == 1);
    CHECK(s.entries == 1);
}

void testOverwrite()
{
    SelectionCache c(4);
    c.store("k", mkResult("a.cpp"));
    c.store("k", mkResult("b.cpp"));
    CHECK(c.size() == 1);

    SelectionResult out;
    CHECK(c.lookup("k", out));
    CHECK(out.files[0].path == "b.cpp");
}

void testLruEviction()
{
    SelectionCache c(2);
    c.store("a", mkResult("a"));
    c.store("b", mkResult("b"));

    SelectionResult out;
    CHECK(c.lookup("a", out));        // "a" jadi paling baru; "b" paling lama
    c.store("c", mkResult("c"));      // "b" harus terbuang

    CHECK(c.size() == 2);
    CHECK(c.lookup("a", out));
    CHECK(c.lookup("c", out));
    CHECK(!c.lookup("b", out));
}

void testDisabledCache()
{
    SelectionCache c(0);
    c.store("k", mkResult("a"));
    SelectionResult out;
    CHECK(!c.lookup("k", out));
    CHECK(c.size() == 0);
}

void testClear()
{
    SelectionCache c(4);
    c.store("k", mkResult("a"));
    c.clear();
    SelectionResult out;
    CHECK(c.size() == 0);
    CHECK(!c.lookup("k", out));
}

void testRequestHash()
{
    ContextRequest a = mkRequest();
    const std::string base = computeSelectionRequestHash(a);

    CHECK(base.size() == 16);
    CHECK(computeSelectionRequestHash(a) == base);          // deterministik

    ContextRequest b = a;
    b.request_text = "tambah fitur Y";
    CHECK(computeSelectionRequestHash(b) != base);

    b = a;
    b.error_output = "error: foo";
    CHECK(computeSelectionRequestHash(b) != base);

    b = a;
    b.gemini_model = "gemini-lain";
    CHECK(computeSelectionRequestHash(b) != base);

    b = a;
    b.max_files = 10;
    CHECK(computeSelectionRequestHash(b) != base);

    b = a;
    b.always_include.push_back("BluePrint.md");
    CHECK(computeSelectionRequestHash(b) != base);

    // Tidak boleh mempengaruhi hash: api_key, endpoint, timeout.
    b = a;
    b.api_key         = "SECRET-2";
    b.gemini_endpoint = "https://contoh.invalid";
    b.gemini_timeout_sec = 5;
    CHECK(computeSelectionRequestHash(b) == base);

    // Batas antar-field tidak boleh ambigu.
    ContextRequest x = mkRequest();
    ContextRequest y = mkRequest();
    x.request_text = "ab";  x.error_output = "c";
    y.request_text = "a";   y.error_output = "bc";
    CHECK(computeSelectionRequestHash(x) != computeSelectionRequestHash(y));
}

void testFileListHash()
{
    std::vector<CollectedFile> files = {
        mkFile("a.cpp", 10), mkFile("b.cpp", 20), mkFile("c.h", 5)
    };
    const std::string base = computeFileListHash(files);
    CHECK(base.size() == 16);

    // Urutan input tidak berpengaruh.
    std::vector<CollectedFile> reversed(files.rbegin(), files.rend());
    CHECK(computeFileListHash(reversed) == base);

    // File ditambah.
    std::vector<CollectedFile> added = files;
    added.push_back(mkFile("d.cpp", 1));
    CHECK(computeFileListHash(added) != base);

    // File dihapus.
    std::vector<CollectedFile> removed(files.begin(), files.begin() + 2);
    CHECK(computeFileListHash(removed) != base);

    // Ukuran berubah.
    std::vector<CollectedFile> resized = files;
    resized[1].size_bytes = 21;
    CHECK(computeFileListHash(resized) != base);

    // Flag skipped berubah.
    std::vector<CollectedFile> skipped = files;
    skipped[0].skipped = true;
    CHECK(computeFileListHash(skipped) != base);

    // Daftar kosong tetap menghasilkan hash valid.
    CHECK(computeFileListHash({}).size() == 16);
}

void testCacheKey()
{
    CHECK(makeSelectionCacheKey("aaaa", "bbbb") == "aaaa:bbbb");
    CHECK(makeSelectionCacheKey("aaaa", "bbbb") != makeSelectionCacheKey("bbbb", "aaaa"));
}

} // namespace

int main()
{
    testStoreAndLookup();
    testOverwrite();
    testLruEviction();
    testDisabledCache();
    testClear();
    testRequestHash();
    testFileListHash();
    testCacheKey();

    if (g_failures == 0) {
        std::printf("selection_cache_tests: OK\n");
        return 0;
    }
    std::fprintf(stderr, "selection_cache_tests: %d kegagalan\n", g_failures);
    return 1;
}