// src/ui/PromptDialog.h
//
// Dialog Prompt Generator — Fase A #43 + Fase C #58.
//
// Hanya mengimplementasi langkah 1 (Permintaan) dan langkah 4
// (Generate + Preview + Copy). Langkah 2 (Pilih file via Gemini)
// dan langkah 3 (SourceCode.md) akan ditambahkan di Fase C.
//
// Fase C #58: Tambah tombol "Tes Koneksi / Daftar Model" untuk
// memverifikasi API key dan menampilkan model yang tersedia.
//
// Alur Fase A:
//   1. User menulis permintaan (+ error output opsional).
//   2. (Fase C) Pilih file via Gemini.
//   3. (Fase C) Bangun SourceCode.md.
//   4. Generate Prompt -> preview read-only -> Copy ke clipboard.
//
// Karena langkah 2-3 belum ada, `context_file_name` dan
// `new_files_hint` dikosongkan. PromptGenerator akan melewatkan
// blok {{#CONTEXT}} dan {{#NEWFILES}}.
//
// Threading: semua operasi di UI thread kecuali listModels yang
// dijalankan di worker thread untuk menghindari freeze UI.
#pragma once
#include "config/AppConfig.h"
#include "ui/SessionState.h"
#include <string>
#include <atomic>
#include <thread>
class Fl_Window;
class Fl_Text_Editor;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Button;
class Fl_Box;
class Fl_Widget;
namespace frans::ui {
class PromptDialog {
public:
// Tampilkan dialog modal. Blok sampai user menutup.
// `session` boleh nullptr (tombol "Ambil dari build terakhir"
// akan menampilkan pesan error bila session kosong).
static void show(SessionState* session,
const frans::config::AppConfig& cfg);
PromptDialog(const PromptDialog&)            = delete;
PromptDialog& operator=(const PromptDialog&) = delete;
private:
explicit PromptDialog(SessionState* session,
const frans::config::AppConfig& cfg);
~PromptDialog();
void buildUI();
void doGenerate();
void doCopy();
void doFetchLastError();
void doTestConnection();
void setStatus(const std::string& msg, unsigned color);
// Load template dari file (cfg.prompt_template_path).
// Fallback ke defaultPromptTemplate() bila file tidak ada / kosong.
std::string loadTemplate();
// Worker thread untuk listModels (Fase C #58)
void workerListModels();
static void cbGenerate(Fl_Widget*, void*);
static void cbCopy(Fl_Widget*, void*);
static void cbFetchLastError(Fl_Widget*, void*);
static void cbTestConnection(Fl_Widget*, void*);
static void cbClose(Fl_Widget*, void*);
// Polling callback untuk mengecek status worker
static void cbPollWorker(void* data);
SessionState*                  session_;
const frans::config::AppConfig& cfg_;
Fl_Window*       win_              = nullptr;
Fl_Text_Editor*  request_editor_   = nullptr;
Fl_Text_Buffer*  request_buf_      = nullptr;
Fl_Text_Editor*  error_editor_     = nullptr;
Fl_Text_Buffer*  error_buf_        = nullptr;
Fl_Button*       fetch_btn_        = nullptr;
Fl_Button*       generate_btn_     = nullptr;
Fl_Button*       copy_btn_         = nullptr;
Fl_Button*       test_conn_btn_    = nullptr;  // Fase C #58
Fl_Text_Display* preview_display_  = nullptr;
Fl_Text_Buffer*  preview_buf_      = nullptr;
Fl_Box*          status_box_       = nullptr;
std::string last_generated_prompt_;
// Fase C #58 -- worker state
std::thread worker_thread_;
std::atomic<bool> worker_running_{false};
std::atomic<bool> worker_cancel_{false};
std::string worker_result_message_;
bool worker_result_ok_ = false;
};
} // namespace frans::ui