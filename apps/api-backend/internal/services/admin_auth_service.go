package services

import (
	"context"
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/crypto"
	"github.com/boomchecker/api-backend/internal/models"
	"github.com/boomchecker/api-backend/internal/repositories"
	"github.com/google/uuid"
)

// AdminAuthService handles the business logic for admin authentication
type AdminAuthService struct {
	adminTokenRepo *repositories.AdminTokenRepository
	emailService   *EmailService
	jwtSecret      string
	adminEmail     string
}

// AdminAuthConfig holds configuration for admin authentication
type AdminAuthConfig struct {
	// JWTSecret is the base64-encoded secret for signing admin JWT tokens
	JWTSecret string
	// AdminEmail is the authorized admin email address
	AdminEmail string
}

// NewAdminAuthService creates a new admin authentication service instance
func NewAdminAuthService(
	adminTokenRepo *repositories.AdminTokenRepository,
	emailService *EmailService,
	config *AdminAuthConfig,
) (*AdminAuthService, error) {
	if adminTokenRepo == nil {
		return nil, fmt.Errorf("admin token repository is required")
	}
	if emailService == nil {
		return nil, fmt.Errorf("email service is required")
	}
	if config == nil {
		return nil, fmt.Errorf("admin auth config is required")
	}
	if config.JWTSecret == "" {
		return nil, fmt.Errorf("JWT secret is required")
	}
	if config.AdminEmail == "" {
		return nil, fmt.Errorf("admin email is required")
	}

	return &AdminAuthService{
		adminTokenRepo: adminTokenRepo,
		emailService:   emailService,
		jwtSecret:      config.JWTSecret,
		adminEmail:     config.AdminEmail,
	}, nil
}

// TokenRequest contains the data needed to request an admin token
type TokenRequest struct {
	Email string `json:"email" binding:"required,email" example:"admin@example.com"`
}

// TokenResponse contains the response after requesting a token
type TokenResponse struct {
	Message   string `json:"message" example:"Admin token has been sent to your email"`
	ExpiresAt string `json:"expires_at" example:"2025-11-13T12:00:00Z"` // UTC timestamp when token expires (RFC3339 format)
}

// RequestToken handles the complete admin token request flow
// This includes:
// 1. Validating the email is the authorized admin email
// 2. Checking rate limiting (1 request per 24 hours)
// 3. Generating a new JWT token
// 4. Storing token hash in database
// 5. Sending token via email
func (s *AdminAuthService) RequestToken(ctx context.Context, req *TokenRequest) (*TokenResponse, error) {
	// Step 1: Validate email is the authorized admin email
	if req.Email != s.adminEmail {
		return nil, fmt.Errorf("unauthorized: email is not authorized for admin access")
	}

	// Step 2: Check rate limiting
	lastRequest, err := s.adminTokenRepo.GetLastRequestByEmail(req.Email)
	if err != nil {
		return nil, fmt.Errorf("failed to check rate limit: %w", err)
	}

	if lastRequest != nil {
		if !models.CanRequestNewToken(lastRequest.RequestedAt) {
			// Calculate when next request is allowed
			nextAllowedAt := lastRequest.RequestedAt.Add(24 * time.Hour)
			timeRemaining := time.Until(nextAllowedAt)
			hoursRemaining := int(timeRemaining.Hours())
			minutesRemaining := int(timeRemaining.Minutes()) % 60

			return nil, fmt.Errorf(
				"rate limit exceeded: you can request a new token in %dh %dm (last request was at %s)",
				hoursRemaining,
				minutesRemaining,
				lastRequest.RequestedAt.Format("2006-01-02 15:04:05 MST"),
			)
		}
	}

	// Step 3: Generate JWT token
	token, expiresAt, err := crypto.GenerateAdminJWT(req.Email, s.jwtSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to generate JWT token: %w", err)
	}

	// Step 4: Hash and store token in database
	tokenHash := crypto.HashToken(token)
	adminToken := &models.AdminToken{
		ID:          uuid.New().String(),
		Email:       req.Email,
		TokenHash:   tokenHash,
		RequestedAt: time.Now().UTC(),
		ExpiresAt:   expiresAt,
		IsUsed:      false,
	}

	if err := s.adminTokenRepo.Create(adminToken); err != nil {
		return nil, fmt.Errorf("failed to store token in database: %w", err)
	}

	// Step 5: Send token via email
	if err := s.emailService.SendAdminToken(ctx, req.Email, token, expiresAt); err != nil {
		return nil, fmt.Errorf("failed to send email: %w", err)
	}

	return &TokenResponse{
		Message:   "Admin token has been sent to your email",
		ExpiresAt: expiresAt.Format(time.RFC3339),
	}, nil
}

// ValidateToken validates an admin token
// This is used by the middleware to verify incoming requests
func (s *AdminAuthService) ValidateToken(tokenString string) (*crypto.AdminClaims, error) {
	// Step 1: Verify JWT signature and expiration
	claims, err := crypto.VerifyAdminJWT(tokenString, s.jwtSecret)
	if err != nil {
		return nil, fmt.Errorf("invalid token: %w", err)
	}

	// Step 2: Check if token exists in database and hasn't been revoked
	tokenHash := crypto.HashToken(tokenString)
	dbToken, err := s.adminTokenRepo.ValidateToken(tokenHash)
	if err != nil {
		return nil, fmt.Errorf("token validation failed: %w", err)
	}

	// Step 3: Mark token as used (first time use tracking)
	if !dbToken.IsUsed {
		if err := s.adminTokenRepo.MarkAsUsed(tokenHash); err != nil {
			// Log error but don't fail the request
			// This is just for tracking purposes
			fmt.Printf("Warning: failed to mark token as used: %v\n", err)
		}
	}

	return claims, nil
}

// CleanupExpiredTokens removes expired tokens from the database
// This should be called periodically (e.g., via a cron job)
func (s *AdminAuthService) CleanupExpiredTokens() (int64, error) {
	count, err := s.adminTokenRepo.CleanupExpired()
	if err != nil {
		return 0, fmt.Errorf("failed to cleanup expired tokens: %w", err)
	}

	return count, nil
}
