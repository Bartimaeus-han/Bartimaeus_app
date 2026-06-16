#pragma once
#include "../helpers.hpp"
#include "../middleware.hpp" // For using session information structs (Middleware header)
#include "../services/board_service.hpp"
#include <httplib.h>
#include <iostream>
#include <string>

class BoardController {
private:
    BoardService &boardService;

public:
    // Dependency Injection
    explicit BoardController(BoardService &service) : boardService(service) {}

    // Handle request to write a new post
    void handleCreatePost(const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        std::string username = ctx.username;

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
    void handleGetPosts(const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        std::string username = ctx.username;

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
    void handleGetPostDetail(const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        std::string username = ctx.username;

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
    void handleDeletePost(const httplib::Request &req, httplib::Response &res, const UserContext &ctx) {
        std::string username = ctx.username;

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