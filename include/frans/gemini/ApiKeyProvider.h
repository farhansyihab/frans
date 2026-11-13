#pragma once

#include <string>
#include <optional>

namespace frans::gemini {

/**
 * @brief Provides the Gemini API key.
 * 
 * The provider attempts to retrieve the API key in the following order:
 * 1. From the specified environment variable (default: GEMINI_API_KEY).
 * 2. From a specified file path (default: ~/.gemini/api_key).
 */
class ApiKeyProvider {
public:
    /**
     * @brief Retrieves the API key.
     * 
     * @param envVarName Name of the environment variable to check first.
     * @param filePath Path to the file to check as a fallback. If empty, 
     *                 defaults to ~/.gemini/api_key.
     * @return std::optional<std::string> The API key if found, otherwise std::nullopt.
     */
    static std::optional<std::string> getApiKey(
        const std::string& envVarName = "GEMINI_API_KEY",
        const std::string& filePath = ""
    );
};

} // namespace frans::gemini