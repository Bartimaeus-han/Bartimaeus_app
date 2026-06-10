#pragma once

#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

// Helper for cookie parsing
inline std::string getCookieValue(const std::string &cookie_header, const std::string &key) {
    // Return empty string if header or key is empty
    if (cookie_header.empty() || key.empty())
        return "";

    // Define the cookie prefix to search for
    std::string target_prefix = key + "=";
    std::string_view header_view(cookie_header);

    size_t pos = 0;
    while (pos < header_view.length()) {
        // 다음 세미콜론 구분자 위치 검색 (Search for the next semicolon separator position)
        size_t next_semicolon = header_view.find(';', pos);
        std::string_view pair = (next_semicolon == std::string_view::npos)
                                    ? header_view.substr(pos)
                                    : header_view.substr(pos, next_semicolon - pos);

        // 앞쪽 공백 제거 (Trim leading whitespace)
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.front()))) {
            pair.remove_prefix(1);
        }
        // 뒤쪽 공백 제거 (Trim trailing whitespace)
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.back()))) {
            pair.remove_suffix(1);
        }
        // 개별 쿠키가 정확히 대상 키로 시작하는지 검증 (Verify if the individual cookie starts exactly with the target key)
        if (pair.rfind(target_prefix, 0) == 0) {
            return std::string(pair.substr(target_prefix.length()));
        }
        // 다음 세그먼트로 이동 (Move to the next segment)
        if (next_semicolon == std::string_view::npos) {
            break;
        }
        pos = next_semicolon + 1;
    }
    return "";
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
