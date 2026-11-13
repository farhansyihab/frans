// src/ui/IngestDialog.cpp
//
// Fase B #50 -- implementasi IngestDialog.
#include "ui/IngestDialog.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace frans::ui {

namespace {

constexpr int kW = 640;
constexpr int kH = 520;
constexpr int kPadX = 15;
constexpr int kInnerW = kW - 2 * kPadX;
constexpr int kBtnH = 28;
constexpr int kRowGap = 8;

std::string resolveTilde(const std::string& in) {
    if (in.empty() || in[0] != '~') return in;
    const char* home = std::getenv("HOME");
    if (!home) return in;
    return std::string(home) + in.substr(1);
}

std::string basenameOf(const std::string& path) {
    namespace fs = std::filesystem;
    return fs::path(path).filename().string();
}

std::string inputValue(const Fl_Input* in) {
    if (!in) return {};
    const char* v = in->value();
    return v ? std::string(v) : std::string{};
}

} // namespace

void IngestDialog::show(const std::string& initial_root,
                        const frans::config::AppConfig& cfg)
{
    IngestDialog* dlg = new IngestDialog(initial_root, cfg);
    dlg->win_->set_modal();
    dlg->win_->show();
    while (dlg->win_->shown()) Fl::wait();
    delete dlg;
}

IngestDialog::IngestDialog(std::string initial_root,
                           const frans::config::AppConfig& cfg)
    : current_root_(std::move(initial_root)), cfg_(cfg)
{
    build();
}

IngestDialog::~IngestDialog() {
    if (is_running_) {
        ingester_.cancel();
        // Worker will finish, no need to join here (destructor handles it)
    }
    Fl::remove_timeout(onPoll, this);
    if (win_) {
        if (win_->shown()) win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

void IngestDialog::build() {
    win_ = new Fl_Window(kW, kH, "Ingest Repo to Markdown");
    win_->begin();
    
    int y = kPadX;
    
    // Title
    {
        Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 20,
                               "Ingest seluruh repo ke satu file Markdown");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 24;
    }
    
    // Project root
    {
        Fl_Box* l = new Fl_Box(kPadX, y, 100, kBtnH, "Project root:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        root_input_ = new Fl_Input(kPadX + 100, y, kInnerW - 100 - 80, kBtnH, nullptr);
        root_input_->value(current_root_.c_str());
        browse_btn_ = new Fl_Button(kPadX + kInnerW - 75, y, 75, kBtnH, "Browse");
        browse_btn_->callback(cbBrowse, this);
        y += kBtnH + kRowGap;
    }
    
    // Options
    {
        exclude_secrets_chk_ = new Fl_Check_Button(kPadX, y, 200, kBtnH,
                                                    "Exclude secret files");
        exclude_secrets_chk_->value(1);  // default on
        exclude_secrets_chk_->labelsize(11);
        
        Fl_Box* l = new Fl_Box(kPadX + 220, y, 120, kBtnH, "Max total (MB):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        
        max_total_mb_input_ = new Fl_Input(kPadX + 340, y, 60, kBtnH, nullptr);
        max_total_mb_input_->value(std::to_string(cfg_.ingest_max_total_mb).c_str());
        y += kBtnH + kRowGap;
    }
    
    // Buttons: Generate + Cancel
    {
        generate_btn_ = new Fl_Button(kPadX, y, 140, kBtnH, "Generate Ingest");
        generate_btn_->callback(cbGenerate, this);
        
        cancel_btn_ = new Fl_Button(kPadX + 150, y, 90, kBtnH, "Cancel");
        cancel_btn_->callback(cbCancel, this);
        cancel_btn_->deactivate();
        y += kBtnH + kRowGap + 4;
    }
    
    // Progress bar
    {
        progress_bar_ = new Fl_Progress(kPadX, y, kInnerW, 20);
        progress_bar_->minimum(0);
        progress_bar_->maximum(100);
        progress_bar_->color(FL_BACKGROUND_COLOR);
        progress_bar_->selection_color(FL_SELECTION_COLOR);
        progress_bar_->labelcolor(FL_BACKGROUND2_COLOR);
        progress_bar_->labelsize(10);
        y += 24 + kRowGap;
    }
    
    // Log display
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16, "Log:");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        y += 18;
        
        int log_h = kH - y - 60;
        log_display_ = new Fl_Text_Display(kPadX, y, kInnerW, log_h, nullptr);
        log_buffer_ = new Fl_Text_Buffer();
        log_display_->buffer(log_buffer_);
        log_display_->textfont(FL_COURIER);
        log_display_->textsize(10);
        log_display_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        log_display_->color(FL_WHITE);
        y += log_h + kRowGap;
    }
    
    // Bottom buttons: Open Folder + Close
    {
        open_folder_btn_ = new Fl_Button(kPadX, y, 140, kBtnH + 4, "Open Output Folder");
        open_folder_btn_->callback(cbOpenFolder, this);
        open_folder_btn_->deactivate();
        
        close_btn_ = new Fl_Button(kPadX + kInnerW - 90, y, 90, kBtnH + 4, "Close");
        close_btn_->callback(cbClose, this);
    }
    
    win_->end();
}

void IngestDialog::doBrowse() {
    const char* start = root_input_ ? root_input_->value() : nullptr;
    const char* dir = fl_dir_chooser("Pilih project root", start, 0);
    if (dir && *dir && root_input_) {
        root_input_->value(dir);
    }
}

void IngestDialog::doGenerate() {
    if (is_running_) return;
    
    std::string project_root = inputValue(root_input_);
    if (project_root.empty()) {
        fl_message("Project root kosong.");
        return;
    }
    
    project_root = resolveTilde(project_root);
    
    // Validate project_root exists
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(project_root, ec) || ec) {
        fl_message("Project root tidak valid: %s", project_root.c_str());
        return;
    }
    
    // Parse max_total_mb
    int max_total_mb = cfg_.ingest_max_total_mb;
    std::string mb_str = inputValue(max_total_mb_input_);
    if (!mb_str.empty()) {
        try {
            max_total_mb = std::stoi(mb_str);
            if (max_total_mb <= 0) max_total_mb = cfg_.ingest_max_total_mb;
        } catch (...) {
            max_total_mb = cfg_.ingest_max_total_mb;
        }
    }
    
    // Prepare output dir
    std::string output_dir = resolveTilde(cfg_.ingest_output_dir);
    fs::create_directories(output_dir, ec);
    if (ec) {
        fl_message("Cannot create output directory: %s", output_dir.c_str());
        return;
    }
    
    // Prepare options
    frans::application::IngestOptions opt;
    opt.project_root = project_root;
    opt.output_dir = output_dir;
    opt.repo_name = basenameOf(project_root);
    opt.max_file_bytes = cfg_.ingest_max_file_kb * 1024;
    opt.max_total_bytes = static_cast<std::size_t>(max_total_mb) * 1024 * 1024;
    opt.exclude_secrets = exclude_secrets_chk_->value() != 0;
    
    // Clear log
    log_buffer_->text("");
    progress_bar_->value(0);
    progress_bar_->label("");
    
    // Start ingester
    ingester_.start(opt);
    is_running_ = true;
    
    setButtonsEnabled(false, true);
    setStatus("Ingesting...", FL_DARK_BLUE);
    
    // Start polling
    Fl::add_timeout(kPollInterval, onPoll, this);
}

void IngestDialog::doCancel() {
    if (!is_running_) return;
    ingester_.cancel();
    setStatus("Cancelling...", FL_DARK_RED);
    appendLog("[INFO] Cancel requested by user");
}

void IngestDialog::doOpenFolder() {
    std::string output_dir = resolveTilde(cfg_.ingest_output_dir);
    
    // Try xdg-open (Linux) or explorer (Windows)
    std::string cmd;
#ifdef _WIN32
    cmd = "explorer \"" + output_dir + "\"";
#else
    cmd = "xdg-open \"" + output_dir + "\" 2>/dev/null &";
#endif
    
    // Assign to variable first to satisfy warn_unused_result attribute,
    // then cast to void to suppress unused variable warning.
    int result = std::system(cmd.c_str());
    (void)result;
}

void IngestDialog::doClose() {
    if (is_running_) {
        ingester_.cancel();
        // Don't wait, destructor will handle cleanup
    }
    if (win_) win_->hide();
}

void IngestDialog::updateProgress() {
    if (!is_running_) return;
    
    // Drain log
    auto lines = ingester_.drainLog();
    for (const auto& line : lines) {
        appendLog(line);
    }
    
    // Update progress bar
    std::size_t total = ingester_.getTotalFiles();
    std::size_t processed = ingester_.getProcessedFiles();
    
    if (total > 0) {
        double pct = (static_cast<double>(processed) / total) * 100.0;
        progress_bar_->value(pct);
        
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%zu / %zu files", processed, total);
        progress_bar_->label(buf);
    }
    
    // Check if finished
    if (ingester_.isFinished()) {
        is_running_ = false;
        Fl::remove_timeout(onPoll, this);
        
        const auto& result = ingester_.getResult();
        
        if (result.cancelled) {
            setStatus("Cancelled", FL_DARK_RED);
            appendLog("[INFO] Ingestion cancelled");
        } else if (result.ok) {
            setStatus("Success: " + std::to_string(result.total_files_included) + " files, " +
                      std::to_string(result.total_content_bytes / 1024) + " KB",
                      FL_DARK_GREEN);
            open_folder_btn_->activate();
            
            appendLog("[INFO] Output: " + result.output_path);
            
            if (!result.skipped_paths.empty()) {
                appendLog("[INFO] Skipped " + std::to_string(result.skipped_paths.size()) + " files:");
                for (const auto& p : result.skipped_paths) {
                    appendLog("  - " + p);
                }
            }
            
            if (!result.budget_dropped.empty()) {
                appendLog("[WARN] Dropped " + std::to_string(result.budget_dropped.size()) +
                          " files (exceed budget):");
                for (const auto& p : result.budget_dropped) {
                    appendLog("  - " + p);
                }
            }
            
            fl_message("Ingestion complete!\n\n%zu files, %zu KB, ~%zu tokens\n\nOutput: %s",
                       result.total_files_included,
                       result.total_content_bytes / 1024,
                       result.est_tokens,
                       result.output_path.c_str());
        } else {
            setStatus("Failed: " + result.error, FL_RED);
            appendLog("[ERROR] " + result.error);
            fl_message("Ingestion failed:\n\n%s", result.error.c_str());
        }
        
        setButtonsEnabled(true, false);
    }
}

void IngestDialog::appendLog(const std::string& line) {
    if (!log_buffer_) return;
    log_buffer_->append((line + "\n").c_str());
    
    // Auto-scroll to bottom
    if (log_display_) {
        log_display_->insert_position(log_buffer_->length());
        log_display_->show_insert_position();
    }
}

void IngestDialog::setStatus(const std::string& msg, unsigned color) {
    // Could add a status box, but for now just update window title
    if (win_) {
        std::string title = "Ingest Repo to Markdown - " + msg;
        win_->label(title.c_str());
    }
}

void IngestDialog::setButtonsEnabled(bool generate_enabled, bool cancel_enabled) {
    if (generate_btn_) {
        if (generate_enabled) generate_btn_->activate();
        else generate_btn_->deactivate();
    }
    if (cancel_btn_) {
        if (cancel_enabled) cancel_btn_->activate();
        else cancel_btn_->deactivate();
    }
    if (browse_btn_) {
        if (generate_enabled) browse_btn_->activate();
        else browse_btn_->deactivate();
    }
    if (root_input_) {
        if (generate_enabled) root_input_->activate();
        else root_input_->deactivate();
    }
}

// Callbacks
void IngestDialog::cbBrowse(Fl_Widget*, void* data) {
    static_cast<IngestDialog*>(data)->doBrowse();
}

void IngestDialog::cbGenerate(Fl_Widget*, void* data) {
    static_cast<IngestDialog*>(data)->doGenerate();
}

void IngestDialog::cbCancel(Fl_Widget*, void* data) {
    static_cast<IngestDialog*>(data)->doCancel();
}

void IngestDialog::cbOpenFolder(Fl_Widget*, void* data) {
    static_cast<IngestDialog*>(data)->doOpenFolder();
}

void IngestDialog::cbClose(Fl_Widget*, void* data) {
    static_cast<IngestDialog*>(data)->doClose();
}

void IngestDialog::onPoll(void* data) {
    auto* self = static_cast<IngestDialog*>(data);
    if (!self) return;
    
    self->updateProgress();
    
    if (self->is_running_) {
        Fl::repeat_timeout(kPollInterval, onPoll, self);
    }
}

} // namespace frans::ui
