#pragma once
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Display.H>
#include <string>
#include "RepoIngester.h"

class IngestDialog : public Fl_Window {
public:
    IngestDialog(int w, int h, const char* title = nullptr);
    ~IngestDialog();

    void start_ingestion();
    void cancel_ingestion();

private:
    Fl_Input* m_path_input;
    Fl_Button* m_browse_btn;
    Fl_Button* m_start_btn;
    Fl_Button* m_cancel_btn;
    Fl_Progress* m_progress_bar;
    Fl_Text_Display* m_log_display;
    Fl_Text_Buffer* m_log_buffer;

    RepoIngester m_ingester;

    // Callbacks for FLTK widgets
    static void cb_browse(Fl_Widget*, void*);
    static void cb_start(Fl_Widget*, void*);
    static void cb_cancel(Fl_Widget*, void*);
    
    // UI update methods (called on main thread via Fl::awake)
    void on_progress(const IngestProgress& p);
    void on_complete(bool success, const std::string& msg);
};