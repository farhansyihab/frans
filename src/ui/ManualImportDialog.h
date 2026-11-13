// src/ui/ManualImportDialog.h
#pragma once
#include "application/build_runner/BuildReport.h"
#include "application/import/MultiFileParser.h"
#include "domain/import/ImportResult.h"
#include "ui/SessionState.h"
#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>
class Fl_Window;
class Fl_Input;
class Fl_Button;
class Fl_Text_Editor;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Browser;
class Fl_Box;
class Fl_Widget;
namespace frans::application {
class BuildRunner;
class ManualImporter;
}
namespace frans::infrastructure {
class SandboxedFileSystem;
}
namespace frans::ui {
// Dialog Manual Import + Build/Debug.
//
// Standalone — tidak butuh AppContext.
//
// Threading model:
//   - Semua UI (Fl_*) hanya diakses dari UI thread.
//   - Build dijalankan di worker thread (build_thread_) supaya UI
//     tetap responsif dan Cancel button bisa diklik.
//   - Worker menulis hasil ke pending_report_ (read-only bagi UI
//     sampai build_finished_ == true).
//   - UI polling via Fl::add_timeout(0.1s) sampai build_finished_.
//   - Cancel: set cancel_requested_ = true; worker membaca flag ini
//     lewat ProcessExecutor::runCommand(cancel_flag).
//
// SessionState (Fase A #44):
//   - Pointer opsional ke SessionState (dimiliki MainWindow).
//   - Setelah build GAGAL, dialog menulis ringkasan error ke
//     session->last_build_error.
//   - Setelah build SUKSES, last_build_error dikosongkan.
//   - Pointer null -> perilaku lama tidak berubah.
class ManualImportDialog {
public:
// `session` boleh nullptr (backwards compatible).
static bool show(const std::string& initial_root,
SessionState* session = nullptr);
ManualImportDialog(const ManualImportDialog&)            = delete;
ManualImportDialog& operator=(const ManualImportDialog&) = delete;
private:
explicit ManualImportDialog(std::string initial_root,
SessionState* session = nullptr);
~ManualImportDialog();
void build();
void rebuildImporter();
void doBrowse();
void doParse();
void doWrite();
void doBuild();
void doCancel();
void doClose();
void refreshPreviewBrowser(const frans::domain::ImportResult& r);
void renderBuildReport(const frans::application::BuildReport& r);
void setStatus(const std::string& msg, unsigned color);
void setButtonsEnabled(bool parsing_enabled, bool writing_enabled);
void updateBuildButtonState();
void resetBuildState(const char* reason);
// Tulis ringkasan error build ke SessionState (Fase A #44).
void publishLastErrorToSession();
void clearLastErrorInSession();
static void cbBrowse(Fl_Widget*, void*);
static void cbParse(Fl_Widget*, void*);
static void cbWrite(Fl_Widget*, void*);
static void cbBuild(Fl_Widget*, void*);
static void cbCancel(Fl_Widget*, void*);
static void cbClose(Fl_Widget*, void*);
static void onBuildPoll(void* data);
static void onPasteModified(int pos, int nInserted, int nDeleted,
int nRestyled, const char* deletedText,
void* cbArg);
Fl_Window*        win_             = nullptr;
Fl_Input*         root_input_      = nullptr;
Fl_Button*        browse_btn_      = nullptr;
Fl_Text_Editor*   paste_editor_    = nullptr;
Fl_Text_Buffer*   paste_buf_       = nullptr;
Fl_Button*        parse_btn_       = nullptr;
Fl_Browser*       preview_browser_ = nullptr;
Fl_Box*           status_box_      = nullptr;
Fl_Button*        write_btn_       = nullptr;
Fl_Button*        build_btn_       = nullptr;
Fl_Button*        cancel_btn_      = nullptr;
Fl_Box*           build_label_     = nullptr;
Fl_Text_Display*  build_output_    = nullptr;
Fl_Text_Buffer*   build_buf_       = nullptr;
Fl_Button*        close_btn_       = nullptr;
std::unique_ptr<frans::infrastructure::SandboxedFileSystem> fs_;
std::unique_ptr<frans::application::ManualImporter>         importer_;
std::unique_ptr<frans::application::BuildRunner>            build_runner_;
frans::domain::ImportResult      last_preview_;
frans::application::BuildReport  pending_report_;
std::map<std::string, frans::domain::GeneratedFile> files_written_;
std::string last_commit_root_;
// State worker thread.
std::thread         build_thread_;
std::atomic<bool>   build_finished_{false};
std::atomic<bool>   cancel_requested_{false};
bool                is_building_    = false;
bool                wrote_anything_ = false;
std::string         current_root_;
// Fase A #44: pointer ke SessionState (boleh nullptr).
SessionState*       session_        = nullptr;
};
} // namespace frans::ui