package main

import (
	"github.com/boomchecker/api-backend/internal/handlers"
	"github.com/gin-gonic/gin"
)

func main() {
	// Create a Gin router with default middleware (logger and recovery)
	router := gin.Default()

	// Register health check endpoint
	router.GET("/ping", handlers.PingHandler)

	// Start server on port 8080
	router.Run(":8080")
}
