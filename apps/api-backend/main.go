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
