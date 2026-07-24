#pragma once              // prevents header files from being included multiple times
#include "db_manager.hpp" // for MySQL connection pool management
#include "db_queries.hpp" // Include SQL query header
#include "helpers.hpp"
#include "picosha2.h" // Include SHA-256 hashing library

#include <cctype> // Including character classification functions for hex validation (std::isxdigit)
#include <iostream>
#include <mutex> // Mutual Exclusion
#include <string>
#include <unordered_map>
#include <vector> // Using dynamic vector

// simple struct to hold member information
struct User {
    std::string username;
    std::string password; // in this time not encryption
    std::string role;     // role field
};

// RAII Guard
struct DbConnectionGuard {
    MYSQL *conn = nullptr;
    DbConnectionGuard() {
        conn = DbManager::getInstance().getConnection();
    }
    ~DbConnectionGuard() {
        if (conn) {
            DbManager::getInstance().releaseConnection(conn);
        }
    }
};

class AuthService {
private:
    std::unordered_map<std::string, User> user_db; // Virtual In-Memory DB
    std::mutex db_mutex;                           // Mutual  exclusion for concurent access

public:
    // When Program started, open DB file
    AuthService() {
        std::cout < "[AuthService] initialized with MySQL DbManager." << std::endl;
    }

    ~AuthService() {}

    // Secure sign up logic
    bool signUp(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        // 1. Borrow DB connection from pool
        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn) {
            std::cerr << "[SignUp Fail] Could not get database connection. " << std::endl;
            return false;
        }

        // 2. Check duplicate username using MySQL Prepared Statement
        MYSQL_STMT *check_stmt = mysql_stmt_init(conn);
        if (!check_stmt || mysql_stmt_prepare(check_stmt, Queries::SECURE_CHECK_USER, strlen(Queries::SECURE_CHECK_USER)) != 0) {
            std::cerr << "[SignUp Fail] Prepare check failed: " << mysql_error(conn) << std::endl;

            if (check_stmt)
                mysql_stmt_close(check_stmt);

            DbManager::getInstance().releaseConnection(conn); // Release connection
            return false;
        }

        // check_stmt에 바인딩 해줄 데이터를 만드는 곳
        MYSQL_BIND check_bind[1];
        memset(check_bind, 0, sizeof(check_bind));
        check_bind[0].buffer_type = MYSQL_TYPE_STRING;
        check_bind[0].buffer = (void *)username.c_str();
        check_bind[0].buffer_length = username.length();

        // 만약 바인딩에 실패했거나 쿼리 실행에 실패한 경우
        if (mysql_stmt_bind_param(check_stmt, check_bind) != 0 || mysql_stmt_execute(check_stmt) != 0) {
            std::cerr << "[SignUp Fail] Execute check failed: " << mysql_stmt_error(check_stmt) << std::endl;

            mysql_stmt_close(check_stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        // 쿼리가 실행된 결과셋을 저장하는 기능 (쿼리가 실행 되어도 일단은 스트림에 있지 메모리에 바로 저장되지는 않는다)
        mysql_stmt_store_result(check_stmt);
        if (mysql_stmt_num_rows(check_stmt) > 0) {
            std::cout << "[SignUp Fail] Duplicate username: " << username << std::endl;
            mysql_stmt_close(check_stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }
        // check_stmt를 해제한다.
        // 왜 해제하나요? -> 이 stmt는 유저 중복 확인만을 위해 생성 되었기 때문이다.
        mysql_stmt_close(check_stmt);

        // 3. 회원가입 레코드 삽입
        MYSQL_STMT *insert_stmt = mysql_stmt_init(conn);
        if (!insert_stmt || mysql_stmt_prepare(insert_stmt, Queries::SECURE_INSERT_USER, strlen(Queries::SECURE_INSERT_USER)) != 0) {
            std::cerr << "[SignUp Fail] Prepare insert failed: " << mysql_error(conn) << std::endl;
            if (insert_stmt)
                mysql_stmt_close(insert_stmt);

            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        std::string hashed_password = hashPasswordArgon2id(password);

        MYSQL_BIND insert_bind[2];
        memset(insert_bind, 0, sizeof(insert_bind)); // 0으로 채우기

        insert_bind[0].buffer_type = MYSQL_TYPE_STRING;
        insert_bind[0].buffer = (void *)username.c_str();
        insert_bind[0].buffer_length = username.length();

        insert_bind[1].buffer_type = MYSQL_TYPE_STRING;
        insert_bind[1].buffer = (void *)hashed_password.c_str();
        insert_bind[1].buffer_length = hashed_password.length();

        if (mysql_stmt_bind_param(insert_stmt, insert_bind) != 0 || mysql_stmt_execute(insert_stmt) != 0) {
            std::cerr << "[SignUp Fail] Insert execution error: " << mysql_stmt_error(insert_stmt) << std::endl;

            mysql_stmt_close(insert_stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        mysql_stmt_close(insert_stmt);
        DbManager::getInstance().releaseConnection(conn);

        mysql_stmt_close(insert_stmt);
        DbManager::getInstance().releaseConnection(conn);

        mysql_stmt_close(check_stmt);
    }

    // 2. Secure log in logic
    bool login(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        sqlite3_stmt *stmt = nullptr;

        // Prepare secure query for login validation
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_SELECT_USER, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[Login Error] Prepare query failed: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }

        // Parameter Binding (only username)
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        bool authenticated = false;
        bool needs_migration = false; // Flag for migration target

        // Execute query and verify result
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            // Get salt and stored password from DB
            std::string db_password_hash = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string db_salt = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));

            // 1. Check if the stored hash is Argon2id format
            if (db_password_hash.find("$argon2id$", 0) == 0) {
                if (verifyPasswordArgon2id(password, db_password_hash)) {
                    authenticated = true;
                    std::cout << "[Login Success] Authenticated via Argon2id: " << username << std::endl;
                } else {
                    std::cout << "[Login Failed] Password mismatch (Argon2id) for: " << username << std::endl;
                }
            }

            // 2. Verify with legacy SHA-256 if not Argon2id
            else {
                std::string stretched_input = stretchPasswordSHA256(password, db_salt);
                if (db_password_hash == stretched_input) {
                    authenticated = true;
                    needs_migration = true;
                    std::cout << "[Login Success] Authenticated via Legacy SHA_256: " << username << std::endl;
                } else {
                    std::cout << "[Login Failed] Password mismatch (SHA-256) for: " << username << std::endl;
                }
            }

        } else {
            std::cout << "[Login Failed] Invalid credentials for: " << username << std::endl;
        }

        sqlite3_finalize(stmt); // Clean up select statement

        // 3. Execute real-time migration for legacy user on successful login
        if (authenticated && needs_migration) {
            sqlite3_stmt *update_stmt = nullptr;
            int prepare_rc = sqlite3_prepare_v2(db, Queries::SECURE_UPDATE_USER_PASSWORD, -1, &update_stmt, nullptr);

            if (prepare_rc == SQLITE_OK) {
                // Generate new Argon2id hash using plain password
                std::string new_hash = hashPasswordArgon2id(password);
                std::string new_salt = "argon2id"; // salt column - until when?

                sqlite3_bind_text(update_stmt, 1, new_hash.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 2, new_salt.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(update_stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

                if (sqlite3_step(update_stmt) == SQLITE_DONE) {
                    std::cout << "[Migration Success] Legacy user '" << username << "' has been successfully upgraded to Argon2id!" << std::endl;
                } else {
                    std::cerr << "[Migration Fail] Failed to update password to DB: " << sqlite3_errmsg(db) << std::endl;
                }

                sqlite3_finalize(update_stmt);
            } else {
                std::cerr << "[Migration Fail] Prepare update statement failed: " << sqlite3_errmsg(db) << std::endl;
            }
        }

        return authenticated;
    }

    std::string getUserRole(const std::string &username) {
        std::lock_guard<std::mutex> lock(db_mutex);

        sqlite3_stmt *stmt = nullptr;

        // Prepare query for role lookup
        int rc = sqlite3_prepare_v2(db, Queries::SECURE_SELECT_USER_ROLE, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[DB Error] Prepare select user role failed: " << sqlite3_errmsg(db) << std::endl;
            return "";
        }

        // Parameter binding
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

        std::string role = "";

        // Execute query and retrieve result
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *val = sqlite3_column_text(stmt, 0);
            if (val) {
                role = reinterpret_cast<const char *>(val);
            }
        }

        sqlite3_finalize(stmt);
        return role;
    }

    // get all registered user list (for admin)
    std::vector<User> getAllUsers() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<User> users;

        const char *query = "SELECT username FROM users;";
        sqlite3_stmt *stmt = nullptr;

        if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
                users.push_back({username, ""});
            }
        }

        sqlite3_finalize(stmt);
        return users;
    }
};