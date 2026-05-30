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
    // When Program started, opne DB file
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

    // sign up logic
    bool signUp(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        std::string check_query = Queries::VULN_CHECK_USER + username + "';";
        sqlite3_stmt *stmt = nullptr;

        int rc = sqlite3_prepare_v2(db, check_query.c_str(), -1, &stmt, nullptr);
        // Duplicated ID Check.
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                std::cout << "[SignUp Fail] Duplicate username: " << username << std::endl;
                sqlite3_finalize(stmt);
                return false;
            }
        }

        // Assemble SQL for user data insertion
        std::string insert_query = Queries::VULN_INSERT_USER + username + "', '" + password + "');";

        char *errMsg = nullptr;
        rc = sqlite3_exec(db, insert_query.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "[SignUp Fail] SQL error: " << (errMsg ? errMsg : "unknown") << std::endl;
            sqlite3_free(errMsg);
            return false;
        }

        std::cout << "[SignUp Success] Registered user: " << username << " (VULNERABLE PATH)" << std::endl;
        return true;
    }

    // 2. Sign In logic
    bool signIn(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        // Assemble SQL for login for login validation
        std::string query = Queries::VULN_SELECT_USER + username + "' AND password = '" + password + "';";
        std::cout << "[Executing Query] " << query << std::endl;

        sqlite3_stmt *stmt = nullptr;
        int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);

        bool authenticated = false;
        // If a row exists, authentication is successful
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                authenticated = true;
                std::cout << "[Login Scuccess] Authenticated: " << username << std::endl;
            } else {
                std::cout << "[Login Fail] Invalid credentials for: " << username << std::endl;
            }
        } else {
            std::cerr << "[Login Error] Prepare failed: " << sqlite3_errmsg(db) << std::endl;
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