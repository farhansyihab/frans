// ~/AgenAI/frans/src/ui/LlamaPanel.cpp
#include "ui/LlamaPanel.h"

#include "application/AppContext.h"
#include "application/Worker.h"
#include "application/resource_guard/ResourceGuard.h"

#include <FL/fl_ask.H>

#include <cstdio>
#include <utility>

namespace frans::ui {

using frans::application::AppContext;
using frans::application::ResourceCheck;
using frans::application::ResourceDecision;
using frans::application::Worker;
using frans::domain::CommandResult;
using frans::infrastructure::LlamaServerProcessController;
using frans::infrastructure::ModelEntry;

namespace {
constexpr int kBtnH = 32;
}

LlamaPanel::LlamaPanel(AppContext* ctx,
                       Worker*     worker,
                       ChatStreamingFn is_chat_streaming)
    : ctx_(ctx), worker_(worker),
      is_chat_streaming_(std::move(is_chat_streaming)) {}

int LlamaPanel::createWidgets(int x, int y, int w) {
    int by = y;

    // ---- Section title ----
    Fl_Box* ltitle = new Fl_Box(x, by, w, 20, "Llama Server (local, llama.cpp)");
    ltitle->labelfont(FL_BOLD);
    ltitle->labelsize(12);
    ltitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    by += 22;

    // ---- Model dropdown + Start / Stop ----
    model_choice_ = new Fl_Choice(x, by, 480, kBtnH, nullptr);
    for (const auto& m : ctx_->registry.models()) {
        char lbl[256];
        std::snprintf(lbl, sizeof(lbl), "%s  [%s, %.1f GB]",
                      m.id.c_str(), m.role.c_str(), m.size_gb);
        model_choice_->add(lbl);
    }
    if (!ctx_->registry.empty()) {
        model_choice_->value(0);
    } else {
        model_choice_->add("(tidak ada model di config/models.json)");
        model_choice_->value(0);
        model_choice_->deactivate();
    }
    model_choice_->tooltip("Pilih model dari config/models.json");
    model_choice_->callback(onModelChoiceChanged, this);

    btn_start_ = new Fl_Button(x + 490, by, 130, kBtnH, "Start Server");
    btn_start_->callback(onStartClicked, this);

    btn_stop_ = new Fl_Button(x + 630, by, 130, kBtnH, "Stop Server");
    btn_stop_->callback(onStopClicked, this);

    by += kBtnH + 4;

    // ---- Estimated RAM ----
    mem_required_label_ = new Fl_Box(x, by, w, 20,
        "Estimated RAM: (menghitung...)");
    mem_required_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    mem_required_label_->labelsize(11);
    mem_required_label_->labelcolor(FL_DARK_GREEN);
    by += 24;

    // ---- Status bar ----
    llama_status_label_ = new Fl_Box(x, by, w, 22, "llama-server: stopped");
    llama_status_label_->box(FL_DOWN_BOX);
    llama_status_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    llama_status_label_->labelsize(11);
    by += 26;

    llama_log_label_ = new Fl_Box(x, by, w, 18,
        "log: (belum ada \u2014 start server dulu)");
    llama_log_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    llama_log_label_->labelsize(10);
    by += 24;

    updateEstimatedRamLabel();
    return by;
}

const ModelEntry* LlamaPanel::selectedModel() const {
    if (!model_choice_) return nullptr;
    const int idx = model_choice_->value();
    if (idx < 0 || idx >= static_cast<int>(ctx_->registry.models().size())) {
        return nullptr;
    }
    return &ctx_->registry.models()[static_cast<std::size_t>(idx)];
}

void LlamaPanel::updateEstimatedRamLabel() {
    if (!mem_required_label_) return;

    const ModelEntry* entry = selectedModel();
    if (!entry) {
        mem_required_buf_ = "Estimated RAM: (tidak ada model dipilih)";
        mem_required_label_->copy_label(mem_required_buf_.c_str());
        return;
    }

    auto req = ctx_->resource_guard->estimateModelRequirement(
        *entry, entry->context_size);

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "Estimated RAM: %.2f GB  (model %.2f + KV %.2f + overhead %.2f)%s",
        req.total_required_gb,
        req.model_file_gb,
        req.kv_cache_gb,
        req.runtime_overhead_gb,
        req.kv_estimated ? "" : "   [KV diestimasi kasar]");
    mem_required_buf_ = buf;
    mem_required_label_->copy_label(mem_required_buf_.c_str());
}

void LlamaPanel::setEnabled(bool on) {
    auto apply = [on](Fl_Widget* w) {
        if (!w) return;
        if (on) w->activate();
        else    w->deactivate();
    };
    apply(btn_start_);
    apply(btn_stop_);
    if (ctx_->registry.empty()) return;   // dropdown sengaja tetap mati
    apply(model_choice_);
}

// ---------------------------------------------------------------------------
// refreshStatus — dipanggil dari timer main-thread
// ---------------------------------------------------------------------------
void LlamaPanel::refreshStatus() {
    if (!ctx_->llama) return;

    // --- Idle unload check ---
    const bool chat_active = is_chat_streaming_ && is_chat_streaming_();
    if (ctx_->llama->isRunning()
        && ctx_->idle_timer
        && ctx_->idle_timer->isRunning()
        && ctx_->idle_timer->expired()
        && !worker_->busy()
        && !chat_active)
    {
        std::fprintf(stderr,
            "[frans] idle unload: timeout tercapai, stop server.\n");
        std::fflush(stderr);
        auto* ctx = ctx_;
        worker_->spawn("Idle Unload", [ctx]() -> CommandResult {
            if (ctx->idle_timer) ctx->idle_timer->stop();
            if (!ctx->llama->isRunning()) {
                return CommandResult::ok("server memang sudah tidak berjalan.");
            }
            ctx->llama->stop();
            return CommandResult::ok(
                "idle timeout tercapai \u2014 server dihentikan.");
        });
    }

    // --- Build status string ---
    std::string s;
    if (ctx_->llama->isRunning()) {
        char head[64];
        if (ctx_->idle_timer && ctx_->idle_timer->isRunning()) {
            const auto rem = ctx_->idle_timer->remaining().count();
            std::snprintf(head, sizeof(head),
                          "auto-unload in %02ld:%02ld",
                          static_cast<long>(rem / 60),
                          static_cast<long>(rem % 60));
        } else {
            std::snprintf(head, sizeof(head), "auto-unload: OFF");
        }

        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "%s   |   llama-server: RUNNING  @  %s  (pid=%d)",
            head,
            ctx_->llama->baseUrl().c_str(),
            static_cast<int>(ctx_->llama->pid()));
        s = buf;
    } else {
        char buf[320];
        const std::string err = ctx_->llama->lastError();
        if (err.empty()) {
            std::snprintf(buf, sizeof(buf),
                "auto-unload: OFF   |   llama-server: stopped");
        } else {
            std::snprintf(buf, sizeof(buf),
                "auto-unload: OFF   |   llama-server: stopped   |   last error: %s",
                err.c_str());
        }
        s = buf;
    }

    if (s != last_llama_status_logged_) {
        std::fprintf(stderr, "[frans] llama_status: '%s'\n", s.c_str());
        std::fflush(stderr);
        last_llama_status_logged_ = s;
    }

    llama_status_buf_ = s;
    if (llama_status_label_) {
        llama_status_label_->copy_label(llama_status_buf_.c_str());
        llama_status_label_->redraw();
    }

    // --- Log path ---
    std::string logp = ctx_->llama->logPath();
    llama_log_buf_ = logp.empty()
        ? std::string("log: (belum ada \u2014 start server dulu)")
        : ("log: " + logp);
    if (llama_log_label_) {
        llama_log_label_->copy_label(llama_log_buf_.c_str());
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void LlamaPanel::onModelChoiceChanged(Fl_Widget*, void* data) {
    static_cast<LlamaPanel*>(data)->updateEstimatedRamLabel();
}

void LlamaPanel::onStartClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<LlamaPanel*>(data);
    AppContext* ctx = self->ctx_;

    if (!ctx->llama) {
        fl_message("Llama controller belum diinisialisasi.");
        return;
    }
    if (ctx->registry.empty()) {
        fl_message("Tidak ada model di config/models.json.");
        return;
    }
    if (ctx->llama->isRunning()) {
        fl_message("Server sudah berjalan. Stop dulu sebelum start ulang.");
        return;
    }

    const ModelEntry* entry = self->selectedModel();
    if (!entry) {
        fl_message("Pilih model dulu dari dropdown.");
        return;
    }
    if (!entry->pathExists()) {
        fl_message("File model tidak ditemukan:\n%s", entry->path.c_str());
        return;
    }

    const ResourceCheck check =
        ctx->resource_guard->check(*entry, entry->context_size);

    std::fprintf(stderr, "[frans] preflight: %s -> decision=%d\n",
                 check.explanation.c_str(),
                 static_cast<int>(check.decision));
    std::fflush(stderr);

    if (check.decision == ResourceDecision::Reject) {
        std::string msg =
            std::string("RAM tidak cukup untuk model ini.\n\n")
            + check.explanation;
        if (!check.suggestion.empty()) {
            msg += "\n\nSaran: " + check.suggestion;
        }
        fl_message("%s", msg.c_str());
        return;
    }

    bool need_clean = false;
    if (check.decision == ResourceDecision::CleanMemoryThenProceed) {
        const int ans = fl_choice(
            "%s\n\nJalankan cleanMemory dulu, lalu start server?",
            "Batal",
            "Ya, bersihkan & start",
            nullptr,
            check.explanation.c_str());
        if (ans != 1) {
            std::fprintf(stderr, "[frans] user Batal start (RAM mepet).\n");
            std::fflush(stderr);
            return;
        }
        need_clean = true;
    }

    // Copy entry — hidup lebih lama dari widget callback.
    const ModelEntry entry_copy = *entry;

    self->worker_->spawn("Start Llama: " + entry_copy.id,
        [ctx, entry_copy, need_clean]() -> CommandResult
    {
        std::string prefix;

        if (need_clean) {
            std::fprintf(stderr, "[frans] preflight cleanMemory (RAM mepet)\n");
            std::fflush(stderr);
            const auto clean = ctx->facade.cleanMemory();
            prefix = "cleanMemory: ";
            prefix += (clean.success ? "OK" : "PARTIAL");
            prefix += "\n";
            prefix += clean.message;
            prefix += "----\n";
        }

        LlamaServerProcessController::LaunchSpec spec;
        spec.model_path   = entry_copy.path;
        spec.context_size = entry_copy.context_size;
        spec.threads      = entry_copy.threads;
        spec.n_gpu_layers = 0;

        if (!ctx->llama->start(spec)) {
            return CommandResult::fail(-1,
                prefix + "start gagal: " + ctx->llama->lastError());
        }
        if (!ctx->llama->waitUntilReady()) {
            const std::string err = ctx->llama->lastError();
            ctx->llama->stop();
            return CommandResult::fail(-1,
                prefix + "tidak ready: " + err);
        }

        if (ctx->idle_timer) {
            ctx->idle_timer->start();
            std::fprintf(stderr,
                "[frans] idle_timer->start() dipanggil (timeout=%llds)\n",
                static_cast<long long>(ctx->idle_timer->timeout().count()));
            std::fflush(stderr);
        }

        return CommandResult::ok(
            prefix + "running di " + ctx->llama->baseUrl()
            + "\nLog: " + ctx->llama->logPath());
    });
}

void LlamaPanel::onStopClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<LlamaPanel*>(data);
    AppContext* ctx = self->ctx_;

    if (!ctx->llama) {
        fl_message("Llama controller belum diinisialisasi.");
        return;
    }

    self->worker_->spawn("Stop Llama Server", [ctx]() -> CommandResult {
        if (ctx->idle_timer) ctx->idle_timer->stop();
        if (!ctx->llama->isRunning()) {
            return CommandResult::ok("server memang tidak berjalan.");
        }
        ctx->llama->stop();
        return CommandResult::ok("server dihentikan.");
    });
}

} // namespace frans::ui
