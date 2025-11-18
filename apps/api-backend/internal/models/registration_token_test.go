package models

import (
	"testing"
	"time"
)

// TestRegistrationTokenTableName tests the table name override
func TestRegistrationTokenTableName(t *testing.T) {
	token := RegistrationToken{}
	want := "registration_tokens"
	
	if got := token.TableName(); got != want {
		t.Errorf("RegistrationToken.TableName() = %q, want %q", got, want)
	}
}

// TestRegistrationTokenIsExpired tests expiration check
func TestRegistrationTokenIsExpired(t *testing.T) {
	now := time.Now().UTC()
	past := now.Add(-1 * time.Hour)
	future := now.Add(1 * time.Hour)
	
	tests := []struct {
		name      string
		expiresAt *time.Time
		want      bool
	}{
		{"expired token", &past, true},
		{"valid token", &future, false},
		{"no expiration", nil, false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			token := &RegistrationToken{ExpiresAt: tt.expiresAt}
			if got := token.IsExpired(); got != tt.want {
				t.Errorf("RegistrationToken.IsExpired() = %v, want %v", got, tt.want)
			}
		})
	}
}

// TestRegistrationTokenHasRemainingUses tests usage limit check
func TestRegistrationTokenHasRemainingUses(t *testing.T) {
	maxUses5 := 5
	maxUses10 := 10
	
	tests := []struct {
		name      string
		usageLimit *int
		usedCount int
		want      bool
	}{
		{"unlimited token", nil, 100, true},
		{"has remaining uses", &maxUses10, 5, true},
		{"exactly at limit", &maxUses5, 5, false},
		{"over limit", &maxUses5, 6, false},
		{"unused with limit", &maxUses5, 0, true},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			token := &RegistrationToken{
				UsageLimit: tt.usageLimit,
				UsedCount:  tt.usedCount,
			}
			if got := token.HasRemainingUses(); got != tt.want {
				t.Errorf("RegistrationToken.HasRemainingUses() = %v, want %v (limit=%v, used=%d)", 
					got, tt.want, tt.usageLimit, tt.usedCount)
			}
		})
	}
}

// TestRegistrationTokenIsValid tests overall token validity
func TestRegistrationTokenIsValid(t *testing.T) {
	now := time.Now().UTC()
	past := now.Add(-1 * time.Hour)
	future := now.Add(1 * time.Hour)
	maxUses5 := 5
	
	tests := []struct {
		name       string
		expiresAt  *time.Time
		usageLimit *int
		usedCount  int
		want       bool
	}{
		{"valid unlimited token", &future, nil, 100, true},
		{"valid with remaining uses", &future, &maxUses5, 3, true},
		{"expired token", &past, &maxUses5, 0, false},
		{"no remaining uses", &future, &maxUses5, 5, false},
		{"expired and exhausted", &past, &maxUses5, 5, false},
		{"no expiration unlimited", nil, nil, 100, true},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			token := &RegistrationToken{
				ExpiresAt:  tt.expiresAt,
				UsageLimit: tt.usageLimit,
				UsedCount:  tt.usedCount,
			}
			if got := token.IsValid(); got != tt.want {
				t.Errorf("RegistrationToken.IsValid() = %v, want %v", got, tt.want)
			}
		})
	}
}

// TestRegistrationTokenCanBeUsedForMac tests MAC authorization check
func TestRegistrationTokenCanBeUsedForMac(t *testing.T) {
	authorizedMAC := "AA:BB:CC:DD:EE:FF"
	
	tests := []struct {
		name          string
		authorizedMac *string
		requestMac    string
		want          bool
	}{
		{"no MAC restriction", nil, "11:22:33:44:55:66", true},
		{"matching MAC", &authorizedMAC, "AA:BB:CC:DD:EE:FF", true},
		{"non-matching MAC", &authorizedMAC, "11:22:33:44:55:66", false},
		{"case insensitive match", &authorizedMAC, "aa:bb:cc:dd:ee:ff", true},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			token := &RegistrationToken{
				PreAuthorizedMacAddress: tt.authorizedMac,
			}
			if got := token.CanBeUsedForMac(tt.requestMac); got != tt.want {
				t.Errorf("RegistrationToken.CanBeUsedForMac(%q) = %v, want %v", tt.requestMac, got, tt.want)
			}
		})
	}
}

// TestRegistrationTokenCreation tests basic token structure
func TestRegistrationTokenCreation(t *testing.T) {
	now := time.Now().UTC()
	expiresAt := now.Add(24 * time.Hour)
	maxUses := 10
	authorizedMAC := "AA:BB:CC:DD:EE:FF"
	
	token := &RegistrationToken{
		ID:                      "token-id-123",
		Token:                   "secure_random_token_value",
		ExpiresAt:               &expiresAt,
		UsageLimit:              &maxUses,
		UsedCount:               0,
		PreAuthorizedMacAddress: &authorizedMAC,
		CreatedAt:               now,
		UpdatedAt:               now,
	}
	
	// Verify fields are set
	if token.ID == "" {
		t.Error("Token ID should not be empty")
	}
	if token.Token == "" {
		t.Error("Token value should not be empty")
	}
	if token.ExpiresAt == nil {
		t.Error("Token ExpiresAt should not be nil")
	}
	if !token.IsValid() {
		t.Error("Token should be valid")
	}
	if !token.HasRemainingUses() {
		t.Error("Token should have remaining uses")
	}
	if token.IsExpired() {
		t.Error("Token should not be expired")
	}
}
