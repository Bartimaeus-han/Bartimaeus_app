#pragma once
#include "db_manager.hpp"
#include "db_queries.hpp"
#include <mysql.h>

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
    std::mutex db_mutex;

public:
    // Connects to database and verifies table
    BoardService() {
        std::cout << "[BoardService] initialized with MariaDB DbManager." << std::endl;
    }

    // In Destructor, close database connection
    ~BoardService() {}

    // 1.

    // Write a new post
    bool writePost(const std::string &title, const std::string &content, const std::string &author) {
        // 0. 동시성 제어를 위해서 항상 뮤텍스 락을 걸어주어야 한다.
        std::lock_guard<std::mutex> lock(db_mutex);

        // stmt life cycle
        // Checkout (DbManager -> conn)
        // init -> prepare -> bind&execute -> close
        // Release (conn -> DbManager)

        // 1. 커넥션 풀에서 DB 커넥션을 대여 한 후 에러 발생 여부까지 확인해준다.
        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn) {
            std::cerr << "[Board Error] Could not get database connection." << std::endl;
            return false;
        }

        // 2. init & prepare
        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_INSERT_POST, strlen(Queries::SECURE_INSERT_POST)) != 0) {
            std::cerr << "[Board Error] Prepare insert post failed: " << mysql_error(conn) << std::endl;

            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        // 3. Set Parameter
        MYSQL_BIND bind[3];
        memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void *)title.c_str();
        bind[0].buffer_length = title.length();

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (void *)content.c_str();
        bind[1].buffer_length = content.length();

        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = (void *)author.c_str();
        bind[2].buffer_length = author.length();

        // Binding & Execute
        bool success = true;
        if (mysql_stmt_bind_param(stmt, bind) != 0 || mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[Board Error] Execute insert post failed: " << mysql_stmt_error(stmt) << std::endl;
            success = false;
        }

        // Release
        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);

        return success;
    }

    // Retrieve summary of all posts
    std::vector<Post> getAllPosts() {
        // 0. 동시성 제어
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<Post> posts;

        // stmt life cycle
        // Checkout (DbManager -> conn)
        // init -> prepare -> execute & store_result -> bind_result -> fetch -> close
        // Release (conn -> DbManager)

        // 1. 커넥션 대여
        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn) {
            std::cerr << "[Board Error] Could not get database connection." << std::endl;
            return posts;
        }

        // 2. init & prepare
        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SELECT_ALL_POSTS, strlen(Queries::SELECT_ALL_POSTS)) != 0) {
            std::cerr << "[Board Error] Prepare select posts failed: " << mysql_error(conn) << std::endl;
            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return posts;
        }

        // 3. execute & store_result
        if (mysql_stmt_execute(stmt) != 0 || mysql_stmt_store_result(stmt) != 0) {
            std::cerr << "[Board Error] Execute select posts failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return posts;
        }

        // 4. Set Parameter (Result Bind)
        int res_id = 0;
        char res_title[256] = {0};
        char res_content[4096] = {0};
        char res_author[256] = {0};
        char res_created_at[64] = {0};

        MYSQL_BIND bind[5];
        memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &res_id;

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = res_title;
        bind[1].buffer_length = sizeof(res_title);

        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = res_content;
        bind[2].buffer_length = sizeof(res_content);

        bind[3].buffer_type = MYSQL_TYPE_STRING;
        bind[3].buffer = res_author;
        bind[3].buffer_length = sizeof(res_author);

        bind[4].buffer_type = MYSQL_TYPE_STRING;
        bind[4].buffer = res_created_at;
        bind[4].buffer_length = sizeof(res_created_at);

        // Binding & Fetch
        if (mysql_stmt_bind_result(stmt, bind) == 0) {
            while (mysql_stmt_fetch(stmt) == 0) {
                posts.push_back({res_id, res_title, res_content, res_author, res_created_at});

                memset(res_title, 0, sizeof(res_title));
                memset(res_content, 0, sizeof(res_content));
                memset(res_author, 0, sizeof(res_author));
                memset(res_created_at, 0, sizeof(res_created_at));
            }
        }

        // Release
        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);

        return posts;
    }

    // Retrieve a specific post details
    Post getPostById(int id) {
        // 0. 동시성 제어
        std::lock_guard<std::mutex> lock(db_mutex);
        Post post;

        // stmt life cycle
        // Checkout (DbManager -> conn)
        // init -> prepare -> bind_param -> execute & store_result -> bind_result -> fetch -> close
        // Release (conn -> DbManager)

        // 1. 커넥션 대여
        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn) {
            std::cerr << "[Board Error] Could not get database connection." << std::endl;
            return post;
        }

        // 2. init & prepare
        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_SELECT_POST, strlen(Queries::SECURE_SELECT_POST)) != 0) {
            std::cerr << "[Board Error] Prepare select post failed: " << mysql_error(conn) << std::endl;
            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return post;
        }

        // 3. Set Parameter (Param Bind)
        MYSQL_BIND param_bind[1];
        memset(param_bind, 0, sizeof(param_bind));

        param_bind[0].buffer_type = MYSQL_TYPE_LONG;
        param_bind[0].buffer = (void *)&id;

        if (mysql_stmt_bind_param(stmt, param_bind) != 0 || mysql_stmt_execute(stmt) != 0 || mysql_stmt_store_result(stmt) != 0) {
            std::cerr << "[Board Error] Execute select post failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return post;
        }

        // 4. Set Parameter (Result Bind)
        int res_id = 0;
        char res_title[256] = {0};
        char res_content[4096] = {0};
        char res_author[256] = {0};
        char res_created_at[64] = {0};

        MYSQL_BIND result_bind[5];
        memset(result_bind, 0, sizeof(result_bind));

        result_bind[0].buffer_type = MYSQL_TYPE_LONG;
        result_bind[0].buffer = &res_id;

        result_bind[1].buffer_type = MYSQL_TYPE_STRING;
        result_bind[1].buffer = res_title;
        result_bind[1].buffer_length = sizeof(res_title);

        result_bind[2].buffer_type = MYSQL_TYPE_STRING;
        result_bind[2].buffer = res_content;
        result_bind[2].buffer_length = sizeof(res_content);

        result_bind[3].buffer_type = MYSQL_TYPE_STRING;
        result_bind[3].buffer = res_author;
        result_bind[3].buffer_length = sizeof(res_author);

        result_bind[4].buffer_type = MYSQL_TYPE_STRING;
        result_bind[4].buffer = res_created_at;
        result_bind[4].buffer_length = sizeof(res_created_at);

        // Binding & Fetch
        if (mysql_stmt_bind_result(stmt, result_bind) == 0) {
            if (mysql_stmt_fetch(stmt) == 0) {
                post.id = res_id;
                post.title = res_title;
                post.content = res_content;
                post.author = res_author;
                post.created_at = res_created_at;
            }
        }

        // Release
        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);

        return post;
    }

    // delete a post
    // This version function has IDOR protection with security authentication
    bool deletePost(int id, const std::string &username, const std::string &role) {
        // 0. 동시성 제어
        std::lock_guard<std::mutex> lock(db_mutex);

        // stmt life cycle
        // Checkout (DbManager -> conn)
        // init -> prepare -> bind_param -> execute -> affected_rows -> close
        // Release (conn -> DbManager)

        // 1. 커넥션 대여
        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn) {
            std::cerr << "[Board Error] Could not get database connection." << std::endl;
            return false;
        }

        // 2. init & prepare
        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_DELETE_POST, strlen(Queries::SECURE_DELETE_POST)) != 0) {
            std::cerr << "[Board Error] Prepare delete post failed: " << mysql_error(conn) << std::endl;
            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        // 3. Set Parameter
        MYSQL_BIND bind[3];
        memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = (void *)&id;

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = (void *)username.c_str();
        bind[1].buffer_length = username.length();

        bind[2].buffer_type = MYSQL_TYPE_STRING;
        bind[2].buffer = (void *)role.c_str();
        bind[2].buffer_length = role.length();

        // Binding & Execute
        bool success = false;
        if (mysql_stmt_bind_param(stmt, bind) == 0 && mysql_stmt_execute(stmt) == 0) {
            // 삭제된 행의 수가 1개 이상인지 확인 (Check affected rows)
            my_ulonglong affected_rows = mysql_stmt_affected_rows(stmt);
            if (affected_rows > 0) {
                success = true;
            }
        } else {
            std::cerr << "[Board Error] Execute delete post failed: " << mysql_stmt_error(stmt) << std::endl;
        }

        // Release
        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);

        return success;
    }
};