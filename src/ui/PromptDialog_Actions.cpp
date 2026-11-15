// src/ui/PromptDialog_Actions.cpp
// Action methods: doGenerate, doCopy, doFetchLastError, doTestConnection,
// doSelectFiles, doAddManual, doCancelSelect, doBuildSourceCode, doOpenFolder
#include "ui/PromptDialog.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ai/ApiKeyProvider.h"
#include "application/ai/GeminiClient.h"
#include "application/context/ContextSelector.h"
#include "application/context/SourceCodeBuilder.h"
#include "infrastructure/http/CurlTransport.h"
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace frans::ui {

namespace {

std::string resolveProjectRoot(SessionState* session,
                               const frans::config::AppConfig& cfg)
{
    if (session && !session->last_project_root.empty()) {
        return session->last_project_root;
    }
    if (!cfg.default_project_root.empty()) {
        return cfg.default_project_root;
    }
    return "";
}

std::string normalizeModelName(const std::string& raw) {
    std::string s = raw;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    
    const std::string prefix = "models/";
    if (s.size() > prefix.size() && s.compare(0, prefix.size(), prefix) == 0) {
        s = s.substr(prefix.size());
    }
    return s;
}

} // namespace

// ===========================================================================
// Actions
// ===========================================================================
void PromptDialog::doGenerate() {
    if (!request_buf_ || !preview_buf_) return;
    
    const char* req_raw = request_buf_->text();
    const std::string request = req_raw ? req_raw : "";
    
    if (request.empty()) {
        fl_message("Permintaan kosong.\nTulis permintaan Anda di kotak atas dulu.");
        return;
    }
    
    Freshness fresh = checkFreshness();
    std::string context_file_name;
    
    if (fresh == Freshness::Fresh) {
        context_file_name = cfg_.context_output_name;
    } else {
        int choice = fl_choice("SourceCode.md tidak segar atau belum ada.\n"
                               "Generate prompt tanpa konteks file?",
                               "Batal", "Tetap generate", nullptr);
        if (choice == 0) return;
        context_file_name = "";
    }
    
    const char* err_raw = error_buf_ ? error_buf_->text() : nullptr;
    const std::string error_output = err_raw ? err_raw : "";
    
    frans::application::PromptInput in;
    in.tmpl              = loadTemplate();
    in.request           = request;
    in.error_output      = error_output;
    in.context_file_name = context_file_name;
    
    std::string new_files_hint;
    for (const auto& nf : last_selection_.new_files_expected) {
        if (!new_files_hint.empty()) new_files_hint += "\n";
        new_files_hint += nf;
    }
    in.new_files_hint = new_files_hint;
    
    last_generated_prompt_ = frans::application::renderPrompt(in);
    preview_buf_->text(last_generated_prompt_.c_str());
    
    if (copy_btn_) copy_btn_->activate();
    
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "Prompt di-generate (%zu karakter). Klik [Copy] untuk menyalin.",
                  last_generated_prompt_.size());
    setStatus(buf, FL_DARK_GREEN);
}

void PromptDialog::doCopy() {
    if (last_generated_prompt_.empty()) {
        fl_message("Belum ada prompt yang di-generate.\nKlik [Generate Prompt] dulu.");
        return;
    }
    
    Fl::copy(last_generated_prompt_.c_str(),
             static_cast<int>(last_generated_prompt_.size()), 1);
    setStatus("Prompt disalin ke clipboard. Paste ke AI browser Anda.", FL_DARK_GREEN);
}

void PromptDialog::doFetchLastError() {
    if (!session_) {
        fl_message("SessionState tidak tersedia.");
        return;
    }
    
    if (session_->last_build_error.empty()) {
        fl_message("Belum ada error dari build terakhir.");
        return;
    }
    
    if (error_buf_) {
        error_buf_->text(session_->last_build_error.c_str());
        setStatus("Error dari build terakhir dimuat.", FL_DARK_GREEN);
    }
}

void PromptDialog::doTestConnection() {
    if (list_models_worker_running_.load()) {
        fl_message("Tes koneksi sedang berjalan.");
        return;
    }
    
    frans::application::ApiKeyOptions key_opt;
    key_opt.key_file = cfg_.gemini_api_key_file;
    auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
    
    if (!key_result.found()) {
        std::string msg = "GEMINI_API_KEY tidak ditemukan.\n" + frans::application::apiKeyMissingHint();
        fl_message("%s", msg.c_str());
        return;
    }
    
    if (list_models_worker_thread_.joinable()) list_models_worker_thread_.join();
    
    if (test_conn_btn_) test_conn_btn_->deactivate();
    setStatus("Menguji koneksi ke Gemini API...", FL_DARK_BLUE);
    
    list_models_worker_cancel_.store(false);
    list_models_worker_running_.store(true);
    list_models_worker_ok_ = false;
    list_models_worker_message_.clear();
    list_models_worker_models_.clear();
    
    list_models_worker_thread_ = std::thread(&PromptDialog::workerListModels, this);
    Fl::add_timeout(0.1, cbPollListModelsWorker, this);
}

void PromptDialog::doSelectFiles() {
    if (select_worker_running_.load()) {
        fl_message("Seleksi file sedang berjalan.");
        return;
    }
    
    if (!request_buf_) return;
    
    const char* req_raw = request_buf_->text();
    std::string request = req_raw ? req_raw : "";
    
    if (request.empty()) {
        fl_message("Permintaan kosong.\nTulis permintaan dulu sebelum memilih file.");
        return;
    }
    
    frans::application::ApiKeyOptions key_opt;
    key_opt.key_file = cfg_.gemini_api_key_file;
    auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
    
    if (!key_result.found()) {
        fl_message("GEMINI_API_KEY tidak ditemukan.");
        return;
    }
    
    std::string project_root = resolveProjectRoot(session_, cfg_);
    if (project_root.empty()) {
        fl_message("Project root belum diatur.\n"
                   "Isi path project di MainWindow atau set 'default_project_root' di config/app.json.");
        return;
    }
    
    std::string model_name = normalizeModelName(cfg_.gemini_model);
    std::fprintf(stderr, "[frans][prompt_dialog] doSelectFiles: project_root='%s', model='%s' (raw='%s')\n",
                 project_root.c_str(), model_name.c_str(), cfg_.gemini_model.c_str());
    std::fflush(stderr);
    
    if (select_worker_thread_.joinable()) select_worker_thread_.join();
    
    if (select_btn_) select_btn_->deactivate();
    if (cancel_select_btn_) cancel_select_btn_->activate();
    setStatus("Memilih file via Gemini (model: " + model_name + ")...", FL_DARK_BLUE);
    
    select_worker_cancel_.store(false);
    select_worker_running_.store(true);
    select_worker_ok_ = false;
    select_worker_error_.clear();
    select_worker_result_ = frans::application::SelectionResult{};
    select_worker_collected_.clear();
    
    select_worker_thread_ = std::thread(&PromptDialog::workerSelectFiles, this);
    Fl::add_timeout(0.1, cbPollSelectWorker, this);
}

void PromptDialog::doAddManual() {
    std::string project_root = resolveProjectRoot(session_, cfg_);
    if (project_root.empty()) {
        fl_message("Project root belum diatur.\n"
                   "Isi path project di MainWindow atau set 'default_project_root' di config/app.json.");
        return;
    }
    
    const char* picked = fl_file_chooser("Pilih file untuk ditambahkan",
                                         "*",
                                         project_root.c_str());
    if (!picked) return;
    
    std::string abs_path = picked;
    std::string rel_path;
    
    if (abs_path.find(project_root) == 0) {
        rel_path = abs_path.substr(project_root.size());
        if (!rel_path.empty() && (rel_path[0] == '/' || rel_path[0] == '\\')) {
            rel_path = rel_path.substr(1);
        }
    } else {
        fl_message("File harus berada di dalam project root.");
        return;
    }
    
    for (char& c : rel_path) if (c == '\\') c = '/';
    
    for (const auto& entry : last_selection_.files) {
        if (entry.path == rel_path) {
            fl_message("File sudah ada dalam daftar.");
            return;
        }
    }
    
    std::ifstream f(abs_path, std::ios::binary | std::ios::ate);
    if (!f) {
        fl_message("Gagal membaca file.");
        return;
    }
    
    std::size_t size = static_cast<std::size_t>(f.tellg());
    
    frans::application::ContextFileEntry entry;
    entry.path = rel_path;
    entry.priority = "manual";
    entry.reason = "Ditambahkan manual";
    entry.size_bytes = size;
    entry.selected = true;
    
    last_selection_.files.push_back(entry);
    updateBrowserFromSelection();
    setStatus("File ditambahkan: " + rel_path, FL_DARK_GREEN);
}

void PromptDialog::doCancelSelect() {
    if (select_worker_running_.load()) {
        select_worker_cancel_.store(true);
        setStatus("Membatalkan seleksi...", FL_DARK_BLUE);
    }
}

void PromptDialog::doBuildSourceCode() {
    if (build_worker_running_.load()) {
        fl_message("Build sedang berjalan.");
        return;
    }
    
    if (last_selection_.files.empty()) {
        fl_message("Belum ada file terpilih.\nKlik [Pilih File via Gemini] dulu.");
        return;
    }
    
    std::string project_root = resolveProjectRoot(session_, cfg_);
    if (project_root.empty()) {
        fl_message("Project root belum diatur.");
        return;
    }
    
    if (!request_buf_) return;
    
    if (build_worker_thread_.joinable()) build_worker_thread_.join();
    
    if (build_btn_) build_btn_->deactivate();
    setStatus("Membangun SourceCode.md...", FL_DARK_BLUE);
    
    build_worker_running_.store(true);
    build_worker_result_ = frans::application::BuildResult{};
    
    build_worker_thread_ = std::thread(&PromptDialog::workerBuildSourceCode, this);
    Fl::add_timeout(0.1, cbPollBuildWorker, this);
}

void PromptDialog::doOpenFolder() {
    if (last_build_result_.source_code_path.empty()) return;
    
    std::string dir = std::filesystem::path(last_build_result_.source_code_path).parent_path().string();
    if (dir.empty()) return;
    
#ifdef _WIN32
    std::string cmd = "explorer \"" + dir + "\"";
#else
    std::string cmd = "xdg-open \"" + dir + "\" &";
#endif
    
    int ret = std::system(cmd.c_str());
    (void)ret;
}

} // namespace frans::ui