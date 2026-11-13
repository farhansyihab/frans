#include "application/ai/ApiKeyProvider.h"

#include <cstdlib>
#include <fstream>
#include <string>

#ifndef _WIN32
#  include <sys/stat.h>
#endif

namespace frans::application {

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return {};
    const std::size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

std::string stripBom(const std::string& s) {
    if (s.size() >= 3
        && static_cast<unsigned char>(s[0]) == 0xEF
        && static_cast<unsigned char>(s[1]) == 0xBB
        && static_cast<unsigned char>(s[2]) == 0xBF) {
        return s.substr(3);
    }
    return s;
}

bool hasInnerWhitespace(const std::string& s) {
    for (const char c : s) {
        if (c == ' ' || c == '\t') return true;
    }
    return false;
}

std::string envValue(const char* name) {
    if (!name || !*name) return {};
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

// Returns a key-free warning when the file is readable/writable by
// group or others. On Windows this check is skipped (TODO #65).
std::string permissionWarning(const std::string& path) {
#ifndef _WIN32
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) return {};
    if ((st.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) != 0) {
        return "Izin file kunci terlalu longgar: " + path
             + " -- jalankan: chmod 600 \"" + path + "\"";
    }
#else
    (void)path;
#endif
    return {};
}

// Reads the first non-empty line of the key file. The key is never logged.
ApiKeyResult readKeyFile(const std::string& path) {
    ApiKeyResult r;
    r.source    = ApiKeySource::File;
    r.file_path = path;

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        // Missing file is not an error here: caller decides the fallback.
        r.source = ApiKeySource::None;
        r.file_path.clear();
        return r;
    }

    r.warning = permissionWarning(path);

    std::string line;
    while (std::getline(f, line)) {
        std::string key = trim(stripBom(line));
        if (key.empty()) continue;
        if (key.front() == '#') continue;          // allow comment lines
        if (hasInnerWhitespace(key)) {
            r.warning = "File kunci harus berisi key saja pada satu baris: "
                      + path;
            r.source  = ApiKeySource::None;
            r.file_path.clear();
            return r;
        }
        r.key = key;
        return r;
    }

    // File exists but carries no usable key.
    r.source = ApiKeySource::None;
    r.file_path.clear();
    if (r.warning.empty()) {
        r.warning = "File kunci kosong: " + path;
    }
    return r;
}

} // namespace

std::string expandUserHome(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    if (path.size() > 1 && path[1] != '/' && path[1] != '\\') return path;

    std::string home = envValue("HOME");
    if (home.empty()) home = envValue("USERPROFILE");
    if (home.empty()) return path;

    if (path.size() == 1) return home;
    return home + path.substr(1);
}

ApiKeyResult ApiKeyProvider::resolve(const ApiKeyOptions& opt) {
    ApiKeyResult r;

    // 1. Environment variable.
    const std::string env_key = trim(envValue(opt.env_var_name.c_str()));
    if (!env_key.empty() && !hasInnerWhitespace(env_key)) {
        r.key    = env_key;
        r.source = ApiKeySource::Env;
        return r;
    }

    // 2. Configured key file.
    std::string warning;
    if (!opt.key_file.empty()) {
        ApiKeyResult fr = readKeyFile(expandUserHome(opt.key_file));
        if (fr.found()) return fr;
        if (!fr.warning.empty()) warning = fr.warning;
    }

    // 3. Default key file.
    if (opt.allow_default_file) {
        ApiKeyResult fr = readKeyFile(expandUserHome("~/.gemini/api_key"));
        if (fr.found()) return fr;
        if (warning.empty() && !fr.warning.empty()) warning = fr.warning;
    }

    r.source  = ApiKeySource::None;
    r.warning = warning;
    return r;
}

std::string maskApiKey(const std::string& key) {
    if (key.size() <= 4) return "...";
    return "..." + key.substr(key.size() - 4);
}

const char* apiKeySourceName(ApiKeySource source) {
    switch (source) {
        case ApiKeySource::Env:  return "env";
        case ApiKeySource::File: return "file";
        case ApiKeySource::None: return "none";
    }
    return "none";
}

std::string apiKeyMissingHint() {
    return
        "GEMINI_API_KEY tidak ditemukan.\n"
        "Linux (dari terminal): export GEMINI_API_KEY=\"...\"\n"
        "Linux (dari menu desktop): taruh export di ~/.profile lalu "
        "logout/login -- ~/.bashrc tidak dibaca oleh .desktop.\n"
        "Alternatif: simpan key di file, lalu set \"gemini_api_key_file\" "
        "di config/app.json (chmod 600).\n"
        "Windows: setx GEMINI_API_KEY \"...\" lalu buka ulang Frans.\n"
        "Tanpa key, tombol Gemini dinonaktifkan; alur manual tetap jalan.";
}

} // namespace frans::application