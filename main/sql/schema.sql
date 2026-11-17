-- Schema for a NEW expense_tracker database. Not applied on normal startup.
-- Before running on an existing database: back it up, inspect SHOW CREATE TABLE,
-- and agree on account ownership. IF NOT EXISTS does not migrate old tables.
-- Uses InnoDB, explicit foreign keys, and collations available in MySQL 5.7/8.x.
-- No DROP, TRUNCATE, data reassignment, seed accounts, or credentials.

CREATE DATABASE IF NOT EXISTS expense_tracker
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE expense_tracker;

CREATE TABLE IF NOT EXISTS users (
    user_id INT NOT NULL AUTO_INCREMENT,
    -- Application stores trimmed, lowercase ASCII [a-z0-9_.-], length 3..64.
    -- The case-insensitive unique key also rejects mixed-case duplicates.
    username VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
    -- Only the library's encoded salted Argon2id hash belongs here.
    password_hash VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    PRIMARY KEY (user_id),
    CONSTRAINT uq_users_username UNIQUE (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS expenses (
    expense_id INT NOT NULL AUTO_INCREMENT,
    user_id INT NOT NULL,
    amount DECIMAL(12,2) NOT NULL,
    category VARCHAR(50) NOT NULL,
    expense_date DATE NOT NULL,
    description VARCHAR(255) NOT NULL,
    PRIMARY KEY (expense_id),
    INDEX idx_expenses_user_date (user_id, expense_date),
    CONSTRAINT fk_expenses_user FOREIGN KEY (user_id)
        REFERENCES users (user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS budgets (
    budget_id INT NOT NULL AUTO_INCREMENT,
    user_id INT NOT NULL,
    budget_month INT NOT NULL,
    budget_year INT NOT NULL,
    budget_amount DECIMAL(12,2) NOT NULL,
    PRIMARY KEY (budget_id),
    CONSTRAINT uq_budgets_user_period UNIQUE (user_id, budget_year, budget_month),
    CONSTRAINT fk_budgets_user FOREIGN KEY (user_id)
        REFERENCES users (user_id) ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Mandatory application validation (including on import):
-- expenses.amount: 0.01..9999999999.99; budgets.budget_amount: 0..9999999999.99.
-- budget_month: 1..12; budget_year: 1000..9999; dates: real YYYY-MM-DD dates.
-- Category: nonblank, <=50 Unicode characters; description: <=255 characters.
-- Do not rely on CHECK constraints: older MySQL releases ignore them.
-- DatabaseManager enables strict SQL handling and uses prepared statements.
