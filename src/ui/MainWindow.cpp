#include "ui/MainWindow.h"

#include "ui/ManualImportDialog.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Window.H>

#include <algorithm>
#include <cstdio>
#include <string>

namespace frans::ui {

namespace {
constexpr int kMinW = 640;
constexpr int kMinH = 320;
}

MainWindow::MainWindow(const frans::config::AppConfig& cfg) : cfg_(cfg) {}
MainWindow::~MainWindow() = default;

Fl_Window* MainWindow::create() {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    Fl::screen_work_area(sx, sy, sw, sh);
    if (sw < kMinW || sh < kMinH) {
        sx = 0; sy = 0;
        sw = Fl::w() > 0 ? Fl::w() : 1024;
        sh = Fl::h() > 0 ? Fl::h() : 720;
    }

    int margin = cfg_.window_margin_px;
    if (margin < 0) margin = 24;
    margin = std::min(margin, std::min(sw, sh) / 4);

    const int avail_w = std::max(sw - 2 * margin, kMinW);
    const int avail_h = std::max(sh - 2 * margin, kMinH);

    int ww = cfg_.window_width  > 0 ? std::min(cfg_.window_width,  avail_w) : avail_w;
    int wh = cfg_.window_height > 0 ? std::min(cfg_.window_height, avail_h) : avail_h;

    win_ = new Fl_Window(sx + margin, sy + margin, ww, wh,
                         cfg_.window_title.c_str());
    win_->resizable(win_);
    win_->size_range(kMinW, kMinH,
                     Fl::w() > 0 ? Fl::w() : 16384,
                     Fl::h() > 0 ? Fl::h() : 16384);

    win_->begin();

    const int pad = 20;
    int y = pad;

    {
        Fl_Box* title = new Fl_Box(pad, y, ww - 2 * pad, 26,
            "Frans — AI Code Importer & Build Runner");
        title->labelfont(FL_BOLD);
        title->labelsize(16);
        title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 34;
    }

    {
        Fl_Box* desc = new Fl_Box(pad, y, ww - 2 * pad, 80,
            "Paste output multi-file dari AI chat browser, preview status\n"
            "tiap file (new / overwrite / blocked), tulis ke project root,\n"
            "lalu build / test / run dalam satu klik.");
        desc->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        desc->labelsize(11);
        y += 90;
    }

    {
        Fl_Box* lbl = new Fl_Box(pad, y, 130, 28, "Project root:");
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        lbl->labelsize(12);

        root_input_ = new Fl_Input(pad + 130, y, ww - 2 * pad - 130 - 160, 28,
                                    nullptr);
        root_input_->value(cfg_.default_project_root.c_str());
        root_input_->tooltip(
            "Folder tempat file akan ditulis. Bisa diganti di dialog berikutnya.");

        Fl_Button* btn = new Fl_Button(ww - pad - 150, y, 150, 28,
            "Open Import Dialog");
        btn->callback(cbOpenImport, this);

        y += 42;
    }

    {
        Fl_Button* q = new Fl_Button(ww - pad - 100, wh - pad - 28, 100, 28,
            "Quit");
        q->callback(cbQuit, this);
    }

    win_->end();
    return win_;
}

void MainWindow::doOpenImport() {
    const char* v = root_input_ ? root_input_->value() : nullptr;
    const std::string initial = (v && *v) ? std::string(v)
                                           : cfg_.default_project_root;
    ManualImportDialog::show(initial);
}

void MainWindow::cbOpenImport(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->doOpenImport();
}

void MainWindow::cbQuit(Fl_Widget*, void*) {
    std::exit(0);
}

} // namespace frans::ui