// src/ui/PromptDialog.h
//
// Dialog Prompt Generator — Fase A #43 + Fase C #58 + Fase C #61.
//
// Implementasi lengkap 4 langkah:
//   1. Permintaan (+ error output opsional)
//   2. Pilih file via Gemini (Fl_Check_Browser, koreksi manual, tambah file)
//   3. Bangun SourceCode.md (status SEGAR/BASI)
//   4. Generate Prompt -> preview read-only -> Copy ke clipboard
//
// Fase C #61: Tambah langkah 2 dan 3 dengan:
//   - Fl_Check_Browser untuk menampilkan file terpilih
//   - Koreksi manual (check/uncheck)
//   - Tombol "Tambah file manual..."
//   - Tombol "Bangun SourceCode.md"
//   - Status SEGAR/BASI berdasarkan manifest
//   - Gating pada Generate Prompt (peringatan jika SourceCode.md tidak segar)
//
// Threading: operasi berat (listModels, selectContext, buildSourceCode)
// dijalankan di worker thread untuk menghindari freeze UI.
#pragma once
#include "config/AppConfig.h"
#include "ui/SessionState.h"
#include "application/context/ContextSelector.h"
#include "application/context/SourceCodeBuilder.h"
#include <string>
#include <atomic>
#include <thread>
#include <vector>
class Fl_Window;
class Fl_Text_Editor;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Button;
class Fl_Box;
class Fl_Check_Browser;
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
// Actions
void doGenerate();
void doCopy();
void doFetchLastError();
void doTestConnection();
void doSelectFiles();
void doAddManual();
void doCancelSelect();
void doBuildSourceCode();
void doOpenFolder();
void setStatus(const std::string& msg, unsigned color);
// Load template dari file (cfg.prompt_template_path).
// Fallback ke defaultPromptTemplate() bila file tidak ada / kosong.
std::string loadTemplate();
// Update UI dari state
void updateBrowserFromSelection();
void updateTotals();
void updateFreshnessDisplay();
void updateKeyStatus();
void updateModeDisplay();
// Freshness check
enum class Freshness { NoFile, Stale, Fresh };
Freshness checkFreshness();
std::string computeRequestHash();
std::string formatBytes(std::size_t bytes);
// Worker threads
void workerListModels();
void workerSelectFiles();
void workerBuildSourceCode();
// Callbacks
static void cbGenerate(Fl_Widget*, void*);
static void cbCopy(Fl_Widget*, void*);
static void cbFetchLastError(Fl_Widget*, void*);
static void cbTestConnection(Fl_Widget*, void*);
static void cbSelectFiles(Fl_Widget*, void*);
static void cbAddManual(Fl_Widget*, void*);
static void cbCancelSelect(Fl_Widget*, void*);
static void cbBrowserChanged(Fl_Widget*, void*);
static void cbBuildSourceCode(Fl_Widget*, void*);
static void cbOpenFolder(Fl_Widget*, void*);
static void cbClose(Fl_Widget*, void*);
// Polling callbacks
static void cbPollListModelsWorker(void*);
static void cbPollSelectWorker(void*);
static void cbPollBuildWorker(void*);
// State
SessionState*                  session_;
const frans::config::AppConfig& cfg_;
// Window & widgets
Fl_Window*       win_              = nullptr;
// Step 1: Permintaan
Fl_Text_Editor*  request_editor_   = nullptr;
Fl_Text_Buffer*  request_buf_      = nullptr;
Fl_Text_Editor*  error_editor_     = nullptr;
Fl_Text_Buffer*  error_buf_        = nullptr;
Fl_Button*       fetch_btn_        = nullptr;
// Step 2: Pilih file
Fl_Button*       select_btn_        = nullptr;
Fl_Box*          key_status_box_    = nullptr;
Fl_Box*          mode_box_          = nullptr;
Fl_Check_Browser* file_browser_     = nullptr;
Fl_Box*          total_box_         = nullptr;
Fl_Button*       add_manual_btn_    = nullptr;
Fl_Button*       cancel_select_btn_ = nullptr;
// Step 3: SourceCode.md
Fl_Button*       build_btn_         = nullptr;
Fl_Box*          freshness_box_     = nullptr;
Fl_Box*          location_box_      = nullptr;
Fl_Button*       open_folder_btn_   = nullptr;
// Step 4: Prompt
Fl_Button*       generate_btn_      = nullptr;
Fl_Button*       copy_btn_          = nullptr;
Fl_Button*       test_conn_btn_     = nullptr;
Fl_Text_Display* preview_display_   = nullptr;
Fl_Text_Buffer*  preview_buf_       = nullptr;
Fl_Box*          status_box_        = nullptr;
// Data state
std::string last_generated_prompt_;
frans::application::SelectionResult last_selection_;
std::vector<frans::application::CollectedFile> last_collected_files_;
frans::application::BuildResult last_build_result_;
std::string current_request_hash_;
bool has_fresh_source_code_ = false;
// Worker state: listModels
std::thread list_models_worker_thread_;
std::atomic<bool> list_models_worker_running_{false};
std::atomic<bool> list_models_worker_cancel_{false};
std::string list_models_worker_message_;
bool list_models_worker_ok_ = false;
// Worker state: selectFiles
std::thread select_worker_thread_;
std::atomic<bool> select_worker_running_{false};
std::atomic<bool> select_worker_cancel_{false};
frans::application::SelectionResult select_worker_result_;
std::vector<frans::application::CollectedFile> select_worker_collected_;
bool select_worker_ok_ = false;
std::string select_worker_error_;
// Worker state: buildSourceCode
std::thread build_worker_thread_;
std::atomic<bool> build_worker_running_{false};
frans::application::BuildResult build_worker_result_;
};
} // namespace frans::ui