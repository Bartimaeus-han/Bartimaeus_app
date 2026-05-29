#include "controllers/auth_controller.hpp"
#include "services/auth_service.hpp"
#include <csignal> // for signal processing
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
    httplib::Server svr;
    global_svr = &svr; // Register current server address in the global pointer

    // Receive SIGINT(Ctrl+C), run `handle_signal` function
    std::signal(SIGINT, handle_signal);
    svr.set_mount_point("/", "./public");

    AuthService auth_service;
    AuthController auth_controller(auth_service);

    // Default Page
    svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("Hello! This is a Vulnerable Secure Web Server.", "text/plaing; charset=UTF-8");
    });

    // Sign Up Route (POST)
    svr.Post("/signup", [&](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleSignUp(req, res);
    });

    // Log In Route (POST)
    svr.Post("/login", [&](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleLogin(req, res);
    });

    // Member inquiry (GET /api/users)
    svr.Get("/api/users", [&](const httplib::Request &req, httplib::Response &res) {
        auth_controller.handleGetUsers(req, res);
    });

    std::cout << "================================================" << std::endl;
    std::cout << " Web Server is starting on http://localhost:8080" << std::endl;
    std::cout << "================================================" << std::endl;

    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }

    // When server loop stopped, code flow under the this line
    std::cout << "Web Server stopped safely." << std::endl;

    return 0; // in this point, every destructor run
}