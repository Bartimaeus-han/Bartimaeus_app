#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <mysql.h>
#include <string>
#include <thread>
#include <vector>

class DbManager {
private:
    std::string host;
    std::string user;
    std::string password;
    std::string dbname;
    unsigned int port;

    std::vector<MYSQL *> connection_pool; // List of available connection objects
    std::mutex pool_mutex;                // For pool access control
    std::condition_variable pool_cv;      // For connection waiting
    size_t pool_size = 5;

    // Constructor
    DbManager() {
        // Loads env varibales and initializes pool
        const char *env_host = std::getenv("DB_HOST");
        host = env_host ? env_host : "127.0.0.1";

        const char *env_user = std::getenv("DB_USER");
        user = env_user ? env_user : "root";

        const char *env_pass = std::getenv("DB_PASSWORD");
        password = env_pass ? env_pass : "rootpassword";

        const char *env_db = std::getenv("DB_NAME");
        dbname = env_db ? env_db : "bartimaeus_db";

        const char *env_port = std::getenv("DB_PORT");
        port = env_port ? std::atoi(env_port) : 3306;

        initializePool();
    }

    ~DbManager() {
        destroyPool();
    }

    void initializePool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        int retry_count = 0;
        const int max_retries = 10;        // Maximum connection retries
        const int retry_delay_seconds = 2; // Retry delay interval

        while (connection_pool.size() < pool_size && retry_count < max_retries) {
            MYSQL *conn = createConnection();

            if (conn)
                connection_pool.push_back(conn);
            else {
                retry_count++;
                std::cout << "[DB Info] Retrying database connection in " << retry_delay_seconds << " seconds... (" << retry_count << "/" << max_retries << ")" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
            }
        }

        if (connection_pool.empty()) {
            std::cerr << "[DB Fatal] Could not connect to database after maximum retries." << std::endl;
        } else {
            std::cout << "[DB Success] Connection pool initialized with " << connection_pool.size() << " connections." << std::endl;
        }
    }

    // Create a single MySQL connection instance
    MYSQL *createConnection() {
        MYSQL *conn = mysql_init(nullptr); // can memory leakage (must call mysql_close())
        if (!conn) {
            std::cerr << "[DB Error] mysql_init failed" << std::endl;
            return nullptr;
        }

        bool reconnect = true;
        mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

        if (!mysql_real_connect(conn, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0)) {
            std::cerr << "[DB Error] Connection failed: " << mysql_error(conn) << std::endl;
            mysql_close(conn);
            return nullptr;
        }
        return conn;
    }

    void destroyPool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        for (MYSQL *conn : connection_pool) {
            if (conn)
                mysql_close(conn);
        }
        connection_pool.clear();
    }

public:
    // Get Singleton instance
    static DbManager &getInstance() {
        static DbManager instance;
        return instance;
    }

    MYSQL *getConnection() {
        std::unique_lock<std::mutex> lock(pool_mutex);

        pool_cv.wait(lock, [this]() { return !connection_pool.empty(); });

        MYSQL *conn = connection_pool.back();
        connection_pool.pop_back();

        // Ping test and reconnect
        if (mysql_ping(conn) != 0) {
            std::cout << "[DB Warning] Lost connection, attempting to reconnect..." << std::endl;
            mysql_close(conn);
            conn = createConnection();
        }

        return conn;
    }

    // Return the connection back to the pool
    void releaseConnection(MYSQL *conn) {
        if (!conn)
            return;

        std::lock_guard<std::mutex> lock(pool_mutex);
        connection_pool.push_back(conn);
        pool_cv.notify_one(); // Notify waiting threads
    }

    // Delete copy constructor and assignment operator
    DbManager(const DbManager &) = delete;
    DbManager &operator=(const DbManager &) = delete;
};