// A controller that receives HTTP request parameters and formats the response.
#pragma once
#include "../services/auth_service.hpp"
#include "../services/login_limiter.hpp"
#include "../services/session_manager.hpp"
#include <httplib.h>
#include <string>

class AuthController {
private:
    AuthService &auth_service;
    SessionManager &session_manager;
    LoginLimiter &login_limiter;

    // Helper function to extract the value of a specific key from the cookie header
    std::string getCookieValue(const std::string &cookie_header, const std::string &key) {
        if (cookie_header.empty())
            return "";

        std::string prefix = key + "=";
        size_t start = cookie_header.find(prefix);
        if (start == std::string::npos)
            return "";

        start += prefix.length();
        size_t end = cookie_header.find(";", start);
        if (end == std::string::npos) {
            return cookie_header.substr(start);
        }
        return cookie_header.substr(start, end - start);
    }

    // Escape special characters in a JSON string
    std::string escapeJson(const std::string &input) {
        std::string output;
        for (char c : input) {
            if (c == '"') {
                output += "\\\"";
            } else if (c == '\\') {
                output += "\\\\";
            } else if (c == '\b') {
                output += "\\b";
            } else if (c == '\f') {
                output += "\\f";
            } else if (c == '\n') {
                output += "\\n";
            } else if (c == '\r') {
                output += "\\r";
            } else if (c == '\t') {
                output += "\\t";
            } else {
                output += c;
            }
        }
        return output;
    }

    // Validate username: only allow alphanumeric, underscore, hyphen (3~32 chars)
    bool isValidUsername(const std::string &username) {
        if (username.length() < 3 || username.length() > 32)
            return false;

        for (char c : username) {
            if (!std::isalnum(c) && c != '_' && c != '-')
                return false;
        }
        return true;
    }

public:
    // explicit : Completely block "Implict Conversion"
    explicit AuthController(AuthService &service, SessionManager &manager, LoginLimiter &limiter) : auth_service(service), session_manager(manager), login_limiter(limiter) {}

    // User registration request handler
    void handleSignUp(const httplib::Request &req, httplib::Response &res) {
        // Extract username and password at Form parameter
        auto username = req.get_param_value("username");
        auto password = req.get_param_value("password");

        if (username.empty() || password.empty()) {
            res.status = 400;
            res.set_content(R"({"status":"error", "message":"Username or password are required"})",
                            "application/json");
            return;
        }

        if (!isValidUsername(username)) {
            res.status = 400;
            res.set_content(R"({"status":"error", "message":"Username must be 3-32 characters"})",
                            "application/json");
            return;
        }

        if (auth_service.signUp(username, password)) {
            res.status = 201;
            res.set_content(R"({"status":"success", "message":"User registered successfully"})", "application/json");
        } else {
            res.status = 409;
            res.set_content(R"({"status":"error", "message":"Username already exists"})", "application/json");
            return;
        }
    }

    // Login request handler
    void handleLogin(const httplib::Request &req, httplib::Response &res) {
        auto username = req.get_param_value("username");
        auto password = req.get_param_value("password");

        if (username.empty() || password.empty()) {
            res.status = 400;
            res.set_content(R"({"status":"error", "message":"Username and password are required"})",
                            "application/json");
            return;
        }

        // 1. Check lockout status
        long long remaining_time = login_limiter.getRemainingLockoutTime(username);
        if (remaining_time > 0) {
            res.status = 429; // Too Many Requests
            res.set_content(R"({"status":"error", "message":"Account is temporarily locked. Please try again in ")" +
                                std::to_string(remaining_time) + R"( seconds."})",
                            "application/json");

            return;
        }

        if (auth_service.login(username, password)) {
            res.status = 200;

            // Reset failure attempts upon successful login
            login_limiter.resetAttempts(username);
            std::string session_id = session_manager.createSession(username);

            // Vulnerable Login authentication - plain text cookie
            res.set_header("Set-Cookie", "auth_session=" + session_id + "; Path=/; HttpOnly; SameSite=Lax; Secure;");
            res.set_content(R"({"status":"success", "message":"Login successful"})", "application/json");
        } else {
            // Record failure and check for lockout
            login_limiter.recordFailure(username);

            res.status = 401;
            res.set_content(R"({"status":"error", "message":"Invalid username or password"})", "application/json");
        }
    }

    // Admin API handler for retrieving the list of registered users as JSON
    void handleGetUsers(const httplib::Request &req, httplib::Response &res) {
        // Extract cookie in the request header
        std::string cookie = req.get_header_value("Cookie");

        // Parse session ID from cookie header
        std::string session_id = getCookieValue(cookie, "auth_session");

        // Validate session via session manager and get logged-in username
        std::string username = session_manager.validateSession(session_id);

        // Get actual role for the user
        std::string role = auth_service.getUserRole(username);

        if (role != "ADMIN") {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"Access denied, Administrator privilege required."})", "application/json");
            return;
        }

        auto users = auth_service.getAllUsers();
        std::string json_result = "[";

        for (size_t i = 0; i < users.size(); ++i) {
            json_result += "{\"username\":\"" + escapeJson(users[i].username) + "\"}";
            if (i < users.size() - 1) {
                json_result += ",";
            }
        }

        json_result += "]";

        res.status = 200;
        res.set_content(json_result, "application/json");
    }

    // Get currently logged-in user profile for frontend UI rendering
    void handleGetMe(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");

        std::string username = session_manager.validateSession(session_id);

        if (username.empty()) {
            res.status = 400;
            res.set_content(R"({"status":"error", "message":"Unathorized"})", "application/json");
            return;
        }

        std::string role = auth_service.getUserRole(username);

        res.status = 200;
        res.set_content(R"({"status":"success", "username":")" + escapeJson(username) + R"(", "role":")" + escapeJson(role) + R"("})", "application/json");
    }

    // User logout request handler
    void handleLogout(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");

        // Destroy session on the server-side session store
        session_manager.destroySession(session_id);

        res.set_header("Set-Cookie", "auth_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0; Secure;");
        res.status = 200;
        res.set_content(R"({"status":"success", "message":"Logged out successfully"})", "application/json");
    }
};