#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <thread>

// Struct to hold progress information for the UI
struct IngestProgress {
    int total_files;
    int processed_files;
    std::string current_file;
};

class RepoIngester {
public:
    RepoIngester();
    ~RepoIngester();

    // Start ingestion in a background thread
    void start(const std::string& repo_path, 
               std::function<void(const IngestProgress&)> on_progress,
               std::function<void(bool, const std::string&)> on_complete);

    // Request cancellation
    void cancel();
    bool is_running() const;

private:
    std::atomic<bool> m_cancel_requested;
    std::atomic<bool> m_is_running;
    std::thread m_worker;
};