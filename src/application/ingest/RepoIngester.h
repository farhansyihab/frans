// src/application/ingest/RepoIngester.h
//
// Fase B #50 -- ingest penuh seluruh repo ke satu file .md.
//
// Alur:
//   1. collectFiles() -> daftar file (tanpa secret/binary)
//   2. Baca isi file dari disk (yang tidak di-skip)
//   3. Batasi total bytes (max_total_bytes)
//   4. Render markdown (tree lengkap + semua isi file)
//   5. Tulis ke <output_dir>/<repo_name>-ingest.md
//
// Thread-safe: worker berjalan di thread terpisah, UI thread
// polling progress via getters. Cancel via atomic flag.
#pragma once
#include "application/ingest/FileCollector.h"
#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace frans::application {

struct IngestOptions {
    std::string project_root;
    std::string output_dir;           // ~/frans-out by default
    std::string repo_name;            // untuk header & nama file output
    std::size_t max_file_bytes = 256 * 1024;   // 256 KB per file
    std::size_t max_total_bytes = 5 * 1024 * 1024;  // 5 MB total
    bool exclude_secrets = true;
    std::vector<std::string> extra_excludes;
};

struct IngestResult {
    bool ok = false;
    bool cancelled = false;
    std::string error;
    std::string output_path;
    
    std::size_t total_files_included = 0;
    std::size_t total_content_bytes = 0;
    std::size_t est_tokens = 0;
    
    std::vector<CollectedFile> all_files;
    std::vector<std::string> skipped_paths;      // path + alasan (format: "path (reason)")
    std::vector<std::string> budget_dropped;     // file yang dibuang karena exceed budget
};

class RepoIngester {
public:
    RepoIngester() = default;
    ~RepoIngester();
    
    RepoIngester(const RepoIngester&) = delete;
    RepoIngester& operator=(const RepoIngester&) = delete;
    
    // Start ingestion in background thread.
    // Return immediately. Poll isFinished() to check completion.
    void start(const IngestOptions& opt);
    
    // Request cancellation. Worker will stop at next checkpoint.
    void cancel();
    
    // Thread-safe getters (called from UI thread)
    bool isRunning() const { return running_.load(); }
    bool isFinished() const { return finished_.load(); }
    
    std::size_t getTotalFiles() const;
    std::size_t getProcessedFiles() const;
    std::size_t getTotalBytes() const;
    std::string getCurrentFile() const;
    
    // Get log lines accumulated by worker.
    // Clears internal buffer after reading.
    std::vector<std::string> drainLog();
    
    // Only valid after isFinished() == true.
    const IngestResult& getResult() const { return result_; }
    
private:
    void workerMain(IngestOptions opt);
    
    std::thread worker_;
    std::atomic<bool> cancel_flag_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> running_{false};
    
    mutable std::mutex progress_mutex_;
    std::size_t total_files_ = 0;
    std::size_t processed_files_ = 0;
    std::size_t total_bytes_ = 0;
    std::string current_file_;
    
    mutable std::mutex log_mutex_;
    std::vector<std::string> log_lines_;
    
    IngestResult result_;
    
    void appendLog(const std::string& line);
};

} // namespace frans::application