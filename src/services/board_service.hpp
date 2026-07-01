#pragma once
#include "db_queries.hpp"
#include "sqlite3.h"

#include <iostream>
#include <mutex>
#include <string>
#include <vector>

// Define post data structure
struct Post {
    int id = 0; // Default to 0 for "not found" detection
    std::string title;
    std::string content;
    std::string author;     // who write this post?
    std::string created_at; // when this post written?
};

class BoardService {
private:
    sqlite3 *db = nullptr;
    std::mutex db_mutex;

public:
    // Connects to database and verifies table
    BoardService() {
        // First, connect to database. if not exist, function create .db file
        int rc = sqlite3_open("server.db", &db);
        // rc : Return Code, success or fail (SQLITE_OK, SQLITE_CNANTOPEN, SQLITE_PERM...)
        if (rc != SQLITE_OK)
            std::cerr << "[DB Error] Cannot open database for board: " << sqlite3_errmsg(db) << std::endl;

        char *errMsg = nullptr;

        rc = sqlite3_exec(db, Queries::CREATE_POSTS_TABLE, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::cerr << "[DB Error] Posts table create failed: " << (errMsg ? errMsg : "unknown") << std::endl;
            sqlite3_free(errMsg);
        } else {
            std::cout << "[DB Success] 'posts' table is ready (checked/created)." << std::endl;
        }
    }

    // In Destructor, close database connection
    ~BoardService() {
        if (db) {
            sqlite3_close(db);
            std::cout << "[DB Success] Board database connection closed." << std::endl;
        }
    }

    // Write a new post
    bool writePost(const std::string &title, const std::string &content, const std::string &author) {
        std::lock_guard<std::mutex> lock(db_mutex); // Lock to prevent race condition

        sqlite3_stmt *stmt = nullptr;

        // Write post using 'Prepared Statements' to block <SQL Injection>
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_INSERT_POST, -1, &stmt, nullptr);

        if (rc != SQLITE_OK) {
            std::cerr << "[Board Error] Prepare insert post failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // Perform data binding
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, author.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return (rc == SQLITE_DONE);
    }

    // Retrieve summary of all posts
    std::vector<Post> getAllPosts() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<Post> posts;
        sqlite3_stmt *stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, Queries::SELECT_ALL_POSTS, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Board Error] Prepare select posts failed: " << sqlite3_errmsg(db) << std::endl;
            return posts;
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            posts.push_back({sqlite3_column_int(stmt, 0),
                             reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),
                             reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)),
                             reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)),
                             reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4))});
        }

        sqlite3_finalize(stmt);

        return posts;
    }

    // Retrieve a specific post details
    Post getPostById(int id) {
        std::lock_guard<std::mutex> lock(db_mutex);
        Post post;
        sqlite3_stmt *stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, Queries::SECURE_SELECT_POST, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Board Error] Prepare select post failed: " << sqlite3_errmsg(db) << std::endl;
            return post;
        }

        sqlite3_bind_int(stmt, 1, id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            post.id = sqlite3_column_int(stmt, 0);
            // sqlite3_column_text return <const unsigned char *>
            // So reinterpret to C++ generally used string type <const char *>
            post.title = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            post.content = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            post.author = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            post.created_at = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        }

        sqlite3_finalize(stmt);

        return post;
    }

    // delete a post
    // This version function has IDOR vulnerable so, need security authentication
    bool deletePost(int id, const std::string &username, const std::string &role) {
        std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_stmt *stmt = nullptr;

        // Apply secure parameterized query
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_DELETE_POST, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Board Error] Prepare delete post failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);

        // Check the number of rows actually delete in the DB
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);

        // Query success & actually delete
        return (rc == SQLITE_DONE && changes > 0);
    }
};