// tests/context/source_code_builder_tests.cpp
//
// Unit tests untuk SourceCodeBuilder (Fase C #60 + #61).
#include "application/context/SourceCodeBuilder.h"
#include "application/ingest/FileCollector.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Helper untuk membuat CollectedFile dummy.
// Field nyata CollectedFile: relative_path, size_bytes, is_text, skipped.
[[maybe_unused]] frans::application::CollectedFile makeCollectedFile(
    const std::string& path,
    const std::string& content = "")
{
    frans::application::CollectedFile f;
    f.relative_path = path;
    f.size_bytes    = content.size();
    f.is_text       = true;
    f.skipped       = false;
    return f;
}

// Helper untuk membuat ContextFileEntry dummy.
[[maybe_unused]] frans::application::ContextFileEntry makeContextEntry(
    const std::string& path,
    bool selected = true,
    const std::string& priority = "must",
    const std::string& reason = "test")
{
    frans::application::ContextFileEntry e;
    e.path = path;
    e.selected = selected;
    e.priority = priority;
    e.reason = reason;
    e.size_bytes = 100;
    return e;
}

// ===========================================================================
// Test 1: fnv1a_64_hex deterministik
// ===========================================================================
bool test_fnv1a_64_hex_deterministic() {
    std::string data = "hello world";
    std::string hash1 = frans::application::fnv1a_64_hex(data);
    std::string hash2 = frans::application::fnv1a_64_hex(data);

    if (hash1 != hash2) {
        std::cerr << "FAIL: fnv1a_64_hex tidak deterministik\n";
        return false;
    }
    if (hash1.size() != 16) {
        std::cerr << "FAIL: fnv1a_64_hex length != 16 (got " << hash1.size() << ")\n";
        return false;
    }
    std::cout << "PASS: test_fnv1a_64_hex_deterministic\n";
    return true;
}

// ===========================================================================
// Test 2: fnv1a_64_hex berbeda untuk input berbeda
// ===========================================================================
bool test_fnv1a_64_hex_different() {
    std::string hash1 = frans::application::fnv1a_64_hex("hello");
    std::string hash2 = frans::application::fnv1a_64_hex("world");

    if (hash1 == hash2) {
        std::cerr << "FAIL: fnv1a_64_hex collision\n";
        return false;
    }
    std::cout << "PASS: test_fnv1a_64_hex_different\n";
    return true;
}

// ===========================================================================
// Test 3: buildManifestJson struktur dasar
// ===========================================================================
bool test_build_manifest_json_structure() {
    frans::application::BuildManifest m;
    m.created_at = "2026-11-14 10:00";
    m.request_hash = "abcdef1234567890";
    m.mode = "full";
    m.model = "gemini-2.5-flash";

    std::vector<frans::application::FileHashInfo> files;
    frans::application::FileHashInfo fi1;
    fi1.path = "src/main.cpp";
    fi1.content_hash = "1234567890abcdef";
    fi1.size_bytes = 1024;
    files.push_back(fi1);

    frans::application::FileHashInfo fi2;
    fi2.path = "src/utils.cpp";
    fi2.content_hash = "fedcba0987654321";
    fi2.size_bytes = 512;
    files.push_back(fi2);

    std::string json = frans::application::buildManifestJson(m, files);

    // Cek field-field penting
    if (json.find("\"created_at\": \"2026-11-14 10:00\"") == std::string::npos) {
        std::cerr << "FAIL: created_at tidak ditemukan\n";
        return false;
    }
    if (json.find("\"request_hash\": \"abcdef1234567890\"") == std::string::npos) {
        std::cerr << "FAIL: request_hash tidak ditemukan\n";
        return false;
    }
    if (json.find("\"mode\": \"full\"") == std::string::npos) {
        std::cerr << "FAIL: mode tidak ditemukan\n";
        return false;
    }
    if (json.find("\"model\": \"gemini-2.5-flash\"") == std::string::npos) {
        std::cerr << "FAIL: model tidak ditemukan\n";
        return false;
    }
    if (json.find("\"path\": \"src/main.cpp\"") == std::string::npos) {
        std::cerr << "FAIL: path file 1 tidak ditemukan\n";
        return false;
    }
    if (json.find("\"hash\": \"1234567890abcdef\"") == std::string::npos) {
        std::cerr << "FAIL: hash file 1 tidak ditemukan\n";
        return false;
    }
    if (json.find("\"size_bytes\": 1024") == std::string::npos) {
        std::cerr << "FAIL: size_bytes file 1 tidak ditemukan\n";
        return false;
    }

    std::cout << "PASS: test_build_manifest_json_structure\n";
    return true;
}

// ===========================================================================
// Test 4: buildManifestJson escape karakter khusus
// ===========================================================================
bool test_build_manifest_json_escape() {
    frans::application::BuildManifest m;
    m.created_at = "2026-11-14 10:00";
    m.request_hash = "abc\"def\n123";
    m.mode = "full";
    m.model = "gemini-2.5-flash";

    std::vector<frans::application::FileHashInfo> files;
    frans::application::FileHashInfo fi;
    fi.path = "src/test\"file.cpp";
    fi.content_hash = "hash\"with\nspecial";
    fi.size_bytes = 100;
    files.push_back(fi);

    std::string json = frans::application::buildManifestJson(m, files);

    // Cek escape
    if (json.find("\"request_hash\": \"abc\\\"def\\n123\"") == std::string::npos) {
        std::cerr << "FAIL: escape request_hash gagal\n";
        return false;
    }
    if (json.find("\"path\": \"src/test\\\"file.cpp\"") == std::string::npos) {
        std::cerr << "FAIL: escape path gagal\n";
        return false;
    }
    if (json.find("\"hash\": \"hash\\\"with\\nspecial\"") == std::string::npos) {
        std::cerr << "FAIL: escape hash gagal\n";
        return false;
    }

    std::cout << "PASS: test_build_manifest_json_escape\n";
    return true;
}

// ===========================================================================
// Test 5: buildManifestJson dengan files kosong
// ===========================================================================
bool test_build_manifest_json_empty_files() {
    frans::application::BuildManifest m;
    m.created_at = "2026-11-14 10:00";
    m.request_hash = "abcdef1234567890";
    m.mode = "index";
    m.model = "gemini-2.5-pro";

    std::vector<frans::application::FileHashInfo> files;

    std::string json = frans::application::buildManifestJson(m, files);

    if (json.find("\"files\": [") == std::string::npos) {
        std::cerr << "FAIL: files array tidak ditemukan\n";
        return false;
    }
    if (json.find("\"path\"") != std::string::npos) {
        std::cerr << "FAIL: files tidak kosong\n";
        return false;
    }

    std::cout << "PASS: test_build_manifest_json_empty_files\n";
    return true;
}

} // namespace

int main() {
    int failures = 0;

    if (!test_fnv1a_64_hex_deterministic()) ++failures;
    if (!test_fnv1a_64_hex_different()) ++failures;
    if (!test_build_manifest_json_structure()) ++failures;
    if (!test_build_manifest_json_escape()) ++failures;
    if (!test_build_manifest_json_empty_files()) ++failures;

    if (failures > 0) {
        std::cerr << "\n" << failures << " test(s) FAILED\n";
        return 1;
    }

    std::cout << "\nAll tests PASSED\n";
    return 0;
}