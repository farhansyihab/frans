#include "RepoIngester.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

RepoIngester::RepoIngester() : m_cancel_requested(false), m_is_running(false) {}

RepoIngester::~RepoIngester() {
    cancel();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void RepoIngester::start(const std::string& repo_path, 
                         std::function<void(const IngestProgress&)> on_progress,
                         std::function<void(bool, const std::string&)> on_complete) {
    if (m_is_running) return;
    m_is_running = true;
    m_cancel_requested = false;

    m_worker = std::thread([this, repo_path, on_progress, on_complete]() {
        IngestProgress progress{0, 0, ""};
        std::error_code ec;
        
        // Pass 1: Count total files for progress bar maximum
        for (auto const& entry : fs::recursive_directory_iterator(repo_path, ec)) {
            if (m_cancel_requested) break;
            if (entry.is_regular_file()) {
                progress.total_files++;
            }
        }
        
        if (m_cancel_requested) {
            m_is_running = false;
            on_complete(false, "Ingestion cancelled by user.");
            return;
        }

        // Pass 2: Process files
        for (auto const& entry : fs::recursive_directory_iterator(repo_path, ec)) {
            if (m_cancel_requested) break;
            if (entry.is_regular_file()) {
                progress.current_file = entry.path().string();
                progress.processed_files++;
                
                // TODO: Implement actual chunking / embedding logic here
                // std::ifstream file(entry.path());
                // ...
                
                on_progress(progress);
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Yield / simulate work
            }
        }

        m_is_running = false;
        if (m_cancel_requested) {
            on_complete(false, "Ingestion cancelled by user.");
        } else {
            on_complete(true, "Ingestion completed successfully.");
        }
    });
}

void RepoIngester::cancel() {
    m_cancel_requested = true;
}

bool RepoIngester::is_running() const {
    return m_is_running;
}