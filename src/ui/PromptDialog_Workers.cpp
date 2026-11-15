// src/ui/PromptDialog_Workers.cpp
// Worker thread implementations
#include "ui/PromptDialog.h"
#include "application/ai/ApiKeyProvider.h"
#include "application/ai/GeminiClient.h"
#include "application/context/ContextSelector.h"
#include "application/context/SourceCodeBuilder.h"
#include "application/ingest/FileCollector.h"
#include "infrastructure/http/CurlTransport.h"
#include <FL/Fl_Text_Buffer.H>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
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

std::string formatTimestampNow() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::string readFileContents(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

// ===========================================================================
// Workers
// ===========================================================================
void PromptDialog::workerListModels() {
    frans::application::ApiKeyOptions key_opt;
    key_opt.key_file = cfg_.gemini_api_key_file;
    auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
    
    if (!key_result.found()) {
        list_models_worker_ok_ = false;
        list_models_worker_message_ = "API key tidak ditemukan.";
        list_models_worker_running_.store(false);
        return;
    }
    
    auto transport = std::make_shared<frans::infrastructure::http::CurlTransport>();
    frans::application::GeminiClient client(transport);
    
    auto result = client.listModels(cfg_.gemini_endpoint, key_result.key, &list_models_worker_cancel_);
    
    if (!result.ok) {
        list_models_worker_ok_ = false;
        list_models_worker_message_ = "Error: " + result.error.message;
        if (result.error.http_status > 0) {
            list_models_worker_message_ += " (HTTP " + std::to_string(result.error.http_status) + ")";
        }
    } else {
        list_models_worker_ok_ = true;
        list_models_worker_models_ = result.models;
        
        std::ostringstream ss;
        ss << "Koneksi berhasil! Ditemukan " << result.models.size() << " model:\n";
        
        int count = 0;
        for (const auto& m : result.models) {
            if (count >= 20) {
                ss << "... dan " << (result.models.size() - 20) << " model lainnya.\n";
                break;
            }
            ss << "\xE2\x80\xA2 " << m.name;
            if (!m.display_name.empty() && m.display_name != m.name) {
                ss << " (" << m.display_name << ")";
            }
            ss << "\n";
            ++count;
        }
        
        std::string cfg_model_norm = normalizeModelName(cfg_.gemini_model);
        bool model_found = false;
        for (const auto& m : result.models) {
            std::string m_norm = normalizeModelName(m.name);
            if (m_norm == cfg_model_norm) {
                model_found = true;
                break;
            }
        }
        
        if (model_found) {
            ss << "\n\xE2\x9C\x93 Model '" << cfg_.gemini_model << "' tersedia.\n";
        } else {
            ss << "\n\xE2\x9A\xA0 Model '" << cfg_.gemini_model
               << "' TIDAK ditemukan dalam daftar!\n";
            ss << "  Buka config/app.json, ganti 'gemini_model' dengan salah satu nama di atas.\n";
        }
        
        list_models_worker_message_ = ss.str();
    }
    
    list_models_worker_running_.store(false);
}

void PromptDialog::workerSelectFiles() {
    frans::application::ApiKeyOptions key_opt;
    key_opt.key_file = cfg_.gemini_api_key_file;
    auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
    
    if (!key_result.found()) {
        select_worker_ok_ = false;
        select_worker_error_ = "API key tidak ditemukan.";
        select_worker_running_.store(false);
        return;
    }
    
    std::string project_root = resolveProjectRoot(session_, cfg_);
    if (project_root.empty()) {
        select_worker_ok_ = false;
        select_worker_error_ = "Project root belum diatur.";
        select_worker_running_.store(false);
        return;
    }
    
    const char* req_raw = request_buf_ ? request_buf_->text() : nullptr;
    std::string request = req_raw ? req_raw : "";
    
    const char* err_raw = error_buf_ ? error_buf_->text() : nullptr;
    std::string error_output = err_raw ? err_raw : "";
    
    std::string selector_prompt = readFileContents(cfg_.selector_prompt_path);
    if (selector_prompt.empty()) {
        selector_prompt = "Anda adalah asisten pemilih konteks. Pilih file yang relevan.";
    }
    
    std::vector<std::string> always_include;
    if (!cfg_.context_always_include.empty()) {
        std::istringstream iss(cfg_.context_always_include);
        std::string item;
        while (std::getline(iss, item, ',')) {
            size_t start = item.find_first_not_of(" \t");
            size_t end = item.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                always_include.push_back(item.substr(start, end - start + 1));
            }
        }
    }
    
    std::string model_name = normalizeModelName(cfg_.gemini_model);
    std::fprintf(stderr, "[frans][prompt_dialog] workerSelectFiles: calling Gemini with model='%s'\n",
                 model_name.c_str());
    std::fflush(stderr);
    
    frans::application::ContextRequest ctx_req;
    ctx_req.project_root = project_root;
    ctx_req.request_text = request;
    ctx_req.error_output = error_output;
    ctx_req.always_include = always_include;
    ctx_req.max_context_bytes = static_cast<std::size_t>(cfg_.context_max_kb) * 1024;
    ctx_req.max_files = static_cast<std::size_t>(cfg_.context_max_files);
    ctx_req.gemini_model = model_name;
    ctx_req.gemini_endpoint = cfg_.gemini_endpoint;
    ctx_req.api_key = key_result.key;
    ctx_req.selector_prompt = selector_prompt;
    ctx_req.gemini_timeout_sec = cfg_.gemini_timeout_sec;
    ctx_req.gemini_full_mode_max_tokens = static_cast<std::size_t>(cfg_.gemini_full_mode_max_tokens);
    ctx_req.gemini_index_head_lines = cfg_.gemini_index_head_lines;
    
    auto transport = std::make_shared<frans::infrastructure::http::CurlTransport>();
    frans::application::GeminiClient client(transport);
    
    auto result = frans::application::selectContext(ctx_req, client, &select_worker_cancel_);
    
    if (select_worker_cancel_.load()) {
        select_worker_ok_ = false;
        select_worker_error_ = "Dibatalkan oleh pengguna.";
    } else if (!result.ok) {
        select_worker_ok_ = false;
        select_worker_error_ = result.error;
        select_worker_used_model_ = model_name;
    } else {
        select_worker_ok_ = true;
        select_worker_result_ = result;
        
        frans::application::CollectOptions col_opt;
        col_opt.max_file_bytes = 256 * 1024;
        col_opt.exclude_secrets = true;
        select_worker_collected_ = frans::application::collectFiles(project_root, col_opt, nullptr).files;
    }
    
    select_worker_running_.store(false);
}

void PromptDialog::workerBuildSourceCode() {
    std::string project_root = resolveProjectRoot(session_, cfg_);
    if (project_root.empty()) {
        build_worker_result_.error = "Project root belum diatur.";
        build_worker_running_.store(false);
        return;
    }
    
    const char* req_raw = request_buf_ ? request_buf_->text() : nullptr;
    std::string request = req_raw ? req_raw : "";
    
    // FIX: Set created_at di manifest
    frans::application::BuildManifest manifest;
    manifest.created_at = formatTimestampNow();
    manifest.request_hash = computeRequestHash();
    manifest.mode = last_selection_.mode;
    manifest.model = cfg_.gemini_model;
    
    auto result = frans::application::buildSourceCode(
        select_worker_collected_.empty() ? last_collected_files_ : select_worker_collected_,
        last_selection_.files,
        project_root,
        cfg_.ingest_output_dir,
        cfg_.context_output_name,
        request,
        manifest,
        nullptr);
    
    build_worker_result_ = result;
    build_worker_running_.store(false);
}

} // namespace frans::ui