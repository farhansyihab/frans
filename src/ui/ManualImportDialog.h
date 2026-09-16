// ~/AgenAI/frans/src/ui/ManualImportDialog.h
#pragma once

#include "application/agent/MultiFileParser.h"        // GeneratedFile
#include "application/build_runner/BuildReport.h"     // BuildReport
#include "application/manual_import/ManualImporter.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// FLTK forward declarations.
//
// SENGAJA tidak include <FL/...> di sini. Header ini bisa di-include dari
// .cpp yang juga butuh <FL/Fl_Text_Editor.H> (via dialog sendiri) —
// Fl_Text_Display.H menarik <FL/x.H> yang meng-include <X11/Xlib.h>, dan
// Xlib mendefinisikan `Status` sebagai macro. Kalau header ini include FL
// penuh di atas, maka siapapun yang meng-include kita LEBIH DULU sebelum
// AppContext.h akan membuat `struct Status` di ProjectGraphCache.h hancur.
//
// Semua widget disimpan sebagai pointer — forward declaration cukup.
// Pola ini identik dengan AgentPanel.h.
// -----------------------------------------------------------------------------
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
struct AppContext;
class BuildRunner;
}

namespace frans::infrastructure {
class SandboxedFileSystem;
}

namespace frans::ui {

// Blueprint Stage 11b — Manual Import Window (UI layer).
// Blueprint Stage 11d — tambah tombol [Build/compile and Debug] + area
// output build.
//
// REVISI UX (2026-01 follow-up):
//   - Tinggi window 780 -> 620 px supaya muat di layar VivoBook X505Z
//     (1366x768) bersama title bar + taskbar.
//   - Field "Default file (opt.)" DIHAPUS. Alasan: di praktik, paste
//     selalu mengandung marker `File:` (multi-file) atau user memang
//     perlu melihat error eksplisit bahwa format salah. Field ini
//     cenderung bikin bingung karena multi-file mengabaikannya, dan
//     single-file tanpa marker sangat jarang dipakai.
//     Kalau user paste tanpa marker, dialog tampilkan pesan error
//     yang mengarahkan user menambah baris `File: nama_file.py`.
//
// State tombol Build:
//   Disabled  — belum ada commit sukses, atau paste area baru saja diubah
//   Enabled   — setelah commit sukses (files_written_ non-empty) DAN
//               tidak sedang building
//
// Lifetime: ManualImportDialog::show() adalah entry point tunggal.
// Dialog di-construct di heap, blocking sampai user close, lalu di-delete.
class ManualImportDialog {
public:
    // Blocking. Return true kalau minimal satu file berhasil ditulis.
    // `initial_root` biasanya di-pre-fill dari AgentPanel root input.
    //
    // `ctx` harus non-null. Kalau ctx->graph_cache != nullptr, dialog akan
    // panggil invalidate() setelah commit berhasil DAN pass ke
    // BuildRunner::runBuild() untuk enrichment.
    static bool show(frans::application::AppContext* ctx,
                     const std::string&               initial_root);

    ManualImportDialog(const ManualImportDialog&)            = delete;
    ManualImportDialog& operator=(const ManualImportDialog&) = delete;

private:
    ManualImportDialog(frans::application::AppContext* ctx,
                       std::string                    initial_root);
    ~ManualImportDialog();

    void build();
    void rebuildImporter();     // construct SandboxedFileSystem + ManualImporter

    void doBrowse();
    void doParse();
    void doWrite();
    void doBuild();             // Stage 11d
    void doClose();

    void refreshPreviewBrowser(const frans::application::ImportResult& r);
    void renderBuildReport(const frans::application::BuildReport& r);
    void setStatus(const std::string& msg, unsigned color);
    void setButtonsEnabled(bool parsing_enabled, bool writing_enabled);
    void updateBuildButtonState();

    // Reset state build (files_written_, build report, output area) —
    // dipanggil ketika paste berubah atau root berubah.
    void resetBuildState(const char* reason);

    // Static FLTK callbacks.
    static void cbBrowse(Fl_Widget*, void*);
    static void cbParse(Fl_Widget*, void*);
    static void cbWrite(Fl_Widget*, void*);
    static void cbBuild(Fl_Widget*, void*);
    static void cbClose(Fl_Widget*, void*);

    // Buffer modify callback — deteksi paste area berubah.
    static void onPasteModified(int pos, int nInserted, int nDeleted,
                                 int nRestyled, const char* deletedText,
                                 void* cbArg);

    frans::application::AppContext* ctx_;

    // ---- widgets ----
    Fl_Window*        win_             = nullptr;
    Fl_Input*         root_input_      = nullptr;
    Fl_Button*        browse_btn_      = nullptr;
    Fl_Text_Editor*   paste_editor_    = nullptr;
    Fl_Text_Buffer*   paste_buf_       = nullptr;
    Fl_Button*        parse_btn_       = nullptr;
    Fl_Browser*       preview_browser_ = nullptr;
    Fl_Box*           status_box_      = nullptr;
    Fl_Button*        write_btn_       = nullptr;

    // Stage 11d — build widgets.
    Fl_Button*        build_btn_       = nullptr;
    Fl_Box*           build_label_     = nullptr;
    Fl_Text_Display*  build_output_    = nullptr;
    Fl_Text_Buffer*   build_buf_       = nullptr;

    Fl_Button*        close_btn_       = nullptr;

    // ---- state ----
    std::unique_ptr<frans::infrastructure::SandboxedFileSystem> fs_;
    std::unique_ptr<frans::application::ManualImporter>         importer_;
    std::unique_ptr<frans::application::BuildRunner>            build_runner_;

    // Hasil preview terakhir — dipakai refreshPreviewBrowser() dan summary.
    frans::application::ImportResult last_preview_;

    // Stage 11d — file yang sudah sukses di-commit, siap di-build.
    // Map supaya commit kedua bisa "menimpa" entri dengan path sama tanpa
    // duplikat. Value-nya full GeneratedFile (path + content).
    std::map<std::string, frans::application::GeneratedFile> files_written_;

    // Sandbox root saat files_written_ terakhir di-commit (canonical).
    // Kalau user ganti root dan commit ulang, reset files_written_.
    std::string last_commit_root_;

    bool        wrote_anything_ = false;
    bool        is_building_    = false;
    std::string current_root_;
};

} // namespace frans::ui