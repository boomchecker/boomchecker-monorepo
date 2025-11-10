package main

import (
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/boomchecker/api-backend/internal/database"
	"github.com/boomchecker/api-backend/internal/handlers"
	"github.com/gin-gonic/gin"
)

func main() {
	// Set Gin mode based on environment variable
	// Default to release mode for production safety
	ginMode := os.Getenv("GIN_MODE")
	if ginMode == "" {
		// If GIN_MODE not set, check APP_ENV or ENV
		env := os.Getenv("APP_ENV")
		if env == "" {
			env = os.Getenv("ENV")
		}

		// Set to release mode unless explicitly in development
		if env == "development" || env == "dev" {
			gin.SetMode(gin.DebugMode)
			log.Println("🔧 Running in DEBUG mode")
		} else {
			gin.SetMode(gin.ReleaseMode)
			log.Println("🚀 Running in RELEASE mode")
		}
	} else {
		gin.SetMode(ginMode)
		log.Printf("🔧 Running in %s mode (from GIN_MODE)", ginMode)
	}

	// Initialize database
	dbConfig := database.DefaultConfig("./data/boomchecker.db")
	db, err := database.InitDB(dbConfig)
	if err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}

	// Ensure database is closed on shutdown
	defer func() {
		if err := database.Close(db); err != nil {
			log.Printf("Error closing database: %v", err)
		}
	}()

	// Setup graceful shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	// Create a Gin router with default middleware (logger and recovery)
	router := gin.Default()

	// Register health check endpoint
	router.GET("/ping", handlers.PingHandler)

	// TODO: Add database health check endpoint
	// router.GET("/health", handlers.HealthCheckHandler(db))

	// Start server on port 8080 in a goroutine
	go func() {
		if err := router.Run(":8080"); err != nil {
			log.Fatalf("Server failed to start: %v", err)
		}
	}()

	log.Println("🚀 Server started on http://localhost:8080")
	log.Println("Press Ctrl+C to shutdown")

	// Wait for interrupt signal
	<-quit
	log.Println("Shutting down server...")
}
