# ~/AgenAI/frans/cmake/ProviderTests.cmake
#
# Test executable untuk provider API (DeepSeek, Gemini) + router 3-tier.
#
# SEMUA test di sini butuh network + API key. Label "network" supaya
# bisa di-skip dengan:
#   ctest -LE network
# atau bersama label lain:
#   ctest -LE "network|requires-llama"
include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Stage 8 — DeepSeekProvider
# ---------------------------------------------------------------------------
# Butuh DEEPSEEK_API_KEY di env + internet.
add_executable(frans_deepseek_test
    tests/deepseek_provider_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config/DeepSeekConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/api_providers/DeepSeekProvider.cpp
)
target_link_libraries(frans_deepseek_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${FRANS_OPENSSL_LIBS}
)
target_include_directories(frans_deepseek_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_definitions(frans_deepseek_test PRIVATE ${FRANS_HTTPLIB_DEFS})
target_compile_options(frans_deepseek_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_deepseek_test COMMAND $<TARGET_FILE:frans_deepseek_test>)
set_tests_properties(frans_deepseek_test PROPERTIES
    LABELS "network"
    TIMEOUT 120
)

# ---------------------------------------------------------------------------
# Stage 12 — GeminiProvider
# ---------------------------------------------------------------------------
# Butuh GEMINI_API_KEY di env + internet.
add_executable(frans_gemini_test
    tests/gemini_provider_smoke_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config/GeminiConfig.cpp
    ${CMAKE_SOURCE_DIR}/src/infrastructure/api_providers/GeminiProvider.cpp
)
target_link_libraries(frans_gemini_test PRIVATE
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${FRANS_OPENSSL_LIBS}
)
target_include_directories(frans_gemini_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)
target_compile_definitions(frans_gemini_test PRIVATE ${FRANS_HTTPLIB_DEFS})
target_compile_options(frans_gemini_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME frans_gemini_test COMMAND $<TARGET_FILE:frans_gemini_test>)
set_tests_properties(frans_gemini_test PROPERTIES
    LABELS "network"
    TIMEOUT 120
)

# ---------------------------------------------------------------------------
# Stage 12 — ProviderRouter 3-tier (unit + network stub)
# ---------------------------------------------------------------------------
add_executable(frans_router_3tier_test tests/router_3tier_smoke_test.cpp)
target_link_libraries(frans_router_3tier_test PRIVATE
    frans_routing
    nlohmann_json::nlohmann_json
    Threads::Threads
)
target_include_directories(frans_router_3tier_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_options(frans_router_3tier_test PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME frans_router_3tier_test COMMAND $<TARGET_FILE:frans_router_3tier_test>)
set_tests_properties(frans_router_3tier_test PROPERTIES
    LABELS "network"
    TIMEOUT 120
)