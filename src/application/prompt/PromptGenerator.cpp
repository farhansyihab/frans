#include "application/prompt/PromptGenerator.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace frans::application {

namespace {

constexpr const char* kLogPrefix = "[frans][prompt]";

// Template default bawaan (fallback).
//
// Sinkron dengan config/prompt_template.txt.
// Triple-backtick di dalam string literal C++ aman (tidak perlu escape).
const std::string kDefaultTemplate =
    "Anda adalah asisten programmer yang ahli. Ikuti aturan berikut dengan ketat:\n"
    "\n"
    "1. Bahasa: gunakan Bahasa Indonesia untuk penjelasan, tapi kode dan komentar\n"
    "   teknis dalam bahasa Inggris bila perlu.\n"
    "\n"
    "2. Format output WAJIB multi-file. Setiap file HARUS diawali baris \"File: <path>\"\n"
    "   (tanpa tanda kutip), diikuti IMMEDIATELY oleh fenced code block dengan tag bahasa.\n"
    "   Contoh:\n"
    "   File: src/main.cpp\n"
    "   ```cpp\n"
    "   #include <iostream>\n"
    "   int main() { ... }\n"
    "   ```\n"
    "\n"
    "3. Path relatif terhadap root proyek. Jangan pakai path absolut.\n"
    "\n"
    "4. Baris \"File: \" HARUS berada di awal baris (tidak ada indentasi, bullet, header,\n"
    "   atau penomoran sebelum kata \"File:\").\n"
    "\n"
    "5. Tutup fenced code block dengan tiga backtick (```) SEBELUM memulai File: berikutnya.\n"
    "\n"
    "6. Di dalam konten file, JANGAN gunakan literal tiga backtick berurutan —\n"
    "   akan merusak parser. Jika perlu menampilkan backtick dalam kode, gunakan\n"
    "   escape atau ganti dengan karakter lain.\n"
    "\n"
    "7. Jangan sertakan file yang tidak berubah.\n"
    "\n"
    "8. Jangan sertakan file binary, gambar, atau file besar yang tidak relevan.\n"
    "\n"
    "9. Full file content, not diff. Tulis isi file LENGKAP, bukan patch atau perubahan parsial.\n"
    "\n"
    "{{#CONTEXT}}\n"
    "10. Source code acuan ada pada file terlampir \"{{CONTEXT_FILE_NAME}}\". Jadikan\n"
    "    itu acuan utama (struktur folder, nama, interface, gaya kode). Jangan mengubah\n"
    "    file yang tidak perlu diubah.\n"
    "{{/CONTEXT}}\n"
    "\n"
    "Then output the code for my request below.\n"
    "\n"
    "--- my request ---\n"
    "{{REQUEST}}\n"
    "\n"
    "{{#NEWFILES}}\n"
    "--- file baru yang kemungkinan perlu dibuat (saran, boleh diabaikan) ---\n"
    "{{NEW_FILES_HINT}}\n"
    "{{/NEWFILES}}\n"
    "\n"
    "{{#ERROR}}\n"
    "--- error output ---\n"
    "{{ERROR_OUTPUT}}\n"
    "{{/ERROR}}\n";

// Tail string hingga max_bytes (dari akhir). Tambahkan prefix jika dipotong.
std::string tailString(const std::string& s, std::size_t max_bytes) {
    if (s.size() <= max_bytes) return s;
    std::string prefix = "\n...(truncated, showing last "
                       + std::to_string(max_bytes) + " bytes)...\n";
    return prefix + s.substr(s.size() - max_bytes);
}

// Cek apakah nama blok kondisional "aktif" (nilainya tidak kosong).
bool isBlockActive(const std::string& block_name, const PromptInput& in) {
    if (block_name == "ERROR")    return !in.error_output.empty();
    if (block_name == "CONTEXT")  return !in.context_file_name.empty();
    if (block_name == "NEWFILES") return !in.new_files_hint.empty();
    return false;
}

// Cek apakah nama placeholder dikenali.
bool isPlaceholderKnown(const std::string& name) {
    return name == "REQUEST"
        || name == "ERROR_OUTPUT"
        || name == "CONTEXT_FILE_NAME"
        || name == "NEW_FILES_HINT";
}

// Ambil nilai placeholder. Untuk ERROR_OUTPUT, diterapkan tail 8 KB.
std::string placeholderValue(const std::string& name, const PromptInput& in) {
    if (name == "REQUEST")           return in.request;
    if (name == "ERROR_OUTPUT")      return tailString(in.error_output, 8 * 1024);
    if (name == "CONTEXT_FILE_NAME") return in.context_file_name;
    if (name == "NEW_FILES_HINT")    return in.new_files_hint;
    return {};
}

// Renderer internal. Dipanggil rekursif untuk isi blok kondisional.
// Tidak melakukan fallback ke default template — fallback hanya di
// entry point renderPrompt().
std::string renderTemplate(const std::string& tmpl, const PromptInput& in) {
    std::string out;
    out.reserve(tmpl.size() + in.request.size());

    std::size_t pos = 0;
    const std::size_t n = tmpl.size();

    while (pos < n) {
        const std::size_t open = tmpl.find("{{", pos);
        if (open == std::string::npos) {
            out.append(tmpl, pos, n - pos);
            break;
        }

        // Teks sebelum "{{".
        out.append(tmpl, pos, open - pos);

        const std::size_t close = tmpl.find("}}", open + 2);
        if (close == std::string::npos) {
            // "{{" tanpa penutup "}}" -> sisa template dirender apa adanya.
            out.append(tmpl, open, n - open);
            std::fprintf(stderr,
                         "%s warning: blok '{{' tanpa penutup '}}' — dirender apa adanya\n",
                         kLogPrefix);
            std::fflush(stderr);
            break;
        }

        const std::string tag = tmpl.substr(open + 2, close - open - 2);
        pos = close + 2;

        // ---- Blok kondisional pembuka: {{#NAME}}...{{/NAME}} ----
        if (!tag.empty() && tag[0] == '#') {
            const std::string block_name = tag.substr(1);
            const std::string end_tag = "{{/" + block_name + "}}";
            const std::size_t end_pos = tmpl.find(end_tag, pos);

            if (end_pos == std::string::npos) {
                // Blok tanpa penutup -> tag pembuka + sisa template
                // dirender apa adanya.
                out.append("{{#");
                out.append(block_name);
                out.append("}}");
                out.append(tmpl, pos, n - pos);
                std::fprintf(stderr,
                             "%s warning: blok kondisional '{{#%s}}' tanpa penutup "
                             "'{{/%s}}' — dirender apa adanya\n",
                             kLogPrefix, block_name.c_str(), block_name.c_str());
                std::fflush(stderr);
                pos = n;
                break;
            }

            if (isBlockActive(block_name, in)) {
                const std::string inner = tmpl.substr(pos, end_pos - pos);
                // Render isi blok secara rekursif (boleh ada placeholder
                // biasa di dalamnya).
                out.append(renderTemplate(inner, in));
            }

            // Skip hingga setelah end_tag.
            pos = end_pos + end_tag.size();
            continue;
        }

        // ---- Blok kondisional penutup: {{/NAME}} ----
        // Seharusnya tidak muncul di level top (sudah dikonsumsi di atas).
        // Kalau tetap muncul, biarkan apa adanya.
        if (!tag.empty() && tag[0] == '/') {
            out.append("{{/");
            out.append(tag.substr(1));
            out.append("}}");
            continue;
        }

        // ---- Placeholder biasa: {{NAME}} ----
        if (isPlaceholderKnown(tag)) {
            out.append(placeholderValue(tag, in));
        } else {
            // Placeholder tak dikenal -> biarkan apa adanya.
            out.append("{{");
            out.append(tag);
            out.append("}}");
        }
    }

    return out;
}

} // namespace

const std::string& defaultPromptTemplate() {
    static const std::string t = kDefaultTemplate;
    return t;
}

std::string renderPrompt(const PromptInput& in) {
    const std::string& tmpl = in.tmpl.empty() ? defaultPromptTemplate() : in.tmpl;
    return renderTemplate(tmpl, in);
}

} // namespace frans::application
