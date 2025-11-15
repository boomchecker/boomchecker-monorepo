package repositories

import (
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"gorm.io/gorm"
)

// AdminTokenRepository handles database operations for admin tokens
type AdminTokenRepository struct {
	db *gorm.DB
}

// NewAdminTokenRepository creates a new admin token repository instance
func NewAdminTokenRepository(db *gorm.DB) *AdminTokenRepository {
	return &AdminTokenRepository{db: db}
}

// Create inserts a new admin token into the database
func (r *AdminTokenRepository) Create(token *models.AdminToken) error {
	if token == nil {
		return fmt.Errorf("token cannot be nil")
	}

	// Ensure timestamps are set in UTC
	now := time.Now().UTC()
	token.CreatedAt = now
	token.UpdatedAt = now

	if err := r.db.Create(token).Error; err != nil {
		return fmt.Errorf("failed to create admin token: %w", err)
	}

	return nil
}

// FindByTokenHash retrieves an admin token by its token hash
// Returns gorm.ErrRecordNotFound if token doesn't exist
func (r *AdminTokenRepository) FindByTokenHash(tokenHash string) (*models.AdminToken, error) {
	if tokenHash == "" {
		return nil, fmt.Errorf("token hash is required")
	}

	var token models.AdminToken
	if err := r.db.Where("token_hash = ?", tokenHash).First(&token).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("token not found")
		}
		return nil, fmt.Errorf("failed to find token: %w", err)
	}

	return &token, nil
}

// GetLastRequestByEmail retrieves the most recent token request for a given email
// Returns nil if no previous requests found
func (r *AdminTokenRepository) GetLastRequestByEmail(email string) (*models.AdminToken, error) {
	if email == "" {
		return nil, fmt.Errorf("email is required")
	}

	var token models.AdminToken
	if err := r.db.Where("email = ?", email).
		Order("requested_at DESC").
		First(&token).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, nil // No previous requests found
		}
		return nil, fmt.Errorf("failed to get last request: %w", err)
	}

	return &token, nil
}

// MarkAsUsed marks a token as used with the current timestamp
func (r *AdminTokenRepository) MarkAsUsed(tokenHash string) error {
	if tokenHash == "" {
		return fmt.Errorf("token hash is required")
	}

	now := time.Now().UTC()
	result := r.db.Model(&models.AdminToken{}).
		Where("token_hash = ?", tokenHash).
		Updates(map[string]interface{}{
			"is_used":    true,
			"used_at":    now,
			"updated_at": now,
		})

	if result.Error != nil {
		return fmt.Errorf("failed to mark token as used: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("token not found")
	}

	return nil
}

// ValidateToken checks if a token is valid for use
// A token is valid if:
// - It exists
// - It hasn't expired
// Note: IsUsed field is for tracking only, not for validation
// Admin tokens can be used multiple times during their 24-hour validity period
func (r *AdminTokenRepository) ValidateToken(tokenHash string) (*models.AdminToken, error) {
	if tokenHash == "" {
		return nil, fmt.Errorf("token hash is required")
	}

	token, err := r.FindByTokenHash(tokenHash)
	if err != nil {
		return nil, err
	}

	// Check expiration
	if token.IsExpired() {
		return nil, fmt.Errorf("token has expired")
	}

	// Token is valid - IsUsed is only for tracking first use, not for preventing reuse
	return token, nil
}

// CleanupExpired removes expired tokens from the database
// Returns the number of tokens deleted
// Use this periodically to keep the database clean
func (r *AdminTokenRepository) CleanupExpired() (int64, error) {
	now := time.Now().UTC()

	result := r.db.Where("expires_at < ?", now).Delete(&models.AdminToken{})
	if result.Error != nil {
		return 0, fmt.Errorf("failed to cleanup expired tokens: %w", result.Error)
	}

	return result.RowsAffected, nil
}

// ListByEmail retrieves all tokens for a given email
// Ordered by request date (newest first)
func (r *AdminTokenRepository) ListByEmail(email string) ([]*models.AdminToken, error) {
	if email == "" {
		return nil, fmt.Errorf("email is required")
	}

	var tokens []*models.AdminToken
	if err := r.db.Where("email = ?", email).
		Order("requested_at DESC").
		Find(&tokens).Error; err != nil {
		return nil, fmt.Errorf("failed to list tokens by email: %w", err)
	}

	return tokens, nil
}

// ListAll retrieves all admin tokens
// Ordered by request date (newest first)
func (r *AdminTokenRepository) ListAll() ([]*models.AdminToken, error) {
	var tokens []*models.AdminToken
	if err := r.db.Order("requested_at DESC").Find(&tokens).Error; err != nil {
		return nil, fmt.Errorf("failed to list all tokens: %w", err)
	}

	return tokens, nil
}

// CountByEmail returns the number of tokens for a given email
func (r *AdminTokenRepository) CountByEmail(email string) (int64, error) {
	if email == "" {
		return 0, fmt.Errorf("email is required")
	}

	var count int64
	if err := r.db.Model(&models.AdminToken{}).
		Where("email = ?", email).
		Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count tokens by email: %w", err)
	}

	return count, nil
}

// Count returns the total number of admin tokens
func (r *AdminTokenRepository) Count() (int64, error) {
	var count int64
	if err := r.db.Model(&models.AdminToken{}).Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count tokens: %w", err)
	}

	return count, nil
}

// InvalidateAllForEmail invalidates all active tokens for a given email
// This is called when a new token is requested to ensure only the latest token is valid
func (r *AdminTokenRepository) InvalidateAllForEmail(email string) (int64, error) {
	if email == "" {
		return 0, fmt.Errorf("email is required")
	}

	now := time.Now().UTC()

	// Set ExpiresAt to now for all active tokens (not expired yet)
	result := r.db.Model(&models.AdminToken{}).
		Where("email = ?", email).
		Where("expires_at > ?", now).
		Update("expires_at", now)

	if result.Error != nil {
		return 0, fmt.Errorf("failed to invalidate tokens: %w", result.Error)
	}

	return result.RowsAffected, nil
}
