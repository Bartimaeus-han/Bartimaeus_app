-- Create and use database

CREATE DATABASE IF NOT EXISTS bartimaeus_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE bartimaeus_db;
-- 1. Create users table
CREATE TABLE IF NOT EXISTS users (
    username VARCHAR(255) PRIMARY KEY,
    password VARCHAR(255) NOT NULL,
    role VARCHAR(50) NOT NULL DEFAULT 'USER'
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- 2. Create posts table
CREATE TABLE IF NOT EXISTS posts (
    id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    content TEXT NOT NULL,
    author VARCHAR(255) NOT NULL,
    create_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

INSERT IGNORE INTO users (username, password, role) VALUES
('admin', 'admin_hash_placeholder', 'ADMIN'),
('user1', 'user1_hash_placeholder', 'USER');

INSERT IGNORE INTO posts (title, content, author) VALUES
('Welcome to MySQL Board!', 'This board is running on Docker Container.', 'admin');