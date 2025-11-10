package models

import (
	"time"
)

// Node represents an IoT device registered in the system.
// All timestamps are stored in UTC.
type Node struct {
	// UUID is the server-generated unique identifier for this node (RFC 4122 v4)
	// Format: 550e8400-e29b-41d4-a716-446655440000
	UUID string `gorm:"primaryKey;type:text;not null" json:"uuid"`

	// MacAddress is the device's MAC address (used for registration and duplicate prevention)
	// Format: AA:BB:CC:DD:EE:FF (uppercase, colon-separated)
	MacAddress string `gorm:"type:text;uniqueIndex;not null" json:"mac_address"`

	// Name is an optional user-friendly label for the node
	// Max 100 characters (e.g., "Node-01", "Living Room Sensor")
	Name *string `gorm:"type:text;size:100" json:"name,omitempty"`

	// JWTSecret is the encrypted JWT secret (AES-256-GCM) for signing node JWTs
	// Stored as base64-encoded encrypted data
	JWTSecret string `gorm:"type:text;not null" json:"-"` // Never expose in JSON

	// FirmwareVersion is the semantic version of the node's firmware
	// Format: "1.0.0", "2.1.3-beta"
	FirmwareVersion *string `gorm:"type:text;size:50" json:"firmware_version,omitempty"`

	// Latitude is the GPS latitude for node location tracking
	// Valid range: -90.0 to 90.0
	Latitude *float64 `gorm:"type:real" json:"latitude,omitempty"`

	// Longitude is the GPS longitude for node location tracking
	// Valid range: -180.0 to 180.0
	Longitude *float64 `gorm:"type:real" json:"longitude,omitempty"`

	// LastSeenAt is automatically updated on each authenticated API request
	// Stored in UTC, format: 2025-11-10T14:30:00Z
	LastSeenAt *time.Time `gorm:"type:datetime" json:"last_seen_at,omitempty"`

	// Status represents the node's operational state
	// Valid values: "active" (normal operation), "disabled" (temporarily inactive), "revoked" (permanently banned)
	Status string `gorm:"type:text;not null;default:active" json:"status"`

	// CreatedAt is the node registration timestamp (immutable)
	// Stored in UTC, format: 2025-11-10T14:30:00Z
	CreatedAt time.Time `gorm:"type:datetime;not null" json:"created_at"`

	// UpdatedAt is the last schema update timestamp (auto-updated by GORM)
	// Stored in UTC, format: 2025-11-10T14:30:00Z
	UpdatedAt time.Time `gorm:"type:datetime;not null" json:"updated_at"`
}

// TableName overrides the default table name for GORM
func (Node) TableName() string {
	return "nodes"
}

// BeforeCreate is a GORM hook that ensures timestamps are in UTC
func (n *Node) BeforeCreate(tx interface{}) error {
	n.CreatedAt = time.Now().UTC()
	n.UpdatedAt = time.Now().UTC()
	if n.LastSeenAt != nil {
		utcTime := n.LastSeenAt.UTC()
		n.LastSeenAt = &utcTime
	}
	return nil
}

// BeforeUpdate is a GORM hook that ensures UpdatedAt is in UTC
func (n *Node) BeforeUpdate(tx interface{}) error {
	n.UpdatedAt = time.Now().UTC()
	if n.LastSeenAt != nil {
		utcTime := n.LastSeenAt.UTC()
		n.LastSeenAt = &utcTime
	}
	return nil
}

// NodeStatus constants for type safety
const (
	NodeStatusActive   = "active"
	NodeStatusDisabled = "disabled"
	NodeStatusRevoked  = "revoked"
)

// IsActive returns true if the node is in active status
func (n *Node) IsActive() bool {
	return n.Status == NodeStatusActive
}

// IsDisabled returns true if the node is in disabled status
func (n *Node) IsDisabled() bool {
	return n.Status == NodeStatusDisabled
}

// IsRevoked returns true if the node is in revoked status
func (n *Node) IsRevoked() bool {
	return n.Status == NodeStatusRevoked
}
