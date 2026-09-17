#pragma once

#include "config/AppConfig.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

// Forward declaration di GLOBAL scope — kalau ditulis di dalam
// namespace frans::ui akan diartikan sebagai frans::ui::Fl_Input.
class Fl_Input;

namespace frans::ui {

// Window utama: tombol buka Manual Import Dialog + info.
class MainWindow {
public:
    explicit MainWindow(const frans::config::AppConfig& cfg);
    ~MainWindow();

    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    Fl_Window* create();

private:
    static void cbOpenImport(Fl_Widget*, void*);
    static void cbQuit(Fl_Widget*, void*);

    void doOpenImport();

    const frans::config::AppConfig& cfg_;
    Fl_Window* win_             = nullptr;
    ::Fl_Input* root_input_     = nullptr;
};

} // namespace frans::ui
