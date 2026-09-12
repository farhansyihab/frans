// ~/AgenAI/frans/src/application/resource_guard/IdleUnloadTimer.cpp
#include "application/resource_guard/IdleUnloadTimer.h"

namespace frans::application {

IdleUnloadTimer::IdleUnloadTimer(std::chrono::seconds timeout)
    : timeout_(timeout) {}

void IdleUnloadTimer::setTimeout(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lk(mtx_);
    timeout_ = timeout;
}

std::chrono::seconds IdleUnloadTimer::timeout() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return timeout_;
}

void IdleUnloadTimer::start() {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = true;
    last_activity_ = std::chrono::steady_clock::now();
}

void IdleUnloadTimer::stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = false;
}

void IdleUnloadTimer::markActivity() {
    std::lock_guard<std::mutex> lk(mtx_);
    running_ = true;
    last_activity_ = std::chrono::steady_clock::now();
}

bool IdleUnloadTimer::isRunning() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return running_;
}

bool IdleUnloadTimer::expired() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!running_) return false;
    const auto now = std::chrono::steady_clock::now();
    return (now - last_activity_) >= timeout_;
}

std::chrono::seconds IdleUnloadTimer::remaining() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!running_) return std::chrono::seconds(0);
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_activity_);
    if (elapsed >= timeout_) return std::chrono::seconds(0);
    return timeout_ - elapsed;
}

} // namespace frans::application
