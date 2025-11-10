package repositories

import (
	"testing"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// setupTestDB creates an in-memory SQLite database for testing
func setupTestDB(t *testing.T) *gorm.DB {
	// Create in-memory database
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent), // Suppress logs during tests
	})
	if err != nil {
		t.Fatalf("failed to connect to test database: %v", err)
	}

	// Enable foreign keys
	sqlDB, err := db.DB()
	if err != nil {
		t.Fatalf("failed to get sql.DB: %v", err)
	}
	if _, err := sqlDB.Exec("PRAGMA foreign_keys = ON"); err != nil {
		t.Fatalf("failed to enable foreign keys: %v", err)
	}

	// Auto-migrate models
	if err := db.AutoMigrate(&models.Node{}, &models.RegistrationToken{}); err != nil {
		t.Fatalf("failed to migrate database: %v", err)
	}

	// Create indexes
	if err := db.Exec("CREATE INDEX IF NOT EXISTS idx_nodes_status ON nodes(status)").Error; err != nil {
		t.Fatalf("failed to create status index: %v", err)
	}
	if err := db.Exec("CREATE INDEX IF NOT EXISTS idx_nodes_last_seen ON nodes(last_seen)").Error; err != nil {
		t.Fatalf("failed to create last_seen index: %v", err)
	}

	return db
}

// TestNodeRepository_Create tests creating a new node
func TestNodeRepository_Create(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:            "550e8400-e29b-41d4-a716-446655440000",
		MacAddress:      "AA:BB:CC:DD:EE:FF",
		JWTSecret:       "encrypted_secret_here",
		Status:          models.NodeStatusActive,
		FirmwareVersion: stringPtr("1.0.0"),
		Latitude:        float64Ptr(50.0755),
		Longitude:       float64Ptr(14.4378),
	}

	err := repo.Create(node)
	if err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Verify node was created
	found, err := repo.FindByUUID(node.UUID)
	if err != nil {
		t.Fatalf("FindByUUID() error = %v", err)
	}
	if found.UUID != node.UUID {
		t.Errorf("UUID = %v, want %v", found.UUID, node.UUID)
	}
	if found.MacAddress != node.MacAddress {
		t.Errorf("MacAddress = %v, want %v", found.MacAddress, node.MacAddress)
	}
}

// TestNodeRepository_Create_DuplicateUUID tests creating a node with duplicate UUID
func TestNodeRepository_Create_DuplicateUUID(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node1 := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret1",
		Status:     models.NodeStatusActive,
	}

	node2 := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000", // Same UUID
		MacAddress: "11:22:33:44:55:66",                    // Different MAC
		JWTSecret:  "secret2",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node1); err != nil {
		t.Fatalf("Create(node1) error = %v", err)
	}

	// Second create should fail due to duplicate UUID
	if err := repo.Create(node2); err == nil {
		t.Error("Create(node2) expected error for duplicate UUID, got nil")
	}
}

// TestNodeRepository_Create_DuplicateMAC tests creating a node with duplicate MAC
func TestNodeRepository_Create_DuplicateMAC(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node1 := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret1",
		Status:     models.NodeStatusActive,
	}

	node2 := &models.Node{
		UUID:       "123e4567-e89b-42d3-a456-426614174000", // Different UUID
		MacAddress: "AA:BB:CC:DD:EE:FF",                    // Same MAC
		JWTSecret:  "secret2",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node1); err != nil {
		t.Fatalf("Create(node1) error = %v", err)
	}

	// Second create should fail due to duplicate MAC
	if err := repo.Create(node2); err == nil {
		t.Error("Create(node2) expected error for duplicate MAC, got nil")
	}
}

// TestNodeRepository_FindByMAC tests finding a node by MAC address
func TestNodeRepository_FindByMAC(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Find by MAC
	found, err := repo.FindByMAC("AA:BB:CC:DD:EE:FF")
	if err != nil {
		t.Fatalf("FindByMAC() error = %v", err)
	}
	if found.UUID != node.UUID {
		t.Errorf("UUID = %v, want %v", found.UUID, node.UUID)
	}

	// Try to find non-existent MAC
	_, err = repo.FindByMAC("99:99:99:99:99:99")
	if err == nil {
		t.Error("FindByMAC() expected error for non-existent MAC, got nil")
	}
}

// TestNodeRepository_Update tests updating a node
func TestNodeRepository_Update(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:            "550e8400-e29b-41d4-a716-446655440000",
		MacAddress:      "AA:BB:CC:DD:EE:FF",
		JWTSecret:       "secret",
		Status:          models.NodeStatusActive,
		FirmwareVersion: stringPtr("1.0.0"),
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Update firmware version
	node.FirmwareVersion = stringPtr("2.0.0")
	if err := repo.Update(node); err != nil {
		t.Fatalf("Update() error = %v", err)
	}

	// Verify update
	found, err := repo.FindByUUID(node.UUID)
	if err != nil {
		t.Fatalf("FindByUUID() error = %v", err)
	}
	if found.FirmwareVersion == nil || *found.FirmwareVersion != "2.0.0" {
		t.Errorf("FirmwareVersion = %v, want 2.0.0", found.FirmwareVersion)
	}
}

// TestNodeRepository_UpdateStatus tests updating node status
func TestNodeRepository_UpdateStatus(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Update status to disabled
	if err := repo.UpdateStatus(node.UUID, models.NodeStatusDisabled); err != nil {
		t.Fatalf("UpdateStatus() error = %v", err)
	}

	// Verify status change
	found, err := repo.FindByUUID(node.UUID)
	if err != nil {
		t.Fatalf("FindByUUID() error = %v", err)
	}
	if found.Status != models.NodeStatusDisabled {
		t.Errorf("Status = %v, want %v", found.Status, models.NodeStatusDisabled)
	}
}

// TestNodeRepository_UpdateLocation tests updating node GPS coordinates
func TestNodeRepository_UpdateLocation(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Update location
	newLat := 48.8566
	newLng := 2.3522
	if err := repo.UpdateLocation(node.UUID, newLat, newLng); err != nil {
		t.Fatalf("UpdateLocation() error = %v", err)
	}

	// Verify location update
	found, err := repo.FindByUUID(node.UUID)
	if err != nil {
		t.Fatalf("FindByUUID() error = %v", err)
	}
	if found.Latitude == nil || *found.Latitude != newLat {
		t.Errorf("Latitude = %v, want %v", found.Latitude, newLat)
	}
	if found.Longitude == nil || *found.Longitude != newLng {
		t.Errorf("Longitude = %v, want %v", found.Longitude, newLng)
	}
}

// TestNodeRepository_UpdateLastSeen tests updating last seen timestamp
func TestNodeRepository_UpdateLastSeen(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Wait a bit to ensure timestamp changes
	time.Sleep(10 * time.Millisecond)

	// Update last seen
	if err := repo.UpdateLastSeen(node.UUID); err != nil {
		t.Fatalf("UpdateLastSeen() error = %v", err)
	}

	// Verify last seen was updated
	found, err := repo.FindByUUID(node.UUID)
	if err != nil {
		t.Fatalf("FindByUUID() error = %v", err)
	}
	if found.LastSeen == nil {
		t.Error("LastSeen should not be nil after UpdateLastSeen()")
	}
	if found.LastSeen != nil && found.LastSeen.Before(node.CreatedAt) {
		t.Error("LastSeen should be after CreatedAt")
	}
}

// TestNodeRepository_ListByStatus tests listing nodes by status
func TestNodeRepository_ListByStatus(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	// Create nodes with different statuses
	nodes := []*models.Node{
		{UUID: "550e8400-e29b-41d4-a716-446655440001", MacAddress: "AA:BB:CC:DD:EE:01", JWTSecret: "s1", Status: models.NodeStatusActive},
		{UUID: "550e8400-e29b-41d4-a716-446655440002", MacAddress: "AA:BB:CC:DD:EE:02", JWTSecret: "s2", Status: models.NodeStatusActive},
		{UUID: "550e8400-e29b-41d4-a716-446655440003", MacAddress: "AA:BB:CC:DD:EE:03", JWTSecret: "s3", Status: models.NodeStatusDisabled},
		{UUID: "550e8400-e29b-41d4-a716-446655440004", MacAddress: "AA:BB:CC:DD:EE:04", JWTSecret: "s4", Status: models.NodeStatusRevoked},
	}

	for _, n := range nodes {
		if err := repo.Create(n); err != nil {
			t.Fatalf("Create() error = %v", err)
		}
	}

	// List active nodes
	activeNodes, err := repo.ListByStatus(models.NodeStatusActive)
	if err != nil {
		t.Fatalf("ListByStatus(active) error = %v", err)
	}
	if len(activeNodes) != 2 {
		t.Errorf("ListByStatus(active) count = %d, want 2", len(activeNodes))
	}

	// List disabled nodes
	disabledNodes, err := repo.ListByStatus(models.NodeStatusDisabled)
	if err != nil {
		t.Fatalf("ListByStatus(disabled) error = %v", err)
	}
	if len(disabledNodes) != 1 {
		t.Errorf("ListByStatus(disabled) count = %d, want 1", len(disabledNodes))
	}
}

// TestNodeRepository_Delete tests soft delete
func TestNodeRepository_Delete(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	node := &models.Node{
		UUID:       "550e8400-e29b-41d4-a716-446655440000",
		MacAddress: "AA:BB:CC:DD:EE:FF",
		JWTSecret:  "secret",
		Status:     models.NodeStatusActive,
	}

	if err := repo.Create(node); err != nil {
		t.Fatalf("Create() error = %v", err)
	}

	// Soft delete
	if err := repo.Delete(node.UUID); err != nil {
		t.Fatalf("Delete() error = %v", err)
	}

	// Try to find - should fail because of soft delete
	_, err := repo.FindByUUID(node.UUID)
	if err == nil {
		t.Error("FindByUUID() after Delete() should return error, got nil")
	}

	// Verify it still exists in DB with DeletedAt set
	var deletedNode models.Node
	if err := db.Unscoped().Where("uuid = ?", node.UUID).First(&deletedNode).Error; err != nil {
		t.Fatalf("Failed to find soft-deleted node: %v", err)
	}
	if deletedNode.DeletedAt.Time.IsZero() {
		t.Error("DeletedAt should be set after soft delete")
	}
}

// TestNodeRepository_CountByStatus tests counting nodes by status
func TestNodeRepository_CountByStatus(t *testing.T) {
	db := setupTestDB(t)
	repo := NewNodeRepository(db)

	// Create nodes
	nodes := []*models.Node{
		{UUID: "550e8400-e29b-41d4-a716-446655440001", MacAddress: "AA:BB:CC:DD:EE:01", JWTSecret: "s1", Status: models.NodeStatusActive},
		{UUID: "550e8400-e29b-41d4-a716-446655440002", MacAddress: "AA:BB:CC:DD:EE:02", JWTSecret: "s2", Status: models.NodeStatusActive},
		{UUID: "550e8400-e29b-41d4-a716-446655440003", MacAddress: "AA:BB:CC:DD:EE:03", JWTSecret: "s3", Status: models.NodeStatusDisabled},
	}

	for _, n := range nodes {
		if err := repo.Create(n); err != nil {
			t.Fatalf("Create() error = %v", err)
		}
	}

	// Count active nodes
	count, err := repo.CountByStatus(models.NodeStatusActive)
	if err != nil {
		t.Fatalf("CountByStatus(active) error = %v", err)
	}
	if count != 2 {
		t.Errorf("CountByStatus(active) = %d, want 2", count)
	}

	// Count total nodes
	totalCount, err := repo.Count()
	if err != nil {
		t.Fatalf("Count() error = %v", err)
	}
	if totalCount != 3 {
		t.Errorf("Count() = %d, want 3", totalCount)
	}
}

// Helper functions
func stringPtr(s string) *string {
	return &s
}

func float64Ptr(f float64) *float64 {
	return &f
}
