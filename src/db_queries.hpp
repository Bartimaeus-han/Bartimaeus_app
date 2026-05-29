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
const std::string VULN_CHECK_USER = "SELECT username FROM users WHERE username = '";

const std::string VULN_INSERT_USER = "INSERT INTO users (username, password) VALUES ('";

const std::string VULN_SELECT_USER = "SELECT * FROM users WHERE username = '";
} // namespace Queries