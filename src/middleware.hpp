#pragma once
#include "helpers.hpp"
#include "services/auth_service.hpp"
#include "services/session_manager.hpp"

#include <functional>
#include <httplib.h>
#include <string>

// Authenticated user information
struct UserContext {
    std::string username;
    std::string session_id;
};

// Handler signature to be executed after middleware
// using : make Type Alias
// std::function : store any callable object
// In this time, only define a type. so can omitted a var name (httplib::Request &)
using AuthenticateHandler = std::function<void(const httplib::Request &, httplib::Response &, const UserContext &)>;

inline std::function<void(const httplib::Request &, httplib::Response &)> requireAuth(
    SessionManager &session_manager,
    AuthenticateHandler handler) {
    return [&session_manager, handler](const httplib::Request &req,
                                       httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");
        std::string username = session_manager.validateSession(session_id);

        // Check user's name
        if (username.empty()) {
            res.status = 401; // Unathorized
            res.set_content(R"({"status":"error", "message":"Unauthorized"})", "application/json");
            return;
        }

        handler(req, res, UserContext{username, session_id});
    };
}

// Authentication + Anti-CSRF middleware
// verify session and X-CSRF-TOKEN header
inline std::function<void(const httplib::Request &, httplib::Response &)> requireAuthAndCsrf(
    SessionManager &session_manager,
    AuthenticateHandler handler) {

    return requireAuth(session_manager, [&session_manager, handler](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        std::string csrf_token_header = req.get_header_value("X-CSRF-TOKEN");

        if (!session_manager.validateCsrfToekn(ctx.session_id, csrf_token_header)) {
            // Destroy server session immediately upon detecting CSRF attack sign
            session_manager.destroySession(ctx.session_id);

            // Force expiration of the client's session cookie
            res.set_header("Set-Cookie", "auth_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0; Secure;");

            res.status = 403; // Forbidden. CSRF token issue
            res.set_content(R"({"status":"error", "message":"CSRF token is invalid"})", "application/json");
            return;
        }
        handler(req, res, ctx);
    });
}

// Admin authorization middleware : Checks session and ADMIN role
inline std::function<void(const httplib::Request &, httplib::Response &)> requireAdmin(
    SessionManager &session_manager,
    AuthService &auth_service,
    AuthenticateHandler handler) {

    return requireAuth(session_manager, [&auth_service, handler](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        // Get role to authorization
        std::string role = auth_service.getUserRole(ctx.username);

        if (role != "ADMIN") {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"Not authorized to access this function"})", "application/json");
            return;
        }
        handler(req, res, ctx);
    });
}