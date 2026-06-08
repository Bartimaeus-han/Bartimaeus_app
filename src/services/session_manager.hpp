#pragma once
#include <chrono>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

// Structure to store session information
struct Session {
    std::string username;                             // Session owner's username
    std::chrono::system_clock::time_point expires_at; // Session expiration time
    std::string csrf_token;                           // Token for CSRF defense
};

class SessionManager {
private:
    // Session store: Session ID(string) -> Session info(Session struct) mapping
    std::unordered_map<std::string, Session> sessions_;

    // Mutex for protecting concurrent access to the session map in a multi-threaded environment.
    std::mutex session_mutex_;

    const std::chrono::minutes SESSION_LIFETIME = std::chrono::minutes(30);

    std::string generateSessionId() {
        static const char hex_chars[] = "0123456789abcdef";

        // Random number engine using hardware source as seed
        std::random_device rd;
        std::mt19937 generator(rd());                        // random number generate algorithm engine
        std::uniform_int_distribution<> distribution(0, 15); // for uniform distribution

        std::stringstream ss;

        for (int i = 0; i < 64; ++i) {
            ss << hex_chars[distribution(generator)];
        }
        return ss.str();
    }

public:
    // Create a new session and return the session ID
    std::string createSession(const std::string &username) {
        // Lock guard for thread-safe concurrent access
        std::lock_guard<std::mutex> lock(session_mutex_);

        std::string session_id = generateSessionId();

        // Re-use generator to create CSRF token
        std::string csrf_token = generateSessionId();

        auto expires_at = std::chrono::system_clock::now() + SESSION_LIFETIME;

        // Session ID -> Session Info
        // Bind generated token to structure
        sessions_[session_id] = {username, expires_at, csrf_token};

        return session_id;
    }

    // Validate session ID and return owner's username
    std::string validateSession(const std::string &session_id) {
        std::lock_guard<std::mutex> lock(session_mutex_);

        auto it = sessions_.find(session_id);

        // If session ID does not exist
        if (it == sessions_.end()) {
            return "";
        }

        // If session expired, delete session info and return empty
        if (std::chrono::system_clock::now() > it->second.expires_at) {
            sessions_.erase(it);
            return "";
        }

        return it->second.username;
    }

    // Retrieve stored CSRF toekn by session ID
    std::string getCsrfToken(const std::string &session_id) {
        std::lock_guard<std::mutex> lock(session_mutex_);

        // find session id in temporary memory (sessions_)
        auto it = sessions_.find(session_id);

        // if session not exist
        if (it == sessions_.end()) {
            // return nothing
            return "";
        }

        // expire session & csrf if that session is expired
        if (std::chrono::system_clock::now() > it->second.expires_at) {
            sessions_.erase(it);
            return "";
        }

        return it->second.csrf_token;
    }

    // Validate the CSRF token
    bool validateCsrfToekn(const std::string &session_id, const std::string &token) {
        // if token value is empty
        if (token.empty()) {
            return false;
        }

        // actual token is stored in server-side storage(sessions_)
        std::string actual_token = getCsrfToken(session_id);

        // !actual_token.empty() => Check that storage not have csrf token
        return (!actual_token.empty() && actual_token == token);
    }

    // Delete session - for logout processing
    void destroySession(const std::string &session_id) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sessions_.erase(session_id);
    }
};