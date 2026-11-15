// src/ui/GitDialog.cpp
#include "ui/GitDialog.h"

#include <FL/fl_ask.H>

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <utility>

namespace frans::ui {

namespace {

constexpr int kMinW = 720;
constexpr int kMinH = 560;

std::string formatStatusLine(const frans::application::GitStatus& st) {
    if (!st.is_repo) {
        return "Not a git repository";
    }
    std::ostringstream oss;
    oss << "Branch: " << (st.branch.empty() ? "(detached)" : st.branch);
    if (!st.upstream.empty()) {
        oss << "  |  Upstream: " << st.upstream;
    } else {
        oss << "  |  Upstream: (none)";
    }
    if (st.ahead > 0 || st.behind > 0) {
        oss << "  |  ahead " << st.ahead << ", behind " << st.behind;
    }
    oss << "  |  " << st.changes.size() << " change(s)";
    return oss.str();
}

char statusChar(char idx, char wt) {
    if (idx != ' ') return idx;
    if (wt != ' ') return wt;
    return ' ';
}

std::string formatBrowserLine(const frans::application::GitFileChange& ch) {
    char s = statusChar(ch.index_status, ch.worktree_status);
    std::ostringstream line;
    
    if (ch.suspicious) {
        line << "@R" << s << "   " << ch.path << "   [!]";
    } else {
        char c = statusChar(ch.index_status, ch.worktree_status);
        switch (c) {
            case 'A': line << "@I" << s << "   " << ch.path; break;
            case 'M': line << "@B" << s << "   " << ch.path; break;
            case 'D': line << "@M" << s << "   " << ch.path; break;
            case 'R': line << "@M" << s << "   " << ch.path; break;
            case '?': line << "@G" << s << "   " << ch.path; break;
            default:  line << s << "   " << ch.path; break;
        }
    }
    return line.str();
}

} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void GitDialog::show(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg) {
    GitDialog dlg(project_root, session, cfg);
    dlg.createUI();
    dlg.win_->set_modal();
    dlg.win_->show();
    dlg.refreshStatus();
    while (dlg.win_->shown()) {
        Fl::wait();
    }
}

GitDialog::GitDialog(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg)
    : project_root_(project_root),
      session_(session),
      cfg_(cfg) {
    git_ = std::make_unique<frans::application::GitFacade>(project_root_);
}

GitDialog::~GitDialog() {
    cancelWorker();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (win_) {
        win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void GitDialog::createUI() {
    int sx = 0, sy = 0, sw = 0, sh = 0;
    Fl::screen_work_area(sx, sy, sw, sh);
    int ww = std::max(kMinW, std::min(sw - 80, 820));
    int wh = std::max(kMinH, std::min(sh - 80, 640));

    win_ = new Fl_Window(sx + 40, sy + 40, ww, wh, "Git — Frans");
    layout();
    win_->end();
    win_->resizable(win_);
    win_->size_range(kMinW, kMinH);
}

void GitDialog::layout() {
    const int pad = 12;
    const int gap = 8;
    const int ww  = win_->w();
    const int wh  = win_->h();
    (void)wh; // suppress unused variable warning

    int y = pad;

    // ---- Status label ----
    status_label_ = new Fl_Box(pad, y, ww - 2 * pad, 22,
                               "Loading git status...");
    status_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_label_->labelfont(FL_COURIER_BOLD);
    status_label_->labelsize(12);
    y += 26;

    // ---- Changes browser ----
    changes_browser_ = new Fl_Browser(pad, y, ww - 2 * pad, 180);
    changes_browser_->type(FL_MULTI_BROWSER);
    changes_browser_->textfont(FL_COURIER);
    changes_browser_->textsize(12);
    changes_browser_->tooltip("Daftar perubahan file. Baris merah = mencurigakan (secret-like).");
    y += 180 + gap;

    // ---- Commit message ----
    {
        Fl_Box* lbl = new Fl_Box(pad, y, 120, 24, "Commit message:");
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        lbl->labelsize(12);

        commit_message_input_ = new Fl_Input(pad + 120, y, ww - 2 * pad - 120, 24);
        commit_message_input_->textfont(FL_COURIER);
        commit_message_input_->textsize(12);
        commit_message_input_->tooltip("Pesan commit. Auto-filled dari status; boleh diedit.");
        y += 28;
    }

    // ---- Output area ----
    {
        Fl_Box* lbl = new Fl_Box(pad, y, 120, 20, "Output:");
        lbl->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        lbl->labelsize(11);

        output_ = new Fl_Output(pad, y + 20, ww - 2 * pad, 140);
        output_->textfont(FL_COURIER);
        output_->textsize(11);
        output_->maximum_size(64 * 1024);
        y += 160 + gap;
    }

    // ---- Button row ----
    const int btn_h = 28;
    const int btn_w = 96;
    int x = pad;

    btn_refresh_ = new Fl_Button(x, y, btn_w, btn_h, "Refresh");
    btn_refresh_->callback(cbRefresh, this);
    btn_refresh_->tooltip("Muat ulang status dari git");
    x += btn_w + gap;

    btn_add_all_ = new Fl_Button(x, y, btn_w, btn_h, "Add All");
    btn_add_all_->callback(cbAddAll, this);
    btn_add_all_->tooltip("git add -A (stage semua perubahan)");
    x += btn_w + gap;

    btn_commit_ = new Fl_Button(x, y, btn_w, btn_h, "Commit");
    btn_commit_->callback(cbCommit, this);
    btn_commit_->tooltip("git commit -m <pesan>");
    x += btn_w + gap;

    btn_push_ = new Fl_Button(x, y, btn_w, btn_h, "Push");
    btn_push_->callback(cbPush, this);
    btn_push_->tooltip("git push (dengan konfirmasi)");
    x += btn_w + gap;

    // Spacer
    // Cancel + Close di kanan
    btn_cancel_ = new Fl_Button(ww - pad - 2 * btn_w - gap, y, btn_w, btn_h, "Cancel");
    btn_cancel_->callback(cbCancel, this);
    btn_cancel_->deactivate();

    btn_close_ = new Fl_Button(ww - pad - btn_w, y, btn_w, btn_h, "Close");
    btn_close_->callback(cbClose, this);
}

// ---------------------------------------------------------------------------
// Status / display helpers
// ---------------------------------------------------------------------------

void GitDialog::updateStatusDisplay() {
    if (!status_label_) return;
    if (!current_status_.error.empty() && !current_status_.is_repo) {
        std::string msg = "Error: " + current_status_.error;
        status_label_->copy_label(msg.c_str());
        return;
    }
    std::string line = formatStatusLine(current_status_);
    if (!current_status_.error.empty()) {
        line += "  |  ! " + current_status_.error;
    }
    status_label_->copy_label(line.c_str());
}

void GitDialog::updateChangesBrowser() {
    if (!changes_browser_) return;
    changes_browser_->clear();

    if (!current_status_.is_repo) {
        changes_browser_->add("(not a git repository)");
        return;
    }
    if (current_status_.changes.empty()) {
        changes_browser_->add("(working tree clean)");
        return;
    }

    for (const auto& ch : current_status_.changes) {
        std::string line = formatBrowserLine(ch);
        changes_browser_->add(line.c_str());
    }
}

void GitDialog::appendOutput(const std::string& text) {
    if (!output_) return;
    output_->insert(text.c_str());
    output_->position(static_cast<int>(output_->size()));
}

void GitDialog::clearOutput() {
    if (!output_) return;
    output_->value("");
}

void GitDialog::setButtonsEnabled(bool enabled) {
    if (btn_refresh_)  enabled ? btn_refresh_->activate()   : btn_refresh_->deactivate();
    if (btn_add_all_)  enabled ? btn_add_all_->activate()   : btn_add_all_->deactivate();
    if (btn_commit_)   enabled ? btn_commit_->activate()    : btn_commit_->deactivate();
    if (btn_push_)     enabled ? btn_push_->activate()      : btn_push_->deactivate();
    if (btn_cancel_)   enabled ? btn_cancel_->deactivate()  : btn_cancel_->activate();
    if (btn_close_)    enabled ? btn_close_->activate()     : btn_close_->deactivate();
}

bool GitDialog::hasSuspiciousFiles() const {
    for (const auto& ch : current_status_.changes) {
        if (ch.suspicious) return true;
    }
    return false;
}

std::string GitDialog::buildSuspiciousWarning() const {
    std::ostringstream oss;
    oss << "Peringatan: ada file yang terdeteksi mencurigakan "
           "(berpotensi secret/credential):\n\n";
    int count = 0;
    for (const auto& ch : current_status_.changes) {
        if (ch.suspicious) {
            oss << "  - " << ch.path << "\n";
            ++count;
            if (count >= 20) {
                oss << "  ... dan " << (current_status_.changes.size() - 20)
                    << " lainnya\n";
                break;
            }
        }
    }
    oss << "\nMelanjutkan 'git add -A' akan men-stage file-file tersebut "
           "dan berpotensi meng-commit secret ke repository.\n\n"
           "Yakin ingin melanjutkan?";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Worker infrastructure
// ---------------------------------------------------------------------------

void GitDialog::startWorker(std::function<void()> task,
                            std::function<void()> on_done) {
    if (worker_running_.load()) return;

    cancel_flag_.store(false);
    worker_running_.store(true);
    worker_callback_ = std::move(on_done);
    setButtonsEnabled(false);

    worker_thread_ = std::thread([this, t = std::move(task)]() {
        try {
            t();
        } catch (const std::exception& e) {
            appendOutput(std::string("\n[worker exception] ") + e.what() + "\n");
        } catch (...) {
            appendOutput("\n[worker exception] unknown\n");
        }
        worker_running_.store(false);
        Fl::awake(pollWorker, this);
    });
}

void GitDialog::cancelWorker() {
    cancel_flag_.store(true);
}

void GitDialog::onWorkerFinished() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    setButtonsEnabled(true);
    if (worker_callback_) {
        auto cb = std::move(worker_callback_);
        worker_callback_ = nullptr;
        cb();
    }
}

void GitDialog::pollWorker(void* data) {
    auto* self = static_cast<GitDialog*>(data);
    if (!self) return;
    if (!self->worker_running_.load()) {
        self->onWorkerFinished();
    } else {
        Fl::repeat_timeout(0.1, pollWorker, data);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void GitDialog::refreshStatus() {
    clearOutput();
    appendOutput("[git status] running...\n");

    startWorker(
        [this]() {
            current_status_ = git_->status(&cancel_flag_);
        },
        [this]() {
            updateStatusDisplay();
            updateChangesBrowser();

            if (!current_status_.is_repo) {
                appendOutput("[git status] " + current_status_.error + "\n");
                return;
            }
            if (!current_status_.git_available) {
                appendOutput("[git status] " + current_status_.error + "\n");
                return;
            }
            if (!current_status_.error.empty()) {
                appendOutput("[git status] warning: " + current_status_.error + "\n");
            } else {
                appendOutput("[git status] OK — " +
                             std::to_string(current_status_.changes.size()) +
                             " change(s)\n");
            }

            // Auto-fill commit message suggestion (only if input is empty
            // or still contains a previous suggestion).
            if (commit_message_input_ && commit_message_input_->size() == 0) {
                std::string suggestion =
                    frans::application::GitFacade::suggestCommitMessage(current_status_);
                commit_message_input_->value(suggestion.c_str());
            }
        }
    );
}

void GitDialog::doAddAll() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository. Tidak bisa add.");
        return;
    }
    if (current_status_.changes.empty()) {
        fl_message("Tidak ada perubahan untuk di-stage.");
        return;
    }

    if (hasSuspiciousFiles()) {
        std::string warn = buildSuspiciousWarning();
        // Use "%s" format to avoid format-security warning
        int choice = fl_choice("%s", "Batal", "Tetap Add All", nullptr, warn.c_str());
        if (choice != 1) {
            appendOutput("[git add] dibatalkan oleh pengguna (file mencurigakan).\n");
            return;
        }
    }

    appendOutput("[git add -A] running...\n");
    startWorker(
        [this]() {
            auto res = git_->addAll(&cancel_flag_);
            appendOutput(res.ok ? "[git add -A] OK\n"
                                : "[git add -A] FAILED: " + res.error + "\n");
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() { refreshStatus(); }
    );
}

void GitDialog::doCommit() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository.");
        return;
    }
    if (!commit_message_input_) return;

    std::string msg = commit_message_input_->value() ? commit_message_input_->value() : "";
    // Trim
    while (!msg.empty() && std::isspace(static_cast<unsigned char>(msg.back()))) msg.pop_back();
    while (!msg.empty() && std::isspace(static_cast<unsigned char>(msg.front()))) msg.erase(msg.begin());

    if (msg.empty()) {
        fl_message("Pesan commit tidak boleh kosong.");
        return;
    }

    appendOutput("[git commit] running...\n");
    startWorker(
        [this, msg]() {
            auto res = git_->commit(msg, &cancel_flag_);
            if (res.ok) {
                appendOutput("[git commit] OK\n");
            } else {
                appendOutput("[git commit] FAILED: " + res.error + "\n");
            }
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() {
            refreshStatus();
            // Clear commit message after successful commit
            if (commit_message_input_) commit_message_input_->value("");
        }
    );
}

void GitDialog::doPush() {
    if (!current_status_.is_repo) {
        fl_message("Bukan git repository.");
        return;
    }

    // Build confirmation text
    std::ostringstream oss;
    oss << "Konfirmasi git push\n\n"
        << "  Branch:   " << (current_status_.branch.empty() ? "(detached)" : current_status_.branch) << "\n"
        << "  Upstream: " << (current_status_.upstream.empty() ? "(none)" : current_status_.upstream) << "\n"
        << "  Ahead:    " << current_status_.ahead << " commit(s)\n\n";

    if (current_status_.upstream.empty()) {
        oss << "PERHATIAN: branch ini belum memiliki upstream.\n"
               "Push mungkin gagal. Jika gagal, jalankan manual:\n"
               "  git push -u origin <branch>\n\n";
    }
    if (current_status_.ahead == 0) {
        oss << "Catatan: tidak ada commit ahead — push mungkin tidak melakukan apa-apa.\n\n";
    }
    oss << "Lanjutkan push?";

    std::string confirm_text = oss.str();
    // Use "%s" format to avoid format-security warning
    int choice = fl_choice("%s", "Batal", "Push", nullptr, confirm_text.c_str());
    if (choice != 1) {
        appendOutput("[git push] dibatalkan oleh pengguna.\n");
        return;
    }

    appendOutput("[git push] running...\n");
    startWorker(
        [this]() {
            auto res = git_->push(&cancel_flag_);
            if (res.ok) {
                appendOutput("[git push] OK\n");
            } else {
                appendOutput("[git push] FAILED: " + res.error + "\n");
            }
            if (!res.output.empty()) appendOutput(res.output);
        },
        [this]() { refreshStatus(); }
    );
}

// ---------------------------------------------------------------------------
// FLTK callbacks
// ---------------------------------------------------------------------------

void GitDialog::cbRefresh(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->refreshStatus();
}

void GitDialog::cbAddAll(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doAddAll();
}

void GitDialog::cbCommit(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doCommit();
}

void GitDialog::cbPush(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->doPush();
}

void GitDialog::cbCancel(Fl_Widget*, void* data) {
    static_cast<GitDialog*>(data)->cancelWorker();
}

void GitDialog::cbClose(Fl_Widget*, void* data) {
    auto* self = static_cast<GitDialog*>(data);
    if (self->worker_running_.load()) {
        int c = fl_choice("Operasi git masih berjalan. Batalkan dan tutup?",
                          "Tidak", "Ya", nullptr);
        if (c != 1) return;
        self->cancelWorker();
    }
    if (self->win_) self->win_->hide();
}

} // namespace frans::ui