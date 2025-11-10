package services

import (
	"crypto/rand"
	"encoding/base64"
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"github.com/boomchecker/api-backend/internal/repositories"
	"github.com/boomchecker/api-backend/internal/validators"
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
	ExpiresInHours   int     `json:"expires_in_hours" binding:"required,min=1"`
	MaxUses          *int    `json:"max_uses,omitempty" binding:"omitempty,min=1"`
	AuthorizedMAC    *string `json:"authorized_mac,omitempty"`
	Description      *string `json:"description,omitempty"`
}

// CreateTokenResponse contains the data returned after creating a token
type CreateTokenResponse struct {
	Token         string     `json:"token"`
	ExpiresAt     time.Time  `json:"expires_at"`
	MaxUses       *int       `json:"max_uses,omitempty"`
	AuthorizedMAC *string    `json:"authorized_mac,omitempty"`
	Description   *string    `json:"description,omitempty"`
	CreatedAt     time.Time  `json:"created_at"`
}

// TokenListResponse contains information about a token for listing
type TokenListResponse struct {
	Token         string     `json:"token"`
	ExpiresAt     time.Time  `json:"expires_at"`
	MaxUses       *int       `json:"max_uses,omitempty"`
	UsedCount     int        `json:"used_count"`
	AuthorizedMAC *string    `json:"authorized_mac,omitempty"`
	Description   *string    `json:"description,omitempty"`
	IsExpired     bool       `json:"is_expired"`
	IsActive      bool       `json:"is_active"`
	CreatedAt     time.Time  `json:"created_at"`
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

	// Calculate expiration time
	now := time.Now().UTC()
	expiresAt := now.Add(time.Duration(req.ExpiresInHours) * time.Hour)

	// Normalize MAC address if provided
	var authorizedMAC *string
	if req.AuthorizedMAC != nil && *req.AuthorizedMAC != "" {
		normalized := validators.NormalizeMACAddress(*req.AuthorizedMAC)
		authorizedMAC = &normalized
	}

	// Create token model
	token := &models.RegistrationToken{
		Token:         tokenValue,
		ExpiresAt:     expiresAt,
		MaxUses:       req.MaxUses,
		UsedCount:     0,
		AuthorizedMac: authorizedMAC,
		Description:   req.Description,
	}

	// Save to database
	if err := s.tokenRepo.Create(token); err != nil {
		return nil, fmt.Errorf("failed to create token: %w", err)
	}

	return &CreateTokenResponse{
		Token:         token.Token,
		ExpiresAt:     token.ExpiresAt,
		MaxUses:       token.MaxUses,
		AuthorizedMAC: token.AuthorizedMac,
		Description:   token.Description,
		CreatedAt:     token.CreatedAt,
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

	return &TokenListResponse{
		Token:         token.Token,
		ExpiresAt:     token.ExpiresAt,
		MaxUses:       token.MaxUses,
		UsedCount:     token.UsedCount,
		AuthorizedMAC: token.AuthorizedMac,
		Description:   token.Description,
		IsExpired:     token.IsExpired(),
		IsActive:      token.IsValid(),
		CreatedAt:     token.CreatedAt,
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
		if !validators.ValidateMACAddress(*req.AuthorizedMAC) {
			return fmt.Errorf("invalid MAC address format: %s", *req.AuthorizedMAC)
		}
	}

	return nil
}

// convertToListResponse converts token models to list response format
func (s *TokenManagementService) convertToListResponse(tokens []*models.RegistrationToken) []*TokenListResponse {
	response := make([]*TokenListResponse, len(tokens))
	for i, token := range tokens {
		response[i] = &TokenListResponse{
			Token:         token.Token,
			ExpiresAt:     token.ExpiresAt,
			MaxUses:       token.MaxUses,
			UsedCount:     token.UsedCount,
			AuthorizedMAC: token.AuthorizedMac,
			Description:   token.Description,
			IsExpired:     token.IsExpired(),
			IsActive:      token.IsValid(),
			CreatedAt:     token.CreatedAt,
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
