// ~/AgenAI/frans/src/ui/ChatView.cpp
#include "ui/ChatView.h"

#include "domain/llm/GenOptions.h"
#include "infrastructure/llama/LlamaCppProvider.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H>

#include <cstdio>
#include <utility>

namespace frans::ui {

using frans::domain::ChatMessage;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::infrastructure::LlamaCppProvider;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
ChatView::ChatView(int x, int y, int w, int h, ChatDeps deps)
    : x_(x), y_(y), w_(w), h_(h), deps_(std::move(deps)) {}

ChatView::~ChatView() {
    // Batalkan streaming yang sedang berjalan, lalu tunggu worker selesai.
    // Worker akan melihat cancel_ dan abort content_receiver httplib.
    cancel_.store(true);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    // output_buf_ sengaja tidak dihapus di sini — display sudah dihapus FLTK
    // saat window destroy; process exit akan membersihkan sisanya.
}

void ChatView::createWidgets() {
    const int gap       = 6;
    const int input_h   = 28;
    const int status_h  = 22;
    const int output_h  = h_ - input_h - status_h - gap * 2;

    // ---- output area ----
    output_ = new Fl_Text_Display(x_, y_, w_, output_h, "");
    output_buf_ = new Fl_Text_Buffer();
    output_->buffer(output_buf_);
    output_->textfont(FL_COURIER);
    output_->textsize(11);
    output_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    output_->color(FL_WHITE);

    // ---- input row ----
    const int row_y = y_ + output_h + gap;

    input_ = new Fl_Input(x_, row_y, w_ - 180, input_h, "");
    input_->when(FL_WHEN_ENTER_KEY);
    input_->callback(onSendClicked, this);
    input_->tooltip("Ketik prompt, tekan Enter atau klik Send.");

    send_btn_ = new Fl_Button(x_ + w_ - 172, row_y, 80, input_h, "Send");
    send_btn_->callback(onSendClicked, this);

    cancel_btn_ = new Fl_Button(x_ + w_ - 84, row_y, 80, input_h, "Cancel");
    cancel_btn_->callback(onCancelClicked, this);
    cancel_btn_->deactivate();

    // ---- status row ----
    const int stat_y = row_y + input_h + gap;

    clear_btn_ = new Fl_Button(x_, stat_y, 80, status_h, "Clear");
    clear_btn_->callback(onClearClicked, this);

    test_btn_ = new Fl_Button(x_ + 88, stat_y, 90, status_h, "Test chat");
    test_btn_->callback(onTestClicked, this);
    test_btn_->tooltip("Kirim prompt singkat untuk verifikasi end-to-end.");

    status_ = new Fl_Box(x_ + 186, stat_y, w_ - 186, status_h, "ready");
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_->labelsize(11);
    status_->labelcolor(FL_DARK_BLUE);
}

// ---------------------------------------------------------------------------
// Widget activation
// ---------------------------------------------------------------------------
void ChatView::setUiStreaming(bool streaming) {
    if (streaming) {
        send_btn_->deactivate();
        input_->deactivate();
        clear_btn_->deactivate();
        test_btn_->deactivate();
        cancel_btn_->activate();
    } else {
        send_btn_->activate();
        input_->activate();
        clear_btn_->activate();
        test_btn_->activate();
        cancel_btn_->deactivate();
    }
}

// ---------------------------------------------------------------------------
// Callbacks (main thread)
// ---------------------------------------------------------------------------
void ChatView::onSendClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<ChatView*>(data);
    if (!self || self->streaming_.load()) return;

    const char* txt = self->input_->value();
    if (!txt || !*txt) return;

    std::string prompt = txt;
    self->input_->value("");
    self->startStream(prompt);
}

void ChatView::onClearClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<ChatView*>(data);
    if (!self || self->streaming_.load()) return;

    self->output_buf_->text("");
    self->full_response_.clear();
    self->token_count_.store(0);
    self->status_->copy_label("ready");
}

void ChatView::onCancelClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<ChatView*>(data);
    if (!self) return;
    self->cancel_.store(true);
    self->status_->copy_label("cancelling...");
}

void ChatView::onTestClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<ChatView*>(data);
    if (!self || self->streaming_.load()) return;

    static const char* kTestPrompt =
        "In one short sentence, say hello and mention that you are a local model.";
    self->startStream(kTestPrompt);
}

// ---------------------------------------------------------------------------
// startStream — bangun messages, spawn worker, reset state
// ---------------------------------------------------------------------------
void ChatView::startStream(const std::string& prompt) {
    if (streaming_.load()) return;

    if (!deps_.is_server_running || !deps_.is_server_running()) {
        fl_message("Llama server tidak berjalan.\n"
                   "Klik \"Start Server\" di bagian atas dulu.");
        return;
    }

    // Reap worker lama (kalau ada dan sudah selesai).
    if (worker_thread_.joinable()) worker_thread_.join();

    // Tampilkan header user message.
    std::string header = "\n\n--- You ---\n";
    header += prompt;
    header += "\n\n--- AI ---\n";
    output_buf_->append(header.c_str());
    output_->insert_position(output_buf_->length());
    output_->show_insert_position();

    // Reset state.
    {
        std::lock_guard<std::mutex> lk(stream_mutex_);
        stream_pending_.clear();
        final_status_.clear();
    }
    full_response_.clear();
    token_count_.store(0);
    cancel_.store(false);
    streaming_.store(true);
    final_status_ready_.store(false);
    stream_start_ = std::chrono::steady_clock::now();

    setUiStreaming(true);
    status_->copy_label("streaming... 0 tokens");

    // Bangun provider config dari deps.
    LlamaCppProvider::Config cfg;
    cfg.host = deps_.get_host ? deps_.get_host() : std::string("127.0.0.1");
    cfg.port = deps_.get_port ? deps_.get_port() : 8080;
    if (deps_.get_model_tag) cfg.model_tag = deps_.get_model_tag();

    // Bangun messages. System prompt ringkas; Stage 5/6 akan ganti
    // dengan context selector tree-sitter.
    std::vector<ChatMessage> messages;
    messages.push_back({
        "system",
        "You are frans_ai, a concise local coding assistant. "
        "Answer briefly and accurately. Prefer code over prose when relevant."
    });
    messages.push_back({"user", prompt});

    GenOptions opts;
    opts.max_tokens     = 1024;
    opts.temperature    = 0.7;
    opts.top_p          = 0.9;

    worker_thread_ = std::thread(
        [this, cfg = std::move(cfg),
              messages = std::move(messages),
              opts = std::move(opts)]() mutable
    {
        // WAJIB: reset idle timer sebelum POST, kalau tidak server akan
        // auto-unload di tengah chat aktif.
        if (deps_.on_activity) deps_.on_activity();

        LlamaCppProvider provider(std::move(cfg));

        std::string full;
        GenResult r = provider.streamGenerate(
            messages, opts,
            [this, &full](const std::string& tok) {
                full += tok;
                {
                    std::lock_guard<std::mutex> lk(stream_mutex_);
                    stream_pending_ += tok;
                }
                token_count_.fetch_add(1);
                // Setiap token = aktivitas; reset timer lagi supaya
                // streaming panjang tidak ke-trigger auto-unload.
                if (deps_.on_activity) deps_.on_activity();
            },
            &cancel_);

        // Tulis status akhir.
        char buf[192];
        if (r.success) {
            if (full.empty()) {
                std::lock_guard<std::mutex> lk(stream_mutex_);
                stream_pending_ += "[no output]\n";
            }
            std::snprintf(buf, sizeof(buf),
                "done in %.2fs — %d tok, %.1f tok/s",
                r.elapsed_sec,
                token_count_.load(),
                r.elapsed_sec > 0 ? token_count_.load() / r.elapsed_sec : 0.0);
        } else {
            const bool was_cancel =
                r.error.find("cancel") != std::string::npos ||
                cancel_.load();
            if (!was_cancel) {
                std::lock_guard<std::mutex> lk(stream_mutex_);
                stream_pending_ += "\n[ERROR] " + r.error + "\n";
            } else {
                std::lock_guard<std::mutex> lk(stream_mutex_);
                stream_pending_ += "\n[cancelled]\n";
            }
            std::snprintf(buf, sizeof(buf),
                "%s in %.2fs",
                was_cancel ? "cancelled" : "error",
                r.elapsed_sec);
        }

        {
            std::lock_guard<std::mutex> lk(stream_mutex_);
            final_status_ = buf;
        }
        final_status_ready_.store(true);
        streaming_.store(false);
    });
}

// ---------------------------------------------------------------------------
// tick — dipanggil dari main-thread timer
// ---------------------------------------------------------------------------
void ChatView::tick() {
    // 1. Drain token pending ke widget.
    std::string pending;
    {
        std::lock_guard<std::mutex> lk(stream_mutex_);
        if (!stream_pending_.empty()) {
            pending.swap(stream_pending_);
        }
    }
    if (!pending.empty()) {
        output_buf_->append(pending.c_str());
        output_->insert_position(output_buf_->length());
        output_->show_insert_position();
    }

    // 2. Live stats selama streaming.
    if (streaming_.load()) {
        const auto now = std::chrono::steady_clock::now();
        const double el = std::chrono::duration<double>(now - stream_start_).count();
        const int n = token_count_.load();
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "streaming... %d tok, %.1f tok/s",
            n, el > 0.0 ? n / el : 0.0);
        status_->copy_label(buf);
    }

    // 3. Status final (sekali konsumsi).
    if (final_status_ready_.exchange(false)) {
        std::string s;
        {
            std::lock_guard<std::mutex> lk(stream_mutex_);
            s = final_status_;
        }
        status_->copy_label(s.c_str());
        setUiStreaming(false);
    }
}

} // namespace frans::ui
