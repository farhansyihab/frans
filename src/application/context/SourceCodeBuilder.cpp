// src/application/context/SourceCodeBuilder.cpp
//
// Fase C #60 + #61 -- SourceCodeBuilder implementation.
#include "application/context/SourceCodeBuilder.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <map>
namespace frans::application {
namespace {
// FNV-1a 64-bit hash.
std::uint64_t fnv1a_64(const std::string& data) {
std::uint64_t hash = 14695981039346656037ULL;
const std::uint64_t prime = 1099511628211ULL;
for (unsigned char c : data) {
hash ^= c;
hash *= prime;
}
return hash;
}
// Format timestamp ISO-like: "2026-11-13 14:05".
std::string formatTimestamp() {
auto now = std::chrono::system_clock::now();
auto time = std::chrono::system_clock::to_time_t(now);
std::tm tm = *std::localtime(&time);
std::ostringstream oss;
oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
return oss.str();
}
// Escape string untuk JSON (sederhana: handle backslash, quote, dan kontrol).
std::string jsonEscape(const std::string& s) {
std::string result;
result.reserve(s.size() + 10);
for (char c : s) {
if (c == '\\') result += "\\\\";
else if (c == '"') result += "\\\"";
else if (c == '\n') result += "\\n";
else if (c == '\r') result += "\\r";
else if (c == '\t') result += "\\t";
else result += c;
}
return result;
}
} // namespace
std::string fnv1a_64_hex(const std::string& data) {
std::uint64_t hash = fnv1a_64(data);
std::ostringstream oss;
oss << std::hex << std::setfill('0') << std::setw(16) << hash;
return oss.str();
}
std::string buildManifestJson(
const BuildManifest& manifest,
const std::vector<FileHashInfo>& file_hashes)
{
std::ostringstream oss;
oss << "{\n";
oss << "  \"created_at\": \"" << jsonEscape(manifest.created_at) << "\",\n";
oss << "  \"request_hash\": \"" << jsonEscape(manifest.request_hash) << "\",\n";
oss << "  \"mode\": \"" << jsonEscape(manifest.mode) << "\",\n";
oss << "  \"model\": \"" << jsonEscape(manifest.model) << "\",\n";
oss << "  \"files\": [\n";
for (std::size_t i = 0; i < file_hashes.size(); ++i) {
const auto& fi = file_hashes[i];
oss << "    { \"path\": \"" << jsonEscape(fi.path) << "\", "
<< "\"hash\": \"" << jsonEscape(fi.content_hash) << "\", "
<< "\"size_bytes\": " << fi.size_bytes << " }";
if (i + 1 < file_hashes.size()) oss << ",";
oss << "\n";
}
oss << "  ]\n";
oss << "}\n";
return oss.str();
}
BuildResult buildSourceCode(
const std::vector<CollectedFile>& all_files,
const std::vector<ContextFileEntry>& selected_files,
const std::string& project_root,
const std::string& output_dir,
const std::string& context_output_name,
const std::string& request_text,
const BuildManifest& manifest,
std::atomic<bool>* cancel)
{
BuildResult result;
// Validasi input.
if (project_root.empty() || output_dir.empty()) {
result.error = "project_root atau output_dir kosong";
return result;
}
if (context_output_name.empty()) {
result.error = "context_output_name kosong";
return result;
}
// Buat output_dir jika belum ada.
std::error_code ec;
std::filesystem::create_directories(output_dir, ec);
if (ec) {
result.error = "gagal membuat output_dir: " + ec.message();
return result;
}
// SandboxedFileSystem untuk baca file dari disk.
infrastructure::SandboxedFileSystem fs(project_root);
// Kumpulkan path file terpilih (yang isinya akan disertakan).
std::vector<std::string> starred_paths;
std::map<std::string, std::string> file_contents;
std::vector<FileHashInfo> file_hashes;
for (const auto& entry : selected_files) {
if (cancel && cancel->load()) {
result.error = "dibatalkan";
return result;
}
if (!entry.selected) continue;
// Baca file dari disk.
std::string content = fs.readFile(entry.path);
if (content.empty() && !fs.exists(entry.path)) {
// File tidak ada, lewati (sudah divalidasi di ContextSelector).
continue;
}
starred_paths.push_back(entry.path);
file_contents[entry.path] = content;
// Hitung hash KONTEN file (bukan path) untuk deteksi freshness.
FileHashInfo fi;
fi.path = entry.path;
fi.content_hash = fnv1a_64_hex(content);
fi.size_bytes = content.size();
file_hashes.push_back(fi);
}
// Render markdown.
RenderInput render_input;
render_input.all_files = all_files;
render_input.starred_paths = starred_paths;
render_input.file_contents = file_contents;
render_input.options.repo_name = std::filesystem::path(project_root).filename().string();
render_input.options.header_title = context_output_name;
render_input.options.created_at = manifest.created_at.empty() ? formatTimestamp() : manifest.created_at;
render_input.options.request_summary = request_text.substr(0, 200);
render_input.options.total_content_bytes = 0;
for (const auto& [path, content] : file_contents) {
render_input.options.total_content_bytes += content.size();
}
render_input.options.total_files_included = file_contents.size();
std::string markdown = renderMarkdown(render_input);
// Tulis SourceCode.md.
std::filesystem::path source_code_path = std::filesystem::path(output_dir) / context_output_name;
{
std::ofstream out(source_code_path, std::ios::binary);
if (!out) {
result.error = "gagal menulis SourceCode.md: " + source_code_path.string();
return result;
}
out << markdown;
}
// Tulis manifest.
std::filesystem::path manifest_path = std::filesystem::path(output_dir) / (context_output_name + ".manifest.json");
{
std::string manifest_json = buildManifestJson(manifest, file_hashes);
std::ofstream out(manifest_path, std::ios::binary);
if (!out) {
result.error = "gagal menulis manifest: " + manifest_path.string();
return result;
}
out << manifest_json;
}
result.ok = true;
result.source_code_path = source_code_path.string();
result.manifest_path = manifest_path.string();
result.total_bytes = markdown.size();
result.files_included = file_contents.size();
return result;
}
} // namespace frans::application