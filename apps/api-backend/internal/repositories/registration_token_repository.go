package repositories

import (
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"gorm.io/gorm"
)

// RegistrationTokenRepository handles database operations for registration tokens
type RegistrationTokenRepository struct {
	db *gorm.DB
}

// NewRegistrationTokenRepository creates a new registration token repository instance
func NewRegistrationTokenRepository(db *gorm.DB) *RegistrationTokenRepository {
	return &RegistrationTokenRepository{db: db}
}

// Create inserts a new registration token into the database
// Returns error if token with same value already exists
func (r *RegistrationTokenRepository) Create(token *models.RegistrationToken) error {
	if token == nil {
		return fmt.Errorf("token cannot be nil")
	}

	// Check for duplicate token value
	if err := r.checkDuplicateToken(token.Token); err != nil {
		return err
	}

	// Ensure timestamps are set in UTC
	now := time.Now().UTC()
	token.CreatedAt = now
	token.UpdatedAt = now

	if err := r.db.Create(token).Error; err != nil {
		return fmt.Errorf("failed to create registration token: %w", err)
	}

	return nil
}

// FindByToken retrieves a registration token by its token value
// Returns gorm.ErrRecordNotFound if token doesn't exist
func (r *RegistrationTokenRepository) FindByToken(tokenValue string) (*models.RegistrationToken, error) {
	if tokenValue == "" {
		return nil, fmt.Errorf("token value is required")
	}

	var token models.RegistrationToken
	if err := r.db.Where("token = ?", tokenValue).First(&token).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("token not found: %s", tokenValue)
		}
		return nil, fmt.Errorf("failed to find token: %w", err)
	}

	return &token, nil
}

// IncrementUsedCount increments the used_count for a token
// This is called each time a token is successfully used for registration
func (r *RegistrationTokenRepository) IncrementUsedCount(tokenValue string) error {
	if tokenValue == "" {
		return fmt.Errorf("token value is required")
	}

	result := r.db.Model(&models.RegistrationToken{}).
		Where("token = ?", tokenValue).
		Updates(map[string]interface{}{
			"used_count": gorm.Expr("used_count + 1"),
			"updated_at": time.Now().UTC(),
		})

	if result.Error != nil {
		return fmt.Errorf("failed to increment used count: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("token not found: %s", tokenValue)
	}

	return nil
}

// ValidateToken checks if a token is valid for use
// A token is valid if:
// - It exists
// - It hasn't expired
// - It has remaining uses (or is unlimited)
// - If mac is provided, it matches the authorized MAC (if any)
func (r *RegistrationTokenRepository) ValidateToken(tokenValue string, macAddress *string) (*models.RegistrationToken, error) {
	if tokenValue == "" {
		return nil, fmt.Errorf("token value is required")
	}

	token, err := r.FindByToken(tokenValue)
	if err != nil {
		return nil, err
	}

	// Check expiration
	if token.IsExpired() {
		return nil, fmt.Errorf("token has expired")
	}

	// Check remaining uses
	if !token.HasRemainingUses() {
		return nil, fmt.Errorf("token has no remaining uses")
	}

	// Check MAC authorization if MAC is provided
	if macAddress != nil {
		if !token.CanBeUsedForMac(*macAddress) {
			return nil, fmt.Errorf("token cannot be used for MAC address: %s", *macAddress)
		}
	}

	return token, nil
}

// CleanupExpired removes expired tokens from the database
// Returns the number of tokens deleted
// Use this periodically to keep the database clean
func (r *RegistrationTokenRepository) CleanupExpired() (int64, error) {
	now := time.Now().UTC()

	result := r.db.Where("expires_at < ?", now).Delete(&models.RegistrationToken{})
	if result.Error != nil {
		return 0, fmt.Errorf("failed to cleanup expired tokens: %w", result.Error)
	}

	return result.RowsAffected, nil
}

// ListAll retrieves all registration tokens
// Ordered by creation date (newest first)
func (r *RegistrationTokenRepository) ListAll() ([]*models.RegistrationToken, error) {
	var tokens []*models.RegistrationToken
	if err := r.db.Order("created_at DESC").Find(&tokens).Error; err != nil {
		return nil, fmt.Errorf("failed to list all tokens: %w", err)
	}

	return tokens, nil
}

// ListActive retrieves all non-expired tokens with remaining uses
func (r *RegistrationTokenRepository) ListActive() ([]*models.RegistrationToken, error) {
	now := time.Now().UTC()

	var tokens []*models.RegistrationToken
	// Find tokens that are not expired and either unlimited or have remaining uses
	if err := r.db.Where("expires_at > ?", now).
		Where("max_uses IS NULL OR used_count < max_uses").
		Order("created_at DESC").
		Find(&tokens).Error; err != nil {
		return nil, fmt.Errorf("failed to list active tokens: %w", err)
	}

	return tokens, nil
}

// FindByMacAddress retrieves all tokens authorized for a specific MAC address
func (r *RegistrationTokenRepository) FindByMacAddress(macAddress string) ([]*models.RegistrationToken, error) {
	if macAddress == "" {
		return nil, fmt.Errorf("mac address is required")
	}

	var tokens []*models.RegistrationToken
	if err := r.db.Where("authorized_mac = ?", macAddress).
		Order("created_at DESC").
		Find(&tokens).Error; err != nil {
		return nil, fmt.Errorf("failed to find tokens by MAC address: %w", err)
	}

	return tokens, nil
}

// Delete permanently removes a token from the database
// WARNING: This cannot be undone
func (r *RegistrationTokenRepository) Delete(tokenValue string) error {
	if tokenValue == "" {
		return fmt.Errorf("token value is required")
	}

	result := r.db.Where("token = ?", tokenValue).Delete(&models.RegistrationToken{})
	if result.Error != nil {
		return fmt.Errorf("failed to delete token: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("token not found: %s", tokenValue)
	}

	return nil
}

// Update updates an existing token
// Typically used to update metadata or extend expiration
func (r *RegistrationTokenRepository) Update(token *models.RegistrationToken) error {
	if token == nil {
		return fmt.Errorf("token cannot be nil")
	}
	if token.Token == "" {
		return fmt.Errorf("token value is required")
	}

	// Ensure UpdatedAt is current
	token.UpdatedAt = time.Now().UTC()

	result := r.db.Model(&models.RegistrationToken{}).
		Where("token = ?", token.Token).
		Updates(token)

	if result.Error != nil {
		return fmt.Errorf("failed to update token: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("token not found: %s", token.Token)
	}

	return nil
}

// Exists checks if a token exists in the database
func (r *RegistrationTokenRepository) Exists(tokenValue string) (bool, error) {
	if tokenValue == "" {
		return false, fmt.Errorf("token value is required")
	}

	var count int64
	if err := r.db.Model(&models.RegistrationToken{}).
		Where("token = ?", tokenValue).
		Count(&count).Error; err != nil {
		return false, fmt.Errorf("failed to check token existence: %w", err)
	}

	return count > 0, nil
}

// Count returns the total number of tokens
func (r *RegistrationTokenRepository) Count() (int64, error) {
	var count int64
	if err := r.db.Model(&models.RegistrationToken{}).Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count tokens: %w", err)
	}

	return count, nil
}

// CountActive returns the number of non-expired tokens with remaining uses
func (r *RegistrationTokenRepository) CountActive() (int64, error) {
	now := time.Now().UTC()

	var count int64
	if err := r.db.Model(&models.RegistrationToken{}).
		Where("expires_at > ?", now).
		Where("max_uses IS NULL OR used_count < max_uses").
		Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count active tokens: %w", err)
	}

	return count, nil
}

// CountExpired returns the number of expired tokens
func (r *RegistrationTokenRepository) CountExpired() (int64, error) {
	now := time.Now().UTC()

	var count int64
	if err := r.db.Model(&models.RegistrationToken{}).
		Where("expires_at < ?", now).
		Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count expired tokens: %w", err)
	}

	return count, nil
}

// Helper functions

func (r *RegistrationTokenRepository) checkDuplicateToken(tokenValue string) error {
	exists, err := r.Exists(tokenValue)
	if err != nil {
		return err
	}
	if exists {
		return fmt.Errorf("token already exists: %s", tokenValue)
	}
	return nil
}
