// ~/AgenAI/frans/src/ui/ManualImportDialog.cpp
//
// PENTING — urutan include:
//   1. Header sendiri (yang sekarang tidak lagi include FL — lihat
//      ManualImportDialog.h untuk alasannya).
//   2. application/AppContext.h — menarik ProjectGraphCache.h yang
//      mendefinisikan `struct Status`. HARUS di-parse SEBELUM X11
//      mendefinisikan macro `Status` via <FL/x.H>.
//   3. Header app/infra lain (pure C++, tidak ada X11).
//   4. BARU FLTK headers — <FL/Fl_Text_Editor.H> menarik <FL/x.H> yang
//      meng-include <X11/Xlib.h> → mendefinisikan macro `Status`. Setelah
//      titik ini `struct Status` sudah tidak bisa dipakai di file ini.
//
// Kalau ada penambahan header baru, jangan letakkan <FL/...> di atas
// AppContext.h.
#include "ui/ManualImportDialog.h"

#include "application/AppContext.h"
#include "application/manual_import/ManualImporter.h"
#include "application/project/ProjectGraphCache.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>

namespace frans::ui {

using frans::application::AppContext;
using frans::application::DangerVerdict;
using frans::application::ImportFilePreview;
using frans::application::ImportFileStatus;
using frans::application::ImportResult;
using frans::application::ManualImporter;
using frans::infrastructure::SandboxedFileSystem;

namespace {

// Dialog tinggi naik dari 620 → 656 (Stage 11b fix) untuk memberi ruang
// satu row "Default file (opt.)" di bawah paste area.
constexpr int kW         = 720;
constexpr int kH         = 656;
constexpr int kPadX      = 15;
constexpr int kInnerW    = kW - 2 * kPadX;

constexpr int kBtnH      = 30;
constexpr int kRowGap    = 6;

// Escape text supaya aman dipakai di Fl_Browser. Fl_Browser interpretasi
// '@' sebagai format char (mis. "@b" = bold, "@C1" = warna) dan '\n' sebagai
// pemisah entry. Path user jarang mengandung '@' tapi kita defensive.
std::string escapeBrowserText(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '@': out += "@@"; break;
            case '\n': out += ' '; break;
            case '\t': out += ' '; break;
            default:   out += c;   break;
        }
    }
    return out;
}

// Format satu baris preview di browser. Panjang diusahakan tetap supaya
// kolom "size" sejajar antar baris.
std::string formatPreviewLine(const ImportFilePreview& p) {
    const char* tag = nullptr;
    if (p.status == ImportFileStatus::Blocked) {
        tag = "[X] BLOCKED   ";
    } else if (p.status == ImportFileStatus::Overwrite) {
        tag = "[~] OVERWRITE ";
    } else if (p.danger == DangerVerdict::NeedsConfirm) {
        tag = "[!] CONFIRM   ";
    } else {
        tag = "[+] NEW       ";
    }

    char size_buf[40];
    std::snprintf(size_buf, sizeof(size_buf), "  (%zu B)", p.size_bytes);

    std::string line = std::string(tag)
                     + escapeBrowserText(p.relative_path)
                     + size_buf;

    if (p.status == ImportFileStatus::Blocked && !p.blocked_reason.empty()) {
        line += "   -- " + escapeBrowserText(p.blocked_reason);
    } else if (p.danger == DangerVerdict::NeedsConfirm
               && !p.danger_explanation.empty()) {
        line += "   -- " + escapeBrowserText(p.danger_explanation);
    }
    return line;
}

std::string humanBytes(std::size_t n) {
    if (n < 1024) return std::to_string(n) + " B";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(n) / 1024.0);
    return buf;
}

std::string resolveTilde(const std::string& in) {
    if (in.empty() || in[0] != '~') return in;
    const char* home = std::getenv("HOME");
    if (!home) return in;
    return std::string(home) + in.substr(1);
}

// Normalisasi hasil Fl_Input::value() yang mungkin nullptr → std::string.
std::string inputValue(const Fl_Input* in) {
    if (!in) return {};
    const char* v = in->value();
    return v ? std::string(v) : std::string{};
}

} // namespace

// ---------------------------------------------------------------------------
// show — entry point
// ---------------------------------------------------------------------------
bool ManualImportDialog::show(AppContext* ctx, const std::string& initial_root) {
    if (!ctx) {
        std::fprintf(stderr,
            "[frans][manual_import_dialog] show() dipanggil dengan ctx null\n");
        std::fflush(stderr);
        return false;
    }
    ManualImportDialog* dlg = new ManualImportDialog(ctx, initial_root);
    dlg->win_->set_modal();
    dlg->win_->show();

    while (dlg->win_->shown()) {
        Fl::wait();
    }

    const bool result = dlg->wrote_anything_;
    delete dlg;
    return result;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
ManualImportDialog::ManualImportDialog(AppContext* ctx, std::string initial_root)
    : ctx_(ctx), current_root_(std::move(initial_root))
{
    build();
}

ManualImportDialog::~ManualImportDialog() {
    if (win_) {
        if (win_->shown()) win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// build — susun widget
// ---------------------------------------------------------------------------
void ManualImportDialog::build() {
    win_ = new Fl_Window(kW, kH, "Manual Import — Paste from AI Browser");
    win_->begin();

    int y = kPadX;

    // ---- Title ----
    {
        Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 22,
            "Paste AI output — multi-file (File: + block) atau single-file");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 26;
    }

    // ---- Project root row ----
    {
        Fl_Box* l = new Fl_Box(kPadX, y, 100, kBtnH, "Project root:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);

        root_input_ = new Fl_Input(kPadX + 100, y, kInnerW - 100 - 84, kBtnH, nullptr);
        root_input_->value(current_root_.c_str());
        root_input_->tooltip(
            "Folder kerja. Sandbox akan dibuat di sini. "
            "Path file di output AI di-resolve relatif ke folder ini.");

        browse_btn_ = new Fl_Button(kPadX + kInnerW - 80, y, 80, kBtnH, "Browse");
        browse_btn_->callback(cbBrowse, this);

        y += kBtnH + kRowGap;
    }

    // ---- Paste area label ----
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 18,
            "Paste AI output below:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        y += 20;
    }

    // ---- Paste editor ----
    {
        const int editor_h = 170;
        paste_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
        paste_buf_ = new Fl_Text_Buffer();
        paste_editor_->buffer(paste_buf_);
        paste_editor_->textfont(FL_COURIER);
        paste_editor_->textsize(10);
        paste_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        paste_editor_->color(FL_WHITE);
        y += editor_h + kRowGap;
    }

    // ---- Default filename (Stage 11b fix) ----
    //
    // Hanya relevan kalau paste TIDAK punya marker "File:" (output single-file
    // dari chat AI biasa). Diabaikan kalau paste multi-file.
    {
        Fl_Box* l = new Fl_Box(kPadX, y, 220, kBtnH, "Default file (opt.):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);

        fallback_input_ = new Fl_Input(kPadX + 220, y, kInnerW - 220, kBtnH, nullptr);
        fallback_input_->tooltip(
            "Isi HANYA kalau paste tidak punya marker 'File:'. "
            "Nama file target, mis. main.py. "
            "Dikosongkan untuk paste multi-file (File: marker).");
        y += kBtnH + kRowGap;
    }

    // ---- [Parse & Preview] ----
    {
        parse_btn_ = new Fl_Button(kPadX, y, 160, kBtnH, "Parse & Preview");
        parse_btn_->callback(cbParse, this);
        parse_btn_->tooltip(
            "Parse teks di atas. Tidak menulis apapun ke disk.");
        y += kBtnH + kRowGap + 4;
    }

    // ---- Preview label ----
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 18, "Preview:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        y += 20;
    }

    // ---- Preview browser ----
    {
        const int browser_h = 170;
        preview_browser_ = new Fl_Browser(kPadX, y, kInnerW, browser_h, nullptr);
        preview_browser_->textfont(FL_COURIER);
        preview_browser_->textsize(10);
        preview_browser_->color(FL_WHITE);
        preview_browser_->type(FL_HOLD_BROWSER);
        y += browser_h + kRowGap;
    }

    // ---- Status box ----
    {
        status_box_ = new Fl_Box(kPadX, y, kInnerW, 24,
            "Idle. Paste output lalu klik [Parse & Preview].");
        status_box_->box(FL_DOWN_BOX);
        status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        status_box_->labelsize(11);
        y += 24 + kRowGap;
    }

    // ---- Bottom buttons ----
    {
        write_btn_ = new Fl_Button(kPadX, y, 160, kBtnH + 4, "Write to Disk");
        write_btn_->callback(cbWrite, this);
        write_btn_->deactivate();   // sampai preview sukses
        write_btn_->tooltip("Tulis semua file berstatus NEW / OVERWRITE "
                            "ke disk. File BLOCKED dilewati.");

        close_btn_ = new Fl_Button(kPadX + kInnerW - 100, y, 100, kBtnH + 4, "Close");
        close_btn_->callback(cbClose, this);
    }

    win_->end();
}

// ---------------------------------------------------------------------------
// rebuildImporter — construct sandbox + importer dari root input saat ini.
// Return silently kalau root kosong.
// ---------------------------------------------------------------------------
void ManualImportDialog::rebuildImporter() {
    if (!root_input_) return;

    const std::string raw = inputValue(root_input_);
    if (raw.empty()) {
        fs_.reset();
        importer_.reset();
        current_root_.clear();
        return;
    }

    const std::string resolved = resolveTilde(raw);

    // Rebuild HANYA kalau root berubah, supaya snapshot di importer
    // sebelumnya tidak terbuang (relevan untuk diff view nanti di 11c).
    if (resolved == current_root_ && importer_) return;

    current_root_ = resolved;

    try {
        fs_ = std::make_unique<SandboxedFileSystem>(resolved);
    } catch (const std::exception& e) {
        std::fprintf(stderr,
            "[frans][manual_import_dialog] gagal buat sandbox: %s\n", e.what());
        std::fflush(stderr);
        fs_.reset();
        importer_.reset();
        return;
    }

    // Invalidate callback: reset cache supaya Agent berikutnya lihat file
    // baru. Konsisten dengan blueprint §5.9.
    AppContext* ctx = ctx_;
    ManualImporter::InvalidateFn cb = [ctx]() {
        if (ctx && ctx->graph_cache) {
            ctx->graph_cache->invalidate();
            std::fprintf(stderr,
                "[frans][manual_import_dialog] graph cache invalidated "
                "setelah commit\n");
            std::fflush(stderr);
        }
    };

    importer_ = std::make_unique<ManualImporter>(*fs_, std::move(cb));
}

// ---------------------------------------------------------------------------
// doBrowse
// ---------------------------------------------------------------------------
void ManualImportDialog::doBrowse() {
    const char* start = root_input_ ? root_input_->value() : nullptr;
    const char* dir = fl_dir_chooser("Pilih project root", start, /*show_files=*/0);
    if (dir && *dir && root_input_) {
        root_input_->value(dir);
    }
}

// ---------------------------------------------------------------------------
// doParse
// ---------------------------------------------------------------------------
void ManualImportDialog::doParse() {
    if (!root_input_ || !paste_buf_ || !preview_browser_) return;

    // Snapshot root berubah antara parse & commit harus di-rebuild.
    rebuildImporter();

    if (!importer_) {
        fl_message("Project root kosong atau tidak valid.\n"
                   "Isi field \"Project root\" dulu.");
        return;
    }

    const std::string text = paste_buf_->text() ? paste_buf_->text() : "";
    if (text.empty()) {
        fl_message("Paste area kosong.");
        return;
    }

    // Stage 11b fix: teruskan field "Default file (opt.)" sebagai
    // fallback_path. Kosong → parser akan tolak kalau paste tidak punya
    // marker "File:" (pesan error informatif dari ManualImporter).
    const std::string fallback = inputValue(fallback_input_);

    setStatus("Parsing...", FL_DARK_BLUE);
    setButtonsEnabled(/*parsing_enabled=*/false, /*writing_enabled=*/false);
    Fl::check();   // paint "Parsing..." sebelum kerja sinkron

    last_preview_ = importer_->preview(text, fallback);

    refreshPreviewBrowser(last_preview_);

    if (last_preview_.parse_failed) {
        std::string msg = "Parse FAILED: " + last_preview_.parse_error;

        // Hint spesifik: kalau gagal karena fallback_path kosong, arahkan
        // user ke field yang benar. Match substring — pesan aslinya dari
        // ManualImporter ("Response single-file, tapi fallback_path kosong.")
        // sengaja tidak di-copy persis supaya kalau wording berubah di masa
        // depan, hint tetap muncul selama kata kuncinya sama.
        if (last_preview_.parse_error.find("fallback_path kosong")
            != std::string::npos) {
            msg += "   —   Isi field \"Default file (opt.)\" di atas "
                   "(mis. main.py), lalu klik Parse lagi.";
        }
        setStatus(msg, FL_RED);
        setButtonsEnabled(true, false);
        return;
    }

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "Parsed: %zu file(s)  |  new=%zu overwrite=%zu blocked=%zu  |  "
        "total=%s  |  mode=%s",
        last_preview_.files.size(),
        last_preview_.new_count,
        last_preview_.overwrite_count,
        last_preview_.blocked_count,
        humanBytes(last_preview_.total_bytes).c_str(),
        last_preview_.multi_file ? "multi-file" : "single-file (fallback)");

    if (last_preview_.has_blocked()) {
        setStatus(std::string(buf) + "  —  file BLOCKED tidak akan ditulis.",
                  FL_DARK_RED);
    } else {
        setStatus(buf, FL_DARK_GREEN);
    }

    // Enable Write hanya kalau ada file yang bisa ditulis.
    const bool can_write = last_preview_.has_writable();
    setButtonsEnabled(/*parsing_enabled=*/true, /*writing_enabled=*/can_write);
}

// ---------------------------------------------------------------------------
// doWrite
// ---------------------------------------------------------------------------
void ManualImportDialog::doWrite() {
    if (!importer_ || !importer_->hasPreview()) {
        fl_message("Belum ada preview. Klik [Parse & Preview] dulu.");
        return;
    }
    if (!last_preview_.has_writable()) {
        fl_message("Tidak ada file yang bisa ditulis "
                   "(semua file BLOCKED atau preview kosong).");
        return;
    }

    // Konfirmasi eksplisit user — konteks ini user-mediated, tapi tetap
    // minta konfirmasi untuk overwrite (kalau ada) karena destructive.
    if (last_preview_.overwrite_count > 0) {
        char prompt[256];
        std::snprintf(prompt, sizeof(prompt),
            "Akan menimpa %zu file yang sudah ada.\n\n"
            "File yang ditimpa tidak punya backup otomatis "
            "(snapshot disimpan in-memory saja).\n\nLanjutkan?",
            last_preview_.overwrite_count);
        const int ans = fl_choice("%s", "Batal", "Ya, timpa", nullptr, prompt);
        if (ans != 1) return;
    }

    setStatus("Writing...", FL_DARK_BLUE);
    setButtonsEnabled(false, false);
    Fl::check();

    const ImportResult res = importer_->commit();

    wrote_anything_ = wrote_anything_ || (res.written_count > 0);

    char buf[320];
    if (res.written_count > 0 && res.failed_count == 0) {
        std::snprintf(buf, sizeof(buf),
            "Wrote %zu file(s), %zu blocked. "
            "ProjectGraphCache invalidated — Agent berikutnya akan "
            "lihat file baru.",
            res.written_count, res.blocked_count);
        setStatus(buf, FL_DARK_GREEN);
    } else if (res.failed_count > 0) {
        std::snprintf(buf, sizeof(buf),
            "Partial: wrote %zu, failed %zu, blocked %zu. "
            "Cek stderr untuk detail.",
            res.written_count, res.failed_count, res.blocked_count);
        setStatus(buf, FL_RED);
    } else {
        std::snprintf(buf, sizeof(buf),
            "Tidak ada file ditulis (blocked=%zu).",
            res.blocked_count);
        setStatus(buf, FL_DARK_RED);
    }

    // Setelah commit, importer->hasPreview() == false. Untuk impor ulang,
    // user harus Parse lagi. Ini by-design (Stage 11a) — mencegah
    // double-write. Sinkronkan UI: disable Write.
    setButtonsEnabled(/*parsing_enabled=*/true, /*writing_enabled=*/false);
}

// ---------------------------------------------------------------------------
// doClose
// ---------------------------------------------------------------------------
void ManualImportDialog::doClose() {
    if (win_) win_->hide();
}

// ---------------------------------------------------------------------------
// refreshPreviewBrowser — render last_preview_ ke Fl_Browser.
// ---------------------------------------------------------------------------
void ManualImportDialog::refreshPreviewBrowser(const ImportResult& r) {
    if (!preview_browser_) return;
    preview_browser_->clear();

    if (r.parse_failed) {
        preview_browser_->add("  (parse gagal — lihat status di bawah)");
        return;
    }
    if (r.files.empty()) {
        preview_browser_->add("  (tidak ada file di output)");
        return;
    }

    for (const auto& f : r.files) {
        preview_browser_->add(formatPreviewLine(f).c_str());
    }
}

// ---------------------------------------------------------------------------
// setStatus / setButtonsEnabled
// ---------------------------------------------------------------------------
void ManualImportDialog::setStatus(const std::string& msg, unsigned color) {
    if (!status_box_) return;
    status_box_->copy_label(msg.c_str());
    status_box_->labelcolor(static_cast<Fl_Color>(color));
    status_box_->redraw();
}

void ManualImportDialog::setButtonsEnabled(bool parsing_enabled,
                                            bool writing_enabled) {
    if (parse_btn_) {
        if (parsing_enabled) parse_btn_->activate();
        else                 parse_btn_->deactivate();
    }
    if (write_btn_) {
        if (writing_enabled) write_btn_->activate();
        else                 write_btn_->deactivate();
    }
    // Close selalu enabled — user bisa batal kapan saja.
}

// ---------------------------------------------------------------------------
// Static callbacks
// ---------------------------------------------------------------------------
void ManualImportDialog::cbBrowse(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doBrowse();
}

void ManualImportDialog::cbParse(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doParse();
}

void ManualImportDialog::cbWrite(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doWrite();
}

void ManualImportDialog::cbClose(Fl_Widget*, void* data) {
    static_cast<ManualImportDialog*>(data)->doClose();
}

} // namespace frans::ui
