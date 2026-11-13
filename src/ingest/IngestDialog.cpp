#include "IngestDialog.h"
#include <FL/Fl.H>
#include <FL/fl_message.H>
#include <FL/Fl_File_Chooser.H>
#include <iostream>
#include <cstring>

// Struct to pass data safely from worker thread to UI thread
struct UIUpdate {
    IngestDialog* self;
    IngestProgress progress;
    bool is_complete;
    bool success;
    std::string msg;
};

IngestDialog::IngestDialog(int w, int h, const char* title) 
    : Fl_Window(w, h, title), m_ingester() {
    
    m_path_input = new Fl_Input(80, 20, w - 160, 25, "Repo Path:");
    m_path_input->value("/home/farhan/Proyek/AgenAI/proyek/frans/");
    
    m_browse_btn = new Fl_Button(w - 70, 20, 60, 25, "Browse");
    m_browse_btn->callback(cb_browse, this);

    m_progress_bar = new Fl_Progress(20, 60, w - 40, 25);
    m_progress_bar->minimum(0);
    m_progress_bar->maximum(100);
    m_progress_bar->color(FL_BACKGROUND_COLOR);
    m_progress_bar->selection_color(FL_SELECTION_COLOR);
    m_progress_bar->labelcolor(FL_BACKGROUND2_COLOR);

    m_log_display = new Fl_Text_Display(20, 100, w - 40, h - 160);
    m_log_buffer = new Fl_Text_Buffer();
    m_log_display->buffer(m_log_buffer);

    m_start_btn = new Fl_Button(w - 160, h - 45, 60, 30, "Start");
    m_start_btn->callback(cb_start, this);

    m_cancel_btn = new Fl_Button(w - 80, h - 45, 60, 30, "Cancel");
    m_cancel_btn->callback(cb_cancel, this);
    m_cancel_btn->deactivate();

    end();
}

IngestDialog::~IngestDialog() {
    delete m_log_buffer;
}

void IngestDialog::cb_browse(Fl_Widget*, void* data) {
    IngestDialog* dlg = static_cast<IngestDialog*>(data);
    Fl_File_Chooser chooser(".", "*", Fl_File_Chooser::DIRECTORY, "Select Repository");
    chooser.show();
    while (chooser.shown()) {
        Fl::wait();
    }
    if (chooser.value()) {
        dlg->m_path_input->value(chooser.value());
    }
}

void IngestDialog::cb_start(Fl_Widget*, void* data) {
    IngestDialog* dlg = static_cast<IngestDialog*>(data);
    dlg->start_ingestion();
}

void IngestDialog::cb_cancel(Fl_Widget*, void* data) {
    IngestDialog* dlg = static_cast<IngestDialog*>(data);
    dlg->cancel_ingestion();
}

void IngestDialog::start_ingestion() {
    const char* path = m_path_input->value();
    if (!path || strlen(path) == 0) {
        fl_message("Please select a repository path.");
        return;
    }

    m_start_btn->deactivate();
    m_cancel_btn->activate();
    m_log_buffer->text("");
    m_progress_bar->value(0);

    // Progress callback: marshals data to UI thread
    auto progress_cb = [this](const IngestProgress& p) {
        auto* update = new UIUpdate{this, p, false, false, ""};
        Fl::awake([](void* d) {
            auto* u = static_cast<UIUpdate*>(d);
            u->self->on_progress(u->progress);
            delete u;
        }, update);
    };

    // Completion callback: marshals data to UI thread
    auto complete_cb = [this](bool success, const std::string& msg) {
        auto* update = new UIUpdate{this, {}, true, success, msg};
        Fl::awake([](void* d) {
            auto* u = static_cast<UIUpdate*>(d);
            u->self->on_complete(u->success, u->msg);
            delete u;
        }, update);
    };

    m_ingester.start(path, progress_cb, complete_cb);
}

void IngestDialog::cancel_ingestion() {
    m_ingester.cancel();
    m_cancel_btn->deactivate();
    m_log_buffer->append("[INFO] Cancellation requested...\n");
}

void IngestDialog::on_progress(const IngestProgress& p) {
    if (p.total_files > 0) {
        m_progress_bar->maximum(p.total_files);
        m_progress_bar->value(p.processed_files);
        char buf[128];
        snprintf(buf, sizeof(buf), "%d / %d", p.processed_files, p.total_files);
        m_progress_bar->label(buf);
    }
    
    std::string log = "Processing: " + p.current_file + "\n";
    m_log_buffer->append(log.c_str());
    m_log_display->scroll(m_log_buffer->length(), 0);
}

void IngestDialog::on_complete(bool success, const std::string& msg) {
    m_start_btn->activate();
    m_cancel_btn->deactivate();
    
    std::string log = success ? "[SUCCESS] " : "[CANCELLED/FAILED] ";
    log += msg + "\n";
    m_log_buffer->append(log.c_str());
    
    fl_message("%s", msg.c_str());
}