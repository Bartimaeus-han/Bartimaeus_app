#pragma once

#include <cstddef>
#include <ostream>
#include <stop_token>
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
    size_t cur_conns = 0;     // 현재 활성화 중인 연결 수
    std::jthread pool_healer; // 소실되어버린 conn을 주기적으로 채워주는 background thread

    // Constructor
    DbManager() {
        // Loads env varibales and initializes pool
        const char *env_host = std::getenv("DB_HOST");
        host = env_host ? env_host : "127.0.0.1"; // 이건 그냥 MariaDB 접속 주소의 fallback 기본값 (bartimaeus-app 접속과는 무관)

        const char *env_user = std::getenv("DB_USER");
        user = env_user ? env_user : "root";

        const char *env_pass = std::getenv("DB_PASSWORD");
        password = env_pass ? env_pass : "rootpassword";

        const char *env_db = std::getenv("DB_NAME");
        dbname = env_db ? env_db : "bartimaeus_db";

        const char *env_port = std::getenv("DB_PORT");
        port = env_port ? std::atoi(env_port) : 3306;

        initializePool();

        pool_healer = std::jthread([this](std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                // 복구 주기마다 cur_conns를 pool_size 개수로 다시 복구한다.
                int recoveryCycle = 2;

                for (int i = 0; i < recoveryCycle && !stoken.stop_requested(); ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                if (stoken.stop_requested())
                    break;

                std::cout << "[Db Info] Healer tick." << std::endl;

                // 일단 현재 연결이 모두 살아있는지 체크를 먼저 한다
                // true인 이유는 cur_conns가 공유변수이기때문에 어쩔수 없어서.
                while (true) {
                    {
                        std::lock_guard<std::mutex> lock(pool_mutex);
                        if (cur_conns >= pool_size)
                            break;
                    }

                    MYSQL *conn = createConnection();
                    // createConnection()이 nullptr을 반환한다면 아직 DB가 복구되지 않았다는 의미 이므로 다음 recoveryCycle에 재시도 하도록 하자
                    if (!conn)
                        break;

                    {
                        std::lock_guard<std::mutex> lock(pool_mutex);
                        connection_pool.push_back(conn);
                        cur_conns++;

                        pool_cv.notify_one();
                        std::cout << "[DB Info] Healer restored a conneciton. cur_conns=" << cur_conns << "/" << pool_size << std::endl;
                    }
                }
            }
        });
    }

    ~DbManager() {
        // pool을 해제하기 전에
        pool_healer.request_stop(); // healer를 먼저 정지한다.
        if (pool_healer.joinable()) pool_healer.join(); // 멈출 때까지 대기

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

        cur_conns = connection_pool.size(); // 이때는 풀이 새롭게 초기화 된 상태이기 때문에 당연히 활성 연결 개수도 풀 사이즈와 동일한것으로 설정한다.
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

        const auto wait_timeout = std::chrono::seconds(3);
        if (!pool_cv.wait_for(lock, wait_timeout, [this]() { return !connection_pool.empty(); })) {
            std::cerr << "[DB Warning] Timed out waiting for an available connection (cur_conns=" << cur_conns << ")." << std::endl;
            return nullptr;
        }

        MYSQL *conn = connection_pool.back();
        connection_pool.pop_back(); // 이 시점에 풀 크기는 1 감소

        // Ping test and reconnect
        // 1회만 재시도 하고 나머지의 복구는 다음 단계의 백그라운드 힐러가 담당한다.
        if (mysql_ping(conn) != 0) {
            std::cout << "[DB Warning] Lost connection, attempting to reconnect..." << std::endl;
            mysql_close(conn);
            conn = createConnection();

            if (!conn) {
                // 이미 이 시점에서는 되돌려 줄 수 없는 conn이므로 영구 차감한다
                cur_conns--;
                std::cerr << "[DB Warning] Reconnect failed. cur_conns now " << cur_conns << "/" << pool_size << "." << std::endl;
            }
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