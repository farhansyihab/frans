// ~/AgenAI/frans/src/application/context_selector/TokenEstimate.h
#pragma once

#include <cstddef>
#include <string>

namespace frans::application {

// Rule-of-thumb token estimator untuk alokasi budget.
//
// Rasio char/token nyata bervariasi (3–5 untuk teks campuran Inggris + kode).
// Kita pakai char/3 (round-up) — KONSERVATIF: over-estimate lebih aman
// daripada under-estimate, karena under-estimate bisa membuat kita overflow
// context window model.
//
// Ini BUKAN tokenizer. Jangan pakai untuk billing. Kalau nanti Stage 8/9
// butuh akurasi ~5% untuk cost tracking API berbayar, ganti fungsi ini
// dengan tiktoken-like estimator — semua pemakai cukup satu titik ini.
inline std::size_t estimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    return (text.size() + 2) / 3;   // ceil(len/3)
}

} // namespace frans::application
