// tests/context/source_code_builder_tests.cpp
//
// Unit tests for SourceCodeBuilder (Fase C #60).
//
// Test strategy:
//   - Pure functions (fnv1a_64_hex, buildManifestJson) tested directly.
//   - buildSourceCode tested with temp directory fixtures.
//   - No network, no Gemini, no git required.

#include "application/context/SourceCodeBuilder.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace frans::application;

// ---------------------------------------------------------------------------
// Test harness (matches the style of frans_session_tests / frans_apikey_tests)
// ---------------------------------------------------------------------------

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(fn) do { \
    ++g_tests_run; \
    std::cout << "[ RUN      ] " << #fn << "\n"; \
    bool ok = false; \
    try { ok = (fn)(); } catch (const std::exception& e) { \
        std::cerr << "  exception: " << e.what() << "\n"; \
    } catch (...) { \
        std::cerr << "  unknown exception\n"; \
    } \
    if (ok) { ++g_tests_passed; std::cout << "[       OK ] " << #fn << "\n"; } \
    else    { ++g_tests_failed; std::cout << "[  FAILED  ] " << #fn << "\n"; } \
} while (0)

#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::cerr << "  EXPECT failed: " << #cond \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } \
} while (0)

#define EXPECT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (!(_a == _b)) { \
        std::cerr << "  EXPECT_EQ failed: " << #a << " == " << #b \
                  << "\n    actual:   [" << _a << "]" \
                  << "\n    expected: [" << _b << "]" \
                  << "\n    at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } \
} while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// RAII wrapper around a temp directory. Cleaned up on destruction.
struct TempDir {
    fs::path path;
    TempDir() {
        auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
        std::string suffix = "frans_scb_" + std::to_string(seed);
        path = fs::temp_directory_path() / suffix;
        std::error_code ec;
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void writeTestFile(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    f << content;
}

std::string readTestFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

BuildManifest makeDefaultManifest() {
    BuildManifest m;
    m.created_at    = "2026-11-14 10:00";
    m.request_hash  = "abcdef0123456789";
    m.mode          = "full";
    m.model         = "gemini-2.5-flash";
    return m;
}

// ---------------------------------------------------------------------------
// Tests: fnv1a_64_hex (pure)
// ---------------------------------------------------------------------------

bool test_fnv1a_64_hex_length() {
    std::string h = fnv1a_64_hex("hello");
    EXPECT_EQ(h.size(), 16u);
    return true;
}

bool test_fnv1a_64_hex_lowercase() {
    std::string h = fnv1a_64_hex("Hello World");
    for (char c : h) {
        bool valid = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        EXPECT(valid);
    }
    return true;
}

bool test_fnv1a_64_hex_consistency() {
    EXPECT_EQ(fnv1a_64_hex("abc"), fnv1a_64_hex("abc"));
    EXPECT_EQ(fnv1a_64_hex(""),    fnv1a_64_hex(""));
    return true;
}

bool test_fnv1a_64_hex_different() {
    EXPECT(fnv1a_64_hex("abc") != fnv1a_64_hex("abd"));
    EXPECT(fnv1a_64_hex("abc") != fnv1a_64_hex(""));
    return true;
}

bool test_fnv1a_64_hex_known_vector() {
    // FNV-1a 64-bit of empty string equals the offset basis:
    // 14695981039346656037 = 0xcbf29ce484222325
    EXPECT_EQ(fnv1a_64_hex(""), std::string("cbf29ce484222325"));
    return true;
}

// ---------------------------------------------------------------------------
// Tests: buildManifestJson (pure)
// ---------------------------------------------------------------------------

bool test_build_manifest_json_structure() {
    BuildManifest m = makeDefaultManifest();
    std::vector<std::pair<std::string, std::size_t>> files = {
        {"src/main.cpp", 1234},
        {"README.md",    567},
    };

    std::string json = buildManifestJson(m, files);

    EXPECT(json.find("\"created_at\": \"2026-11-14 10:00\"") != std::string::npos);
    EXPECT(json.find("\"request_hash\": \"abcdef0123456789\"") != std::string::npos);
    EXPECT(json.find("\"mode\": \"full\"") != std::string::npos);
    EXPECT(json.find("\"model\": \"gemini-2.5-flash\"") != std::string::npos);
    EXPECT(json.find("\"files\": [") != std::string::npos);
    EXPECT(json.find("\"path\": \"src/main.cpp\"") != std::string::npos);
    EXPECT(json.find("\"size_bytes\": 1234") != std::string::npos);
    EXPECT(json.find("\"path\": \"README.md\"") != std::string::npos);
    EXPECT(json.find("\"size_bytes\": 567") != std::string::npos);
    return true;
}

bool test_build_manifest_json_escape() {
    BuildManifest m = makeDefaultManifest();
    std::vector<std::pair<std::string, std::size_t>> files = {
        {"path/with\"quote",      100},
        {"path\\with\\backslash", 200},
    };

    std::string json = buildManifestJson(m, files);
    EXPECT(json.find("path/with\\\"quote") != std::string::npos);
    EXPECT(json.find("path\\\\with\\\\backslash") != std::string::npos);
    return true;
}

bool test_build_manifest_json_empty_files() {
    BuildManifest m = makeDefaultManifest();
    std::vector<std::pair<std::string, std::size_t>> files;

    std::string json = buildManifestJson(m, files);
    EXPECT(json.find("\"files\": [") != std::string::npos);
    // No path entries should appear
    EXPECT(json.find("\"path\":") == std::string::npos);
    return true;
}

// ---------------------------------------------------------------------------
// Tests: buildSourceCode (integration with disk)
// ---------------------------------------------------------------------------

bool test_build_source_code_basic() {
    TempDir root;
    TempDir out;

    writeTestFile(root.path / "README.md",      "# Hello\n");
    writeTestFile(root.path / "src" / "main.cpp", "int main() { return 0; }\n");
    writeTestFile(root.path / "src" / "util.cpp", "void util() {}\n");

    std::vector<CollectedFile> all_files = {
        {"README.md",      8,  true, false, ""},
        {"src/main.cpp",   23, true, false, ""},
        {"src/util.cpp",   14, true, false, ""},
    };

    std::vector<ContextFileEntry> selected = {
        {"src/main.cpp", "must",    "entry point", 23, true},
        {"README.md",    "helpful", "docs",         8, true},
    };

    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        out.path.string(),
        "SourceCode.md",
        "buatkan fitur X",
        manifest,
        nullptr);

    EXPECT(result.ok);
    EXPECT(result.error.empty());
    EXPECT_EQ(result.files_included, 2u);
    EXPECT(result.total_bytes > 0);

    fs::path sc_path = out.path / "SourceCode.md";
    EXPECT(fs::exists(sc_path));
    std::string content = readTestFile(sc_path);
    EXPECT(content.find("README.md") != std::string::npos);
    EXPECT(content.find("src/main.cpp") != std::string::npos);
    EXPECT(content.find("FILE: src/main.cpp") != std::string::npos);
    EXPECT(content.find("int main() { return 0; }") != std::string::npos);

    fs::path m_path = out.path / "SourceCode.md.manifest.json";
    EXPECT(fs::exists(m_path));
    std::string m_content = readTestFile(m_path);
    EXPECT(m_content.find("\"files\": [") != std::string::npos);
    return true;
}

bool test_build_source_code_unselected_excluded() {
    TempDir root;
    TempDir out;

    writeTestFile(root.path / "a.txt", "aaa\n");
    writeTestFile(root.path / "b.txt", "bbb\n");

    std::vector<CollectedFile> all_files = {
        {"a.txt", 4, true, false, ""},
        {"b.txt", 4, true, false, ""},
    };

    // Only a.txt selected; b.txt is in the list but not selected.
    std::vector<ContextFileEntry> selected = {
        {"a.txt", "must",    "", 4, true},
        {"b.txt", "helpful", "", 4, false},
    };

    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        out.path.string(),
        "SourceCode.md",
        "test",
        manifest,
        nullptr);

    EXPECT(result.ok);
    EXPECT_EQ(result.files_included, 1u);

    std::string content = readTestFile(out.path / "SourceCode.md");
    EXPECT(content.find("FILE: a.txt") != std::string::npos);
    EXPECT(content.find("FILE: b.txt") == std::string::npos);
    return true;
}

bool test_build_source_code_content_matches_disk() {
    TempDir root;
    TempDir out;

    // Content with characters that could trip up naive renderers.
    std::string special =
        "line1\n"
        "line2\n"
        "with \"quotes\" and \\backslash\n"
        "and some unicode: \xC3\xA9\xC3\xA0\xC3\xBC\n";
    writeTestFile(root.path / "special.txt", special);

    std::vector<CollectedFile> all_files = {
        {"special.txt", special.size(), true, false, ""},
    };

    std::vector<ContextFileEntry> selected = {
        {"special.txt", "must", "", special.size(), true},
    };

    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        out.path.string(),
        "SourceCode.md",
        "test",
        manifest,
        nullptr);

    EXPECT(result.ok);
    std::string content = readTestFile(out.path / "SourceCode.md");
    // Content must be byte-identical to what is on disk.
    EXPECT(content.find(special) != std::string::npos);
    return true;
}

bool test_build_source_code_empty_selection() {
    TempDir root;
    TempDir out;

    writeTestFile(root.path / "a.txt", "aaa\n");

    std::vector<CollectedFile> all_files = {
        {"a.txt", 4, true, false, ""},
    };

    std::vector<ContextFileEntry> selected;  // empty

    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        out.path.string(),
        "SourceCode.md",
        "test",
        manifest,
        nullptr);

    EXPECT(result.ok);
    EXPECT_EQ(result.files_included, 0u);

    std::string content = readTestFile(out.path / "SourceCode.md");
    // Tree should still reference the file.
    EXPECT(content.find("a.txt") != std::string::npos);
    // But no FILE: block for it.
    EXPECT(content.find("FILE: a.txt") == std::string::npos);
    return true;
}

bool test_build_source_code_error_empty_root() {
    TempDir out;

    std::vector<CollectedFile>    all_files;
    std::vector<ContextFileEntry> selected;
    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        "",  // empty root
        out.path.string(),
        "SourceCode.md",
        "test",
        manifest,
        nullptr);

    EXPECT(!result.ok);
    EXPECT(!result.error.empty());
    return true;
}

bool test_build_source_code_error_empty_output_dir() {
    TempDir root;

    std::vector<CollectedFile>    all_files;
    std::vector<ContextFileEntry> selected;
    BuildManifest manifest = makeDefaultManifest();

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        "",  // empty output dir
        "SourceCode.md",
        "test",
        manifest,
        nullptr);

    EXPECT(!result.ok);
    EXPECT(!result.error.empty());
    return true;
}

bool test_build_source_code_cancel() {
    TempDir root;
    TempDir out;

    writeTestFile(root.path / "a.txt", "aaa\n");
    writeTestFile(root.path / "b.txt", "bbb\n");

    std::vector<CollectedFile> all_files = {
        {"a.txt", 4, true, false, ""},
        {"b.txt", 4, true, false, ""},
    };

    std::vector<ContextFileEntry> selected = {
        {"a.txt", "must", "", 4, true},
        {"b.txt", "must", "", 4, true},
    };

    BuildManifest manifest = makeDefaultManifest();
    std::atomic<bool> cancel{true};  // already cancelled

    BuildResult result = buildSourceCode(
        all_files, selected,
        root.path.string(),
        out.path.string(),
        "SourceCode.md",
        "test",
        manifest,
        &cancel);

    EXPECT(!result.ok);
    // Implementation uses "dibatalkan" as the cancel error message.
    EXPECT(result.error.find("dibatalkan") != std::string::npos);
    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "=== SourceCodeBuilder unit tests ===\n\n";

    // Pure functions
    RUN_TEST(test_fnv1a_64_hex_length);
    RUN_TEST(test_fnv1a_64_hex_lowercase);
    RUN_TEST(test_fnv1a_64_hex_consistency);
    RUN_TEST(test_fnv1a_64_hex_different);
    RUN_TEST(test_fnv1a_64_hex_known_vector);

    RUN_TEST(test_build_manifest_json_structure);
    RUN_TEST(test_build_manifest_json_escape);
    RUN_TEST(test_build_manifest_json_empty_files);

    // Integration with disk
    RUN_TEST(test_build_source_code_basic);
    RUN_TEST(test_build_source_code_unselected_excluded);
    RUN_TEST(test_build_source_code_content_matches_disk);
    RUN_TEST(test_build_source_code_empty_selection);
    RUN_TEST(test_build_source_code_error_empty_root);
    RUN_TEST(test_build_source_code_error_empty_output_dir);
    RUN_TEST(test_build_source_code_cancel);

    std::cout << "\n=== Summary ===\n";
    std::cout << "Tests run:    " << g_tests_run    << "\n";
    std::cout << "Tests passed: " << g_tests_passed << "\n";
    std::cout << "Tests failed: " << g_tests_failed << "\n";

    return g_tests_failed == 0 ? 0 : 1;
}