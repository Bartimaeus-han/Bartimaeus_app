#pragma once
#include <chrono>
#include <mutex>
#include <unordered_map>

// Track login failure attempts
struct LoginAttempt {
    int failed_attempts = 0;                              // Number of failed attempts
    std::chrono::system_clock::time_point lockout_until;  // Lockout expiration time
    std::chrono::system_clock::time_point last_fail_time; // Last failed attempt time
};

class LoginLimiter {
private:
    // Store login attempts per user
    std::unordered_map<std::string, LoginAttempt> attempts_;

    // Multi-thread synchronization
    std::mutex limiter_mutex_;

    // Maximum allowed failed attempts
    const int MAX_FAILED_ATTEMPTS = 5;

    // Lockout duration
    const std::chrono::seconds LOCKOUT_DURATION = std::chrono::seconds(30);

    // Cooldown duration for failed attempts
    const std::chrono::seconds COOLDOWN_DURATION = std::chrono::seconds(60);

public:
    LoginLimiter() = default;

    // Check if the user is currently locked out
    // Returns remaining lockout seconds, or 0 if not locked
    long long getRemainingLockoutTime(const std::string &username) {
        std::lock_guard<std::mutex> lock(limiter_mutex_);

        auto it = attempts_.find(username);

        if (it == attempts_.end()) {
            return 0;
        }

        auto now = std::chrono::system_clock::now();

        // Cooldown check
        if (now >= it->second.lockout_until && (now - it->second.last_fail_time) >= COOLDOWN_DURATION) {
            attempts_.erase(it);
            return 0;
        }

        if (now < it->second.lockout_until) {
            // Calculate remaining time in seconds
            auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second.lockout_until - now);
            return remaining.count();
        }

        return 0;
    }

    // Record a login failure and trigger lockout if needed
    void recordFailure(const std::string &username) {
        std::lock_guard<std::mutex> lock(limiter_mutex_);

        auto now = std::chrono::system_clock::now();
        auto &attempt = attempts_[username];

        // Cooldown check
        if (now >= attempt.last_fail_time && (now - attempt.last_fail_time) >= COOLDOWN_DURATION) {
            attempt.failed_attempts = 0;
        }

        attempt.failed_attempts++;
        attempt.last_fail_time = now;

        if (attempt.failed_attempts >= MAX_FAILED_ATTEMPTS) {
            attempt.lockout_until = std::chrono::system_clock::now() + LOCKOUT_DURATION;
        }
    }

    // Reset failure attempts upon successful login
    void resetAttempts(const std::string &username) {
        std::lock_guard<std::mutex> lock(limiter_mutex_);
        attempts_.erase(username);
    }
};