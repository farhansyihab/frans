// src/application/ingest/RepoIngester.cpp
//
// Fase B #50 -- implementasi RepoIngester.
#include "application/ingest/RepoIngester.h"
#include "application/ingest/MarkdownRenderer.h"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace frans::application {

namespace {

std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    return buf;
}

std::string formatBytes(std::size_t bytes) {
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%zu B", bytes);
    } else if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    }
    return buf;
}

} // namespace

RepoIngester::~RepoIngester() {
    cancel();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void RepoIngester::start(const IngestOptions& opt) {
    if (running_.load()) return;
    
    running_.store(true);
    finished_.store(false);
    cancel_flag_.store(false);
    
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        total_files_ = 0;
        processed_files_ = 0;
        total_bytes_ = 0;
        current_file_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_lines_.clear();
    }
    
    result_ = IngestResult{};
    
    worker_ = std::thread([this, opt]() {
        workerMain(opt);
    });
}

void RepoIngester::cancel() {
    cancel_flag_.store(true);
}

std::size_t RepoIngester::getTotalFiles() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return total_files_;
}

std::size_t RepoIngester::getProcessedFiles() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return processed_files_;
}

std::size_t RepoIngester::getTotalBytes() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return total_bytes_;
}

std::string RepoIngester::getCurrentFile() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return current_file_;
}

std::vector<std::string> RepoIngester::drainLog() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::vector<std::string> lines = std::move(log_lines_);
    log_lines_.clear();
    return lines;
}

void RepoIngester::appendLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_lines_.push_back(line);
}

void RepoIngester::workerMain(IngestOptions opt) {
    appendLog("[INFO] Starting ingestion for: " + opt.project_root);
    
    // 1. Collect files
    appendLog("[INFO] Collecting files...");
    CollectOptions copt;
    copt.max_file_bytes = opt.max_file_bytes;
    copt.exclude_secrets = opt.exclude_secrets;
    copt.extra_excludes = opt.extra_excludes;
    copt.output_dir = opt.output_dir;
    
    CollectResult collected = collectFiles(opt.project_root, copt, &cancel_flag_);
    
    if (cancel_flag_.load()) {
        result_.ok = false;
        result_.cancelled = true;
        result_.error = "Cancelled by user";
        appendLog("[CANCEL] Ingestion cancelled");
        finished_.store(true);
        running_.store(false);
        return;
    }
    
    if (!collected.ok) {
        result_.ok = false;
        result_.error = "collectFiles failed: " + collected.error;
        appendLog("[ERROR] " + result_.error);
        finished_.store(true);
        running_.store(false);
        return;
    }
    
    appendLog("[INFO] Found " + std::to_string(collected.files.size()) + " files" +
              (collected.used_git ? " (via git ls-files)" : " (via filesystem walker)"));
    
    // 2. Read file contents and apply budget
    std::map<std::string, std::string> file_contents;
    std::vector<std::string> starred_paths;
    std::size_t total_bytes = 0;
    
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        total_files_ = collected.files.size();
    }
    
    for (const auto& f : collected.files) {
        if (cancel_flag_.load()) break;
        
        if (f.skipped) {
            result_.skipped_paths.push_back(f.relative_path + " (" + f.skip_reason + ")");
            continue;
        }
        
        // Read file content
        std::string full_path = opt.project_root + "/" + f.relative_path;
        std::ifstream ifs(full_path, std::ios::binary);
        if (!ifs) {
            appendLog("[WARN] Cannot read: " + f.relative_path);
            continue;
        }
        
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
        
        // Check budget
        if (total_bytes + content.size() > opt.max_total_bytes) {
            result_.budget_dropped.push_back(f.relative_path);
            appendLog("[BUDGET] Dropped (exceed limit): " + f.relative_path);
            continue;
        }
        
        file_contents[f.relative_path] = std::move(content);
        starred_paths.push_back(f.relative_path);
        total_bytes += file_contents[f.relative_path].size();
        
        // Update progress
        {
            std::lock_guard<std::mutex> lock(progress_mutex_);
            processed_files_++;
            total_bytes_ = total_bytes;
            current_file_ = f.relative_path;
        }
        
        appendLog("[READ] " + f.relative_path + " (" + formatBytes(file_contents[f.relative_path].size()) + ")");
    }
    
    if (cancel_flag_.load()) {
        result_.ok = false;
        result_.cancelled = true;
        result_.error = "Cancelled by user";
        appendLog("[CANCEL] Ingestion cancelled");
        finished_.store(true);
        running_.store(false);
        return;
    }
    
    // 3. Render markdown
    appendLog("[INFO] Rendering markdown...");
    
    RenderInput rin;
    rin.all_files = collected.files;
    rin.starred_paths = starred_paths;
    rin.file_contents = file_contents;
    rin.options.repo_name = opt.repo_name;
    rin.options.header_title = "Ingest";
    rin.options.created_at = currentTimestamp();
    rin.options.total_files_included = starred_paths.size();
    rin.options.total_content_bytes = total_bytes;
    
    std::string md = renderMarkdown(rin);
    
    // 4. Write to file
    std::string output_path = opt.output_dir + "/" + opt.repo_name + "-ingest.md";
    appendLog("[INFO] Writing to: " + output_path);
    
    std::ofstream ofs(output_path);
    if (!ofs) {
        result_.ok = false;
        result_.error = "Cannot write to: " + output_path;
        appendLog("[ERROR] " + result_.error);
        finished_.store(true);
        running_.store(false);
        return;
    }
    ofs << md;
    ofs.close();
    
    // 5. Done
    result_.ok = true;
    result_.output_path = output_path;
    result_.total_files_included = starred_paths.size();
    result_.total_content_bytes = total_bytes;
    result_.est_tokens = total_bytes / 4;  // rough estimate
    result_.all_files = collected.files;
    
    appendLog("[SUCCESS] Ingestion complete: " + std::to_string(starred_paths.size()) + " files, " +
              formatBytes(total_bytes) + ", ~" + std::to_string(result_.est_tokens) + " tokens");
    
    finished_.store(true);
    running_.store(false);
}

} // namespace frans::application