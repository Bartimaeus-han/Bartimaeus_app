#include "controllers/auth_controller.hpp"
#include "services/auth_service.hpp"
#include "services/login_limiter.hpp"   // Login limiter header
#include "services/session_manager.hpp" // Session manager header
#include <csignal>                      // for signal processing
#include <httplib.h>
#include <iostream>

// to allow the signal handler function to access the server object
httplib::Server *global_svr = nullptr;

// When Ctrl + C, funciton to execute
void handle_signal(int signal) {
    std::cout << "\n[Signal] Signal (" << signal << ") received. Stopping server gracefully..." << std::endl;
    if (global_svr) {
        global_svr->stop();
    }
}

// Helper for cookie parsing
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

int main() {
    // Initialize HTTPS server by setting paths to self-signed certificate and private key files (/certs/cert.pem&key.pem)
    httplib::SSLServer svr("./certs/cert.pem", "./certs/key.pem");
    global_svr = &svr; // Register current server address in the global pointer

    // CSP Option
    // default-src 'self' : Basic resources such as images and fonts are allowed to be fetched only from the current server origin
    // script-src 'self' : Restrict JS to load and execute only files that exist statically on the server (same domain)
    //  => Inline script or Inline event handler attack is restricted at browser level
    //     - <script>alert(1)</script>       - <img src=x onerror=alert(1)>
    // style-src 'self' : CSS stylesheet allow same domain resource and inline style in tag
    svr.set_default_headers({{"Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'"},
                             {"X-Content-Type-Options", "nosniff"},
                             {"X-Frame-Options", "DENY"},
                             {"X-XSS-Protection", "1; mode=block"}});

    // Receive SIGINT(Ctrl+C), run `handle_signal` function
    std::signal(SIGINT, handle_signal);

    AuthService auth_service;
    SessionManager session_manager;
    LoginLimiter login_limiter;

    AuthController auth_controller(auth_service, session_manager, login_limiter);

    svr.set_pre_routing_handler([&session_manager](const httplib::Request &req, httplib::Response &res) {
        if (req.path == "/" || req.path == "/index.html") {
            bool is_logged_in = false;

            // Session validation phase
            if (req.has_header("Cookie")) {
                std::string cookie = req.get_header_value("Cookie");
                std::string session_id = getCookieValue(cookie, "auth_session");

                // if session is exist
                if (!session_manager.validateSession(session_id).empty()) {
                    is_logged_in = true;
                }
            }

            if (is_logged_in) {
                // Guide to 'index.html' only if entering via root(/)
                if (req.path == "/") {
                    res.set_redirect("/index.html");
                    return httplib::Server::HandlerResponse::Handled;
                }
                // If already requesting index.html, bypass control and pass to static handler
                return httplib::Server::HandlerResponse::Unhandled;
            } else {
                res.set_redirect("/login.html");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Mount the static directory after the root handler
    svr.set_mount_point("/", "./public");

    // Sign Up Route (POST)
    svr.Post("/signup", [&auth_controller](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleSignUp(req, res);
    });

    // Log In Route (POST)
    svr.Post("/login", [&auth_controller](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleLogin(req, res);
    });

    // Member inquiry (GET /api/users)
    svr.Get("/api/users", [&auth_controller](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleGetUsers(req, res);
    });

    // Current session inquiry API route
    svr.Get("/api/me", [&auth_controller](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleGetMe(req, res);
    });

    // Logout API route
    svr.Post("/logout", [&auth_controller](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleLogout(req, res);
    });

    std::cout << "========================================================" << std::endl;
    std::cout << " Secure Web Server is starting on https://localhost:9090" << std::endl;
    std::cout << "========================================================" << std::endl;

    if (!svr.listen("0.0.0.0", 9090)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }

    // When server loop stopped, code flow under the this line
    std::cout << "Web Server stopped safely." << std::endl;

    return 0; // in this point, every destructor run
}