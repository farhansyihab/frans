// src/ui/MainWindow.h
#pragma once

#include "config/AppConfig.h"
#include "ui/SessionState.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

class Fl_Input;

namespace frans::ui {

class MainWindow {
public:
    explicit MainWindow(const frans::config::AppConfig& cfg);
    ~MainWindow();

    MainWindow(const MainWindow&)            = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    Fl_Window* create();

    // Akses session state untuk dialog lain.
    SessionState* session() { return &session_; }

private:
    static void cbOpenImport(Fl_Widget*, void*);
    static void cbPrompt(Fl_Widget*, void*);
    static void cbQuit(Fl_Widget*, void*);
    static void cbPlaceholder(Fl_Widget*, void*);

    void doOpenImport();
    void doPrompt();

    const frans::config::AppConfig& cfg_;
    SessionState                    session_;
    Fl_Window*                      win_         = nullptr;
    ::Fl_Input*                     root_input_  = nullptr;
};

} // namespace frans::ui