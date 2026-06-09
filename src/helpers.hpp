#pragma once

#include <string>
#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>

// Helper for cookie parsing
inline std::string getCookieValue(const std::string &cookie_header, const std::string &key) {
    if (cookie_header.empty())
        return "";
    std::string prefix = key + "=";
    size_t start = cookie_header.find(prefix);
    if (start == std::string::npos)
        return "";

    start += prefix.length();
    size_t end = cookie_header.find(";", start);

    if (end == std::string::npos) {
        return cookie_header.substr(start);
    }
    return cookie_header.substr(start, end - start);
}

// Generate unique ID for error tracking
inline std::string generateErrorTrackingId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;

    ss << "ERR-";
    for (int i = 0; i < 6; i++) {
        ss << std::hex << dis(gen);
    }

    return ss.str();
}

// Generate formatted string for current time
inline std::string getCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm tm_now;

#if defined(_MSC_VER)
    localtime_s(&tm_now, &time_t_now); // Use safe function for MSVC compiler
#else
    localtime_r(&time_t_now, &tm_now); // Use POSIX standard function
#endif

    std::stringstream ss;
    ss << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// Write error log to both console and log file(error.log)
inline void logErrorToConsoleAndFile(const std::string &tracking_id, int status, const std::string &path, const std::string &method, const std::string &details) {
    std::string time_str = getCurrentTimeString();

    std::stringstream log_ss;
    log_ss << "[" << time_str << "][" << tracking_id << "] Status: " << status
           << " | Method: " << method << " | Path: " << path
           << " | Details: " << details << "\n";
    std::string log_msg = log_ss.str();

    // Print error log to console standard error
    std::cerr << log_msg;

    std::ofstream log_file("error.log", std::ios::app);

    if (log_file.is_open()) {
        log_file << log_msg;
        log_file.close();
    } else {
        std::cerr << "[Warning] Failed to open error.log for writing.\n";
    }
}

// Read file and convert to string
inline std::string readFileToString(const std::string &file_path) {
    std::ifstream file(file_path);

    if (!file.is_open())
        return "";

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Replace placeholders in string with dynamic values
inline std::string replacePlaceholder(std::string str, const std::string &placeholder, const std::string &value) {
    size_t pos = str.find(placeholder);

    // if fine that placeholder
    while (pos != std::string::npos) {
        str.replace(pos, placeholder.length(), value);
        pos = str.find(placeholder, pos + value.length());
    }
    return str;
}
