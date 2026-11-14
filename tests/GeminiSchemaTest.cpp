#include "application/ai/GeminiSchema.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

void assertTrue(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "[FAIL] %s\n", msg);
        std::exit(1);
    }
}

void testSchemaStructure() {
    auto schema = frans::application::ai::buildSelectorResponseSchema();

    // Root must be OBJECT
    assertTrue(schema.contains("type"), "schema missing 'type'");
    assertTrue(schema["type"] == "OBJECT", "root type must be OBJECT");

    // Root must have 'properties' and 'required'
    assertTrue(schema.contains("properties"), "schema missing 'properties'");
    assertTrue(schema.contains("required"), "schema missing 'required'");

    auto& props = schema["properties"];
    assertTrue(props.contains("files"), "missing 'files' property");
    assertTrue(props.contains("new_files_expected"), "missing 'new_files_expected'");
    assertTrue(props.contains("notes"), "missing 'notes'");

    // 'files' is required
    auto& required = schema["required"];
    bool filesRequired = false;
    for (auto& r : required) {
        if (r == "files") filesRequired = true;
    }
    assertTrue(filesRequired, "'files' must be in required array");
}

void testFilesArraySchema() {
    auto schema = frans::application::ai::buildSelectorResponseSchema();
    auto& filesProp = schema["properties"]["files"];

    assertTrue(filesProp["type"] == "ARRAY", "files must be ARRAY");
    assertTrue(filesProp.contains("items"), "files missing 'items'");

    auto& itemSchema = filesProp["items"];
    assertTrue(itemSchema["type"] == "OBJECT", "file item must be OBJECT");

    auto& itemProps = itemSchema["properties"];
    assertTrue(itemProps.contains("path"), "file item missing 'path'");
    assertTrue(itemProps.contains("priority"), "file item missing 'priority'");
    assertTrue(itemProps.contains("reason"), "file item missing 'reason'");

    // priority must have enum
    auto& priorityProp = itemProps["priority"];
    assertTrue(priorityProp.contains("enum"), "priority missing 'enum'");
    
    bool hasMust = false, hasHelpful = false;
    for (auto& e : priorityProp["enum"]) {
        if (e == "must") hasMust = true;
        if (e == "helpful") hasHelpful = true;
    }
    assertTrue(hasMust && hasHelpful, "priority enum must contain 'must' and 'helpful'");

    // All three fields required in item
    auto& itemRequired = itemSchema["required"];
    bool pathReq = false, prioReq = false, reasonReq = false;
    for (auto& r : itemRequired) {
        if (r == "path") pathReq = true;
        if (r == "priority") prioReq = true;
        if (r == "reason") reasonReq = true;
    }
    assertTrue(pathReq && prioReq && reasonReq, "file item must require path, priority, reason");
}

void testNewFilesExpectedSchema() {
    auto schema = frans::application::ai::buildSelectorResponseSchema();
    auto& nfe = schema["properties"]["new_files_expected"];

    assertTrue(nfe["type"] == "ARRAY", "new_files_expected must be ARRAY");
    assertTrue(nfe.contains("items"), "new_files_expected missing 'items'");
    assertTrue(nfe["items"]["type"] == "STRING", "new_files_expected items must be STRING");
}

void testNotesSchema() {
    auto schema = frans::application::ai::buildSelectorResponseSchema();
    auto& notes = schema["properties"]["notes"];

    assertTrue(notes["type"] == "STRING", "notes must be STRING");
}

void testSelectorPromptFileExists() {
    // Verify the prompt file exists and is non-empty
    std::ifstream f("config/gemini_selector_prompt.txt");
    assertTrue(f.good(), "config/gemini_selector_prompt.txt must exist");

    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    assertTrue(content.size() > 100, "prompt file must have substantial content");
    assertTrue(content.find("JSON") != std::string::npos, 
               "prompt must mention JSON output");
}

} // namespace

int main() {
    std::printf("[TEST] GeminiSchemaTest starting...\n");
    
    testSchemaStructure();
    std::printf("[PASS] Schema structure\n");
    
    testFilesArraySchema();
    std::printf("[PASS] Files array schema\n");
    
    testNewFilesExpectedSchema();
    std::printf("[PASS] new_files_expected schema\n");
    
    testNotesSchema();
    std::printf("[PASS] notes schema\n");
    
    testSelectorPromptFileExists();
    std::printf("[PASS] Selector prompt file\n");
    
    std::printf("[TEST] All GeminiSchemaTest passed!\n");
    return 0;
}