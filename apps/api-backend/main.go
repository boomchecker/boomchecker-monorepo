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
	swaggerFiles "github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"

	_ "github.com/boomchecker/api-backend/docs" // swagger docs
)

// @title BoomChecker API
// @version 1.0
// @description REST API for BoomChecker IoT system - device registration and management with JWT-based authentication
// @termsOfService https://boomchecker.com/terms

// @contact.name API Support
// @contact.email support@boomchecker.com

// @license.name MIT
// @license.url https://opensource.org/licenses/MIT

// @host localhost:8080
// @BasePath /

// @securityDefinitions.apikey BearerAuth
// @in header
// @name Authorization
// @description Type "Bearer" followed by a space and JWT token for node authentication

// @securityDefinitions.apikey AdminAuth
// @in header
// @name Authorization
// @description Type "Bearer" followed by a space and JWT token for admin authentication

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
	// Get database path from environment variable, fallback to default
	dbPath := os.Getenv("DB_PATH")
	if dbPath == "" {
		dbPath = "./data/boomchecker.db"
		log.Println("DB_PATH not set, using default: ./data/boomchecker.db")
	}
	dbConfig := database.DefaultConfig(dbPath)
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
	adminTokenRepo := repositories.NewAdminTokenRepository(db)

	// Initialize email service for admin authentication
	emailService, err := services.NewEmailService(&services.EmailConfig{
		FromEmail: os.Getenv("AWS_SES_FROM_EMAIL"),
		Region:    os.Getenv("AWS_SES_REGION"),
	})
	if err != nil {
		log.Fatalf("Failed to initialize email service: %v\n"+
			"Please ensure AWS_SES_FROM_EMAIL and AWS_SES_REGION are set in .env", err)
	}

	// Initialize admin authentication service
	adminAuthService, err := services.NewAdminAuthService(
		adminTokenRepo,
		emailService,
		&services.AdminAuthConfig{
			JWTSecret:  os.Getenv("ADMIN_JWT_SECRET"),
			AdminEmail: os.Getenv("ADMIN_EMAIL"),
		},
	)
	if err != nil {
		log.Fatalf("Failed to initialize admin auth service: %v\n"+
			"Please ensure ADMIN_JWT_SECRET and ADMIN_EMAIL are set in .env", err)
	}
	log.Println("Admin authentication service initialized")

	// Initialize services
	registrationService := services.NewNodeRegistrationService(nodeRepo, tokenRepo)
	tokenManagementService := services.NewTokenManagementService(tokenRepo)

	// Initialize handlers
	nodeRegistrationHandler := handlers.NewNodeRegistrationHandler(registrationService)
	tokenManagementHandler := handlers.NewTokenManagementHandler(tokenManagementService)
	adminAuthHandler := handlers.NewAdminAuthHandler(adminAuthService)

	// Create a Gin router with default middleware (logger and recovery)
	router := gin.Default()

	// Swagger documentation endpoint
	router.GET("/swagger/*any", ginSwagger.WrapHandler(swaggerFiles.Handler))

	// Register health check endpoint
	router.GET("/ping", handlers.PingHandler)

	// TODO: Add database health check endpoint
	// router.GET("/health", handlers.HealthCheckHandler(db))

	// Register node registration endpoint (public)
	router.POST("/nodes/register", nodeRegistrationHandler.RegisterNode)

	// Register admin authentication endpoint (public - must be outside admin group)
	// This endpoint allows admins to request a JWT token via email
	router.POST("/admin/auth/request", adminAuthHandler.RequestToken)

	// Register admin endpoints (protected by JWT authentication middleware)
	// Admin must first request a token via POST /admin/auth/request
	// Token is sent via email and must be included in Authorization header: Bearer <token>
	adminGroup := router.Group("/admin")
	adminGroup.Use(middleware.AdminAuthMiddleware(adminAuthService))
	{
		// Device registration token management
		adminGroup.POST("/registration-node-tokens", tokenManagementHandler.CreateToken)
		adminGroup.GET("/registration-node-tokens", tokenManagementHandler.ListAllTokens)
		adminGroup.GET("/registration-node-tokens/active", tokenManagementHandler.ListActiveTokens)
		adminGroup.GET("/registration-node-tokens/statistics", tokenManagementHandler.GetStatistics)
		adminGroup.POST("/registration-node-tokens/cleanup", tokenManagementHandler.CleanupExpiredTokens)
		adminGroup.GET("/registration-node-tokens/:token", tokenManagementHandler.GetToken)
		adminGroup.DELETE("/registration-node-tokens/:token", tokenManagementHandler.DeleteToken)
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
