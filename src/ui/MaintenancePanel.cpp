// ~/AgenAI/frans/src/ui/MaintenancePanel.cpp
#include "ui/MaintenancePanel.h"

#include "application/AppContext.h"
#include "application/Worker.h"
#include "infrastructure/llama/ModelRegistry.h"

#include <FL/fl_ask.H>

#include <cstdio>
#include <utility>

namespace frans::ui {

using frans::application::AppContext;
using frans::application::ResourceDecision;
using frans::application::Worker;
using frans::domain::CommandResult;
using frans::infrastructure::MemoryInfo;
using frans::infrastructure::ModelEntry;

namespace {
constexpr int kBtnH   = 32;
constexpr int kBtnGap = 8;
}

MaintenancePanel::MaintenancePanel(AppContext* ctx,
                                   Worker*     worker,
                                   ModelProvider get_model)
    : ctx_(ctx), worker_(worker), get_model_(std::move(get_model)) {}

int MaintenancePanel::createWidgets(int x, int y, int w) {
    // ---- RAM/Swap labels ----
    Fl_Box* title = new Fl_Box(x, y, 400, 18,
        "Memory status  (auto-refresh 2 detik)");
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    title->labelsize(11);
    y += 20;

    mem_label_ = new Fl_Box(x, y, w, 26, "RAM  :  (loading...)");
    mem_label_->box(FL_DOWN_BOX);
    mem_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    mem_label_->labelsize(12);
    y += 30;

    swap_label_ = new Fl_Box(x, y, w, 26, "Swap :  (loading...)");
    swap_label_->box(FL_DOWN_BOX);
    swap_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    swap_label_->labelsize(12);
    y += 34;

    // ---- Maintenance buttons row 1 ----
    int bx = x;
    btn_clean_ = new Fl_Button(bx, y, 200, kBtnH, "Clean Memory (all 4)");
    btn_clean_->callback(onCleanMemory, this);
    bx += 200 + kBtnGap;

    btn_swap_ = new Fl_Button(bx, y, 170, kBtnH, "Swap Cycle");
    btn_swap_->callback(onSwapCycle, this);
    bx += 170 + kBtnGap;

    btn_sync_ = new Fl_Button(bx, y, 80, kBtnH, "Sync");
    btn_sync_->callback(onSyncOnly, this);
    bx += 80 + kBtnGap;

    btn_drop_ = new Fl_Button(bx, y, 120, kBtnH, "Drop Caches");
    btn_drop_->callback(onDropOnly, this);

    y += kBtnH + 6;

    // ---- Utility buttons row 2 ----
    Fl_Button* sudo_btn = new Fl_Button(x, y, 120, kBtnH, "Check sudo");
    sudo_btn->callback(onCheckSudo, this);

    btn_reset_ = new Fl_Button(x + 130, y, 120, kBtnH, "Reset UI");
    btn_reset_->callback(onResetUi, this);
    btn_reset_->tooltip("Paksa re-enable semua tombol kalau terkunci.");

    Fl_Button* check_mem_btn =
        new Fl_Button(x + 260, y, 140, kBtnH, "Check Memory");
    check_mem_btn->callback(onCheckMemory, this);
    check_mem_btn->tooltip("Dry-run ResourceGuard untuk model terpilih.");

    return y + kBtnH;
}

void MaintenancePanel::refreshMemoryDisplay() {
    if (!ctx_) return;
    const MemoryInfo info = ctx_->facade.getMemoryStatus();

    char buf[256];

    std::snprintf(buf, sizeof(buf),
        "RAM  :  %.2f GB total   /   %.2f GB available",
        info.mem_total_gb(), info.mem_available_gb());
    mem_buf_ = buf;
    if (mem_label_) mem_label_->copy_label(mem_buf_.c_str());

    std::snprintf(buf, sizeof(buf),
        "Swap :  %.2f GB total   /   %.2f GB free",
        static_cast<double>(info.swap_total_kb) / 1024.0 / 1024.0,
        static_cast<double>(info.swap_free_kb)  / 1024.0 / 1024.0);
    swap_buf_ = buf;
    if (swap_label_) swap_label_->copy_label(swap_buf_.c_str());
}

void MaintenancePanel::setEnabled(bool on) {
    auto apply = [on](Fl_Widget* w) {
        if (!w) return;
        if (on) w->activate();
        else    w->deactivate();
    };
    apply(btn_clean_);
    apply(btn_swap_);
    apply(btn_sync_);
    apply(btn_drop_);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void MaintenancePanel::onCleanMemory(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    self->worker_->spawn("Clean Memory (all 4)", [self] {
        return self->ctx_->facade.cleanMemory();
    });
}

void MaintenancePanel::onSwapCycle(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    self->worker_->spawn("Swap Cycle", [self] {
        return self->ctx_->facade.swapCycle();
    });
}

void MaintenancePanel::onSyncOnly(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    self->worker_->spawn("Sync", [self] {
        return self->ctx_->facade.syncDisks();
    });
}

void MaintenancePanel::onDropOnly(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    self->worker_->spawn("Drop Caches", [self] {
        return self->ctx_->facade.dropCaches();
    });
}

void MaintenancePanel::onCheckSudo(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    const bool ok = self->ctx_->facade.isSudoAvailable();
    fl_message("%s", ok ? "sudo -n true : OK" : "sudo -n true : GAGAL");
}

void MaintenancePanel::onResetUi(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    std::fprintf(stderr, "[frans] RESET UI dipanggil user.\n");
    std::fflush(stderr);
    self->worker_->forceReset();
}

void MaintenancePanel::onCheckMemory(Fl_Widget*, void* data) {
    auto* self = static_cast<MaintenancePanel*>(data);
    const ModelEntry* entry = self->get_model_ ? self->get_model_() : nullptr;
    if (!entry) {
        fl_message("Pilih model dulu dari dropdown.");
        return;
    }

    const auto r = self->ctx_->resource_guard->check(*entry, entry->context_size);

    const char* verdict = nullptr;
    switch (r.decision) {
        case ResourceDecision::Proceed:
            verdict = "PROCEED"; break;
        case ResourceDecision::CleanMemoryThenProceed:
            verdict = "CLEAN FIRST"; break;
        case ResourceDecision::Reject:
            verdict = "REJECT"; break;
    }

    std::string msg = std::string("Model: ") + entry->id
                    + "\nContext: " + std::to_string(entry->context_size)
                    + "\n\nDecision: " + verdict
                    + "\n\n" + r.explanation;
    if (!r.suggestion.empty()) {
        msg += "\n\nSaran: " + r.suggestion;
    }
    fl_message("%s", msg.c_str());
}

} // namespace frans::ui
