#pragma once // prevents header files from being included multiple times
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

public:
    // sign up logic
    bool signUp(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        // ID duplicate check
        if (user_db.find(username) != user_db.end()) {
            std::cout << "[SignUp Fail] Duplicate username: " << username << std::endl;
            return false;
        }

        //
        user_db[username] = User{username, password};
        std::cout << "[SignUp Success] Registered user: " << username
                  << "(Password stored in PLAIN TEXT)" << std::endl;
        return true;
    }

    bool signIn(const std::string &username, const std::string &password) {
        std::lock_guard<std::mutex> lock(db_mutex);

        auto it = user_db.find(username);
        // If user not founded
        if (it == user_db.end()) {
            std::cout << "[Login Fail] User not found: " << username << std::endl;
            return false;
        }

        if (it->second.password == password) {
            std::cout << "[Login Success] User authnticated: " << username
                      << std::endl;
            return true;
        }

        std::cout << "[Loing Fail] Invalid password for user" << username
                  << std::endl;
        return false;
    }

    // get all registered user list (for admin)
    std::vector<User> getAllUsers() {
        std::lock_guard<std::mutex> lock(db_mutex);
        std::vector<User> users;

        for (const auto &[username, user] : user_db) {
            users.push_back(user);
        }
        return users;
    }
};