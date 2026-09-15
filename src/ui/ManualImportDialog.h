// ~/AgenAI/frans/src/ui/ManualImportDialog.h
#pragma once

#include "application/manual_import/ManualImporter.h"

#include <memory>
#include <string>

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
class Fl_Browser;
class Fl_Box;
class Fl_Widget;

namespace frans::application {
struct AppContext;
}

namespace frans::infrastructure {
class SandboxedFileSystem;
}

namespace frans::ui {

// Blueprint Stage 11b — Manual Import Window (UI layer).
//
// Modal dialog untuk paste output multi-file dari chat AI di browser
// (DeepSeek Chat / dsb), parse, preview, lalu tulis ke disk. Menggunakan
// ManualImporter (Stage 11a) untuk logic — dialog ini murni UI.
//
// Alur (blocking sampai user klik Close / X):
//
//   [ManualImportDialog::show(ctx, initial_root)]
//          │
//          ├── buka modal window, blocking Fl::wait() loop
//          │
//          ├── [Parse & Preview] → ManualImporter::preview() → isi browser
//          │
//          ├── [Write to Disk]   → ManualImporter::commit()  → invalidate cache
//          │
//          └── [Close] / X       → hide, show() return
//
// Dua mode paste yang didukung:
//   1. Multi-file: marker "File: path" + fenced block. Field "Default file"
//      diabaikan.
//   2. Single-file: hanya fenced block, tanpa marker. Field "Default file"
//      WAJIB diisi (nama file target) — akan diteruskan sebagai fallback_path
//      ke ManualImporter::preview().
//
// Lifetime: ManualImportDialog::show() adalah entry point tunggal. Dialog
// di-construct di heap, blocking sampai user close, lalu di-delete.
// Caller tidak pernah menyentuh objek dialog secara langsung.
class ManualImportDialog {
public:
    // Blocking. Return true kalau minimal satu file berhasil ditulis.
    // `initial_root` biasanya di-pre-fill dari AgentPanel root input —
    // boleh kosong (user isi manual).
    //
    // `ctx` harus non-null. Kalau ctx->graph_cache != nullptr, dialog akan
    // panggil invalidate() setelah commit berhasil (Stage 11c — graph
    // rebuild otomatis setelah import).
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
    void doClose();

    void refreshPreviewBrowser(const frans::application::ImportResult& r);
    void setStatus(const std::string& msg, unsigned color);
    void setButtonsEnabled(bool parsing_enabled, bool writing_enabled);

    // ---- static FLTK callbacks ----
    static void cbBrowse(Fl_Widget*, void*);
    static void cbParse(Fl_Widget*, void*);
    static void cbWrite(Fl_Widget*, void*);
    static void cbClose(Fl_Widget*, void*);

    frans::application::AppContext* ctx_;

    // ---- widgets ----
    Fl_Window*       win_           = nullptr;
    Fl_Input*        root_input_    = nullptr;
    Fl_Button*       browse_btn_    = nullptr;
    Fl_Text_Editor*  paste_editor_  = nullptr;
    Fl_Text_Buffer*  paste_buf_     = nullptr;
    Fl_Input*        fallback_input_ = nullptr;  // Stage 11b fix — single-file
    Fl_Button*       parse_btn_     = nullptr;
    Fl_Browser*      preview_browser_ = nullptr;
    Fl_Box*          status_box_    = nullptr;
    Fl_Button*       write_btn_     = nullptr;
    Fl_Button*       close_btn_     = nullptr;

    // ---- state ----
    std::unique_ptr<frans::infrastructure::SandboxedFileSystem> fs_;
    std::unique_ptr<frans::application::ManualImporter>         importer_;

    // Snapshot hasil preview terakhir — dipakai refreshPreviewBrowser()
    // dan untuk menghitung summary di status box setelah commit.
    frans::application::ImportResult last_preview_;

    bool wrote_anything_ = false;
    std::string current_root_;
};

} // namespace frans::ui
