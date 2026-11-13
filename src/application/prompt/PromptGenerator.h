#pragma once
#include <string>

namespace frans::application {

// Input untuk renderPrompt.
//
//   tmpl               : template teks. Kalau kosong, pakai defaultPromptTemplate().
//   request            : permintaan user (wajib).
//   error_output       : output error dari build/run terakhir (opsional).
//   context_file_name  : nama file konteks (mis. "SourceCode.md"). Kosong = tidak ada.
//   new_files_hint     : daftar file baru yang diperkirakan (opsional).
//
// Placeholder yang dikenali:
//   {{REQUEST}}  {{ERROR_OUTPUT}}  {{CONTEXT_FILE_NAME}}  {{NEW_FILES_HINT}}
//
// Blok kondisional (dirender hanya jika nilai terkait tidak kosong):
//   {{#ERROR}}...{{/ERROR}}
//   {{#CONTEXT}}...{{/CONTEXT}}
//   {{#NEWFILES}}...{{/NEWFILES}}
//
// Perilaku:
//   - Placeholder tak dikenal dibiarkan apa adanya (mis. {{FOO}} -> {{FOO}}).
//   - Blok tanpa penutup -> dirender apa adanya + warning ke stderr.
//   - error_output di-tail +/- 8 KB.
//   - Tidak ada escaping; caller bertanggung jawab atas isi yang aman.
struct PromptInput {
    std::string tmpl;
    std::string request;
    std::string error_output;
    std::string context_file_name;
    std::string new_files_hint;
};

// Render template dengan placeholder dan blok kondisional.
// Murni fungsi teks: tidak mengakses filesystem, jaringan, atau clipboard.
std::string renderPrompt(const PromptInput& in);

// Template default yang di-embed di source (fallback bila file config hilang).
const std::string& defaultPromptTemplate();

} // namespace frans::application