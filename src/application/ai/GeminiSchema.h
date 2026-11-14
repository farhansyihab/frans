#pragma once

#include <nlohmann/json.hpp>

namespace frans::application::ai {

// Build the responseSchema for Gemini generateContent.
// This schema forces Gemini to return a structured JSON for context selection.
// Note: Type names (OBJECT, ARRAY, STRING) are uppercase as per initial draft;
// exact casing will be verified during item #58.
nlohmann::json buildSelectorResponseSchema();

} // namespace frans::application::ai