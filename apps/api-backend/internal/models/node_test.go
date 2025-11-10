package models

import (
	"testing"
	"time"
)

// TestNodeTableName tests the table name override
func TestNodeTableName(t *testing.T) {
	node := Node{}
	want := "nodes"
	
	if got := node.TableName(); got != want {
		t.Errorf("Node.TableName() = %q, want %q", got, want)
	}
}

// TestNodeIsActive tests node status helper methods
func TestNodeIsActive(t *testing.T) {
	tests := []struct {
		name   string
		status string
		want   bool
	}{
		{"active node", NodeStatusActive, true},
		{"disabled node", NodeStatusDisabled, false},
		{"revoked node", NodeStatusRevoked, false},
		{"empty status", "", false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			node := &Node{Status: tt.status}
			if got := node.IsActive(); got != tt.want {
				t.Errorf("Node.IsActive() with status %q = %v, want %v", tt.status, got, tt.want)
			}
		})
	}
}

// TestNodeIsDisabled tests disabled status check
func TestNodeIsDisabled(t *testing.T) {
	tests := []struct {
		name   string
		status string
		want   bool
	}{
		{"active node", NodeStatusActive, false},
		{"disabled node", NodeStatusDisabled, true},
		{"revoked node", NodeStatusRevoked, false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			node := &Node{Status: tt.status}
			if got := node.IsDisabled(); got != tt.want {
				t.Errorf("Node.IsDisabled() with status %q = %v, want %v", tt.status, got, tt.want)
			}
		})
	}
}

// TestNodeIsRevoked tests revoked status check
func TestNodeIsRevoked(t *testing.T) {
	tests := []struct {
		name   string
		status string
		want   bool
	}{
		{"active node", NodeStatusActive, false},
		{"disabled node", NodeStatusDisabled, false},
		{"revoked node", NodeStatusRevoked, true},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			node := &Node{Status: tt.status}
			if got := node.IsRevoked(); got != tt.want {
				t.Errorf("Node.IsRevoked() with status %q = %v, want %v", tt.status, got, tt.want)
			}
		})
	}
}

// TestNodeStatusConstants tests that status constants are defined
func TestNodeStatusConstants(t *testing.T) {
	if NodeStatusActive != "active" {
		t.Errorf("NodeStatusActive = %q, want %q", NodeStatusActive, "active")
	}
	if NodeStatusDisabled != "disabled" {
		t.Errorf("NodeStatusDisabled = %q, want %q", NodeStatusDisabled, "disabled")
	}
	if NodeStatusRevoked != "revoked" {
		t.Errorf("NodeStatusRevoked = %q, want %q", NodeStatusRevoked, "revoked")
	}
}

// TestNodeCreation tests basic node structure
func TestNodeCreation(t *testing.T) {
	now := time.Now().UTC()
	firmwareVersion := "1.0.0"
	lat := 50.0755
	lng := 14.4378
	
	node := &Node{
		UUID:            "123e4567-e89b-12d3-a456-426614174000",
		MacAddress:      "AA:BB:CC:DD:EE:FF",
		JWTSecret:       "encrypted_secret",
		Status:          NodeStatusActive,
		FirmwareVersion: &firmwareVersion,
		Latitude:        &lat,
		Longitude:       &lng,
		LastSeenAt:      &now,
		CreatedAt:       now,
		UpdatedAt:       now,
	}
	
	// Verify fields are set
	if node.UUID == "" {
		t.Error("Node UUID should not be empty")
	}
	if node.MacAddress == "" {
		t.Error("Node MacAddress should not be empty")
	}
	if !node.IsActive() {
		t.Error("Node should be active")
	}
	if node.FirmwareVersion == nil {
		t.Error("Node FirmwareVersion should not be nil")
	}
	if node.Latitude == nil || node.Longitude == nil {
		t.Error("Node GPS coordinates should not be nil")
	}
}
