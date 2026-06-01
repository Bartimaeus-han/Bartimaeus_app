#pragma once              // prevents header files from being included multiple times
#include "db_queries.hpp" // Include SQL query header
#include "sqlite3.h"
#include <iostream>
#include <mutex> // Mutual Exclusion
#include <string>
#include <unordered_map>

// simple struct to hold member information
struct User {
    std::string username;
    std::string password; // in this time not encryption
};

class AuthService {
private:
    // Virtual In-Memory DB
    std::unordered_map<std::string, User> user_db; // username : User{username, password}
    std::mutex db_mutex;                           // Simultaneous access control DB in multi-thread env

    sqlite3 *db = nullptr;

public:
    // When Program started, open DB file
    AuthService() {
        // create/connect local file database named server.db
        int rc = sqlite3_open("server.db", &db);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB Error] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        } else {
            std::cout << "[DB Success] Connected to server.db successfully!" << std::endl;
        }

        char *errMsg = nullptr;

        // Create table using the separated constant
        rc = sqlite3_exec(db, Queries::CREATE_USERS_TABLE, nullptr, nullptr, &errMsg);

        if (rc != SQLITE_OK) {
            std::cerr << "[DB Error] Table create failed: " << (errMsg ? errMsg : "unknown") << std::endl;
            sqlite3_free(errMsg); // Free error message memory
        } else {
            std::cout << "[DB Success] 'users' table is ready (checked/created)." << std::endl;
        }
    }

    ~AuthService() {
        if (db) {
            sqlite3_close(db);
            std::cout << "[DB Success] Database connection closed." << std::endl;
        }
    }

    // Secure sign up logic
    bool signUp(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        sqlite3_stmt *check_stmt = nullptr;

        // Prepare secure query for duplicate ID check
        // rc(Return Code)
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_CHECK_USER, -1, &check_stmt, nullptr);
        // SQLITE_OK : sqlite's successfult result
        if (rc != SQLITE_OK) {
            std::cerr << "[SignUp Fail] Prepare check failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // Parameter binding
        // SQLITE_TRANSIENT : tells SQLite to copy the string
        sqlite3_bind_text(check_stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        // Execute query and check for duplicate
        // SQLITE_ROW : return data
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            std::cout << "[SignUp Fail] Duplicate username: " << username << std::endl;
            sqlite3_finalize(check_stmt); // Clean up resource
            return false;
        }
        sqlite3_finalize(check_stmt); // Clean up resource

        // User register phase
        sqlite3_stmt *insert_stmt = nullptr;

        // Prepare secure query for user registration
        rc = sqlite3_prepare_v2(db, Queries::SECURE_INSERT_USER, -1, &insert_stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[SignUp Fail] Prepare insert failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // Parameter Binding
        sqlite3_bind_text(insert_stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insert_stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

        // Execute query and verify insert success
        rc = sqlite3_step(insert_stmt);
        sqlite3_finalize(insert_stmt); // Clean up resource

        // SQLITE_DONE : sql statement step has finished executing
        if (rc != SQLITE_DONE) {
            std::cerr << "[SignUp Fail] Insert execution error: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        std::cout << "[SignUp Success] Registered user: " << username << " (SECURED PATH)" << std::endl;
        return true;
    }

    // 2. Secure sign In logic
    bool signIn(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        sqlite3_stmt *stmt = nullptr;

        // Prepare secure query for login validation
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_SELECT_USER, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Login Error] Prepare query failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // Parameter Binding
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

        bool authenticated = false;

        // Execute query and verify result
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            authenticated = true;
            std::cout << "[Login Success] Authenticated: " << username << std::endl;
        } else {
            std::cout << "[Login Failed] Invalid credentials for: " << username << std::endl;
        }

        sqlite3_finalize(stmt);

        return authenticated;
    }

    // get all registered user list (for admin)
    std::vector<User> getAllUsers() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<User> users;

        const char *query = "SELECT username, password FROM users;";
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                std::string password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
                users.push_back({username, password});
            }
        }

        sqlite3_finalize(stmt);
        return users;
    }
};