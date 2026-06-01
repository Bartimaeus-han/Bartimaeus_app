#pragma once
#include <string>

namespace Queries {
// Create table query
constexpr const char *CREATE_USERS_TABLE =
    "CREATE TABLE IF NOT EXISTS users("
    "username TEXT PRIMARY KEY, "
    "password TEXT NOT NULL"
    ");";

// Check ID, SQL query injection vulnerability exists
// Vulnerable query for checking duplicate ID
const std::string VULN_CHECK_USER = "SELECT username FROM users WHERE username = '";

// Vulnerable query for user registration
const std::string VULN_INSERT_USER = "INSERT INTO users (username, password) VALUES ('";

// Vulnerable query for login validation
const std::string VULN_SELECT_USER = "SELECT * FROM users WHERE username = '";

// ========================================================
// Add secure query constants below (Defense SQL Injeciton)
// ========================================================

// Secure query for duplicate ID check
constexpr const char *SECURE_CHECK_USER = "SELECT username FROM users WHERE username = ?;";

// Secure query for user registration
constexpr const char *SECURE_INSERT_USER = "INSERT INTO users (username, password) VALUES (?, ?);";

// Secure query for login validation
constexpr const char *SECURE_SELECT_USER = "SELECT * FROM users WHERE username = ? AND password = ?;";
} // namespace Queries
