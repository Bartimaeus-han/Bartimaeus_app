#pragma once
#include "../helpers.hpp"
#include "../services/board_service.hpp"
#include "../services/session_manager.hpp"
#include <httplib.h>
#include <iostream>
#include <string>

class BoardController {
private:
    BoardService &boardService;
    SessionManager &sessionManager;

public:
    // Dependency Injection
    BoardController(BoardService &service, SessionManager &manager) : boardService(service), sessionManager(manager) {}

    // Handle request to write a new post
    void handleCreatePost(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");
        std::string username = sessionManager.validateSession(session_id);

        // Restrict access for unathenticated users
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"status":"error", "message":"Unauthorized"})", "application/json");
            return;
        }

        // Perform X-CSRF-TOKEN header validation => CSRF Attack
        std::string csrf_token_header = req.get_header_value("X-CSRF-TOKEN");
        if (!sessionManager.validateCsrfToekn(session_id, csrf_token_header)) {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"Invalid CSRF token"})", "application/json");
            return;
        }

        // Parse title and content from request body
        auto title = req.get_param_value("title");
        auto content = req.get_param_value("content");

        if (title.empty() || content.empty()) {
            res.status = 400; // Bad Request
            res.set_content(R"({"status":"error", "message":"Title and content cannot be empty"})", "application/json");
            return;
        }

        // Create Post -> Insert DB
        if (boardService.writePost(title, content, username)) {
            std::cout << "[Board Success] User '" << username << "' created a post: '" << title << "'" << std::endl; // Log if writing is success.
            res.status = 201;                                                                                        // OK
            res.set_content(R"({"status":"success", "message":"Post created successfully"})", "application/json");
        } else {
            res.status = 500; // Internal Server Error
            res.set_content(R"({"status":"error", "message":"Failed to create post"})", "application/json");
        }
    }

    // 'API' to retrieve post summary list
    void handleGetPosts(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");
        std::string username = sessionManager.validateSession(session_id);

        if (username.empty()) {
            res.status = 401; // Unauthorized
            res.set_content(R"({"status":"error", "message":"Unauthorized"})", "application/json");
            return;
        }

        std::vector<Post> posts = boardService.getAllPosts();
        // Log to retrieve all posts successfully
        std::cout << "[Board Success] User '" << username << "' retrieved all posts (" << posts.size() << " posts found)." << std::endl;

        std::string json = "[";

        for (size_t i = 0; i < posts.size(); i++) {
            json += "{\"id\":" + std::to_string(posts[i].id) +
                    ", \"title\":\"" + escapeJson(posts[i].title) + "\"" +
                    ", \"content\":\"" + escapeJson(posts[i].content) + "\"" +
                    ", \"author\":\"" + escapeJson(posts[i].author) + "\"" +
                    "}";
            if (i < posts.size() - 1) {
                json += ",";
            }
        }
        json += "]";

        res.status = 200; // OK
        res.set_content(json, "application/json");
    }

    // API to retrieve a specific post 'details'
    void handleGetPostDetail(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");
        std::string username = sessionManager.validateSession(session_id);

        // username(ID) check
        if (username.empty()) {
            res.status = 401; // Unauthorized
            res.set_content(R"({"status":"error", "message":"Unauthorized"})", "application/json");
            return;
        }

        // Post's ID check
        std::string id_str = req.get_param_value("id");
        if (id_str.empty()) {
            res.status = 400; // Bad Request
            res.set_content(R"({"status":"error", "message":"Post ID is required"})", "application/json");
            return;
        }

        int id = std::stoi(id_str);
        Post post = boardService.getPostById(id);

        if (post.id == 0) {
            res.status = 404; // Not Found
            res.set_content(R"({"status":"error", "message":"Post not found"})", "application/json");
            return;
        }

        // Log to retrieve a specific post
        std::cout << "[Board Success] User '" << username << "' read post ID: " << id << " ('" << post.title << "')" << std::endl;

        std::string json = "{\"id\":" + std::to_string(post.id) +
                           ", \"title\":\"" + escapeJson(post.title) + "\"" +
                           ", \"content\":\"" + escapeJson(post.content) + "\"" +
                           ", \"author\":\"" + escapeJson(post.author) + "\"" +
                           "}";
        res.status = 200; // OK
        res.set_content(json, "application/json");
    }

    // API to handle post deletion request
    // Defense CSRF to check CSRF token
    // IDOR vulnerability - don't check that delete person and author is same user
    void handleDeletePost(const httplib::Request &req, httplib::Response &res) {
        std::string cookie_header = req.get_header_value("Cookie");
        std::string session_id = getCookieValue(cookie_header, "auth_session");
        std::string username = sessionManager.validateSession(session_id);

        // username(ID) check
        if (username.empty()) {
            res.status = 401; // Unauthorized
            res.set_content(R"({"status":"error", "message":"Unauthorized"})", "application/json");
            return;
        }

        // Validate CSRF token on delete method
        std::string csrf_token_header = req.get_header_value("X-CSRF-TOKEN");
        if (!sessionManager.validateCsrfToekn(session_id, csrf_token_header)) {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"Invalid CSRF token"})", "application/json");
            return;
        }

        auto id_str = req.get_param_value("id");
        if (id_str.empty()) {
            res.status = 400; // Bad Request
            res.set_content(R"({"status":"error", "message":"Post ID is required"})", "application/json");
            return;
        }

        int id = std::stoi(id_str);

        // Check post is really exist
        Post post = boardService.getPostById(id);

        if (post.id == 0) {
            res.status = 404; // Page Not Found
            res.set_content(R"({"status":"error", "message":"Post not found"})", "application/json");
            return;
        }

        // Authorization.  username == post's author??
        if (post.author != username) {
            res.status = 403; // Forbidden
            res.set_content(R"({"status":"error", "message":"You are not authorized to delete this post"})", "application/json");
            return;
        }

        if (boardService.deletePost(id)) {
            // Log to delete post successfully
            std::cout << "[Board Success] User '" << username << "' deleted post ID: " << id << std::endl;
            res.status = 200; // OK
            res.set_content(R"({"status":"success", "message":"Post deleted successfully"})", "application/json");
        } else {
            res.status = 500; // Internal Server Error
            res.set_content(R"({"status":"error", "message":"Failed to delete post"})", "application/json");
        }
    }
};