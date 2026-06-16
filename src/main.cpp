#include "controllers/auth_controller.hpp"
#include "controllers/board_controller.hpp"
#include "helpers.hpp"    // Helper functions
#include "middleware.hpp" // middleware helper header
#include "services/auth_service.hpp"
#include "services/board_service.hpp"
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

    // Global error handler
    svr.set_error_handler([](const httplib::Request &req, httplib::Response &res) {
        std::string tracking_id = generateErrorTrackingId();

        // Set detailed error description for internal logging
        std::string error_details = "HTTP Client Error";
        if (res.status == 404) {
            error_details = "Resource not found on path: " + req.path;
        } else if (res.status == 403) {
            error_details = "Access Denied / Forbidden";
        } else if (res.status == 500) {
            error_details = "Internal Server Error";
        }

        // Dual loggin to console and file(error.log)
        logErrorToConsoleAndFile(tracking_id, res.status, req.path, req.method, error_details);

        // Check if it is an API request
        // If api, return safe JSON response
        bool is_api_request = req.path.rfind("/api", 0) == 0;

        if (is_api_request) {
            std::string json_res = R"({"status":"error", "message":"An unexpected error occurred.", "tracking_id":")" + tracking_id + R"("})";
            if (res.status == 404) {
                json_res = R"({"status":"error", "message":"Resource not found", "tracking_id":")" + tracking_id + R"("})";
            } else if (res.status == 403) {
                json_res = R"({"status":"error", "message":"Access forbidden", "tracking_id":")" + tracking_id + R"("})";
            } else if (res.status == 500) {
                json_res = R"({"status":"error", "message":"Internal server error", "tracking_id":")" + tracking_id + R"("})";
            }
            res.set_content(json_res, "application/json; charset=utf-8");
        } else {
            // Load template error HTML
            std::string html = readFileToString("./public/error.html");

            // If faile to load file, fallback message
            if (html.empty()) {
                res.set_content("An error occurred. Tracking ID: " + tracking_id, "text/plain; charset=utf-8");
                return;
            }

            std::string title = "An Error Occurred";
            std::string desc = "An unexpected error occured while processing your request.";

            if (res.status == 404) {
                title = "Page Not Found (404)";
                desc = "The requested page does not exist or has been moved.";
            } else if (res.status == 403) {
                title = "Access Denied (403)";
                desc = "You do not have permission to access this resource.";
            } else if (res.status == 500) {
                title = "Internal Server Error (500)";
                desc = "An internal error occurred on the server. Please try again later.";
            }

            // Replacement placeholder
            html = replacePlaceholder(html, "{{TITLE}}", title);
            html = replacePlaceholder(html, "{{DESCRIPTION}}", desc);
            html = replacePlaceholder(html, "{{TRACKING_ID}}", tracking_id);
            res.set_content(html, "text/html; charset=utf-8");
        }
    });

    // Receive SIGINT(Ctrl+C), run `handle_signal` function
    std::signal(SIGINT, handle_signal);

    AuthService auth_service;
    SessionManager session_manager;
    LoginLimiter login_limiter;

    AuthController auth_controller(auth_service, session_manager, login_limiter);

    BoardService board_service;                      // Create board service instance
    BoardController board_controller(board_service); // Cretae board controller instance

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

    // ===== Board related API =====
    // csrf validation is used for POST, DELETE method requests only

    // Write a post API route
    svr.Post("/api/post", requireAuthAndCsrf(session_manager, [&board_controller](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
                 board_controller.handleCreatePost(req, res, ctx);
             }));

    // Retrieve all posts API route
    svr.Get("/api/posts", requireAuth(session_manager, [&board_controller](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
                board_controller.handleGetPosts(req, res, ctx);
            }));

    // Retrieve a specific post detais API route
    svr.Get("/api/post", requireAuth(session_manager, [&board_controller](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
                board_controller.handleGetPostDetail(req, res, ctx);
            }));

    // Delete a post API route
    svr.Delete("/api/post", requireAuthAndCsrf(session_manager, [&board_controller](const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
                   board_controller.handleDeletePost(req, res, ctx);
               }));

    std::cout
        << "========================================================" << std::endl;
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