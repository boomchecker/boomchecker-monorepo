package models

import "time"

// AdminToken represents a JWT token request for admin authentication
// Tokens are valid for 24 hours and can only be requested once per 24 hours
type AdminToken struct {
	ID          string    `gorm:"primaryKey;type:uuid" json:"id"`
	Email       string    `gorm:"not null;index" json:"email"`
	TokenHash   string    `gorm:"not null;uniqueIndex" json:"-"` // SHA-256 hash of JWT token
	RequestedAt time.Time `gorm:"not null" json:"requested_at"`
	ExpiresAt   time.Time `gorm:"not null;index" json:"expires_at"`
	IsUsed      bool      `gorm:"default:false" json:"is_used"`
	UsedAt      *time.Time `json:"used_at,omitempty"`
	CreatedAt   time.Time `gorm:"autoCreateTime" json:"created_at"`
	UpdatedAt   time.Time `gorm:"autoUpdateTime" json:"updated_at"`
}

// TableName specifies the table name for GORM
func (AdminToken) TableName() string {
	return "admin_tokens"
}

// IsExpired checks if the token has expired
func (at *AdminToken) IsExpired() bool {
	return time.Now().After(at.ExpiresAt)
}

// CanRequestNewToken checks if enough time has passed to request a new token
// Returns true if the last token was requested more than 24 hours ago
func CanRequestNewToken(lastRequestedAt time.Time) bool {
	return time.Since(lastRequestedAt) >= 24*time.Hour
}
