package database

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// Config holds database configuration options
type Config struct {
	// DatabasePath is the file path to the SQLite database
	// Example: "./data/boomchecker.db" or ":memory:" for in-memory database
	DatabasePath string

	// LogLevel sets GORM logging verbosity
	// Silent = no logs, Error = errors only, Warn = warnings + errors, Info = all queries
	LogLevel logger.LogLevel

	// MaxIdleConns sets the maximum number of idle connections in the pool
	MaxIdleConns int

	// MaxOpenConns sets the maximum number of open connections to the database
	MaxOpenConns int

	// ConnMaxLifetime sets the maximum amount of time a connection may be reused
	ConnMaxLifetime time.Duration
}

// DefaultConfig returns sensible default configuration for production
func DefaultConfig(dbPath string) *Config {
	return &Config{
		DatabasePath:    dbPath,
		LogLevel:        logger.Warn, // Only log warnings and errors in production
		MaxIdleConns:    10,
		MaxOpenConns:    100,
		ConnMaxLifetime: time.Hour,
	}
}

// TestConfig returns configuration suitable for testing (in-memory database)
func TestConfig() *Config {
	return &Config{
		DatabasePath:    ":memory:",
		LogLevel:        logger.Info, // Verbose logging for tests
		MaxIdleConns:    5,
		MaxOpenConns:    10,
		ConnMaxLifetime: time.Minute * 30,
	}
}

// InitDB initializes the database connection and runs migrations
// Returns a GORM DB instance or an error if initialization fails
func InitDB(config *Config) (*gorm.DB, error) {
	if config == nil {
		config = DefaultConfig("./data/boomchecker.db")
	}

	// Create database directory if it doesn't exist (for file-based databases)
	if config.DatabasePath != ":memory:" {
		if err := ensureDBDirectory(config.DatabasePath); err != nil {
			return nil, fmt.Errorf("failed to create database directory: %w", err)
		}

		// Log database path for debugging
		log.Printf("Database path: %s", config.DatabasePath)

		// Check if we can write to the database directory
		if err := checkDatabaseWritePermissions(config.DatabasePath); err != nil {
			return nil, fmt.Errorf("database directory permission check failed: %w", err)
		}
	}

	// Configure GORM logger
	gormConfig := &gorm.Config{
		Logger: logger.Default.LogMode(config.LogLevel),
		NowFunc: func() time.Time {
			// Ensure all GORM timestamps use UTC
			return time.Now().UTC()
		},
	}

	// Open SQLite connection using pure-Go driver (modernc.org/sqlite)
	// This avoids CGO dependency required by mattn/go-sqlite3
	// sqlite.Open() automatically uses the pure-Go driver without CGO
	log.Printf("Opening SQLite database: %s", config.DatabasePath)
	db, err := gorm.Open(sqlite.Open(config.DatabasePath), gormConfig)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to database at %s: %w", config.DatabasePath, err)
	}

	// Get underlying SQL database for connection pool configuration
	sqlDB, err := db.DB()
	if err != nil {
		return nil, fmt.Errorf("failed to get underlying SQL database: %w", err)
	}

	// Configure connection pool
	sqlDB.SetMaxIdleConns(config.MaxIdleConns)
	sqlDB.SetMaxOpenConns(config.MaxOpenConns)
	sqlDB.SetConnMaxLifetime(config.ConnMaxLifetime)

	// Enable foreign key constraints (CRITICAL for SQLite)
	// SQLite disables foreign keys by default
	if err := db.Exec("PRAGMA foreign_keys = ON;").Error; err != nil {
		return nil, fmt.Errorf("failed to enable foreign key constraints: %w", err)
	}

	// Enable Write-Ahead Logging for better concurrency
	if err := db.Exec("PRAGMA journal_mode = WAL;").Error; err != nil {
		// Non-fatal: log warning but continue
		log.Printf("WARNING: Failed to enable WAL mode: %v", err)
	}

	// Run auto-migrations
	if err := runMigrations(db); err != nil {
		return nil, fmt.Errorf("failed to run migrations: %w", err)
	}

	log.Println("Database initialized successfully")
	return db, nil
}

// runMigrations executes GORM AutoMigrate for all models
func runMigrations(db *gorm.DB) error {
	// AutoMigrate will create tables, indexes, and constraints
	// Order matters: create independent tables first
	if err := db.AutoMigrate(
		&models.Node{},
		&models.RegistrationToken{},
	); err != nil {
		return fmt.Errorf("AutoMigrate failed: %w", err)
	}

	// Create additional indexes that GORM tags might not handle
	if err := createCustomIndexes(db); err != nil {
		return fmt.Errorf("failed to create custom indexes: %w", err)
	}

	log.Println("Database migrations completed")
	return nil
}

// createCustomIndexes creates indexes that aren't automatically created by GORM tags
func createCustomIndexes(db *gorm.DB) error {
	indexes := []string{
		// Index for filtering active/disabled/revoked nodes
		"CREATE INDEX IF NOT EXISTS idx_nodes_status ON nodes(status);",

		// Index for finding inactive nodes (cleanup queries)
		"CREATE INDEX IF NOT EXISTS idx_nodes_last_seen ON nodes(last_seen_at);",

		// Composite index for token validation (used_count + usage_limit checks)
		"CREATE INDEX IF NOT EXISTS idx_registration_tokens_usage ON registration_tokens(used_count, usage_limit);",

		// Index for expired token cleanup queries
		"CREATE INDEX IF NOT EXISTS idx_registration_tokens_expires_at ON registration_tokens(expires_at);",
	}

	for _, indexSQL := range indexes {
		if err := db.Exec(indexSQL).Error; err != nil {
			return fmt.Errorf("failed to create index: %w (SQL: %s)", err, indexSQL)
		}
	}

	return nil
}

// Close gracefully closes the database connection
func Close(db *gorm.DB) error {
	sqlDB, err := db.DB()
	if err != nil {
		return fmt.Errorf("failed to get underlying SQL database: %w", err)
	}

	if err := sqlDB.Close(); err != nil {
		return fmt.Errorf("failed to close database connection: %w", err)
	}

	log.Println("Database connection closed")
	return nil
}

// Ping checks if the database connection is alive
func Ping(db *gorm.DB) error {
	sqlDB, err := db.DB()
	if err != nil {
		return fmt.Errorf("failed to get underlying SQL database: %w", err)
	}

	if err := sqlDB.Ping(); err != nil {
		return fmt.Errorf("database ping failed: %w", err)
	}

	return nil
}

// ensureDBDirectory creates the directory for the database file if it doesn't exist
func ensureDBDirectory(dbPath string) error {
	// Extract directory from database path
	dir := dbPath
	// Find last slash (works for both / and \ on Windows)
	lastSlash := -1
	for i := len(dbPath) - 1; i >= 0; i-- {
		if dbPath[i] == '/' || dbPath[i] == '\\' {
			lastSlash = i
			break
		}
	}

	// If no directory separator found, database is in current directory
	if lastSlash == -1 {
		return nil
	}

	dir = dbPath[:lastSlash]

	// Create directory with proper permissions (0755 = rwxr-xr-x)
	// os.MkdirAll creates parent directories as needed
	if err := ensureDirExists(dir); err != nil {
		return fmt.Errorf("failed to create directory %s: %w", dir, err)
	}

	return nil
}

// ensureDirExists is a helper to create directory if it doesn't exist
func ensureDirExists(dir string) error {
	// Import os package at the top if not already imported
	// Check if directory exists
	info, err := os.Stat(dir)
	if err == nil {
		// Directory exists, check if it's actually a directory
		if !info.IsDir() {
			return fmt.Errorf("%s exists but is not a directory", dir)
		}
		return nil
	}

	// If error is not "not exists", return it
	if !os.IsNotExist(err) {
		return err
	}

	// Directory doesn't exist, create it
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}

	log.Printf("Created database directory: %s", dir)
	return nil
}

// checkDatabaseWritePermissions verifies that we can write to the database directory
func checkDatabaseWritePermissions(dbPath string) error {
	// Extract directory from database path
	dir := dbPath
	lastSlash := -1
	for i := len(dbPath) - 1; i >= 0; i-- {
		if dbPath[i] == '/' || dbPath[i] == '\\' {
			lastSlash = i
			break
		}
	}

	if lastSlash != -1 {
		dir = dbPath[:lastSlash]
	} else {
		dir = "."
	}

	// Check if directory exists and get its info
	info, err := os.Stat(dir)
	if err != nil {
		return fmt.Errorf("cannot access database directory %s: %w", dir, err)
	}

	if !info.IsDir() {
		return fmt.Errorf("%s is not a directory", dir)
	}

	// Try to create a test file to verify write permissions
	testFile := dir + "/.write_test_" + fmt.Sprintf("%d", time.Now().UnixNano())
	f, err := os.Create(testFile)
	if err != nil {
		return fmt.Errorf("cannot write to database directory %s: %w (check permissions)", dir, err)
	}
	f.Close()
	os.Remove(testFile)

	log.Printf("Database directory %s is writable", dir)
	return nil
}
