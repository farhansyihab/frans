#pragma once

// Fase C #54 -- ApiKeyProvider (BluePrint v9 Addendum §4.4.7)
//
// Lookup order:
//   1. Environment variable GEMINI_API_KEY
//   2. Key file from config (gemini_api_key_file), '~' expanded
//   3. Default key file ~/.gemini/api_key
//
// Hard rules:
//   - The key is NEVER logged, never stored in SessionState / app.json.
//   - Only the last 4 characters may be shown in UI (see maskApiKey()).
//   - Warning / hint strings produced here never contain the key.

#include <string>

namespace frans::application {

enum class ApiKeySource {
    None,
    Env,
    File
};

struct ApiKeyOptions {
    // Name of the environment variable checked first.
    std::string env_var_name = "GEMINI_API_KEY";
    // Value of AppConfig::gemini_api_key_file (may be empty, may start '~').
    std::string key_file;
    // If true, fall back to ~/.gemini/api_key when key_file is empty.
    bool allow_default_file = true;
};

struct ApiKeyResult {
    std::string  key;                       // SECRET -- do not log
    ApiKeySource source = ApiKeySource::None;
    std::string  file_path;                 // set when source == File
    std::string  warning;                   // key-free diagnostic (may be empty)

    bool found() const { return !key.empty(); }
};

class ApiKeyProvider {
public:
    static ApiKeyResult resolve(const ApiKeyOptions& opt = ApiKeyOptions{});
};

// "AQ.Ab8RN6...mIpg" -> "...mIpg". Short / empty keys -> "...".
// Never reveals more than the last 4 characters.
std::string maskApiKey(const std::string& key);

// Expand a leading "~" using HOME (POSIX) or USERPROFILE (Windows).
std::string expandUserHome(const std::string& path);

// "env" | "file" | "none" -- for UI labels, never the key itself.
const char* apiKeySourceName(ApiKeySource source);

// Actionable, key-free instructions shown when no key was found (§4.4.7).
std::string apiKeyMissingHint();

} // namespace frans::application