// ~/AgenAI/frans/src/ui/ChatView.h
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ---- FLTK forward declarations ----
// Fl_Widget WAJIB di-declare karena dipakai sebagai tipe parameter callback.
// Yang lain hanya dipakai sebagai pointer member — boleh forward.
class Fl_Widget;
class Fl_Box;
class Fl_Button;
class Fl_Input;
class Fl_Text_Display;
class Fl_Text_Buffer;

namespace frans::ui {

// Callback ke pemilik UI — menghindari coupling langsung ke main.cpp.
// Semua callback HANYA dipanggil dari main thread (kecuali dinyatakan lain).
struct ChatDeps {
    std::function<bool()>        is_server_running;
    std::function<std::string()> get_host;
    std::function<int()>         get_port;
    std::function<std::string()> get_model_tag;
    std::function<void()>        on_activity;   // reset idle unload timer
};

// Blueprint §6 — FLTK tetap single-thread.
//
// ChatView mengelola:
//   - Widget: input, output (Fl_Text_Display), tombol Send/Cancel/Clear/Test.
//   - Streaming: worker thread memanggil LlamaCppProvider::streamGenerate,
//     menumpuk token ke buffer di bawah mutex.
//   - tick(): dipanggil dari timer main thread, drain buffer ke Text_Buffer
//     dan update status tok/s. FLTK widget HANYA disentuh dari sini.
class ChatView {
public:
    ChatView(int x, int y, int w, int h, ChatDeps deps);
    ~ChatView();

    ChatView(const ChatView&)            = delete;
    ChatView& operator=(const ChatView&) = delete;

    // Panggil di antara Fl_Window::begin()/end().
    void createWidgets();

    // Dipanggil dari main-thread timer (mis. Fl::repeat_timeout 150 ms).
    void tick();

    bool isStreaming() const { return streaming_.load(); }

private:
    static void onSendClicked(Fl_Widget*, void*);
    static void onClearClicked(Fl_Widget*, void*);
    static void onCancelClicked(Fl_Widget*, void*);
    static void onTestClicked(Fl_Widget*, void*);

    void startStream(const std::string& prompt);
    void setUiStreaming(bool streaming);

    // ---- layout ----
    int x_, y_, w_, h_;

    // ---- widgets ----
    Fl_Input*         input_      = nullptr;
    Fl_Text_Display*  output_     = nullptr;
    Fl_Text_Buffer*   output_buf_ = nullptr;
    Fl_Button*        send_btn_   = nullptr;
    Fl_Button*        cancel_btn_ = nullptr;
    Fl_Button*        clear_btn_  = nullptr;
    Fl_Button*        test_btn_   = nullptr;
    Fl_Box*           status_     = nullptr;

    // ---- deps ----
    ChatDeps deps_;

    // ---- streaming state (worker <-> main) ----
    std::mutex              stream_mutex_;
    std::string             stream_pending_;   // token belum ditampilkan
    std::string             final_status_;     // status akhir (dibaca sekali)

    std::atomic<bool>       streaming_{false};
    std::atomic<bool>       cancel_{false};
    std::atomic<bool>       final_status_ready_{false};

    // ---- stats (di-update dari worker, dibaca main via stream_mutex_) ----
    std::atomic<int>        token_count_{0};
    std::chrono::steady_clock::time_point stream_start_{};

    // ---- worker thread ----
    std::thread             worker_thread_;

    // Full response accumulated (worker-side), dipakai untuk logging/analisis.
    std::string             full_response_;
};

} // namespace frans::ui
