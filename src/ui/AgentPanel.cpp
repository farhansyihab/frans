// ~/AgenAI/frans/src/ui/AgentPanel.cpp
#include "ui/AgentPanel.h"

#include "application/AppContext.h"
#include "application/agent/AgentContext.h"
#include "application/agent/Agent.h"
#include "application/agent/TargetScopeResolver.h"
#include "application/budget/DailyBudgetGuard.h"
#include "application/context_budget/ContextBudgetManager.h"
#include "application/project/ProjectGraphCache.h"
#include "application/provider_router/ProviderRouter.h"
#include "config/DeepSeekConfig.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/ILlmProvider.h"
#include "infrastructure/api_providers/DeepSeekProvider.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include "infrastructure/llama/LlamaCppProvider.h"
#include "infrastructure/llama/LlamaServerProcessController.h"
#include "infrastructure/system/DiffEngine.h"
#include "ui/ManualImportDialog.h"   // Stage 11b

#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <utility>
#include <vector>

namespace frans::ui {

using frans::application::Agent;
using frans::application::AgentContext;
using frans::application::AppContext;
using frans::application::ContextBudgetManager;
using frans::application::ProviderRouter;
using frans::application::TargetScopeResolver;
using frans::application::TaskProgress;
using frans::application::TaskRunner;
using frans::infrastructure::DeepSeekProvider;
using frans::infrastructure::LlamaCppProvider;
using frans::infrastructure::SandboxedFileSystem;

namespace {

constexpr int kBtnH = 28;

std::string humanBytes(std::size_t n) {
    if (n < 1024) return std::to_string(n) + " B";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(n) / 1024.0);
    return buf;
}

std::string tailLines(const std::string& s, std::size_t max_lines) {
    if (s.empty()) return {};
    std::vector<std::size_t> newlines;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') newlines.push_back(i);
    }
    if (newlines.size() <= max_lines) return s;
    const std::size_t start = newlines[newlines.size() - max_lines - 1] + 1;
    return s.substr(start);
}

std::string resolveTilde(const std::string& in) {
    if (in.empty() || in[0] != '~') return in;
    const char* home = std::getenv("HOME");
    if (!home) return in;
    return std::string(home) + in.substr(1);
}

} // namespace

AgentPanel::AgentPanel(AppContext* ctx, Deps deps)
    : ctx_(ctx), deps_(std::move(deps))
{
    runner_ = std::make_unique<TaskRunner>();

    ContextBudgetManager mgr;
    code_budget_ = mgr.allocate(4096).code_context_tokens;

    std::fprintf(stderr,
        "[frans][agent_panel] code_budget_ = %d token (dari ContextBudgetManager)\n",
        code_budget_);
    std::fflush(stderr);
}

AgentPanel::~AgentPanel() {
    if (runner_) {
        runner_->requestCancel();
    }
}

int AgentPanel::createWidgets(int x, int y, int w) {
    int by = y;

    // ---- Title ----
    Fl_Box* title = new Fl_Box(x, by, w, 20, "Agent Task Runner");
    title->labelfont(FL_BOLD);
    title->labelsize(12);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    by += 22;

    // ---- Project root row ----
    Fl_Box* lbl_root = new Fl_Box(x, by, 100, 20, "Project root:");
    lbl_root->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_root->labelsize(11);

    root_input_ = new Fl_Input(x + 100, by, w - 100 - 80, kBtnH, nullptr);
    root_input_->value("~/AgenAI/frans/proyek/test");
    root_input_->tooltip("Folder kerja Agent. Folder akan dibuat kalau belum ada.");

    browse_btn_ = new Fl_Button(x + w - 76, by, 76, kBtnH, "Browse");
    browse_btn_->callback(onBrowseClicked, this);

    by += kBtnH + 4;

    // ---- Task description ----
    Fl_Box* lbl_task = new Fl_Box(x, by, 100, 20, "Task:");
    lbl_task->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_task->labelsize(11);

    task_input_ = new Fl_Multiline_Input(x + 100, by, w - 100, 60, nullptr);
    task_input_->value(
        "Create a simple Python Hello World program with a function hello() "
        "that returns the string \"Hello, World!\", and a main block that "
        "prints it.");
    task_input_->tooltip("Deskripsi task dalam bahasa natural (1-3 baris).");

    by += 60 + 4;

    // ---- Target file ----
    Fl_Box* lbl_target = new Fl_Box(x, by, 100, 20, "Target file:");
    lbl_target->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_target->labelsize(11);

    target_input_ = new Fl_Input(x + 100, by, 240, kBtnH, nullptr);
    target_input_->value("");
    target_input_->tooltip(
        "Kosongkan untuk auto-detect dari prompt. "
        "Isi manual untuk override entry point.");

    by += kBtnH + 4;

    // ---- Detected scope label ----
    scope_label_ = new Fl_Box(x + 100, by, w - 100, 18,
        "Detected scope: (isi task & root dulu)");
    scope_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    scope_label_->labelsize(10);
    scope_label_->labelcolor(FL_DARK_GREEN);

    by += 20;

    // ---- Stage 9c: Daily API budget label ----
    budget_label_ = new Fl_Box(x + 100, by, w - 100, 18,
        "Daily API budget: (menunggu data)");
    budget_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    budget_label_->labelsize(10);
    budget_label_->labelcolor(FL_DARK_BLUE);

    by += 20;

    // ---- Buttons ----
    // Stage 11b: [Import Paste] ditambahkan di row yang sama. Layout
    // total 130+10+100+10+100+10+110 = 470px — cukup untuk panel
    // lebar ≥ 700px (typical dari MainWindow 800+ dengan padding).
    run_btn_ = new Fl_Button(x, by, 130, kBtnH, "Run Task");
    run_btn_->callback(onRunClicked, this);

    cancel_btn_ = new Fl_Button(x + 140, by, 100, kBtnH, "Cancel");
    cancel_btn_->callback(onCancelClicked, this);
    cancel_btn_->deactivate();

    clear_btn_ = new Fl_Button(x + 250, by, 100, kBtnH, "Clear Output");
    clear_btn_->callback(onClearClicked, this);

    import_btn_ = new Fl_Button(x + 360, by, 110, kBtnH, "Import Paste");
    import_btn_->callback(onImportClicked, this);
    import_btn_->tooltip(
        "Paste output multi-file dari chat AI di browser "
        "(DeepSeek Chat / dsb) langsung ke project tanpa API call.");

    by += kBtnH + 6;

    // ---- Status ----
    status_label_ = new Fl_Box(x, by, w, 22, "Idle.");
    status_label_->box(FL_DOWN_BOX);
    status_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_label_->labelsize(11);
    by += 26;

    // ---- Stage 7g: toggle [Log] / [Diff] ----
    constexpr int kToggleH = 22;
    btn_log_view_ = new Fl_Light_Button(x, by, 70, kToggleH, "Log");
    btn_log_view_->value(1);   // Log aktif secara default
    btn_log_view_->callback(onLogViewClicked, this);

    btn_diff_view_ = new Fl_Light_Button(x + 76, by, 70, kToggleH, "Diff");
    btn_diff_view_->callback(onDiffViewClicked, this);

    by += kToggleH + 2;

    // ---- Output / Diff (overlapping, satu terlihat) ----
    const int out_h = 220;

    output_ = new Fl_Text_Display(x, by, w, out_h, nullptr);
    output_buf_ = new Fl_Text_Buffer();
    output_->buffer(output_buf_);
    output_->textfont(FL_COURIER);
    output_->textsize(10);
    output_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    output_->color(FL_WHITE);

    diff_view_ = new Fl_Text_Display(x, by, w, out_h, nullptr);
    diff_buf_ = new Fl_Text_Buffer();
    diff_view_->buffer(diff_buf_);
    diff_view_->textfont(FL_COURIER);
    diff_view_->textsize(10);
    diff_view_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    diff_view_->color(FL_WHITE);
    diff_view_->hide();   // default: Log yang tampil

    by += out_h + 4;

    return by;
}

void AgentPanel::setEnabled(bool on) {
    auto apply = [on](Fl_Widget* w) {
        if (!w) return;
        if (on) w->activate();
        else    w->deactivate();
    };
    apply(root_input_);
    apply(browse_btn_);
    apply(task_input_);
    apply(target_input_);
    apply(run_btn_);
    apply(clear_btn_);
    apply(import_btn_);   // Stage 11b — cegah import saat task running
}

void AgentPanel::tick() {
    if (!runner_) return;

    runner_->poll();
    const TaskProgress p = runner_->snapshot();

    {
        std::ostringstream oss;
        if (p.running) {
            oss << "RUNNING  |  state="
                << (p.current_state.empty() ? "(starting)" : p.current_state)
                << "  attempt=" << (p.attempt + 1) << "/" << p.max_attempts
                << "  elapsed=" << p.elapsed_sec << "s";
        } else if (p.done) {
            if (p.cancelled) {
                oss << "CANCELLED  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            } else if (p.success) {
                oss << "SUCCESS  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            } else {
                oss << "FAILED  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            }
        } else {
            oss << "Idle.";
        }
        const std::string s = oss.str();
        if (s != status_buf_ && status_label_) {
            status_buf_ = s;
            status_label_->copy_label(status_buf_.c_str());
            status_label_->redraw();
        }
    }

    if (run_btn_) {
        if (p.running) run_btn_->deactivate();
        else           run_btn_->activate();
    }
    if (cancel_btn_) {
        if (p.running) cancel_btn_->activate();
        else           cancel_btn_->deactivate();
    }

    {
        const std::string msg = runner_->takePendingConfirmMessage();
        if (!msg.empty()) {
            const int ans = fl_choice(
                "%s\n\nLanjutkan eksekusi?",
                "Batal", "Ya, lanjut",
                nullptr, msg.c_str());
            runner_->resolvePendingConfirm(ans == 1);
        }
    }

    if (p.done && !result_shown_) {
        result_shown_ = true;
        renderResult(p);
        renderDiff(p);   // Stage 7g
    }

    if (!p.running && ctx_ && ctx_->graph_cache) {
        const std::string cur_task = task_input_ ? task_input_->value()    : "";
        const std::string cur_root = root_input_ ? root_input_->value()    : "";
        if (cur_task != last_scope_task_ || cur_root != last_scope_root_) {
            last_scope_task_ = cur_task;
            last_scope_root_ = cur_root;
            updateScopeLabel(cur_task, cur_root);
        }
    }

    // Stage 9c — budget label (hanya saat idle; hindari spam redraw).
    if (!p.running) {
        updateBudgetLabel();
    }
}

void AgentPanel::updateScopeLabel(const std::string& task,
                                   const std::string& root)
{
    if (!scope_label_) return;

    if (task.empty() || root.empty() || !ctx_ || !ctx_->graph_cache) {
        scope_label_->copy_label("Detected scope: (isi task & root dulu)");
        scope_label_->redraw();
        return;
    }

    const std::string resolved_root = resolveTilde(root);
    const auto& graph = ctx_->graph_cache->getOrBuild(resolved_root);

    TargetScopeResolver resolver;
    const auto scope = resolver.resolve(task, /*user_target=*/"",
                                        resolved_root, graph, code_budget_);

    char buf[192];
    if (scope.primary_entry.empty()) {
        std::snprintf(buf, sizeof(buf),
            "Detected scope: %zu file(s)  —  no entry detected (isi Target manual)",
            scope.in_scope_files.size());
    } else {
        std::snprintf(buf, sizeof(buf),
            "Detected scope: %zu file(s)  —  entry auto-detect: %s%s",
            scope.in_scope_files.size(),
            scope.primary_entry.c_str(),
            scope.entry_from_user ? " (user override)" : "");
    }
    scope_label_->copy_label(buf);
    scope_label_->redraw();
}

// ---------------------------------------------------------------------------
// Stage 9c — update budget label dari ctx_->budget_guard->status().
//
// Hanya menulis ulang label kalau string berubah. tick() dipanggil tiap
// 100ms — tanpa guard ini, label akan trigger redraw terus-menerus.
// ---------------------------------------------------------------------------
void AgentPanel::updateBudgetLabel() {
    if (!budget_label_) return;
    if (!ctx_ || !ctx_->budget_guard) {
        const std::string s = "Daily API budget: (tidak tersedia)";
        if (s != last_budget_str_) {
            last_budget_str_ = s;
            budget_label_->copy_label(last_budget_str_.c_str());
            budget_label_->redraw();
        }
        return;
    }

    const auto st = ctx_->budget_guard->status();

    char buf[224];
    if (st.budget_usd <= 0.0) {
        std::snprintf(buf, sizeof(buf),
            "Daily API budget: DISABLED  (spent today: $%.6f)",
            st.spent_usd);
    } else {
        const double pct = (st.budget_usd > 0.0)
            ? (st.spent_usd / st.budget_usd) * 100.0
            : 0.0;
        std::snprintf(buf, sizeof(buf),
            "Daily API budget: $%.4f / $%.2f used  (%.1f%%)  "
            "— remaining $%.4f",
            st.spent_usd, st.budget_usd, pct, st.remaining_usd);
    }

    const std::string s = buf;
    if (s != last_budget_str_) {
        last_budget_str_ = s;
        budget_label_->copy_label(last_budget_str_.c_str());
        budget_label_->redraw();
    }
}

void AgentPanel::startTask() {
    if (!ctx_ || !ctx_->llama) {
        fl_message("AppContext/llama belum siap.");
        return;
    }
    if (!ctx_->graph_cache) {
        fl_message("ProjectGraphCache belum siap.");
        return;
    }
    if (runner_->isRunning()) {
        fl_message("Task masih berjalan.");
        return;
    }

    const std::string root_raw = root_input_ ? root_input_->value()    : "";
    const std::string task     = task_input_ ? task_input_->value()    : "";
    const std::string user_tgt = target_input_ ? target_input_->value(): "";

    if (root_raw.empty()) { fl_message("Project root kosong."); return; }
    if (task.empty())     { fl_message("Task description kosong."); return; }

    if (deps_.is_server_running && !deps_.is_server_running()) {
        fl_message("Llama server tidak berjalan.\n"
                   "Klik \"Start Server\" di panel Llama dulu.");
        return;
    }

    const std::string resolved_root = resolveTilde(root_raw);

    try {
        fs_ = std::make_unique<SandboxedFileSystem>(resolved_root);
    } catch (const std::exception& e) {
        fl_message("Gagal membuat sandbox:\n%s", e.what());
        return;
    }
    std::fprintf(stderr,
        "[frans][agent_panel] sandbox root = %s\n",
        fs_->root().string().c_str());
    std::fflush(stderr);

    const std::string canonical_root = fs_->root().string();
    const auto& graph_ref = ctx_->graph_cache->getOrBuild(canonical_root);

    TargetScopeResolver resolver;
    const auto scope = resolver.resolve(task, user_tgt, canonical_root,
                                         graph_ref, code_budget_);

    std::fprintf(stderr,
        "[frans][agent_panel] scope resolve: %zu files, entry='%s' (from_user=%d), seeds=%zu\n",
        scope.in_scope_files.size(),
        scope.primary_entry.c_str(),
        scope.entry_from_user ? 1 : 0,
        scope.seed_symbols.size());
    for (const auto& f : scope.in_scope_files) {
        std::fprintf(stderr, "[frans][agent_panel]   in-scope: %s\n",
                     f.c_str());
    }
    std::fflush(stderr);

    if (scope.primary_entry.empty()) {
        fl_message("Tidak bisa menentukan file target secara otomatis.\n\n"
                   "Isi field \"Target file\" secara manual.");
        return;
    }

    // ---- Local provider (llama-server) ----
    LlamaCppProvider::Config cfg;
    cfg.host = deps_.get_host ? deps_.get_host() : std::string("127.0.0.1");
    cfg.port = deps_.get_port ? deps_.get_port() : 8080;
    provider_ = std::make_unique<LlamaCppProvider>(cfg);

    // ---- Stage 9c: paid provider + router ----
    // DeepSeekConfig::loadFromFile mengembalikan config dengan api_key kosong
    // kalau env var / secrets file tidak ada. Dalam kasus itu,
    // DeepSeekProvider::isAvailable() == false, dan ProviderRouter akan
    // selalu fallback ke local — perilaku identik dengan sebelum Stage 9c.
    const auto ds_cfg =
        frans::config::DeepSeekConfig::loadFromFile("config/providers.json");
    deepseek_provider_ =
        std::make_unique<DeepSeekProvider>(ds_cfg);

    router_ = std::make_unique<ProviderRouter>(
        *provider_,
        deepseek_provider_.get(),
        ctx_->budget_guard.get());

    std::fprintf(stderr,
        "[frans][agent_panel] router: local='%s', paid='%s' (available=%d), "
        "budget_guard=%p\n",
        provider_->name().c_str(),
        deepseek_provider_->name().c_str(),
        deepseek_provider_->isAvailable() ? 1 : 0,
        static_cast<const void*>(ctx_->budget_guard.get()));
    std::fflush(stderr);

    result_shown_ = false;
    if (output_buf_) output_buf_->text("");
    if (diff_buf_)   diff_buf_->text("");

    if (deps_.on_activity) deps_.on_activity();

    Agent::Deps adeps;
    adeps.provider            = provider_.get();       // fallback kalau router null
    adeps.fs                  = fs_.get();
    adeps.graph               = &graph_ref;
    adeps.sandbox_root        = canonical_root;
    adeps.execute_timeout_sec = 30;
    adeps.router              = router_.get();         // Stage 9c
    adeps.budget_guard        = ctx_->budget_guard.get(); // Stage 9c

    AgentContext actx;
    actx.taskDescription    = task;
    actx.targetRelativePath = scope.primary_entry;
    actx.inScopeFiles       = scope.in_scope_files;

    std::fprintf(stderr,
        "[frans][agent_panel] start task: root=%s target=%s in_scope=%zu\n",
        canonical_root.c_str(), scope.primary_entry.c_str(),
        scope.in_scope_files.size());
    std::fflush(stderr);

    if (!runner_->start(std::move(adeps), std::move(actx))) {
        fl_message("TaskRunner sudah menjalankan task lain.");
        return;
    }

    if (output_buf_) {
        std::string hdr = "=== Starting agent task ===\n";
        hdr += "Project root: " + canonical_root + "\n";
        hdr += "Target file : " + scope.primary_entry;
        if (scope.entry_from_user) hdr += "  (user override)";
        else                       hdr += "  (auto-detected)";
        hdr += "\n";
        hdr += "In scope    : " + std::to_string(scope.in_scope_files.size())
             + " file(s)\n";
        for (const auto& f : scope.in_scope_files) {
            hdr += "              - " + f + "\n";
        }
        hdr += "Task        : " + task + "\n";
        hdr += "Test-aware  : ON  (test_*.py / *_test.py akan dijalankan "
               "sebelum entry point)\n";
        hdr += "Stage 9c    : hybrid routing (local + paid) + daily budget "
               "guard\n";
        hdr += "Stage 7g    : cancel responsif (SIGKILL via cancel_flag) + "
               "diff view\n\n";
        output_buf_->append(hdr.c_str());
    }
}

void AgentPanel::renderResult(const TaskProgress& p) {
    if (!output_buf_) return;

    std::ostringstream oss;

    oss << "=== Result ===\n";
    if (p.cancelled) {
        oss << "Status: CANCELLED\n";
    } else if (p.success) {
        oss << "Status: SUCCESS\n";
    } else {
        oss << "Status: FAILED\n";
    }
    oss << "Attempts: " << p.history.size()
        << " / " << p.max_attempts << "\n";
    oss << "Elapsed : " << p.elapsed_sec << "s\n";

    if (!p.final_error.empty() && !p.success && !p.cancelled) {
        oss << "\nFinal error: " << p.final_error << "\n";
    }

    // ---- Stage 7f.5 — test execution summary ----
    oss << "\n--- Test execution (Stage 7f.5) ---\n";
    if (p.last_test_ran) {
        oss << "File  : " << p.last_test_file << "\n";
        oss << "Result: " << (p.last_test_exit_code == 0 ? "PASS" : "FAIL")
            << " (exit=" << p.last_test_exit_code << ")\n";
        if (p.last_test_exit_code != 0) {
            oss << "Note  : entry point NOT executed "
                   "(test failed first — FixState will target test failure)\n";
        } else {
            oss << "Note  : test passed; entry point executed after.\n";
        }
    } else {
        oss << "(no test file detected — entry point executed directly)\n";
    }

    // ---- Attempt history ----
    oss << "\n--- Attempt history ---\n";
    for (const auto& a : p.history) {
        oss << "Attempt " << a.attempt << ": "
            << "exit=" << a.exit_code
            << (a.timed_out ? " [TIMEOUT]" : "")
            << "\n";
        if (!a.observation.empty()) {
            oss << "  " << a.observation.substr(0, 400) << "\n";
        }
    }

    // ---- Files written ----
    if (!p.files.empty()) {
        oss << "\n--- Files written ---\n";
        for (const auto& f : p.files) {
            oss << "  " << f.relative_path
                << "  (" << humanBytes(f.content.size()) << ")\n";
        }
    }

    // ---- Final stdout/stderr ----
    if (!p.stdout_text.empty()) {
        oss << "\n--- Final stdout ---\n";
        oss << p.stdout_text;
        if (p.stdout_text.back() != '\n') oss << "\n";
    }
    if (!p.stderr_text.empty() && !p.success) {
        oss << "\n--- Final stderr (tail) ---\n";
        oss << tailLines(p.stderr_text, 15);
        if (!p.stderr_text.empty() && p.stderr_text.back() != '\n') {
            oss << "\n";
        }
    }

    output_buf_->append(oss.str().c_str());
    if (output_) {
        output_->insert_position(output_buf_->length());
        output_->show_insert_position();
    }
}

// ---------------------------------------------------------------------------
// Stage 7g — render unified diff dari snapshot vs konten akhir
// ---------------------------------------------------------------------------
void AgentPanel::renderDiff(const TaskProgress& p) {
    if (!diff_buf_) return;

    diff_buf_->text("");

    if (p.files.empty()) {
        diff_buf_->append(
            "(no files written in this run — tidak ada diff)\n");
        return;
    }

    std::size_t files_with_diff = 0;

    for (const auto& f : p.files) {
        std::string old_content;
        auto it = p.file_snapshots.find(f.relative_path);
        if (it != p.file_snapshots.end()) {
            old_content = it->second;
        }

        const auto diff =
            frans::infrastructure::computeDiff(
                f.relative_path, old_content, f.content);

        if (diff.added_count > 0 || diff.removed_count > 0
            || diff.is_new_file || diff.is_deleted) {
            ++files_with_diff;
        }

        const std::string formatted =
            frans::infrastructure::formatDiff(diff);
        diff_buf_->append(formatted.c_str());
        diff_buf_->append("\n");
    }

    if (files_with_diff == 0) {
        diff_buf_->append(
            "(tidak ada perubahan bersih — semua file yang ditulis "
            "identik dengan snapshot)\n");
    }
}

void AgentPanel::onBrowseClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    const char* start = self->root_input_ ? self->root_input_->value() : nullptr;
    const char* dir = fl_dir_chooser("Pilih project root", start, /*show_files=*/0);
    if (dir && *dir && self->root_input_) {
        self->root_input_->value(dir);
    }
}

void AgentPanel::onRunClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    self->startTask();
}

void AgentPanel::onCancelClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->runner_) {
        self->runner_->requestCancel();
    }
}

void AgentPanel::onClearClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->runner_ && self->runner_->isRunning()) return;
    if (self->output_buf_) self->output_buf_->text("");
    if (self->diff_buf_)   self->diff_buf_->text("");
    self->result_shown_ = false;
}

// ---------------------------------------------------------------------------
// Stage 7g — toggle callbacks
// ---------------------------------------------------------------------------
void AgentPanel::onLogViewClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->output_)    self->output_->show();
    if (self->diff_view_) self->diff_view_->hide();
    if (self->btn_log_view_)  self->btn_log_view_->value(1);
    if (self->btn_diff_view_) self->btn_diff_view_->value(0);
}

void AgentPanel::onDiffViewClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->output_)    self->output_->hide();
    if (self->diff_view_) self->diff_view_->show();
    if (self->btn_log_view_)  self->btn_log_view_->value(0);
    if (self->btn_diff_view_) self->btn_diff_view_->value(1);
}

// ---------------------------------------------------------------------------
// Stage 11b — [Import Paste] callback.
//
// Guard: kalau task running, tolak (setEnabled(false) seharusnya sudah
// mencegah, tapi defensive — bisa juga task dimulai dari sisi lain).
// ---------------------------------------------------------------------------
void AgentPanel::onImportClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (!self->ctx_) return;

    if (self->runner_ && self->runner_->isRunning()) {
        fl_message("Task masih berjalan. Tunggu sampai selesai.");
        return;
    }

    const std::string initial_root =
        self->root_input_ ? self->root_input_->value() : "";

    const bool wrote = ManualImportDialog::show(self->ctx_, initial_root);

    if (wrote) {
        std::fprintf(stderr,
            "[frans][agent_panel] Manual Import: minimal 1 file ditulis — "
            "graph cache sudah di-invalidate\n");
        std::fflush(stderr);
    }
}

} // namespace frans::ui
