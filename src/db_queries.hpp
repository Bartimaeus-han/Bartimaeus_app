#pragma once

namespace Queries {
// Create table query
constexpr const char *CREATE_USERS_TABLE =
    "CREATE TABLE IF NOT EXISTS users("
    "username TEXT PRIMARY KEY, "
    "password TEXT NOT NULL, "
    "salt TEXT," // To store salt value
    "role TEXT NOT NULL DEFAULT 'USER'"
    ");";

// ========================================================
// Add secure query constants below (Defense SQL Injeciton)
// ========================================================

// Secure query for duplicate ID check
constexpr const char *SECURE_CHECK_USER = "SELECT username FROM users WHERE username = ?;";

// Secure query for user registration
constexpr const char *SECURE_INSERT_USER = "INSERT INTO users (username, password, salt) VALUES (?, ?, ?);";

// Secure query for login validation
constexpr const char *SECURE_SELECT_USER = "SELECT password, salt, role FROM users WHERE username = ?;";

constexpr const char *SECURE_SELECT_USER_ROLE = "SELECT role FROM users WHERE username = ?;";

// ===========
// Forum Query
// ===========

// Create posts table query
// Table structure example
// +----+----------------------+---------------------------------+--------+---------------------+
// | id | title                | content                         | author | created_at          |
// +----+----------------------+---------------------------------+--------+---------------------+
// | 1  | "First Announcement" | "Welcome to our secure board!"  | "admin"| 2026-06-11 01:10:00 |
// | 2  | "Security Lab"       | "Let's check IDOR authorization"| "user1"| 2026-06-11 01:15:30 |
// +----+----------------------+---------------------------------+--------+---------------------+
constexpr const char *CREATE_POSTS_TABLE =
    "CREATE TABLE IF NOT EXISTS posts("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "title TEXT NOT NULL, "
    "content TEXT NOT NULL, "
    "author TEXT NOT NULL, "
    "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
    ");";

// Secure parameterized query for writing a post
constexpr const char *SECURE_INSERT_POST = "INSERT INTO posts (title, content, author) VALUES (?, ?, ?);";

// Retrieving summary of all posts
constexpr const char *SELECT_ALL_POSTS = "SELECT id, title, content, author, created_at FROM posts ORDER BY id DESC;";

// Secure parameterized query for retrieving a specific post
constexpr const char *SECURE_SELECT_POST = "SELECT * FROM posts WHERE id = ?;";

// Vulnerable post deletion query for IDOR lab
constexpr const char *VULN_DELETE_POST = "DELETE FROM posts WHERE id = ?;";
} // namespace Queries

// constexpr (Constant Expression)
// : This value is not determined at runtime, but is completly decided at compile time and does not change