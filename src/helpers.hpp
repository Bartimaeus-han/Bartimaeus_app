#pragma once

#include <argon2.h> // Argon2 hashing header
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional> // To return safe return values
#include <random>
#include <sstream>
#include <stdexcept> // For exception handling
#include <string>
#include <string_view>

#include "picosha2.h" // SHA-256 hash library for generating secure session IDs

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
        // Search for the next semicolon separator position
        size_t next_semicolon = header_view.find(';', pos);
        std::string_view pair = (next_semicolon == std::string_view::npos)
                                    ? header_view.substr(pos)
                                    : header_view.substr(pos, next_semicolon - pos);

        // Trim leading whitespace
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.front()))) {
            pair.remove_prefix(1);
        }
        // Trim trailing whitespace
        while (!pair.empty() && std::isspace(static_cast<unsigned char>(pair.back()))) {
            pair.remove_suffix(1);
        }
        // Verify if the individual cookie starts exactly with the target key
        if (pair.rfind(target_prefix, 0) == 0) {
            return std::string(pair.substr(target_prefix.length()));
        }
        // Move to the next segment
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

// Escape special characters in a JSON string
inline std::string escapeJson(const std::string &input) {
    std::string output;
    for (char c : input) {
        if (c == '"')
            output += "\\\"";
        else if (c == '\\')
            output += "\\\\";
        else if (c == '\b')
            output += "\\b";
        else if (c == '\f')
            output += "\\f";
        else if (c == '\n')
            output += "\\n";
        else if (c == '\r')
            output += "\\r";
        else if (c == '\t')
            output += "\\t";
        else
            output += c;
    }
    return output;
}

inline std::optional<int> safeStoi(const std::string &str) {
    if (str.empty()) {
        return std::nullopt;
    }

    // Ensure it consists of digits
    size_t start = 0;
    if (str[0] == '-' || str[0] == '+') {
        // Return nullopt if only a sign is provided
        if (str.length() == 1)
            return std::nullopt;
        start = 1;
    }

    for (size_t i = start; i < str.length(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            // Return nullopt if non-digit character is found
            return std::nullopt;
        }
    }

    try {
        return std::stoi(str);
    } catch (const std::invalid_argument &) {
        // In case of invalid format
        return std::nullopt;
    } catch (const std::out_of_range &) {
        // In case of integer overflow/underflow
        return std::nullopt;
    }
}

// Generate cryptographically secure random salt (32-character hex string)
inline std::string generateSecureSalt() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;

    // Generate 16 random bytes and convert to 32-character hex string
    for (int i = 0; i < 32; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

// Stretch password with salt N times using SHA-256
inline std::string stretchPasswordSHA256(const std::string &password, const std::string &salt, int iterations = 10000) {
    // hash (password + salt)
    std::string current_hash = picosha2::hash256_hex_string(password + salt);

    // second hash (first hash + salt) * `iterations` times
    for (int i = 1; i < iterations; i++) {
        current_hash = picosha2::hash256_hex_string(current_hash + salt);
    }

    return current_hash;
}

// Hash password to encoded string using Argon2id
inline std::string hashPasswordArgon2id(const std::string &password) {
    // 1. Generate secure random 16-byte salt
    std::vector<uint8_t> salt(16);
    static std::random_device rd;
    static std::mt19937 gen(rd()); // 19937: Mersenne Prime

    std::uniform_int_distribution<uint16_t> dis(0, 255);

    for (auto &val : salt) {
        val = static_cast<uint8_t>(dis(gen));
    }

    // 2. Define upgraded hashing parameters
    uint32_t t_cost = 3;      // Number of passes
    uint32_t m_cost = 32768;  // Memory usage
    uint32_t parallelism = 1; // Number of parallel threads
    uint32_t hash_len = 32;   // Length of the output hash

    // 3. Calculate required length for the encoded buffer
    size_t encoded_len = argon2_encodedlen(t_cost, m_cost, parallelism, salt.size(), hash_len, Argon2_id);
    std::string encoded(encoded_len, '\0');

    // 4. Perform Argon2id encoding hash
    int rc = argon2id_hash_encoded(
        t_cost, m_cost, parallelism,
        password.data(), password.size(),
        salt.data(), salt.size(),
        hash_len,
        encoded.data(), encoded.size());

    if (rc != ARGON2_OK) { // ARGON2_OK<enum Argon2_ErrorCodes>
        throw std::runtime_error("Argon2id hashing failed: " + std::string(argon2_error_message(rc)));
    }

    // 5. Remove trailing null chars and return
    encoded.erase(encoded.find('\0'));
    return encoded;
}

// Verify input password against Argon2id hash
inline bool verifyPasswordArgon2id(const std::string &password, const std::string &encoded_hash) {
    int rc = argon2id_verify(encoded_hash.c_str(), password.data(), password.size());
    return rc == ARGON2_OK;
}

// Convert HTML special characters in string to safe HTML entities
inline std::string htmlEscape(const std::string &data) {
    std::string buffer;
    buffer.reserve(data.size() * 1.1);

    for (size_t pos = 0; pos != data.size(); ++pos) {
        switch (data[pos]) {
        case '&':
            buffer.append("&amp;");
            break; // 앰퍼샌드 치환 (Ampersand replacement)
        case '\"':
            buffer.append("&quot;");
            break; // 쌍따옴표 치환 (Double quote replacement)
        case '\'':
            buffer.append("&#x27;");
            break; // 외따옴표 치환 (Single quote replacement)
        case '<':
            buffer.append("&lt;");
            break; // 미만 부호 치환 (Less-than sign replacement)
        case '>':
            buffer.append("&gt;");
            break; // 초과 부호 치환 (Greater-than sign replacement)
        case '/':
            buffer.append("&#x2F;");
            break; // 슬래시 치환으로 HTML 태그 닫기 우회 방지 (Slash replacement to prevent closing tag bypass)
        default:
            buffer.append(1, data[pos]);
            break;
        }
    }
    return buffer;
}
