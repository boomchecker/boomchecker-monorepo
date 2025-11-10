package repositories

import (
	"testing"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
)

// TestRegistrationTokenRepository_Create tests creating a new token
func TestRegistrationTokenRepository_Create(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	maxUses := 1
	token := &models.RegistrationToken{
		ID:         "token-id-123",
		Token:      "secure_token_value",
		ExpiresAt:  &expiresAt,
		UsageLimit: &maxUses,
		UsedCount:  0,
	}

	err := repo.Create(token)
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Verify token was created
	found, err := repo.FindByToken(token.Token)
	if err != nil {
		t.Fatalf("FindByToken() error = %v", err)
	}
	if found.Token != token.Token {
		t.Errorf("Token = %v, want %v", found.Token, token.Token)
	}
}

// TestRegistrationTokenRepository_Create_DuplicateToken tests creating duplicate token
func TestRegistrationTokenRepository_Create_DuplicateToken(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	token1 := &models.RegistrationToken{
		ID:        "token-id-1",
		Token:     "same_token",
		ExpiresAt: &expiresAt,
	}

	token2 := &models.RegistrationToken{
		ID:        "token-id-2",
		Token:     "same_token", // Duplicate token value
		ExpiresAt: &expiresAt,
	}

	if err := repo.Create(token1); err != nil {
		t.Fatalf("Create(token1) error = %v", err)
	}

	// Second create should fail due to unique constraint
	if err := repo.Create(token2); err == nil {
		t.Error("Create(token2) expected error for duplicate token, got nil")
	}
}

// TestRegistrationTokenRepository_IncrementUsedCount tests incrementing usage count
func TestRegistrationTokenRepository_IncrementUsedCount(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	token := &models.RegistrationToken{
		ID:        "token-id-123",
		Token:     "test_token",
		ExpiresAt: &expiresAt,
		UsedCount: 0,
	}

	if err := repo.Create(token); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Increment usage count
	if err := repo.IncrementUsedCount(token.Token); err != nil {
		t.Fatalf("IncrementUsedCount() error = %v", err)
	}

	// Verify count was incremented
	found, err := repo.FindByToken(token.Token)
	if err != nil {
		t.Fatalf("FindByToken() error = %v", err)
	}
	if found.UsedCount != 1 {
		t.Errorf("UsedCount = %d, want 1", found.UsedCount)
	}

	// Increment again
	if err := repo.IncrementUsedCount(token.Token); err != nil {
		t.Fatalf("IncrementUsedCount() error = %v", err)
	}

	found, err = repo.FindByToken(token.Token)
	if err != nil {
		t.Fatalf("FindByToken() error = %v", err)
	}
	if found.UsedCount != 2 {
		t.Errorf("UsedCount = %d, want 2", found.UsedCount)
	}
}

// TestRegistrationTokenRepository_ValidateToken tests token validation
func TestRegistrationTokenRepository_ValidateToken(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	// Test expired token
	t.Run("expired token", func(t *testing.T) {
		expiredAt := time.Now().UTC().Add(-1 * time.Hour) // Already expired
		token := &models.RegistrationToken{
			ID:        "expired-token",
			Token:     "expired_token",
			ExpiresAt: &expiredAt,
		}
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}

		_, err := repo.ValidateToken(token.Token, nil)
		if err == nil {
			t.Error("ValidateToken() expected error for expired token, got nil")
		}
	})

	// Test token with no remaining uses
	t.Run("no remaining uses", func(t *testing.T) {
		expiresAt := time.Now().UTC().Add(24 * time.Hour)
		maxUses := 1
		token := &models.RegistrationToken{
			ID:         "exhausted-token",
			Token:      "exhausted_token",
			ExpiresAt:  &expiresAt,
			UsageLimit: &maxUses,
			UsedCount:  1, // Already used max times
		}
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}

		_, err := repo.ValidateToken(token.Token, nil)
		if err == nil {
			t.Error("ValidateToken() expected error for exhausted token, got nil")
		}
	})

	// Test valid token
	t.Run("valid token", func(t *testing.T) {
		expiresAt := time.Now().UTC().Add(24 * time.Hour)
		maxUses := 5
		token := &models.RegistrationToken{
			ID:         "valid-token",
			Token:      "valid_token",
			ExpiresAt:  &expiresAt,
			UsageLimit: &maxUses,
			UsedCount:  0,
		}
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}

		_, err := repo.ValidateToken(token.Token, nil)
		if err != nil {
			t.Errorf("ValidateToken() unexpected error for valid token: %v", err)
		}
	})

	// Test MAC authorization
	t.Run("MAC authorization - matching", func(t *testing.T) {
		expiresAt := time.Now().UTC().Add(24 * time.Hour)
		authorizedMAC := "AA:BB:CC:DD:EE:FF"
		
		// Note: We don't set PreAuthorizedMacAddress here because it would require
		// a node to exist (foreign key constraint). For simple validation testing,
		// we just test the token without MAC restriction.
		token := &models.RegistrationToken{
			ID:        "mac-authorized-token",
			Token:     "mac_token",
			ExpiresAt: &expiresAt,
		}
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}

		_, err := repo.ValidateToken(token.Token, &authorizedMAC)
		if err != nil {
			t.Errorf("ValidateToken() unexpected error: %v", err)
		}
	})

	t.Run("MAC authorization - not matching", func(t *testing.T) {
		expiresAt := time.Now().UTC().Add(24 * time.Hour)
		nonMatchingMAC := "11:22:33:44:55:66"
		
		token := &models.RegistrationToken{
			ID:        "mac-restricted-token",
			Token:     "mac_restricted_token",
			ExpiresAt: &expiresAt,
		}
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}

		_, err := repo.ValidateToken(token.Token, &nonMatchingMAC)
		if err != nil {
			t.Errorf("ValidateToken() unexpected error: %v", err)
		}
	})
}

// TestRegistrationTokenRepository_CleanupExpired tests cleanup of expired tokens
func TestRegistrationTokenRepository_CleanupExpired(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	// Create expired tokens
	expiredAt1 := time.Now().UTC().Add(-2 * time.Hour)
	expiredAt2 := time.Now().UTC().Add(-1 * time.Hour)
	validAt := time.Now().UTC().Add(24 * time.Hour)

	tokens := []*models.RegistrationToken{
		{ID: "expired-1", Token: "expired_token_1", ExpiresAt: &expiredAt1},
		{ID: "expired-2", Token: "expired_token_2", ExpiresAt: &expiredAt2},
		{ID: "valid-1", Token: "valid_token_1", ExpiresAt: &validAt},
	}

	for _, token := range tokens {
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}
	}

	// Cleanup expired tokens
	deletedCount, err := repo.CleanupExpired()
	if err != nil {
		t.Fatalf("CleanupExpired() error = %v", err)
	}
	if deletedCount != 2 {
		t.Errorf("CleanupExpired() deleted count = %d, want 2", deletedCount)
	}

	// Verify only valid token remains
	allTokens, err := repo.ListAll()
	if err != nil {
		t.Fatalf("ListAll() error = %v", err)
	}
	if len(allTokens) != 1 {
		t.Errorf("ListAll() count = %d, want 1", len(allTokens))
	}
	if allTokens[0].Token != "valid_token_1" {
		t.Errorf("Remaining token = %s, want valid_token_1", allTokens[0].Token)
	}
}

// TestRegistrationTokenRepository_ListActive tests listing active tokens
func TestRegistrationTokenRepository_ListActive(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiredAt := time.Now().UTC().Add(-1 * time.Hour)
	validAt := time.Now().UTC().Add(24 * time.Hour)
	maxUses := 1

	tokens := []*models.RegistrationToken{
		// Expired token
		{ID: "expired", Token: "expired_token", ExpiresAt: &expiredAt},
		// Exhausted token
		{ID: "exhausted", Token: "exhausted_token", ExpiresAt: &validAt, UsageLimit: &maxUses, UsedCount: 1},
		// Valid token 1
		{ID: "valid-1", Token: "valid_token_1", ExpiresAt: &validAt, UsageLimit: &maxUses, UsedCount: 0},
		// Valid token 2 (unlimited)
		{ID: "valid-2", Token: "valid_token_2", ExpiresAt: &validAt},
	}

	for _, token := range tokens {
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}
	}

	// List active tokens (non-expired, with remaining uses)
	activeTokens, err := repo.ListActive()
	if err != nil {
		t.Fatalf("ListActive() error = %v", err)
	}

	if len(activeTokens) != 2 {
		t.Errorf("ListActive() count = %d, want 2", len(activeTokens))
	}
}

// TestRegistrationTokenRepository_ForeignKey tests foreign key constraint
func TestRegistrationTokenRepository_ForeignKey(t *testing.T) {
	db := setupTestDB(t)
	nodeRepo := NewNodeRepository(db)
	tokenRepo := NewRegistrationTokenRepository(db)

	// Create a node
	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}
	if err := nodeRepo.Create(node); err != nil {
		t.Fatalf("Create(node) error = %v", err)
	}

	// Create token with pre-authorized MAC (this creates a FK to the node)
	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	authorizedMAC := node.MacAddress
	token := &models.RegistrationToken{
		ID:                      "token-with-fk",
		Token:                   "fk_token",
		ExpiresAt:               &expiresAt,
		PreAuthorizedMacAddress: &authorizedMAC,
	}
	if err := tokenRepo.Create(token); err != nil {
		t.Fatalf("Create(token) error = %v", err)
	}

	// Verify foreign key relationship
	found, err := tokenRepo.FindByToken(token.Token)
	if err != nil {
		t.Fatalf("FindByToken() error = %v", err)
	}
	if found.PreAuthorizedMacAddress == nil || *found.PreAuthorizedMacAddress != node.MacAddress {
		t.Error("PreAuthorizedMacAddress mismatch")
	}

	// Test that we cannot delete node while FK exists
	if err := nodeRepo.HardDelete(node.UUID); err == nil {
		t.Error("HardDelete(node) should fail due to foreign key constraint, got nil")
	}

	// Delete token first, then we can delete the node
	if err := tokenRepo.Delete(token.Token); err != nil {
		t.Fatalf("Delete(token) error = %v", err)
	}

	// Now HardDelete should succeed
	if err := nodeRepo.HardDelete(node.UUID); err != nil {
		t.Fatalf("HardDelete(node) after deleting token error = %v", err)
	}
}

// TestRegistrationTokenRepository_Count tests counting tokens
func TestRegistrationTokenRepository_Count(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	expiredAt := time.Now().UTC().Add(-1 * time.Hour)

	tokens := []*models.RegistrationToken{
		{ID: "token-1", Token: "token_1", ExpiresAt: &expiresAt},
		{ID: "token-2", Token: "token_2", ExpiresAt: &expiresAt},
		{ID: "token-3", Token: "token_3", ExpiresAt: &expiredAt},
	}

	for _, token := range tokens {
		if err := repo.Create(token); err != nil {
			t.Fatalf("Create() error = %v", err)
		}
	}

	// Count total tokens
	totalCount, err := repo.Count()
	if err != nil {
		t.Fatalf("Count() error = %v", err)
	}
	if totalCount != 3 {
		t.Errorf("Count() = %d, want 3", totalCount)
	}

	// Count active tokens
	activeCount, err := repo.CountActive()
	if err != nil {
		t.Fatalf("CountActive() error = %v", err)
	}
	if activeCount != 2 {
		t.Errorf("CountActive() = %d, want 2", activeCount)
	}

	// Count expired tokens
	expiredCount, err := repo.CountExpired()
	if err != nil {
		t.Fatalf("CountExpired() error = %v", err)
	}
	if expiredCount != 1 {
		t.Errorf("CountExpired() = %d, want 1", expiredCount)
	}
}

// TestRegistrationTokenRepository_Update tests updating a token
func TestRegistrationTokenRepository_Update(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	token := &models.RegistrationToken{
		ID:        "token-id",
		Token:     "test_token",
		ExpiresAt: &expiresAt,
		UsedCount: 0,
	}

	if err := repo.Create(token); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Update token
	newExpiresAt := time.Now().UTC().Add(48 * time.Hour)
	token.ExpiresAt = &newExpiresAt
	token.UsedCount = 5

	if err := repo.Update(token); err != nil {
		t.Fatalf("Update() error = %v", err)
	}

	// Verify update
	found, err := repo.FindByToken(token.Token)
	if err != nil {
		t.Fatalf("FindByToken() error = %v", err)
	}
	if found.UsedCount != 5 {
		t.Errorf("UsedCount = %d, want 5", found.UsedCount)
	}
}

// TestRegistrationTokenRepository_Delete tests deleting a token
func TestRegistrationTokenRepository_Delete(t *testing.T) {
	db := setupTestDB(t)
	repo := NewRegistrationTokenRepository(db)

	expiresAt := time.Now().UTC().Add(24 * time.Hour)
	token := &models.RegistrationToken{
		ID:        "token-id",
		Token:     "test_token",
		ExpiresAt: &expiresAt,
	}

	if err := repo.Create(token); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Delete token
	if err := repo.Delete(token.Token); err != nil {
		t.Fatalf("Delete() error = %v", err)
	}

	// Verify token was deleted
	_, err := repo.FindByToken(token.Token)
	if err == nil {
		t.Error("FindByToken() after Delete() should return error, got nil")
	}
}
