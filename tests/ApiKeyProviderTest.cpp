// tests/ApiKeyProviderTest.cpp
//
// Fase C #54 -- ApiKeyProvider (BluePrint v9 Addendum §4.4.7).
//
// Semua test memakai HOME palsu di direktori temp. File kunci asli pengguna
// (~/.gemini/api_key) TIDAK pernah disentuh. Tidak ada key yang dicetak.
//
// Jalankan:
//   cmake -S . -B build -DFRANS_BUILD_TESTS=ON
//   cmake --build build -j2
//   ./build/bin/frans_apikey_tests

#include "application/ai/ApiKeyProvider.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifndef _WIN32
#  include <sys/stat.h>
#endif

namespace {

namespace fs = std::filesystem;

using frans::application::ApiKeyOptions;
using frans::application::ApiKeyProvider;
using frans::application::ApiKeyResult;
using frans::application::ApiKeySource;
using frans::application::apiKeyMissingHint;
using frans::application::apiKeySourceName;
using frans::application::expandUserHome;
using frans::application::maskApiKey;

int g_pass = 0;
int g_fail = 0;

void beginTest(const char* name, const char* desc) {
    std::printf("\n=== %s ===\n  %s\n", name, desc);
}

void checkTrue(bool cond, const char* label) {
    if (cond) ++g_pass; else ++g_fail;
    std::printf("  %s %s\n", cond ? "[ok]  " : "[FAIL]", label);
}

void checkEq(const std::string& actual, const std::string& expected,
             const char* label)
{
    const bool ok = (actual == expected);
    if (ok) ++g_pass; else ++g_fail;
    // Nilai tidak dicetak: bisa berupa key.
    std::printf("  %s %s\n", ok ? "[ok]  " : "[FAIL]", label);
}

void checkNotContains(const std::string& hay, const std::string& needle,
                      const char* label)
{
    const bool ok = hay.find(needle) == std::string::npos;
    if (ok) ++g_pass; else ++g_fail;
    std::printf("  %s %s\n", ok ? "[ok]  " : "[FAIL]", label);
}

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    ::setenv(name, value.c_str(), 1);
#endif
}

void unsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    ::unsetenv(name);
#endif
}

std::string getEnv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

// Env var yang dipakai test -- sengaja bukan GEMINI_API_KEY asli.
const char* kTestEnv = "FRANS_TEST_GEMINI_KEY_54";

class TempHome {
public:
    TempHome() {
        std::error_code ec;
        fs::path base = fs::temp_directory_path(ec);
        if (ec) return;
        dir_ = base / "frans_apikey_test_54";
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_, ec);
        old_home_    = getEnv("HOME");
        old_profile_ = getEnv("USERPROFILE");
        had_home_    = !old_home_.empty();
        had_profile_ = !old_profile_.empty();
        setEnv("HOME", dir_.string());
        setEnv("USERPROFILE", dir_.string());
        ok_ = fs::is_directory(dir_, ec) && !ec;
    }

    ~TempHome() {
        if (had_home_)    setEnv("HOME", old_home_);       else unsetEnv("HOME");
        if (had_profile_) setEnv("USERPROFILE", old_profile_); else unsetEnv("USERPROFILE");
        std::error_code ec;
        if (!dir_.empty()) fs::remove_all(dir_, ec);
    }

    bool ok() const { return ok_; }
    fs::path dir() const { return dir_; }

    std::string write(const std::string& rel, const std::string& content,
                      int posix_mode = 0600) const
    {
        const fs::path p = dir_ / rel;
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        {
            std::ofstream f(p, std::ios::binary | std::ios::trunc);
            f.write(content.data(), static_cast<std::streamsize>(content.size()));
        }
#ifndef _WIN32
        ::chmod(p.c_str(), static_cast<mode_t>(posix_mode));
#else
        (void)posix_mode;
#endif
        return p.string();
    }

private:
    fs::path    dir_;
    std::string old_home_, old_profile_;
    bool had_home_ = false, had_profile_ = false, ok_ = false;
};

// ---------------------------------------------------------------------------

void testEnvWins(const TempHome& home) {
    beginTest("apikey-env-priority",
              "Env var menang atas file kunci; source = env.");
    const std::string file = home.write(".gemini/api_key", "FILE_KEY_loses\n");
    setEnv(kTestEnv, "ENV_KEY_wins");

    ApiKeyOptions opt;
    opt.env_var_name = kTestEnv;
    opt.key_file     = file;
    const ApiKeyResult r = ApiKeyProvider::resolve(opt);

    checkTrue(r.found(),                        "key ditemukan");
    checkEq(r.key, "ENV_KEY_wins",              "nilai dari env");
    checkTrue(r.source == ApiKeySource::Env,    "source == env");
    checkEq(apiKeySourceName(r.source), "env",  "label source");
    unsetEnv(kTestEnv);
}

void testEnvBlankFallsBack(const TempHome& home) {
    beginTest("apikey-env-blank-fallback",
              "Env kosong / spasi -> jatuh ke file kunci.");
    const std::string file = home.write("keys/blank.txt", "  FILE_KEY_ok  \n");

    ApiKeyOptions opt;
    opt.env_var_name = kTestEnv;
    opt.key_file     = file;

    setEnv(kTestEnv, "   ");
    const ApiKeyResult r1 = ApiKeyProvider::resolve(opt);
    checkTrue(r1.found() && r1.source == ApiKeySource::File, "spasi -> file");
    checkEq(r1.key, "FILE_KEY_ok", "nilai file di-trim");

    setEnv(kTestEnv, "");
    const ApiKeyResult r2 = ApiKeyProvider::resolve(opt);
    checkTrue(r2.found() && r2.source == ApiKeySource::File, "kosong -> file");

    unsetEnv(kTestEnv);
    const ApiKeyResult r3 = ApiKeyProvider::resolve(opt);
    checkTrue(r3.found() && r3.source == ApiKeySource::File, "unset -> file");
    checkEq(r3.file_path, file, "file_path tercatat");
}

void testDefaultFileAndTilde(const TempHome& home) {
    beginTest("apikey-default-file-tilde",
              "Tanpa key_file -> ~/.gemini/api_key; '~' di-expand.");
    home.write(".gemini/api_key", "DEFAULT_KEY_1234\n");
    unsetEnv(kTestEnv);

    ApiKeyOptions opt;
    opt.env_var_name = kTestEnv;
    const ApiKeyResult r = ApiKeyProvider::resolve(opt);
    checkTrue(r.found() && r.source == ApiKeySource::File, "default file dibaca");
    checkEq(r.key, "DEFAULT_KEY_1234", "nilai default file");
    checkEq(expandUserHome("~/x.txt"), (home.dir() / "x.txt").string(),
            "expandUserHome('~/x.txt')");
    checkEq(expandUserHome("/abs/path"), "/abs/path", "path absolut utuh");

    ApiKeyOptions opt2;
    opt2.env_var_name = kTestEnv;
    opt2.key_file     = "~/.gemini/api_key";
    const ApiKeyResult r2 = ApiKeyProvider::resolve(opt2);
    checkEq(r2.key, "DEFAULT_KEY_1234", "key_file dengan '~' dibaca");

    ApiKeyOptions opt3;
    opt3.env_var_name       = kTestEnv;
    opt3.allow_default_file = false;
    const ApiKeyResult r3 = ApiKeyProvider::resolve(opt3);
    checkTrue(!r3.found(), "allow_default_file=false -> tidak cari default");
}

void testFileParsing(const TempHome& home) {
    beginTest("apikey-file-parsing",
              "BOM, komentar, CRLF, baris kosong ditangani; sampah ditolak.");
    unsetEnv(kTestEnv);

    ApiKeyOptions opt;
    opt.env_var_name       = kTestEnv;
    opt.allow_default_file = false;

    opt.key_file = home.write("keys/bom.txt", "\xEF\xBB\xBF" "BOM_KEY_9\n");
    checkEq(ApiKeyProvider::resolve(opt).key, "BOM_KEY_9", "BOM dibuang");

    opt.key_file = home.write("keys/crlf.txt", "\r\n# komentar\r\nCRLF_KEY_7\r\n");
    checkEq(ApiKeyProvider::resolve(opt).key, "CRLF_KEY_7", "CRLF + komentar");

    opt.key_file = home.write("keys/multi.txt", "FIRST_KEY_1\nSECOND_KEY_2\n");
    checkEq(ApiKeyProvider::resolve(opt).key, "FIRST_KEY_1", "baris pertama dipakai");

    opt.key_file = home.write("keys/spaces.txt", "dua token\n");
    const ApiKeyResult bad = ApiKeyProvider::resolve(opt);
    checkTrue(!bad.found(), "key dengan spasi internal ditolak");
    checkTrue(!bad.warning.empty(), "ada warning untuk file rusak");

    opt.key_file = home.write("keys/empty.txt", "\n   \n");
    checkTrue(!ApiKeyProvider::resolve(opt).found(), "file kosong -> not found");

    opt.key_file = home.dir().string();  // direktori, bukan file
    checkTrue(!ApiKeyProvider::resolve(opt).found(), "direktori -> not found, aman");
}

void testNotFound(const TempHome& home) {
    beginTest("apikey-not-found",
              "Tidak ada env & file -> found()=false, hint jelas, tanpa key.");
    (void)home;
    unsetEnv(kTestEnv);

    ApiKeyOptions opt;
    opt.env_var_name       = kTestEnv;
    opt.key_file           = "~/tidak_ada_54.txt";
    opt.allow_default_file = false;
    const ApiKeyResult r = ApiKeyProvider::resolve(opt);

    checkTrue(!r.found(), "found() == false");
    checkTrue(r.source == ApiKeySource::None, "source == none");
    checkEq(apiKeySourceName(r.source), "none", "label none");

    const std::string hint = apiKeyMissingHint();
    checkTrue(hint.find("GEMINI_API_KEY") != std::string::npos, "hint sebut env var");
    checkTrue(hint.find(".profile") != std::string::npos, "hint sebut ~/.profile");
    checkTrue(hint.find("setx") != std::string::npos, "hint sebut setx (Windows)");
}

void testPermissions(const TempHome& home) {
    beginTest("apikey-file-permission",
              "Izin longgar -> warning; 0600 -> tanpa warning (§4.4.7).");
    unsetEnv(kTestEnv);
#ifdef _WIN32
    (void)home;
    std::printf("  [SKIP] cek izin POSIX tidak berlaku di Windows\n");
    ++g_pass;
#else
    ApiKeyOptions opt;
    opt.env_var_name       = kTestEnv;
    opt.allow_default_file = false;

    opt.key_file = home.write("keys/loose.txt", "LOOSE_KEY_5\n", 0644);
    const ApiKeyResult loose = ApiKeyProvider::resolve(opt);
    checkTrue(loose.found(), "key tetap dibaca");
    checkTrue(loose.warning.find("chmod 600") != std::string::npos,
              "warning menyarankan chmod 600");
    checkNotContains(loose.warning, loose.key, "warning tidak membocorkan key");

    opt.key_file = home.write("keys/tight.txt", "TIGHT_KEY_5\n", 0600);
    const ApiKeyResult tight = ApiKeyProvider::resolve(opt);
    checkTrue(tight.found(), "key 0600 dibaca");
    checkTrue(tight.warning.empty(), "0600 -> tanpa warning");
#endif
}

void testMask() {
    beginTest("apikey-mask",
              "maskApiKey hanya membuka 4 karakter terakhir.");
    // Prefiks key Gemini bisa "AIza..." maupun "AQ.Ab..." -- jangan diasumsikan.
    checkEq(maskApiKey("AQ.Ab8RN6abcdefmIpg"), "...mIpg", "mask key panjang");
    checkEq(maskApiKey("AIzaSyDUMMYx7Qk"),   "...x7Qk", "mask prefiks AIza");
    checkEq(maskApiKey("abcd"),              "...",    "key pendek disamarkan");
    checkEq(maskApiKey(""),                  "...",    "key kosong aman");
    checkNotContains(maskApiKey("AQ.Ab8RN6abcdefmIpg"), "AQ.Ab8RN6",
                     "prefiks tidak bocor");
    checkTrue(maskApiKey("AQ.Ab8RN6abcdefmIpg").size() <= 7, "mask pendek");
}

void testNoLeakInDiagnostics(const TempHome& home) {
    beginTest("apikey-no-leak",
              "Warning / hint / label tidak memuat key apa pun.");
    const std::string file = home.write("keys/leak.txt", "LEAKCHECK_KEY_42\n", 0644);
    unsetEnv(kTestEnv);

    ApiKeyOptions opt;
    opt.env_var_name       = kTestEnv;
    opt.key_file           = file;
    opt.allow_default_file = false;
    const ApiKeyResult r = ApiKeyProvider::resolve(opt);

    checkNotContains(r.warning, "LEAKCHECK_KEY_42", "warning bersih");
    checkNotContains(apiKeyMissingHint(), "LEAKCHECK_KEY_42", "hint bersih");
    checkNotContains(apiKeySourceName(r.source), "LEAKCHECK_KEY_42", "label bersih");
    checkNotContains(maskApiKey(r.key), "LEAKCHECK", "mask bersih");
}

} // namespace

int main() {
    std::printf("Frans -- test Fase C #54 (ApiKeyProvider)\n");

    TempHome home;
    if (!home.ok()) {
        std::printf("[SKIP] tidak bisa membuat direktori temp test\n");
        return 0;
    }
    // Kunci asli pengguna tidak boleh ikut terbaca selama test.
    unsetEnv(kTestEnv);

    testEnvWins(home);
    testEnvBlankFallsBack(home);
    testDefaultFileAndTilde(home);
    testFileParsing(home);
    testNotFound(home);
    testPermissions(home);
    testMask();
    testNoLeakInDiagnostics(home);

    std::printf("\n=== Summary: %d checks passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}