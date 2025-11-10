package services

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"github.com/boomchecker/api-backend/internal/repositories"
	"github.com/boomchecker/api-backend/internal/validators"
	"github.com/google/uuid"
)

// TokenManagementService handles the business logic for registration token management
type TokenManagementService struct {
	tokenRepo *repositories.RegistrationTokenRepository
}

// NewTokenManagementService creates a new token management service instance
func NewTokenManagementService(tokenRepo *repositories.RegistrationTokenRepository) *TokenManagementService {
	return &TokenManagementService{
		tokenRepo: tokenRepo,
	}
}

// CreateTokenRequest contains the data needed to create a registration token
type CreateTokenRequest struct {
	ExpiresInHours   int     `json:"expires_in_hours" binding:"required,min=1" example:"24" swaggertype:"integer" minimum:"1"`
	MaxUses          *int    `json:"max_uses,omitempty" binding:"omitempty,min=1" example:"1" swaggertype:"integer" minimum:"1"` // If not provided, defaults to 1
	AuthorizedMAC    *string `json:"authorized_mac,omitempty" example:"AA:BB:CC:DD:EE:FF"`
	Description      *string `json:"description,omitempty" example:"Token for production nodes"`
}

// CreateTokenResponse contains the data returned after creating a token
type CreateTokenResponse struct {
	Token         string     `json:"token" example:"a1b2c3d4-e5f6-7890-abcd-ef1234567890"`
	ExpiresAt     string     `json:"expires_at" example:"2025-11-11T14:30:00Z"`
	MaxUses       *int       `json:"max_uses,omitempty" example:"1"`
	AuthorizedMAC *string    `json:"authorized_mac,omitempty" example:"AA:BB:CC:DD:EE:FF"`
	Description   *string    `json:"description,omitempty" example:"Token for production nodes"`
	CreatedAt     string     `json:"created_at" example:"2025-11-10T14:30:00Z"`
}

// TokenListResponse contains information about a token for listing
type TokenListResponse struct {
	Token         string     `json:"token" example:"a1b2c3d4-e5f6-7890-abcd-ef1234567890"`
	ExpiresAt     string     `json:"expires_at" example:"2025-11-11T14:30:00Z"`
	MaxUses       *int       `json:"max_uses,omitempty" example:"1"`
	UsedCount     int        `json:"used_count" example:"0"`
	AuthorizedMAC *string    `json:"authorized_mac,omitempty" example:"AA:BB:CC:DD:EE:FF"`
	Description   *string    `json:"description,omitempty" example:"Token for production nodes"`
	IsExpired     bool       `json:"is_expired" example:"false"`
	IsActive      bool       `json:"is_active" example:"true"`
	CreatedAt     string     `json:"created_at" example:"2025-11-10T14:30:00Z"`
}

// CreateToken generates a new registration token
func (s *TokenManagementService) CreateToken(req *CreateTokenRequest) (*CreateTokenResponse, error) {
	// Validate request
	if err := s.validateCreateTokenRequest(req); err != nil {
		return nil, fmt.Errorf("validation failed: %w", err)
	}

	// Generate secure random token
	tokenValue, err := generateSecureToken(32) // 32 bytes = 256 bits
	if err != nil {
		return nil, fmt.Errorf("failed to generate token: %w", err)
	}

	// Generate UUID for token ID
	tokenID := uuid.New().String()

	// Calculate expiration time
	now := time.Now().UTC()
	expiresAt := now.Add(time.Duration(req.ExpiresInHours) * time.Hour)

	// Normalize MAC address if provided
	var authorizedMAC *string
	if req.AuthorizedMAC != nil && *req.AuthorizedMAC != "" {
		normalized, err := validators.NormalizeMACAddress(*req.AuthorizedMAC)
		if err != nil {
			return nil, fmt.Errorf("invalid MAC address: %w", err)
		}
		authorizedMAC = &normalized
	}

	// Set default max uses to 1 if not provided
	maxUses := req.MaxUses
	if maxUses == nil {
		defaultMaxUses := 1
		maxUses = &defaultMaxUses
	}

	// Create token model
	token := &models.RegistrationToken{
		ID:                      tokenID,
		Token:                   tokenValue,
		ExpiresAt:               &expiresAt,
		UsageLimit:              maxUses,
		UsedCount:               0,
		PreAuthorizedMacAddress: authorizedMAC,
	}

	// Save to database
	if err := s.tokenRepo.Create(token); err != nil {
		return nil, fmt.Errorf("failed to create token: %w", err)
	}

	return &CreateTokenResponse{
		Token:         token.Token,
		ExpiresAt:     token.ExpiresAt.UTC().Format(time.RFC3339),
		MaxUses:       token.UsageLimit,
		AuthorizedMAC: token.PreAuthorizedMacAddress,
		Description:   req.Description,
		CreatedAt:     token.CreatedAt.UTC().Format(time.RFC3339),
	}, nil
}

// ListAllTokens returns all registration tokens
func (s *TokenManagementService) ListAllTokens() ([]*TokenListResponse, error) {
	tokens, err := s.tokenRepo.ListAll()
	if err != nil {
		return nil, fmt.Errorf("failed to list tokens: %w", err)
	}

	return s.convertToListResponse(tokens), nil
}

// ListActiveTokens returns only active (non-expired, with remaining uses) tokens
func (s *TokenManagementService) ListActiveTokens() ([]*TokenListResponse, error) {
	tokens, err := s.tokenRepo.ListActive()
	if err != nil {
		return nil, fmt.Errorf("failed to list active tokens: %w", err)
	}

	return s.convertToListResponse(tokens), nil
}

// GetToken retrieves a specific token by its value
func (s *TokenManagementService) GetToken(tokenValue string) (*TokenListResponse, error) {
	token, err := s.tokenRepo.FindByToken(tokenValue)
	if err != nil {
		return nil, fmt.Errorf("token not found: %w", err)
	}

	expiresAt := ""
	if token.ExpiresAt != nil {
		expiresAt = token.ExpiresAt.UTC().Format(time.RFC3339)
	}

	return &TokenListResponse{
		Token:         token.Token,
		ExpiresAt:     expiresAt,
		MaxUses:       token.UsageLimit,
		UsedCount:     token.UsedCount,
		AuthorizedMAC: token.PreAuthorizedMacAddress,
		Description:   nil, // Model doesn't have Description field
		IsExpired:     token.IsExpired(),
		IsActive:      token.IsValid(),
		CreatedAt:     token.CreatedAt.UTC().Format(time.RFC3339),
	}, nil
}

// DeleteToken removes a token from the database
func (s *TokenManagementService) DeleteToken(tokenValue string) error {
	if err := s.tokenRepo.Delete(tokenValue); err != nil {
		return fmt.Errorf("failed to delete token: %w", err)
	}
	return nil
}

// CleanupExpiredTokens removes all expired tokens
// Returns the number of tokens deleted
func (s *TokenManagementService) CleanupExpiredTokens() (int64, error) {
	count, err := s.tokenRepo.CleanupExpired()
	if err != nil {
		return 0, fmt.Errorf("failed to cleanup expired tokens: %w", err)
	}
	return count, nil
}

// GetStatistics returns statistics about registration tokens
func (s *TokenManagementService) GetStatistics() (map[string]interface{}, error) {
	totalCount, err := s.tokenRepo.Count()
	if err != nil {
		return nil, fmt.Errorf("failed to get total count: %w", err)
	}

	activeCount, err := s.tokenRepo.CountActive()
	if err != nil {
		return nil, fmt.Errorf("failed to get active count: %w", err)
	}

	expiredCount, err := s.tokenRepo.CountExpired()
	if err != nil {
		return nil, fmt.Errorf("failed to get expired count: %w", err)
	}

	return map[string]interface{}{
		"total_tokens":   totalCount,
		"active_tokens":  activeCount,
		"expired_tokens": expiredCount,
	}, nil
}

// validateCreateTokenRequest validates the token creation request
func (s *TokenManagementService) validateCreateTokenRequest(req *CreateTokenRequest) error {
	if req.ExpiresInHours < 1 {
		return fmt.Errorf("expires_in_hours must be at least 1")
	}

	if req.MaxUses != nil && *req.MaxUses < 1 {
		return fmt.Errorf("max_uses must be at least 1")
	}

	// Validate MAC address if provided
	if req.AuthorizedMAC != nil && *req.AuthorizedMAC != "" {
		if err := validators.ValidateMACAddress(*req.AuthorizedMAC, "authorized_mac"); err != nil {
			return err
		}
	}

	return nil
}

// convertToListResponse converts token models to list response format
func (s *TokenManagementService) convertToListResponse(tokens []*models.RegistrationToken) []*TokenListResponse {
	response := make([]*TokenListResponse, len(tokens))
	for i, token := range tokens {
		expiresAt := ""
		if token.ExpiresAt != nil {
			expiresAt = token.ExpiresAt.UTC().Format(time.RFC3339)
		}

		response[i] = &TokenListResponse{
			Token:         token.Token,
			ExpiresAt:     expiresAt,
			MaxUses:       token.UsageLimit,
			UsedCount:     token.UsedCount,
			AuthorizedMAC: token.PreAuthorizedMacAddress,
			Description:   nil, // Model doesn't have Description field
			IsExpired:     token.IsExpired(),
			IsActive:      token.IsActive(),
			CreatedAt:     token.CreatedAt.UTC().Format(time.RFC3339),
		}
	}
	return response
}

// generateSecureToken generates a cryptographically secure random token
// The token is base64-url-encoded for safe use in URLs and JSON
func generateSecureToken(length int) (string, error) {
	bytes := make([]byte, length)
	if _, err := rand.Read(bytes); err != nil {
		return "", fmt.Errorf("failed to generate random bytes: %w", err)
	}

	// Use URL-safe base64 encoding (no padding)
	token := base64.RawURLEncoding.EncodeToString(bytes)
	return token, nil
}
