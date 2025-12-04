package models

import (
	"strings"
	"time"

	"gorm.io/gorm"
)

// RegistrationToken stores one-time or limited-use tokens for node registration.
// Admins generate these tokens to control which nodes can register.
// All timestamps are stored in UTC.
type RegistrationToken struct {
	// ID is the internal token identifier (UUID)
	// Separate from Token field for security reasons
	ID string `gorm:"primaryKey;type:text;not null" json:"id"`

	// Token is the actual registration token shared with node operator
	// Format: UUID (e.g., a1b2c3d4-e5f6-7890-abcd-ef1234567890)
	Token string `gorm:"type:text;not null;uniqueIndex" json:"token"`

	// ExpiresAt is the optional expiration timestamp
	// If NULL, token never expires
	// Stored in UTC, format: 2025-12-31T23:59:59Z
	ExpiresAt *time.Time `gorm:"type:datetime" json:"expires_at,omitempty"`

	// UsageLimit is the maximum number of times this token can be used
	// NULL or 0 = unlimited uses
	// Positive N = max N uses
	UsageLimit *int `gorm:"type:integer" json:"usage_limit,omitempty"`

	// UsedCount is incremented each time token is successfully used for registration
	UsedCount int `gorm:"type:integer;not null;default:0" json:"used_count"`

	// PreAuthorizedMacAddress optionally restricts token to a specific MAC address
	// If set, token can only register this MAC address
	// Format: AA:BB:CC:DD:EE:FF (uppercase, colon-separated)
	// NOTE: This is a soft reference - the MAC address doesn't need to exist yet in nodes table
	PreAuthorizedMacAddress *string `gorm:"type:text" json:"pre_authorized_mac_address,omitempty"`

	// CreatedAt is the token creation timestamp
	// Stored in UTC, format: 2025-11-10T14:30:00Z
	CreatedAt time.Time `gorm:"type:datetime;not null" json:"created_at"`

	// UpdatedAt is the last modification timestamp
	// Stored in UTC, format: 2025-11-10T14:30:00Z
	UpdatedAt time.Time `gorm:"type:datetime;not null" json:"updated_at"`
}

// TableName overrides the default table name for GORM
func (RegistrationToken) TableName() string {
	return "registration_tokens"
}

// BeforeCreate is a GORM hook that ensures timestamps are in UTC
func (rt *RegistrationToken) BeforeCreate(tx *gorm.DB) error {
	rt.CreatedAt = time.Now().UTC()
	rt.UpdatedAt = time.Now().UTC()
	if rt.ExpiresAt != nil {
		utcTime := rt.ExpiresAt.UTC()
		rt.ExpiresAt = &utcTime
	}
	return nil
}

// BeforeUpdate is a GORM hook that ensures UpdatedAt is in UTC
func (rt *RegistrationToken) BeforeUpdate(tx *gorm.DB) error {
	rt.UpdatedAt = time.Now().UTC()
	if rt.ExpiresAt != nil {
		utcTime := rt.ExpiresAt.UTC()
		rt.ExpiresAt = &utcTime
	}
	return nil
}

// IsExpired checks if the token has expired
// Returns false if ExpiresAt is NULL (never expires)
func (rt *RegistrationToken) IsExpired() bool {
	if rt.ExpiresAt == nil {
		return false
	}
	return time.Now().UTC().After(*rt.ExpiresAt)
}

// HasRemainingUses checks if the token has remaining uses
// Returns true if:
// - UsageLimit is NULL (unlimited)
// - UsageLimit is 0 (unlimited)
// - UsedCount < UsageLimit
func (rt *RegistrationToken) HasRemainingUses() bool {
	if rt.UsageLimit == nil || *rt.UsageLimit == 0 {
		return true // Unlimited uses
	}
	return rt.UsedCount < *rt.UsageLimit
}

// IsValid checks if the token is valid (not expired and has remaining uses)
func (rt *RegistrationToken) IsValid() bool {
	return !rt.IsExpired() && rt.HasRemainingUses()
}

// CanBeUsedForMac checks if the token can be used for a specific MAC address
// Returns true if:
// - PreAuthorizedMacAddress is NULL (no restriction)
// - PreAuthorizedMacAddress matches the provided MAC (case-insensitive)
func (rt *RegistrationToken) CanBeUsedForMac(macAddress string) bool {
	if rt.PreAuthorizedMacAddress == nil {
		return true // No MAC restriction
	}
	return strings.EqualFold(*rt.PreAuthorizedMacAddress, macAddress)
}
