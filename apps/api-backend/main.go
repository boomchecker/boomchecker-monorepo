package main

import (
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/boomchecker/api-backend/internal/crypto"
	"github.com/boomchecker/api-backend/internal/database"
	"github.com/boomchecker/api-backend/internal/handlers"
	"github.com/boomchecker/api-backend/internal/middleware"
	"github.com/boomchecker/api-backend/internal/repositories"
	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	// Load .env file if it exists (development)
	// In production, environment variables are set by systemd/docker
	if err := godotenv.Load(); err != nil {
		log.Println("No .env file found, using system environment variables")
	} else {
		log.Println("Loaded .env file")
	}

	// Validate encryption key is configured
	if err := crypto.ValidateEncryptionKey(); err != nil {
		log.Fatalf("Encryption key validation failed: %v\n"+
			"Please set JWT_ENCRYPTION_KEY in .env or environment.\n"+
			"Generate key with: go run scripts/generate_keys.go", err)
	}
	log.Println("Encryption key validated")

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
			log.Println("Running in DEBUG mode")
		} else {
			gin.SetMode(gin.ReleaseMode)
			log.Println("Running in RELEASE mode")
		}
	} else {
		gin.SetMode(ginMode)
		log.Printf("Running in %s mode (from GIN_MODE)", ginMode)
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

	// Initialize repositories
	nodeRepo := repositories.NewNodeRepository(db)
	tokenRepo := repositories.NewRegistrationTokenRepository(db)

	// Initialize services
	registrationService := services.NewNodeRegistrationService(nodeRepo, tokenRepo)
	tokenManagementService := services.NewTokenManagementService(tokenRepo)

	// Initialize handlers
	nodeRegistrationHandler := handlers.NewNodeRegistrationHandler(registrationService)
	tokenManagementHandler := handlers.NewTokenManagementHandler(tokenManagementService)

	// Create a Gin router with default middleware (logger and recovery)
	router := gin.Default()

	// Register health check endpoint
	router.GET("/ping", handlers.PingHandler)

	// TODO: Add database health check endpoint
	// router.GET("/health", handlers.HealthCheckHandler(db))

	// Register node registration endpoint (public)
	router.POST("/nodes/register", nodeRegistrationHandler.RegisterNode)

	// TODO: Admin Authentication - Email-based JWT login flow
	// Current state: Admin endpoints are UNPROTECTED (middleware allows all requests)
	// Required implementation:
	//   1. POST /admin/auth/request - Admin provides email, receives JWT via email (24h validity)
	//   2. Update middleware.AdminAuthMiddleware() to validate JWT from Authorization header
	//   3. Configure email service (SMTP/SendGrid/etc) for sending login tokens
	//   4. Add ADMIN_JWT_SECRET to .env (separate from node JWT encryption key)
	// See internal/middleware/admin_auth.go for detailed implementation plan

	// Register admin endpoints (protected by middleware)
	// WARNING: Currently unprotected - AdminAuthMiddleware is a placeholder
	adminGroup := router.Group("/admin")
	adminGroup.Use(middleware.AdminAuthMiddleware()) // TODO: Implement proper JWT validation
	{
		// Device registration token management
		adminGroup.POST("/registration-node-tokens", tokenManagementHandler.CreateToken)
		adminGroup.GET("/registration-node-tokens", tokenManagementHandler.ListAllTokens)
		adminGroup.GET("/registration-node-tokens/active", tokenManagementHandler.ListActiveTokens)
		adminGroup.GET("/registration-node-tokens/statistics", tokenManagementHandler.GetStatistics)
		adminGroup.POST("/registration-node-tokens/cleanup", tokenManagementHandler.CleanupExpiredTokens)
		adminGroup.GET("/registration-node-tokens/:token", tokenManagementHandler.GetToken)
		adminGroup.DELETE("/registration-node-tokens/:token", tokenManagementHandler.DeleteToken)

		// TODO: Add admin auth endpoints here when implemented
		// adminGroup.POST("/auth/request", adminAuthHandler.RequestLogin)
	}

	// Start server on port 8080 in a goroutine
	go func() {
		if err := router.Run(":8080"); err != nil {
			log.Fatalf("Server failed to start: %v", err)
		}
	}()

	log.Println("Server started on http://localhost:8080")
	log.Println("Press Ctrl+C to shutdown")

	// Wait for interrupt signal
	<-quit
	log.Println("Shutting down server...")
}
