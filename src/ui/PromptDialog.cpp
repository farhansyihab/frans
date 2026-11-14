// src/ui/PromptDialog.cpp
#include "ui/PromptDialog.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ai/ApiKeyProvider.h"
#include "application/ai/GeminiClient.h"
#include "application/context/ContextSelector.h"
#include "application/context/SourceCodeBuilder.h"
#include "infrastructure/http/CurlTransport.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
namespace frans::ui {
namespace {
constexpr int kW      = 720;
constexpr int kH      = 850;
constexpr int kPadX   = 15;
constexpr int kInnerW = kW - 2 * kPadX;
constexpr int kBtnH   = 28;
constexpr int kRowGap = 6;
// Baca seluruh isi file teks jadi std::string.
std::string readFileContents(const std::string& path) {
std::ifstream f(path, std::ios::binary);
if (!f) return {};
std::ostringstream ss;
ss << f.rdbuf();
return ss.str();
}
// Tampilkan pesan panjang dalam dialog scrollable.
void showScrollableMessage(const char* title, const std::string& message) {
constexpr int dlg_w = 650;
constexpr int dlg_h = 500;
constexpr int pad   = 10;
constexpr int btn_w = 80;
constexpr int btn_h = 30;
Fl_Window* win = new Fl_Window(dlg_w, dlg_h, title);
win->set_modal();
win->begin();
Fl_Text_Display* display = new Fl_Text_Display(
pad, pad, dlg_w - 2 * pad, dlg_h - 2 * pad - btn_h - pad);
Fl_Text_Buffer* buf = new Fl_Text_Buffer();
display->buffer(buf);
display->textfont(FL_COURIER);
display->textsize(11);
display->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
display->color(FL_WHITE);
buf->text(message.c_str());
Fl_Button* ok_btn = new Fl_Button(
dlg_w - pad - btn_w, dlg_h - pad - btn_h, btn_w, btn_h, "OK");
ok_btn->callback([](Fl_Widget* w, void*) {
if (Fl_Window* win = w ? w->window() : nullptr) {
win->hide();
}
});
ok_btn->take_focus();
win->end();
win->resizable(display);
win->show();
while (win->shown()) Fl::wait();
delete buf;
delete win;
}
// Format bytes ke string readable.
std::string formatBytesHelper(std::size_t bytes) {
if (bytes < 1024) return std::to_string(bytes) + " B";
if (bytes < 1024 * 1024) {
char buf[32];
std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
return buf;
}
char buf[32];
std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
return buf;
}
} // namespace
// ===========================================================================
// Public
// ===========================================================================
void PromptDialog::show(SessionState* session,
const frans::config::AppConfig& cfg)
{
PromptDialog* dlg = new PromptDialog(session, cfg);
dlg->win_->set_modal();
dlg->win_->show();
while (dlg->win_->shown()) Fl::wait();
delete dlg;
}
PromptDialog::PromptDialog(SessionState* session,
const frans::config::AppConfig& cfg)
: session_(session), cfg_(cfg)
{
buildUI();
updateKeyStatus();
updateFreshnessDisplay();
}
PromptDialog::~PromptDialog() {
delete request_buf_;     request_buf_    = nullptr;
delete error_buf_;       error_buf_      = nullptr;
delete preview_buf_;     preview_buf_    = nullptr;
// Cancel workers
if (list_models_worker_running_.load()) {
list_models_worker_cancel_.store(true);
if (list_models_worker_thread_.joinable()) list_models_worker_thread_.join();
}
if (select_worker_running_.load()) {
select_worker_cancel_.store(true);
if (select_worker_thread_.joinable()) select_worker_thread_.join();
}
if (build_worker_running_.load()) {
if (build_worker_thread_.joinable()) build_worker_thread_.join();
}
if (win_) {
if (win_->shown()) win_->hide();
delete win_;
win_ = nullptr;
}
}
// ===========================================================================
// UI construction
// ===========================================================================
void PromptDialog::buildUI() {
win_ = new Fl_Window(kW, kH, "Prompt Generator");
win_->begin();
int y = kPadX;
// ==== Langkah 1: Permintaan ====
{
Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 20, "1. Permintaan");
t->labelfont(FL_BOLD);
t->labelsize(13);
t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
y += 22;
}
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16,
"Tulis permintaan Anda untuk AI (wajib):");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
y += 18;
}
{
const int editor_h = 100;
request_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
request_buf_ = new Fl_Text_Buffer();
request_editor_->buffer(request_buf_);
request_editor_->textfont(FL_COURIER);
request_editor_->textsize(11);
request_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
request_editor_->color(FL_WHITE);
y += editor_h + kRowGap;
}
// Error output + fetch button
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW - 180, 18, "Error output (opsional):");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
fetch_btn_ = new Fl_Button(kPadX + kInnerW - 170, y, 170, 20, "Ambil dari build terakhir");
fetch_btn_->callback(cbFetchLastError, this);
y += 22;
}
{
const int editor_h = 60;
error_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
error_buf_ = new Fl_Text_Buffer();
error_editor_->buffer(error_buf_);
error_editor_->textfont(FL_COURIER);
error_editor_->textsize(10);
error_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
error_editor_->color(FL_WHITE);
y += editor_h + kRowGap;
}
// ==== Langkah 2: Pilih file ====
{
Fl_Box* t = new Fl_Box(kPadX, y, 120, 20, "2. Pilih file");
t->labelfont(FL_BOLD);
t->labelsize(13);
t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
select_btn_ = new Fl_Button(kPadX + 130, y, 180, 22, "Pilih File via Gemini");
select_btn_->callback(cbSelectFiles, this);
key_status_box_ = new Fl_Box(kPadX + 320, y, kInnerW - 320, 20, "");
key_status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
key_status_box_->labelsize(10);
y += 24;
}
{
mode_box_ = new Fl_Box(kPadX, y, kInnerW - 200, 18, "Mode: - | Token: -");
mode_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
mode_box_->labelsize(10);
mode_box_->labelcolor(FL_DARK_GREEN);
test_conn_btn_ = new Fl_Button(kPadX + kInnerW - 190, y, 190, 20, "Tes Koneksi / Daftar Model");
test_conn_btn_->callback(cbTestConnection, this);
y += 22;
}
{
const int browser_h = 130;
file_browser_ = new Fl_Check_Browser(kPadX, y, kInnerW, browser_h);
file_browser_->callback(cbBrowserChanged, this);
file_browser_->textfont(FL_COURIER);
file_browser_->textsize(10);
y += browser_h + kRowGap;
}
{
total_box_ = new Fl_Box(kPadX, y, kInnerW - 280, 20, "Total terpilih: 0 file, 0 B");
total_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
total_box_->labelsize(11);
add_manual_btn_ = new Fl_Button(kPadX + kInnerW - 270, y, 130, 22, "+ Tambah manual...");
add_manual_btn_->callback(cbAddManual, this);
cancel_select_btn_ = new Fl_Button(kPadX + kInnerW - 130, y, 130, 22, "Batal");
cancel_select_btn_->callback(cbCancelSelect, this);
cancel_select_btn_->deactivate();
y += 24 + kRowGap;
}
// ==== Langkah 3: SourceCode.md ====
{
Fl_Box* t = new Fl_Box(kPadX, y, 140, 20, "3. SourceCode.md");
t->labelfont(FL_BOLD);
t->labelsize(13);
t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
build_btn_ = new Fl_Button(kPadX + 150, y, 180, 22, "Bangun SourceCode.md");
build_btn_->callback(cbBuildSourceCode, this);
freshness_box_ = new Fl_Box(kPadX + 340, y, kInnerW - 340, 20, "Status: -");
freshness_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
freshness_box_->labelsize(11);
y += 24;
}
{
location_box_ = new Fl_Box(kPadX, y, kInnerW - 120, 18, "Lokasi: -");
location_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
location_box_->labelsize(10);
open_folder_btn_ = new Fl_Button(kPadX + kInnerW - 110, y, 110, 20, "Buka Folder");
open_folder_btn_->callback(cbOpenFolder, this);
open_folder_btn_->deactivate();
y += 22 + kRowGap;
}
// ==== Status bar ====
{
status_box_ = new Fl_Box(kPadX, y, kInnerW, 22,
"Idle. Tulis permintaan lalu klik [Pilih File via Gemini].");
status_box_->box(FL_DOWN_BOX);
status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
status_box_->labelsize(11);
y += 22 + kRowGap;
}
// ==== Tombol aksi ====
{
generate_btn_ = new Fl_Button(kPadX, y, 160, kBtnH, "Generate Prompt");
generate_btn_->callback(cbGenerate, this);
copy_btn_ = new Fl_Button(kPadX + 170, y, 100, kBtnH, "Copy");
copy_btn_->callback(cbCopy, this);
copy_btn_->deactivate();
Fl_Button* close_btn = new Fl_Button(kPadX + kInnerW - 100, y, 100, kBtnH, "Close");
close_btn_->callback(cbClose, this);
y += kBtnH + kRowGap;
}
// ==== Langkah 4: Preview ====
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16, "4. Preview Prompt (read-only):");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
y += 18;
}
{
const int preview_h = kH - y - kPadX;
preview_display_ = new Fl_Text_Display(kPadX, y, kInnerW, preview_h > 60 ? preview_h : 60, nullptr);
preview_buf_ = new Fl_Text_Buffer();
preview_display_->buffer(preview_buf_);
preview_display_->textfont(FL_COURIER);
preview_display_->textsize(10);
preview_display_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
preview_display_->color(FL_WHITE);
}
win_->end();
win_->resizable(preview_display_);
}
// ===========================================================================
// Helper: format bytes
// ===========================================================================
std::string PromptDialog::formatBytes(std::size_t bytes) {
return formatBytesHelper(bytes);
}
// ===========================================================================
// Helper: compute request hash
// ===========================================================================
std::string PromptDialog::computeRequestHash() {
if (!request_buf_) return "";
const char* text = request_buf_->text();
std::string req = text ? text : "";
return frans::application::fnv1a_64_hex(req);
}
// ===========================================================================
// Helper: check freshness
// ===========================================================================
PromptDialog::Freshness PromptDialog::checkFreshness() {
if (last_build_result_.manifest_path.empty()) return Freshness::NoFile;
// Baca manifest
std::ifstream f(last_build_result_.manifest_path);
if (!f) return Freshness::NoFile;
std::ostringstream ss;
ss << f.rdbuf();
std::string manifest_text = ss.str();
try {
auto manifest = nlohmann::json::parse(manifest_text);
// Cek request hash
std::string manifest_request_hash = manifest.value("request_hash", "");
std::string current_hash = computeRequestHash();
if (manifest_request_hash != current_hash) return Freshness::Stale;
// Cek file hashes
auto files = manifest["files"];
if (!files.is_array()) return Freshness::Stale;
std::string project_root = session_ ? session_->last_project_root : "";
infrastructure::SandboxedFileSystem fs(project_root);
for (const auto& file_entry : files) {
std::string path = file_entry.value("path", "");
std::string manifest_hash = file_entry.value("hash", "");
if (path.empty() || manifest_hash.empty()) continue;
// Baca file dari disk
std::string content = fs.readFile(path);
std::string current_hash_file = frans::application::fnv1a_64_hex(content);
if (current_hash_file != manifest_hash) return Freshness::Stale;
}
return Freshness::Fresh;
} catch (...) {
return Freshness::Stale;
}
}
// ===========================================================================
// Update UI helpers
// ===========================================================================
void PromptDialog::updateKeyStatus() {
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (key_result.found()) {
std::string masked = frans::application::maskApiKey(key_result.key);
if (key_status_box_) {
std::string label = "key: " + masked + " \xE2\x9C\x93"; // ✓
key_status_box_->copy_label(label.c_str());
key_status_box_->labelcolor(FL_DARK_GREEN);
}
if (select_btn_) select_btn_->activate();
} else {
if (key_status_box_) {
key_status_box_->copy_label("key: tidak ditemukan");
key_status_box_->labelcolor(FL_RED);
}
if (select_btn_) select_btn_->deactivate();
}
}
void PromptDialog::updateModeDisplay() {
if (!mode_box_) return;
if (last_selection_.files.empty()) {
mode_box_->copy_label("Mode: - | Token: -");
return;
}
std::string mode_str = last_selection_.mode.empty() ? "-" : last_selection_.mode;
std::string token_str = formatBytesHelper(last_selection_.est_tokens * 4) + " (~" +
std::to_string(last_selection_.est_tokens) + " token)";
std::string label = "Mode: " + mode_str + " | Dikirim: " +
formatBytesHelper(last_selection_.sent_bytes) + " | " + token_str;
mode_box_->copy_label(label.c_str());
}
void PromptDialog::updateBrowserFromSelection() {
if (!file_browser_) return;
file_browser_->clear();
for (const auto& entry : last_selection_.files) {
std::string prio = entry.priority;
if (prio.empty()) prio = "?";
// Format: [priority] path (size) - reason
std::string item = "[" + prio + "] " + entry.path +
" (" + formatBytesHelper(entry.size_bytes) + ")";
if (!entry.reason.empty()) {
item += " - " + entry.reason;
}
file_browser_->add(item.c_str());
int idx = file_browser_->nitems();
file_browser_->checked(idx, entry.selected);
}
updateTotals();
}
void PromptDialog::updateTotals() {
if (!total_box_) return;
std::size_t total_files = 0;
std::size_t total_bytes = 0;
for (const auto& entry : last_selection_.files) {
if (entry.selected) {
++total_files;
total_bytes += entry.size_bytes;
}
}
std::string label = "Total terpilih: " + std::to_string(total_files) +
" file, " + formatBytesHelper(total_bytes);
total_box_->copy_label(label.c_str());
}
void PromptDialog::updateFreshnessDisplay() {
if (!freshness_box_) return;
Freshness fresh = checkFreshness();
switch (fresh) {
case Freshness::Fresh:
freshness_box_->copy_label("Status: SEGAR \xE2\x9C\x93"); // ✓
freshness_box_->labelcolor(FL_DARK_GREEN);
has_fresh_source_code_ = true;
if (location_box_) {
std::string loc = "Lokasi: " + last_build_result_.source_code_path;
location_box_->copy_label(loc.c_str());
}
if (open_folder_btn_) open_folder_btn_->activate();
break;
case Freshness::Stale:
freshness_box_->copy_label("Status: BASI !");
freshness_box_->labelcolor(FL_RED);
has_fresh_source_code_ = false;
break;
case Freshness::NoFile:
freshness_box_->copy_label("Status: belum ada");
freshness_box_->labelcolor(FL_DARK3);
has_fresh_source_code_ = false;
if (location_box_) location_box_->copy_label("Lokasi: -");
if (open_folder_btn_) open_folder_btn_->deactivate();
break;
}
}
void PromptDialog::setStatus(const std::string& msg, unsigned color) {
if (!status_box_) return;
status_box_->copy_label(msg.c_str());
status_box_->labelcolor(static_cast<Fl_Color>(color));
status_box_->redraw();
}
// ===========================================================================
// Actions
// ===========================================================================
std::string PromptDialog::loadTemplate() {
const std::string path = cfg_.prompt_template_path;
if (!path.empty()) {
std::string contents = readFileContents(path);
if (!contents.empty()) return contents;
}
return frans::application::defaultPromptTemplate();
}
void PromptDialog::doGenerate() {
if (!request_buf_ || !preview_buf_) return;
const char* req_raw = request_buf_->text();
const std::string request = req_raw ? req_raw : "";
if (request.empty()) {
fl_message("Permintaan kosong.\nTulis permintaan Anda di kotak atas dulu.");
return;
}
// Gating: cek freshness
Freshness fresh = checkFreshness();
std::string context_file_name;
if (fresh == Freshness::Fresh) {
context_file_name = cfg_.context_output_name;
} else {
// Peringatkan user
int choice = fl_choice("SourceCode.md tidak segar atau belum ada.\n"
"Generate prompt tanpa konteks file?",
"Batal", "Tetap generate", nullptr);
if (choice == 0) return; // Batal
context_file_name = "";
}
const char* err_raw = error_buf_ ? error_buf_->text() : nullptr;
const std::string error_output = err_raw ? err_raw : "";
frans::application::PromptInput in;
in.tmpl              = loadTemplate();
in.request           = request;
in.error_output      = error_output;
in.context_file_name = context_file_name;
// New files hint dari selection
std::string new_files_hint;
for (const auto& nf : last_selection_.new_files_expected) {
if (!new_files_hint.empty()) new_files_hint += "\n";
new_files_hint += nf;
}
in.new_files_hint = new_files_hint;
last_generated_prompt_ = frans::application::renderPrompt(in);
preview_buf_->text(last_generated_prompt_.c_str());
if (copy_btn_) copy_btn_->activate();
char buf[256];
std::snprintf(buf, sizeof(buf),
"Prompt di-generate (%zu karakter). Klik [Copy] untuk menyalin.",
last_generated_prompt_.size());
setStatus(buf, FL_DARK_GREEN);
}
void PromptDialog::doCopy() {
if (last_generated_prompt_.empty()) {
fl_message("Belum ada prompt yang di-generate.\nKlik [Generate Prompt] dulu.");
return;
}
Fl::copy(last_generated_prompt_.c_str(),
static_cast<int>(last_generated_prompt_.size()), 1);
setStatus("Prompt disalin ke clipboard. Paste ke AI browser Anda.", FL_DARK_GREEN);
}
void PromptDialog::doFetchLastError() {
if (!session_) {
fl_message("SessionState tidak tersedia.");
return;
}
if (session_->last_build_error.empty()) {
fl_message("Belum ada error dari build terakhir.");
return;
}
if (error_buf_) {
error_buf_->text(session_->last_build_error.c_str());
setStatus("Error dari build terakhir dimuat.", FL_DARK_GREEN);
}
}
void PromptDialog::doTestConnection() {
if (list_models_worker_running_.load()) {
fl_message("Tes koneksi sedang berjalan.");
return;
}
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
std::string msg = "GEMINI_API_KEY tidak ditemukan.\n" + frans::application::apiKeyMissingHint();
fl_message("%s", msg.c_str());
return;
}
if (test_conn_btn_) test_conn_btn_->deactivate();
setStatus("Menguji koneksi ke Gemini API...", FL_DARK_BLUE);
list_models_worker_cancel_.store(false);
list_models_worker_running_.store(true);
list_models_worker_ok_ = false;
list_models_worker_message_.clear();
list_models_worker_thread_ = std::thread(&PromptDialog::workerListModels, this);
Fl::add_timeout(0.1, cbPollListModelsWorker, this);
}
void PromptDialog::doSelectFiles() {
if (select_worker_running_.load()) {
fl_message("Seleksi file sedang berjalan.");
return;
}
if (!request_buf_) return;
const char* req_raw = request_buf_->text();
std::string request = req_raw ? req_raw : "";
if (request.empty()) {
fl_message("Permintaan kosong.\nTulis permintaan dulu sebelum memilih file.");
return;
}
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
fl_message("GEMINI_API_KEY tidak ditemukan.");
return;
}
if (select_btn_) select_btn_->deactivate();
if (cancel_select_btn_) cancel_select_btn_->activate();
setStatus("Memilih file via Gemini...", FL_DARK_BLUE);
select_worker_cancel_.store(false);
select_worker_running_.store(true);
select_worker_ok_ = false;
select_worker_error_.clear();
select_worker_result_ = frans::application::SelectionResult{};
select_worker_collected_.clear();
select_worker_thread_ = std::thread(&PromptDialog::workerSelectFiles, this);
Fl::add_timeout(0.1, cbPollSelectWorker, this);
}
void PromptDialog::doAddManual() {
if (!session_ || session_->last_project_root.empty()) {
fl_message("Project root belum diatur.\nBuka proyek dulu.");
return;
}
const char* picked = fl_file_chooser("Pilih file untuk ditambahkan",
"*",
session_->last_project_root.c_str());
if (!picked) return;
std::string abs_path = picked;
std::string project_root = session_->last_project_root;
// Hitung relative path
std::string rel_path;
if (abs_path.find(project_root) == 0) {
rel_path = abs_path.substr(project_root.size());
if (!rel_path.empty() && (rel_path[0] == '/' || rel_path[0] == '\\')) {
rel_path = rel_path.substr(1);
}
} else {
fl_message("File harus berada di dalam project root.");
return;
}
// Normalisasi slash
for (char& c : rel_path) if (c == '\\') c = '/';
// Cek duplikat
for (const auto& entry : last_selection_.files) {
if (entry.path == rel_path) {
fl_message("File sudah ada dalam daftar.");
return;
}
}
// Baca ukuran file
std::ifstream f(abs_path, std::ios::binary | std::ios::ate);
if (!f) {
fl_message("Gagal membaca file.");
return;
}
std::size_t size = f.tellg();
// Tambahkan ke selection
frans::application::ContextFileEntry entry;
entry.path = rel_path;
entry.priority = "manual";
entry.reason = "Ditambahkan manual";
entry.size_bytes = size;
entry.selected = true;
last_selection_.files.push_back(entry);
updateBrowserFromSelection();
setStatus("File ditambahkan: " + rel_path, FL_DARK_GREEN);
}
void PromptDialog::doCancelSelect() {
if (select_worker_running_.load()) {
select_worker_cancel_.store(true);
setStatus("Membatalkan seleksi...", FL_DARK_BLUE);
}
}
void PromptDialog::doBuildSourceCode() {
if (build_worker_running_.load()) {
fl_message("Build sedang berjalan.");
return;
}
if (last_selection_.files.empty()) {
fl_message("Belum ada file terpilih.\nKlik [Pilih File via Gemini] dulu.");
return;
}
if (!session_ || session_->last_project_root.empty()) {
fl_message("Project root belum diatur.");
return;
}
if (!request_buf_) return;
const char* req_raw = request_buf_->text();
std::string request = req_raw ? req_raw : "";
if (build_btn_) build_btn_->deactivate();
setStatus("Membangun SourceCode.md...", FL_DARK_BLUE);
build_worker_running_.store(true);
build_worker_result_ = frans::application::BuildResult{};
build_worker_thread_ = std::thread(&PromptDialog::workerBuildSourceCode, this);
Fl::add_timeout(0.1, cbPollBuildWorker, this);
}
void PromptDialog::doOpenFolder() {
if (last_build_result_.source_code_path.empty()) return;
std::string dir = std::filesystem::path(last_build_result_.source_code_path).parent_path().string();
if (dir.empty()) return;
#ifdef _WIN32
std::string cmd = "explorer \"" + dir + "\"";
#else
std::string cmd = "xdg-open \"" + dir + "\" &";
#endif
int ret = std::system(cmd.c_str());
(void)ret; // Suppress unused result warning
}
// ===========================================================================
// Workers
// ===========================================================================
void PromptDialog::workerListModels() {
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
list_models_worker_ok_ = false;
list_models_worker_message_ = "API key tidak ditemukan.";
list_models_worker_running_.store(false);
return;
}
auto transport = std::make_shared<frans::infrastructure::http::CurlTransport>();
frans::application::GeminiClient client(transport);
auto result = client.listModels(cfg_.gemini_endpoint, key_result.key, &list_models_worker_cancel_);
if (!result.ok) {
list_models_worker_ok_ = false;
list_models_worker_message_ = "Error: " + result.error.message;
if (result.error.http_status > 0) {
list_models_worker_message_ += " (HTTP " + std::to_string(result.error.http_status) + ")";
}
} else {
list_models_worker_ok_ = true;
std::ostringstream ss;
ss << "Koneksi berhasil! Ditemukan " << result.models.size() << " model:\n";
int count = 0;
for (const auto& m : result.models) {
if (count >= 20) {
ss << "... dan " << (result.models.size() - 20) << " model lainnya.\n";
break;
}
ss << "• " << m.name;
if (!m.display_name.empty() && m.display_name != m.name) {
ss << " (" << m.display_name << ")";
}
ss << "\n";
++count;
}
bool model_found = false;
for (const auto& m : result.models) {
if (m.name == cfg_.gemini_model || m.name == "models/" + cfg_.gemini_model) {
model_found = true;
break;
}
}
if (model_found) {
ss << "\n✓ Model '" << cfg_.gemini_model << "' tersedia.\n";
} else {
ss << "\n⚠ Model '" << cfg_.gemini_model << "' TIDAK ditemukan.\n";
}
list_models_worker_message_ = ss.str();
}
list_models_worker_running_.store(false);
}
void PromptDialog::workerSelectFiles() {
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
select_worker_ok_ = false;
select_worker_error_ = "API key tidak ditemukan.";
select_worker_running_.store(false);
return;
}
if (!session_ || session_->last_project_root.empty()) {
select_worker_ok_ = false;
select_worker_error_ = "Project root belum diatur.";
select_worker_running_.store(false);
return;
}
const char* req_raw = request_buf_->text();
std::string request = req_raw ? req_raw : "";
const char* err_raw = error_buf_ ? error_buf_->text() : nullptr;
std::string error_output = err_raw ? err_raw : "";
// Baca selector prompt
std::string selector_prompt = readFileContents(cfg_.selector_prompt_path);
if (selector_prompt.empty()) {
selector_prompt = "Anda adalah asisten pemilih konteks. Pilih file yang relevan.";
}
// Parse always_include
std::vector<std::string> always_include;
if (!cfg_.context_always_include.empty()) {
std::istringstream iss(cfg_.context_always_include);
std::string item;
while (std::getline(iss, item, ',')) {
// Trim
size_t start = item.find_first_not_of(" \t");
size_t end = item.find_last_not_of(" \t");
if (start != std::string::npos && end != std::string::npos) {
always_include.push_back(item.substr(start, end - start + 1));
}
}
}
// Setup request
frans::application::ContextRequest ctx_req;
ctx_req.project_root = session_->last_project_root;
ctx_req.request_text = request;
ctx_req.error_output = error_output;
ctx_req.always_include = always_include;
ctx_req.max_context_bytes = cfg_.context_max_kb * 1024;
ctx_req.max_files = cfg_.context_max_files;
ctx_req.gemini_model = cfg_.gemini_model;
ctx_req.gemini_endpoint = cfg_.gemini_endpoint;
ctx_req.api_key = key_result.key;
ctx_req.selector_prompt = selector_prompt;
ctx_req.gemini_timeout_sec = cfg_.gemini_timeout_sec;
ctx_req.gemini_full_mode_max_tokens = cfg_.gemini_full_mode_max_tokens;
ctx_req.gemini_index_head_lines = cfg_.gemini_index_head_lines;
// Create client
auto transport = std::make_shared<frans::infrastructure::http::CurlTransport>();
frans::application::GeminiClient client(transport);
// Call selectContext
auto result = frans::application::selectContext(ctx_req, client, &select_worker_cancel_);
if (select_worker_cancel_.load()) {
select_worker_ok_ = false;
select_worker_error_ = "Dibatalkan oleh pengguna.";
} else if (!result.ok) {
select_worker_ok_ = false;
select_worker_error_ = result.error;
} else {
select_worker_ok_ = true;
select_worker_result_ = result;
// Kumpulkan file untuk build nanti
frans::application::CollectOptions col_opt;
col_opt.max_file_bytes = 256 * 1024;
col_opt.exclude_secrets = true;
select_worker_collected_ = frans::application::collectFiles(ctx_req.project_root, col_opt, nullptr).files;
}
select_worker_running_.store(false);
}
void PromptDialog::workerBuildSourceCode() {
if (!session_ || session_->last_project_root.empty()) {
build_worker_result_.error = "Project root belum diatur.";
build_worker_running_.store(false);
return;
}
const char* req_raw = request_buf_->text();
std::string request = req_raw ? req_raw : "";
// Setup manifest
frans::application::BuildManifest manifest;
manifest.request_hash = computeRequestHash();
manifest.mode = last_selection_.mode;
manifest.model = cfg_.gemini_model;
// Call buildSourceCode
auto result = frans::application::buildSourceCode(
select_worker_collected_.empty() ? last_collected_files_ : select_worker_collected_,
last_selection_.files,
session_->last_project_root,
cfg_.ingest_output_dir,
cfg_.context_output_name,
request,
manifest,
nullptr);
build_worker_result_ = result;
build_worker_running_.store(false);
}
// ===========================================================================
// Callbacks
// ===========================================================================
void PromptDialog::cbGenerate(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doGenerate();
}
void PromptDialog::cbCopy(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doCopy();
}
void PromptDialog::cbFetchLastError(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doFetchLastError();
}
void PromptDialog::cbTestConnection(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doTestConnection();
}
void PromptDialog::cbSelectFiles(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doSelectFiles();
}
void PromptDialog::cbAddManual(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doAddManual();
}
void PromptDialog::cbCancelSelect(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doCancelSelect();
}
void PromptDialog::cbBrowserChanged(Fl_Widget*, void* data) {
auto* dlg = static_cast<PromptDialog*>(data);
if (!dlg || !dlg->file_browser_) return;
// Sync browser state dengan last_selection_
int n = dlg->file_browser_->nitems();
for (int i = 0; i < n && i < static_cast<int>(dlg->last_selection_.files.size()); ++i) {
dlg->last_selection_.files[i].selected = dlg->file_browser_->checked(i + 1);
}
dlg->updateTotals();
}
void PromptDialog::cbBuildSourceCode(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doBuildSourceCode();
}
void PromptDialog::cbOpenFolder(Fl_Widget*, void* data) {
static_cast<PromptDialog*>(data)->doOpenFolder();
}
void PromptDialog::cbClose(Fl_Widget* w, void*) {
if (Fl_Window* win = w ? w->window() : nullptr) {
win->hide();
}
}
// ===========================================================================
// Polling callbacks
// ===========================================================================
void PromptDialog::cbPollListModelsWorker(void* data) {
auto* dlg = static_cast<PromptDialog*>(data);
if (!dlg) return;
if (dlg->list_models_worker_running_.load()) {
Fl::repeat_timeout(0.1, cbPollListModelsWorker, data);
return;
}
if (dlg->list_models_worker_thread_.joinable()) {
dlg->list_models_worker_thread_.join();
}
if (dlg->test_conn_btn_) dlg->test_conn_btn_->activate();
if (dlg->list_models_worker_ok_) {
dlg->setStatus("Tes koneksi berhasil.", FL_DARK_GREEN);
showScrollableMessage("Daftar Model Gemini", dlg->list_models_worker_message_);
} else {
dlg->setStatus("Tes koneksi gagal.", FL_RED);
showScrollableMessage("Tes Koneksi Gagal", dlg->list_models_worker_message_);
}
dlg->list_models_worker_message_.clear();
dlg->list_models_worker_ok_ = false;
}
void PromptDialog::cbPollSelectWorker(void* data) {
auto* dlg = static_cast<PromptDialog*>(data);
if (!dlg) return;
if (dlg->select_worker_running_.load()) {
Fl::repeat_timeout(0.1, cbPollSelectWorker, data);
return;
}
if (dlg->select_worker_thread_.joinable()) {
dlg->select_worker_thread_.join();
}
if (dlg->select_btn_) dlg->select_btn_->activate();
if (dlg->cancel_select_btn_) dlg->cancel_select_btn_->deactivate();
if (dlg->select_worker_ok_) {
dlg->last_selection_ = dlg->select_worker_result_;
dlg->last_collected_files_ = dlg->select_worker_collected_;
dlg->updateBrowserFromSelection();
dlg->updateModeDisplay();
dlg->setStatus("Seleksi file selesai. Tinjau dan koreksi centang.", FL_DARK_GREEN);
} else {
dlg->setStatus("Seleksi file gagal: " + dlg->select_worker_error_, FL_RED);
fl_message("Seleksi file gagal:\n%s", dlg->select_worker_error_.c_str());
}
dlg->select_worker_error_.clear();
dlg->select_worker_ok_ = false;
}
void PromptDialog::cbPollBuildWorker(void* data) {
auto* dlg = static_cast<PromptDialog*>(data);
if (!dlg) return;
if (dlg->build_worker_running_.load()) {
Fl::repeat_timeout(0.1, cbPollBuildWorker, data);
return;
}
if (dlg->build_worker_thread_.joinable()) {
dlg->build_worker_thread_.join();
}
if (dlg->build_btn_) dlg->build_btn_->activate();
if (dlg->build_worker_result_.ok) {
dlg->last_build_result_ = dlg->build_worker_result_;
if (dlg->session_) {
dlg->session_->last_context_path = dlg->last_build_result_.source_code_path;
}
dlg->updateFreshnessDisplay();
char buf[256];
std::snprintf(buf, sizeof(buf),
"SourceCode.md dibangun: %zu file, %s",
dlg->last_build_result_.files_included,
formatBytesHelper(dlg->last_build_result_.total_bytes).c_str());
dlg->setStatus(buf, FL_DARK_GREEN);
} else {
dlg->setStatus("Build gagal: " + dlg->build_worker_result_.error, FL_RED);
fl_message("Build SourceCode.md gagal:\n%s", dlg->build_worker_result_.error.c_str());
}
}
} // namespace frans::ui