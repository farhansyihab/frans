// src/application/build_runner/BuildErrorSummary.h
//
// Fase A #44 — ringkasan error BuildReport untuk SessionState.
//
// Dipisah dari ManualImportDialog.cpp supaya:
//   1. Bisa di-test otomatis tanpa FLTK (tests/SessionStateTest.cpp).
//   2. Tidak ada duplikasi logika antara UI dan test.
//
// Murni fungsi teks: tidak mengakses filesystem, jaringan, maupun UI.
#pragma once

#include "application/build_runner/BuildReport.h"
#include <cstddef>
#include <string>

namespace frans::application {

// Ringkas BuildReport yang GAGAL menjadi teks siap-tempel ke kotak
// "Error output" di PromptDialog.
//
// Format:
//   Build FAILED pada phase <PHASE> (<build_system>).
//   Error: <error_message>
//   Related files:
//     * <path>:<line> -- <note>
//   stderr tail:
//     <max_stderr_bytes terakhir dari command pertama yang gagal>
//
// Catatan:
//   - Kalau report.overall_success == true -> return string kosong
//     (tidak ada error untuk diringkas).
//   - Kalau tidak ada command gagal yang punya stderr, bagian
//     "stderr tail:" tidak ditulis (mis. error Python via stdout).
//   - Tidak pernah throw.
std::string summarizeBuildError(const BuildReport& r,
                                std::size_t max_stderr_bytes = 2048);

// Apakah kegagalan ini karena user menekan Cancel?
//
// Dipakai UI untuk memutuskan: JANGAN menimpa last_build_error dengan
// teks cancel (bukan error kompilasi/runtime yang perlu di-prompt-kan).
//
// Deteksi dari flag CommandResult::cancelled (sumber utama), dengan
// fallback ke penanda "Dibatalkan" pada error_message phase.
bool wasCancelledByUser(const BuildReport& r);

} // namespace frans::application