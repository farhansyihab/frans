// ~/AgenAI/frans/src/ui/MaintenancePanel.h
#pragma once

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

#include <functional>
#include <string>

class Fl_Widget;

namespace frans::application {
struct AppContext;
class Worker;
}

namespace frans::infrastructure {
struct ModelEntry;
}

namespace frans::ui {

// Blueprint §5.1 — System Maintenance UI.
//
// Menyusun widget untuk:
//   - status RAM & Swap (auto-refresh dari timer MainWindow)
//   - tombol maintenance: Clean Memory / Swap Cycle / Sync / Drop Caches
//   - tombol utilitas: Check sudo / Reset UI / Check Memory (dry-run)
class MaintenancePanel {
public:
    // Dipakai oleh tombol "Check Memory" untuk mendapat model yang sedang
    // dipilih di dropdown LlamaPanel. Boleh nullptr kalau belum dipilih.
    using ModelProvider =
        std::function<const frans::infrastructure::ModelEntry*()>;

    MaintenancePanel(frans::application::AppContext* ctx,
                     frans::application::Worker*     worker,
                     ModelProvider                   get_model);

    // Bangun widget di region (x, y, w). Return y akhir (bottom + margin).
    int createWidgets(int x, int y, int w);

    // Refresh label RAM/Swap dari /proc/meminfo. Dipanggil timer.
    void refreshMemoryDisplay();

    // Aktif/nonaktif tombol (dipanggil MainWindow saat worker busy).
    void setEnabled(bool on);

private:
    static void onCleanMemory(Fl_Widget*, void*);
    static void onSwapCycle(Fl_Widget*, void*);
    static void onSyncOnly(Fl_Widget*, void*);
    static void onDropOnly(Fl_Widget*, void*);
    static void onCheckSudo(Fl_Widget*, void*);
    static void onResetUi(Fl_Widget*, void*);
    static void onCheckMemory(Fl_Widget*, void*);

    frans::application::AppContext* ctx_;
    frans::application::Worker*     worker_;
    ModelProvider                   get_model_;

    Fl_Box*    mem_label_  = nullptr;
    Fl_Box*    swap_label_ = nullptr;
    Fl_Button* btn_clean_  = nullptr;
    Fl_Button* btn_swap_   = nullptr;
    Fl_Button* btn_sync_   = nullptr;
    Fl_Button* btn_drop_   = nullptr;
    Fl_Button* btn_reset_  = nullptr;

    std::string mem_buf_;
    std::string swap_buf_;
};

} // namespace frans::ui
