#include "application/ai/GeminiSchema.h"

namespace frans::application::ai {

nlohmann::json buildSelectorResponseSchema() {
    using json = nlohmann::json;

    // Schema for a single file item in the "files" array
    json file_item_properties = {
        {"path", {{"type", "STRING"}}},
        {"priority", {
            {"type", "STRING"},
            {"enum", {"must", "helpful"}}
        }},
        {"reason", {{"type", "STRING"}}}
    };

    json file_item_schema = {
        {"type", "OBJECT"},
        {"properties", file_item_properties},
        {"required", {"path", "priority", "reason"}}
    };

    // Top-level properties
    json properties = {
        {"files", {
            {"type", "ARRAY"},
            {"items", file_item_schema}
        }},
        {"new_files_expected", {
            {"type", "ARRAY"},
            {"items", {{"type", "STRING"}}}
        }},
        {"notes", {{"type", "STRING"}}}
    };

    // Root schema object
    json schema = {
        {"type", "OBJECT"},
        {"properties", properties},
        {"required", {"files"}}
    };

    return schema;
}

} // namespace frans::application::ai