// src/ui/PromptDialog.cpp
#include "ui/PromptDialog.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ai/ApiKeyProvider.h"
#include "application/ai/GeminiClient.h"
#include "infrastructure/http/CurlTransport.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
namespace frans::ui {
namespace {
constexpr int kW      = 720;
constexpr int kH      = 640;
constexpr int kPadX   = 15;
constexpr int kInnerW = kW - 2 * kPadX;
constexpr int kBtnH   = 30;
constexpr int kRowGap = 6;
// Baca seluruh isi file teks jadi std::string.
// Return string kosong bila file tidak bisa dibuka.
std::string readFileContents(const std::string& path) {
std::ifstream f(path, std::ios::binary);
if (!f) return {};
std::ostringstream ss;
ss << f.rdbuf();
return ss.str();
}
// Tampilkan pesan panjang dalam dialog scrollable.
// Modal, blok sampai user klik OK.
void showScrollableMessage(const char* title, const std::string& message) {
constexpr int dlg_w = 650;
constexpr int dlg_h = 500;
constexpr int pad   = 10;
constexpr int btn_w = 80;
constexpr int btn_h = 30;
Fl_Window* win = new Fl_Window(dlg_w, dlg_h, title);
win->set_modal();
win->begin();
// Text display (read-only, scrollable)
Fl_Text_Display* display = new Fl_Text_Display(
pad, pad, dlg_w - 2 * pad, dlg_h - 2 * pad - btn_h - pad);
Fl_Text_Buffer* buf = new Fl_Text_Buffer();
display->buffer(buf);
display->textfont(FL_COURIER);
display->textsize(11);
display->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
display->color(FL_WHITE);
buf->text(message.c_str());
// OK button
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
// Loop modal sampai window di-hide
while (win->shown()) Fl::wait();
// Cleanup
delete buf;
delete win;
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
// Loop modal FLTK — blok sampai window di-hide.
while (dlg->win_->shown()) Fl::wait();
delete dlg;
}
PromptDialog::PromptDialog(SessionState* session,
const frans::config::AppConfig& cfg)
: session_(session), cfg_(cfg)
{
buildUI();
}
PromptDialog::~PromptDialog() {
// Hapus buffer teks sebelum window (FLTK ownership).
delete request_buf_;     request_buf_    = nullptr;
delete error_buf_;       error_buf_      = nullptr;
delete preview_buf_;     preview_buf_    = nullptr;
// Cancel worker jika masih berjalan
if (worker_running_.load()) {
worker_cancel_.store(true);
if (worker_thread_.joinable()) {
worker_thread_.join();
}
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
// ---- Langkah 1: Permintaan ----
{
Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 20,
"1. Permintaan");
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
const int editor_h = 120;
request_editor_ = new Fl_Text_Editor(
kPadX, y, kInnerW, editor_h, nullptr);
request_buf_ = new Fl_Text_Buffer();
request_editor_->buffer(request_buf_);
request_editor_->textfont(FL_COURIER);
request_editor_->textsize(11);
request_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
request_editor_->color(FL_WHITE);
request_editor_->tooltip(
"Permintaan akan menggantikan placeholder {{REQUEST}} "
"di template prompt.");
y += editor_h + kRowGap;
}
// ---- Error output (opsional) + tombol "Ambil dari build terakhir" ----
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW - 180, 20,
"Error output (opsional):");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
fetch_btn_ = new Fl_Button(
kPadX + kInnerW - 170, y, 170, 22,
"Ambil dari build terakhir");
fetch_btn_->callback(cbFetchLastError, this);
fetch_btn_->tooltip(
"Ambil ringkasan error dari build/test/run terakhir.\n"
"Hanya berfungsi jika Manual Import pernah dijalankan\n"
"dan build-nya gagal.");
y += 22;
}
{
const int editor_h = 80;
error_editor_ = new Fl_Text_Editor(
kPadX, y, kInnerW, editor_h, nullptr);
error_buf_ = new Fl_Text_Buffer();
error_editor_->buffer(error_buf_);
error_editor_->textfont(FL_COURIER);
error_editor_->textsize(10);
error_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
error_editor_->color(FL_WHITE);
error_editor_->tooltip(
"Error output akan menggantikan {{ERROR_OUTPUT}}.\n"
"Otomatis di-tail +/- 8 KB saat di-render.");
y += editor_h + kRowGap;
}
// ---- Fase C #58: Test Connection button ----
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW - 200, 20,
"Verifikasi API Key & Model:");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
test_conn_btn_ = new Fl_Button(
kPadX + kInnerW - 190, y, 190, 22,
"Tes Koneksi / Daftar Model");
test_conn_btn_->callback(cbTestConnection, this);
test_conn_btn_->tooltip(
"Verifikasi GEMINI_API_KEY dan tampilkan daftar model\n"
"yang tersedia. Memanggil GET /models endpoint.");
y += 22 + kRowGap;
}
// ---- Status bar ----
{
status_box_ = new Fl_Box(kPadX, y, kInnerW, 22,
"Idle. Tulis permintaan lalu klik [Generate Prompt].");
status_box_->box(FL_DOWN_BOX);
status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
status_box_->labelsize(11);
y += 22 + kRowGap;
}
// ---- Tombol aksi: Generate / Copy / Close ----
{
generate_btn_ = new Fl_Button(
kPadX, y, 180, kBtnH, "Generate Prompt");
generate_btn_->callback(cbGenerate, this);
generate_btn_->tooltip(
"Render template + permintaan menjadi prompt.\n"
"Hasil muncul di panel Preview di bawah.");
copy_btn_ = new Fl_Button(
kPadX + 190, y, 120, kBtnH, "Copy");
copy_btn_->callback(cbCopy, this);
copy_btn_->deactivate();
copy_btn_->tooltip(
"Salin prompt yang sudah di-generate ke clipboard.\n"
"Tidak ada auto-copy (Anda paste manual ke AI browser).");
Fl_Button* close_btn = new Fl_Button(
kPadX + kInnerW - 100, y, 100, kBtnH, "Close");
close_btn->callback(cbClose, this);
y += kBtnH + kRowGap;
}
// ---- Langkah 4: Preview ----
{
Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16,
"4. Preview Prompt (read-only):");
l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
l->labelsize(11);
l->labelcolor(FL_DARK_BLUE);
y += 18;
}
{
// Sisa ruang vertikal untuk preview.
const int preview_h = kH - y - kPadX;
preview_display_ = new Fl_Text_Display(
kPadX, y, kInnerW, preview_h > 60 ? preview_h : 60, nullptr);
preview_buf_ = new Fl_Text_Buffer();
preview_display_->buffer(preview_buf_);
preview_display_->textfont(FL_COURIER);
preview_display_->textsize(10);
preview_display_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
preview_display_->color(FL_WHITE);
// Fl_Text_Display sudah read-only secara bawaan (hanya Fl_Text_Editor
// yang bisa diedit), jadi tidak perlu memanggil set_readonly().
preview_display_->tooltip(
"Hasil render prompt. Read-only — salin lewat tombol [Copy].");
}
win_->end();
win_->resizable(preview_display_);
}
// ===========================================================================
// Actions
// ===========================================================================
std::string PromptDialog::loadTemplate() {
const std::string path = cfg_.prompt_template_path;
if (!path.empty()) {
std::string contents = readFileContents(path);
if (!contents.empty()) {
std::fprintf(stderr,
"[frans][prompt_dialog] template loaded from %s (%zu bytes)\n",
path.c_str(), contents.size());
std::fflush(stderr);
return contents;
}
std::fprintf(stderr,
"[frans][prompt_dialog] template '%s' tidak ditemukan / kosong "
"-- pakai default template bawaan.\n",
path.c_str());
std::fflush(stderr);
}
return frans::application::defaultPromptTemplate();
}
void PromptDialog::doGenerate() {
if (!request_buf_ || !preview_buf_) return;
const char* req_raw = request_buf_->text();
const std::string request = req_raw ? req_raw : "";
if (request.empty()) {
fl_message("Permintaan kosong.\n"
"Tulis permintaan Anda di kotak atas dulu.");
return;
}
const char* err_raw = error_buf_ ? error_buf_->text() : nullptr;
const std::string error_output = err_raw ? err_raw : "";
frans::application::PromptInput in;
in.tmpl              = loadTemplate();
in.request           = request;
in.error_output      = error_output;
// Fase A: langkah 2-3 belum ada, jadi context & new_files kosong.
// PromptGenerator akan melewatkan blok {{#CONTEXT}} dan {{#NEWFILES}}.
in.context_file_name = "";
in.new_files_hint    = "";
last_generated_prompt_ = frans::application::renderPrompt(in);
preview_buf_->text(last_generated_prompt_.c_str());
if (copy_btn_) copy_btn_->activate();
char buf[256];
std::snprintf(buf, sizeof(buf),
"Prompt di-generate (%zu karakter). Klik [Copy] untuk menyalin.",
last_generated_prompt_.size());
setStatus(buf, FL_DARK_GREEN);
std::fprintf(stderr,
"[frans][prompt_dialog] prompt generated: %zu bytes\n",
last_generated_prompt_.size());
std::fflush(stderr);
}
void PromptDialog::doCopy() {
if (last_generated_prompt_.empty()) {
fl_message("Belum ada prompt yang di-generate.\n"
"Klik [Generate Prompt] dulu.");
return;
}
// Fl::copy(text, len, destination)
//   destination = 0 -> primary selection (X11 only)
//   destination = 1 -> clipboard global (Windows & X11 CLIPBOARD)
// Kita pakai clipboard global supaya Ctrl+V bekerja di browser.
Fl::copy(last_generated_prompt_.c_str(),
static_cast<int>(last_generated_prompt_.size()),
1);
setStatus("Prompt disalin ke clipboard. Paste ke AI browser Anda.",
FL_DARK_GREEN);
std::fprintf(stderr,
"[frans][prompt_dialog] prompt copied to clipboard (%zu bytes)\n",
last_generated_prompt_.size());
std::fflush(stderr);
}
void PromptDialog::doFetchLastError() {
if (!session_) {
fl_message("SessionState tidak tersedia.\n"
"Dialog dipanggil tanpa session pointer.");
return;
}
if (session_->last_build_error.empty()) {
fl_message("Belum ada error dari build terakhir.\n"
"Jalankan Manual Import + Build dulu, lalu coba lagi "
"setelah build gagal.");
return;
}
if (error_buf_) {
error_buf_->text(session_->last_build_error.c_str());
setStatus("Error dari build terakhir dimuat ke kotak error output.",
FL_DARK_GREEN);
}
}
// ===========================================================================
// Fase C #58 -- Test Connection
// ===========================================================================
void PromptDialog::doTestConnection() {
if (worker_running_.load()) {
fl_message("Tes koneksi sedang berjalan. Tunggu hingga selesai "
"atau tutup dialog untuk membatalkan.");
return;
}
// Resolve API key
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
std::string msg = "GEMINI_API_KEY tidak ditemukan.\n\n" + frans::application::apiKeyMissingHint();
fl_message("%s", msg.c_str());
return;
}
// Disable button during test
if (test_conn_btn_) test_conn_btn_->deactivate();
setStatus("Menguji koneksi ke Gemini API...", FL_DARK_BLUE);
// Start worker thread
worker_cancel_.store(false);
worker_running_.store(true);
worker_result_ok_ = false;
worker_result_message_.clear();
worker_thread_ = std::thread(&PromptDialog::workerListModels, this);
// Start polling
Fl::add_timeout(0.1, cbPollWorker, this);
}
void PromptDialog::workerListModels() {
// Resolve API key again in worker (safe, no UI access)
frans::application::ApiKeyOptions key_opt;
key_opt.key_file = cfg_.gemini_api_key_file;
auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
if (!key_result.found()) {
worker_result_ok_ = false;
worker_result_message_ = "API key tidak ditemukan.";
worker_running_.store(false);
return;
}
// Create transport and client
auto transport = std::make_shared<frans::infrastructure::http::CurlTransport>();
frans::application::GeminiClient client(transport);
// Call listModels
auto result = client.listModels(
cfg_.gemini_endpoint,
key_result.key,
&worker_cancel_);
if (!result.ok) {
worker_result_ok_ = false;
worker_result_message_ = "Error: " + result.error.message;
if (result.error.http_status > 0) {
worker_result_message_ += " (HTTP " + std::to_string(result.error.http_status) + ")";
}
} else {
worker_result_ok_ = true;
std::ostringstream ss;
ss << "Koneksi berhasil! Ditemukan " << result.models.size() << " model:\n\n";
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
if (!m.description.empty()) {
// Truncate description to keep output readable
std::string desc = m.description;
if (desc.size() > 100) {
desc = desc.substr(0, 97) + "...";
}
ss << "  " << desc << "\n";
}
if (!m.supported_methods.empty()) {
ss << "  Methods: ";
for (size_t i = 0; i < m.supported_methods.size(); ++i) {
if (i > 0) ss << ", ";
ss << m.supported_methods[i];
}
ss << "\n";
}
ss << "\n";
++count;
}
// Check if configured model is in the list
bool model_found = false;
for (const auto& m : result.models) {
if (m.name == cfg_.gemini_model || m.name == "models/" + cfg_.gemini_model) {
model_found = true;
break;
}
}
if (model_found) {
ss << "✓ Model '" << cfg_.gemini_model << "' tersedia untuk akun Anda.\n";
} else {
ss << "⚠ Model '" << cfg_.gemini_model << "' TIDAK ditemukan dalam daftar.\n";
ss << "  Periksa config/app.json dan ganti 'gemini_model' dengan nama yang valid.\n";
}
worker_result_message_ = ss.str();
}
worker_running_.store(false);
}
void PromptDialog::setStatus(const std::string& msg, unsigned color) {
if (!status_box_) return;
status_box_->copy_label(msg.c_str());
status_box_->labelcolor(static_cast<Fl_Color>(color));
status_box_->redraw();
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
void PromptDialog::cbClose(Fl_Widget* w, void*) {
if (Fl_Window* win = w ? w->window() : nullptr) {
win->hide();
}
}
void PromptDialog::cbPollWorker(void* data) {
auto* dlg = static_cast<PromptDialog*>(data);
if (!dlg) return;
// Check if worker is still running
if (dlg->worker_running_.load()) {
// Still running, poll again
Fl::repeat_timeout(0.1, cbPollWorker, data);
return;
}
// Worker finished, join thread
if (dlg->worker_thread_.joinable()) {
dlg->worker_thread_.join();
}
// Re-enable button
if (dlg->test_conn_btn_) {
dlg->test_conn_btn_->activate();
}
// Show result using scrollable dialog
if (dlg->worker_result_ok_) {
dlg->setStatus("Tes koneksi berhasil. Lihat detail di dialog.", FL_DARK_GREEN);
showScrollableMessage("Daftar Model Gemini", dlg->worker_result_message_);
} else {
dlg->setStatus("Tes koneksi gagal.", FL_RED);
showScrollableMessage("Tes Koneksi Gagal", dlg->worker_result_message_);
}
// Clear worker state
dlg->worker_result_message_.clear();
dlg->worker_result_ok_ = false;
}
} // namespace frans::ui