// src/ui/PromptDialog.cpp
#include "ui/PromptDialog.h"
#include "application/prompt/PromptGenerator.h"
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
        fl_message("Belum ada error dari build terakhir.\n\n"
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

void PromptDialog::cbClose(Fl_Widget* w, void*) {
    if (Fl_Window* win = w ? w->window() : nullptr) {
        win->hide();
    }
}

} // namespace frans::ui
