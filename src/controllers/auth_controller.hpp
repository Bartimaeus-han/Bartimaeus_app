// A controller that receives HTTP request parameters and formats the response.
#pragma once
#include "../services/auth_service.hpp"
#include <httplib.h>

class AuthController {
private:
    AuthService &auth_service;

public:
    // explicit : Completely block "Implict Conversion"
    explicit AuthController(AuthService &service) : auth_service(service) {}

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

        if (auth_service.signIn(username, password)) {
            res.status = 200;

            // Vulnerable Login authentication - plain text cookie
            res.set_header("Set-Cookie", "auth_session=" + username + ";Path=/;");
            res.set_content(R"({"status":"success", "message":"Login successful"})", "application/json");
        } else {
            res.status = 401;
            res.set_content(R"({"status":"error", "message":"Invalid username or password"})", "application/json");
        }
    }

    // Admin API handler for retrieving the list of registered users as JSON
    void handleGetUsers(const httplib::Request &req, httplib::Response &res) {
        // Extract cookie in the request header
        std::string cookie = req.get_header_value("Cookie");

        // Check if auth_session=admin in the cookie
        if (cookie.find("auth_session=admin") == std::string::npos) {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"Access denied. Administrator privilege required."})", "application/json");
            return;
        }

        // Upon successful authentication, retrieve all user information and process it into JSON format
        auto users = auth_service.getAllUsers();
        std::string json_result = "[";

        for (size_t i = 0; i < users.size(); ++i) {
            // R"({"username":")" => R"(       )" + {"username":"
            // R"(","password":")" => R"(       )" + ","password":"
            // R"("})" => R"(       )" + "}
            // ==> {"username":"users[i].username","password":"users[i].password"}
            json_result += R"({"username":")" + users[i].username + R"(","password":")" + users[i].password + R"("})";

            if (i < users.size() - 1) {
                json_result += ",";
            }
        }
        json_result += "]";

        res.status = 200;
        res.set_content(json_result, "application/json");
    }
};