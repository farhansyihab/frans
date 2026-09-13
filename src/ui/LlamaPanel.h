// ~/AgenAI/frans/src/ui/LlamaPanel.h
#pragma once

#include "infrastructure/llama/ModelRegistry.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>

#include <functional>
#include <string>

class Fl_Widget;

namespace frans::application {
struct AppContext;
class Worker;
}

namespace frans::ui {

// Panel kontrol llama-server lokal: pilih model, start/stop, status bar
// (auto-unload countdown + URL + pid), dan estimasi RAM.
class LlamaPanel {
public:
    // Cek apakah chat sedang streaming — dipakai supaya idle unload tidak
    // menendang server di tengah chat panjang (belt-and-suspenders: ChatView
    // juga memanggil idle_timer->markActivity() per token).
    using ChatStreamingFn = std::function<bool()>;

    LlamaPanel(frans::application::AppContext* ctx,
               frans::application::Worker*     worker,
               ChatStreamingFn                 is_chat_streaming);

    // Bangun widget. Return y akhir.
    int createWidgets(int x, int y, int w);

    // Auto-refresh dari timer main-thread: cek idle unload, update status
    // label, update log path label.
    void refreshStatus();

    void setEnabled(bool on);

    // Model yang saat ini terpilih. Nullptr kalau registry kosong.
    const frans::infrastructure::ModelEntry* selectedModel() const;

    // Update label "Estimated RAM: ...". Dipanggil saat pilihan berubah
    // dan saat awal start.
    void updateEstimatedRamLabel();

private:
    static void onModelChoiceChanged(Fl_Widget*, void*);
    static void onStartClicked(Fl_Widget*, void*);
    static void onStopClicked(Fl_Widget*, void*);

    frans::application::AppContext* ctx_;
    frans::application::Worker*     worker_;
    ChatStreamingFn                 is_chat_streaming_;

    Fl_Choice* model_choice_       = nullptr;
    Fl_Button* btn_start_          = nullptr;
    Fl_Button* btn_stop_           = nullptr;
    Fl_Box*    mem_required_label_ = nullptr;
    Fl_Box*    llama_status_label_ = nullptr;
    Fl_Box*    llama_log_label_    = nullptr;

    std::string mem_required_buf_;
    std::string llama_status_buf_;
    std::string llama_log_buf_;
    std::string last_llama_status_logged_;
};

} // namespace frans::ui
