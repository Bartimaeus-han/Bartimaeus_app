#pragma once              // prevents header files from being included multiple times
#include "db_manager.hpp" // for MySQL connection pool management
#include "db_queries.hpp" // Include SQL query header
#include "field_types.h"
#include "helpers.hpp"
#include <mysql.h>
#include "picosha2.h" // Include SHA-256 hashing library

#include <cctype> // Including character classification functions for hex validation (std::isxdigit)
#include <cstring>
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
        std::cout << "[AuthService] initialized with MySQL DbManager." << std::endl;
    }

    ~AuthService() {}

    // 아래 기능 함수들의 전체적인 순서는 아래와 같다
    // 1. 연결 획득 (Get Connection)
    // 2. 바인딩(인자 매칭) & 쿼리 실행
    // 3. Result Binding
    // 4. 결과 패치

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
        return true;
    }

    // 2. Secure log in logic
    bool login(const std::string &username, const std::string &password) {
        // 1. Get Connection
        // 1-1. Using mutex for safe(Concurrency Control) connection
        std::lock_guard<std::mutex> lock(db_mutex);

        // 1-2. get connection instance
        MYSQL *conn = DbManager::getInstance().getConnection();
        // 1-3. check connection instance is valiable
        if (!conn)
            return false;

        // 1-4. initialize the stmt using conn instance
        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        // 1-5. check stmt, and prepare the stmt(both check)
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_SELECT_USER, strlen(Queries::SECURE_SELECT_USER)) != 0) {
            std::cerr << "[Login Error] Prepare query failed: " << mysql_error(conn) << std::endl;
            // 1-5-1. if stmt is connected, close before end.
            if (stmt)
                mysql_stmt_close(stmt);
            // 1-5-2.
            DbManager::getInstance().releaseConnection(conn);

            return false;
        }

        // 2. Binding input parameter
        // SELECT password, role FROM users WHERE username = ?;
        // 2-1. make bind list store input parameter
        MYSQL_BIND bind[1];
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void *)username.c_str();
        bind[0].buffer_length = username.length();

        // 2-2. binding parameter to query and execute (check to)
        if (mysql_stmt_bind_param(stmt, bind) || mysql_stmt_execute(stmt)) {
            std::cerr << "[Login Error] Execute failed: " << mysql_stmt_error(stmt) << std::endl;
            // 2-2-... close stmt and release connection (ending sequence)
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        // 3.Result Binding
        char db_password_hash[255] = {0};
        char db_role[50] = {0};
        unsigned long hash_len, role_len;

        MYSQL_BIND result_bind[2];
        memset(result_bind, 0, sizeof(result_bind));

        result_bind[0].buffer_type = MYSQL_TYPE_STRING;
        result_bind[0].buffer = db_password_hash;
        result_bind[0].buffer_length = sizeof(db_password_hash);
        result_bind[0].length = &hash_len; // write actual length of return data(hash password)

        result_bind[1].buffer_type = MYSQL_TYPE_STRING;
        result_bind[1].buffer = db_role;
        result_bind[1].buffer_length = sizeof(db_role);
        result_bind[1].length = &role_len;

        if (mysql_stmt_bind_result(stmt, result_bind)) {
            std::cerr << "[Login Error] Bind result failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        // 4. Fetch Result & Verify Password
        int fetch_res = mysql_stmt_fetch(stmt);

        if (fetch_res != 0) {
            std::cout << "[Login Fail] User not found: " << username << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return false;
        }

        bool is_valid = verifyPasswordArgon2id(password, db_password_hash);

        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);

        if (is_valid) {
            std::cout << "[Login Success] Welcome, " << username << "!" << std::endl;
            return true;
        } else {
            std::cout << "[Login Fail] Invalid password for user: " << username << std::endl;
            return false;
        }
    }

    // 3. Get user role logic
    std::string getUserRole(const std::string &username) {
        // 1. Get Connection
        std::lock_guard<std::mutex> lock(db_mutex);

        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn)
            return "USER";

        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_SELECT_USER_ROLE, strlen(Queries::SECURE_SELECT_USER_ROLE)) != 0) {
            std::cerr << "[GetUserRole Error] Prepare query failed: " << mysql_error(conn) << std::endl;
            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return "USER";
        }

        // 2. Binding input parameter
        MYSQL_BIND bind[1];
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = (void *)username.c_str();
        bind[0].buffer_length = username.length();

        if (mysql_stmt_bind_param(stmt, bind) || mysql_stmt_execute(stmt)) {
            std::cerr << "[GetUserRole Error] Execute failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return "USER";
        }

        // 3. Result Binding
        char db_role[50] = {0};
        unsigned long role_len = 0;

        MYSQL_BIND result_bind[1];
        memset(result_bind, 0, sizeof(result_bind));

        result_bind[0].buffer_type = MYSQL_TYPE_STRING;
        result_bind[0].buffer = db_role;
        result_bind[0].buffer_length = sizeof(db_role);
        result_bind[0].length = &role_len;

        if (mysql_stmt_bind_result(stmt, result_bind)) {
            std::cerr << "[GetUserRole Error] Bind result failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return "USER";
        }

        // 4. Fetch Result & Return Role
        int fetch_res = mysql_stmt_fetch(stmt);
        std::string role = "USER";

        if (fetch_res == 0) {
            role = std::string(db_role, role_len);
        } else {
            std::cout << "[GetUserRole Fail] User not found: " << username << std::endl;
        }

        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);
        return role;
    }

    // 4. Get all registered user list (for admin)
    std::vector<User> getAllUsers() {
        // 1. Get Connection
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<User> users;

        MYSQL *conn = DbManager::getInstance().getConnection();
        if (!conn)
            return users;

        MYSQL_STMT *stmt = mysql_stmt_init(conn);
        if (!stmt || mysql_stmt_prepare(stmt, Queries::SECURE_SELECT_ALL_USERS, strlen(Queries::SECURE_SELECT_ALL_USERS)) != 0) {
            std::cerr << "[GetAllUsers Error] Prepare query failed: " << mysql_error(conn) << std::endl;
            if (stmt)
                mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return users;
        }

        // 2. Execute Query
        if (mysql_stmt_execute(stmt) != 0) {
            std::cerr << "[GetAllUsers Error] Execute failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return users;
        }

        // 3. Result Binding
        char db_username[255] = {0};
        char db_role[50] = {0};
        unsigned long name_len = 0, role_len = 0;

        MYSQL_BIND result_bind[2];
        memset(result_bind, 0, sizeof(result_bind));

        result_bind[0].buffer_type = MYSQL_TYPE_STRING;
        result_bind[0].buffer = db_username;
        result_bind[0].buffer_length = sizeof(db_username);
        result_bind[0].length = &name_len;

        result_bind[1].buffer_type = MYSQL_TYPE_STRING;
        result_bind[1].buffer = db_role;
        result_bind[1].buffer_length = sizeof(db_role);
        result_bind[1].length = &role_len;

        if (mysql_stmt_bind_result(stmt, result_bind) != 0) {
            std::cerr << "[GetAllUsers Error] Bind result failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            DbManager::getInstance().releaseConnection(conn);
            return users;
        }

        // 4. Store Result & Fetch Rows in Loop
        mysql_stmt_store_result(stmt);

        while (mysql_stmt_fetch(stmt) == 0) {
            User u;
            u.username = std::string(db_username, name_len);
            u.role = std::string(db_role, role_len);
            users.push_back(u);

            memset(db_username, 0, sizeof(db_username));
            memset(db_role, 0, sizeof(db_role));
        }

        mysql_stmt_close(stmt);
        DbManager::getInstance().releaseConnection(conn);
        return users;
    }
};