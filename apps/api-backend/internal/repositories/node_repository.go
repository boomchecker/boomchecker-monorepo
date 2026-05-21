package repositories

import (
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"gorm.io/gorm"
)

// NodeRepository handles database operations for nodes
type NodeRepository struct {
	db *gorm.DB
}

// NewNodeRepository creates a new node repository instance
func NewNodeRepository(db *gorm.DB) *NodeRepository {
	return &NodeRepository{db: db}
}

// Create inserts a new node into the database
// Returns error if node with same UUID or MAC already exists
func (r *NodeRepository) Create(node *models.Node) error {
	if node == nil {
		return fmt.Errorf("node cannot be nil")
	}

	// Check for duplicate UUID
	if err := r.checkDuplicateUUID(node.UUID); err != nil {
		return err
	}

	// Check for duplicate MAC address
	if err := r.checkDuplicateMAC(node.MacAddress); err != nil {
		return err
	}

	// Ensure timestamps are set in UTC
	now := time.Now().UTC()
	node.CreatedAt = now
	node.UpdatedAt = now

	if err := r.db.Create(node).Error; err != nil {
		return fmt.Errorf("failed to create node: %w", err)
	}

	return nil
}

// FindByUUID retrieves a node by its UUID
// Returns gorm.ErrRecordNotFound if node doesn't exist
func (r *NodeRepository) FindByUUID(uuid string) (*models.Node, error) {
	if uuid == "" {
		return nil, fmt.Errorf("uuid is required")
	}

	var node models.Node
	if err := r.db.Where("uuid = ?", uuid).First(&node).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("node not found: %s", uuid)
		}
		return nil, fmt.Errorf("failed to find node: %w", err)
	}

	return &node, nil
}

// FindByMAC retrieves a node by its MAC address
// Returns gorm.ErrRecordNotFound if node doesn't exist
func (r *NodeRepository) FindByMAC(macAddress string) (*models.Node, error) {
	if macAddress == "" {
		return nil, fmt.Errorf("mac address is required")
	}

	var node models.Node
	if err := r.db.Where("mac_address = ?", macAddress).First(&node).Error; err != nil {
		if err == gorm.ErrRecordNotFound {
			return nil, fmt.Errorf("node not found with MAC: %s", macAddress)
		}
		return nil, fmt.Errorf("failed to find node: %w", err)
	}

	return &node, nil
}

// Update updates an existing node
// Only updates provided fields, timestamps are updated automatically
func (r *NodeRepository) Update(node *models.Node) error {
	if node == nil {
		return fmt.Errorf("node cannot be nil")
	}
	if node.UUID == "" {
		return fmt.Errorf("node UUID is required")
	}

	// Ensure UpdatedAt is current
	node.UpdatedAt = time.Now().UTC()

	result := r.db.Model(&models.Node{}).Where("uuid = ?", node.UUID).Updates(node)
	if result.Error != nil {
		return fmt.Errorf("failed to update node: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("node not found: %s", node.UUID)
	}

	return nil
}

// UpdateLastSeen updates the last_seen_at timestamp for a node
// Used to track node activity
func (r *NodeRepository) UpdateLastSeen(uuid string) error {
	if uuid == "" {
		return fmt.Errorf("uuid is required")
	}

	now := time.Now().UTC()
	result := r.db.Model(&models.Node{}).
		Where("uuid = ?", uuid).
		Updates(map[string]interface{}{
			"last_seen_at": now,
			"updated_at":   now,
		})

	if result.Error != nil {
		return fmt.Errorf("failed to update last seen: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("node not found: %s", uuid)
	}

	return nil
}

// UpdateStatus changes the status of a node (active, disabled, revoked)
func (r *NodeRepository) UpdateStatus(uuid string, status string) error {
	if uuid == "" {
		return fmt.Errorf("uuid is required")
	}
	if status == "" {
		return fmt.Errorf("status is required")
	}

	// Validate status value
	if !isValidStatus(status) {
		return fmt.Errorf("invalid status: %s (allowed: active, disabled, revoked)", status)
	}

	result := r.db.Model(&models.Node{}).
		Where("uuid = ?", uuid).
		Updates(map[string]interface{}{
			"status":     status,
			"updated_at": time.Now().UTC(),
		})

	if result.Error != nil {
		return fmt.Errorf("failed to update status: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("node not found: %s", uuid)
	}

	return nil
}

// UpdateLocation updates GPS coordinates for a node
func (r *NodeRepository) UpdateLocation(uuid string, latitude, longitude float64) error {
	if uuid == "" {
		return fmt.Errorf("uuid is required")
	}

	result := r.db.Model(&models.Node{}).
		Where("uuid = ?", uuid).
		Updates(map[string]interface{}{
			"latitude":   latitude,
			"longitude":  longitude,
			"updated_at": time.Now().UTC(),
		})

	if result.Error != nil {
		return fmt.Errorf("failed to update location: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("node not found: %s", uuid)
	}

	return nil
}

// ListByStatus retrieves all nodes with a specific status
func (r *NodeRepository) ListByStatus(status string) ([]*models.Node, error) {
	if status == "" {
		return nil, fmt.Errorf("status is required")
	}

	if !isValidStatus(status) {
		return nil, fmt.Errorf("invalid status: %s", status)
	}

	var nodes []*models.Node
	if err := r.db.Where("status = ?", status).Order("created_at DESC").Find(&nodes).Error; err != nil {
		return nil, fmt.Errorf("failed to list nodes by status: %w", err)
	}

	return nodes, nil
}

// ListAll retrieves all nodes
func (r *NodeRepository) ListAll() ([]*models.Node, error) {
	var nodes []*models.Node
	if err := r.db.Order("created_at DESC").Find(&nodes).Error; err != nil {
		return nil, fmt.Errorf("failed to list all nodes: %w", err)
	}

	return nodes, nil
}

// FindInactive returns nodes that haven't been seen within the threshold duration
// Example: FindInactive(24 * time.Hour) returns nodes inactive for more than 24 hours
func (r *NodeRepository) FindInactive(threshold time.Duration) ([]*models.Node, error) {
	cutoffTime := time.Now().UTC().Add(-threshold)

	var nodes []*models.Node
	if err := r.db.Where("last_seen_at < ? OR last_seen_at IS NULL", cutoffTime).
		Order("last_seen_at ASC").
		Find(&nodes).Error; err != nil {
		return nil, fmt.Errorf("failed to find inactive nodes: %w", err)
	}

	return nodes, nil
}

// Delete performs a soft delete by setting status to 'revoked'
// Use this for audit trail preservation
func (r *NodeRepository) Delete(uuid string) error {
	if uuid == "" {
		return fmt.Errorf("uuid is required")
	}

	return r.UpdateStatus(uuid, models.NodeStatusRevoked)
}

// HardDelete permanently removes a node from the database
// WARNING: This cannot be undone. Use only for cleanup/testing
func (r *NodeRepository) HardDelete(uuid string) error {
	if uuid == "" {
		return fmt.Errorf("uuid is required")
	}

	result := r.db.Where("uuid = ?", uuid).Delete(&models.Node{})
	if result.Error != nil {
		return fmt.Errorf("failed to delete node: %w", result.Error)
	}

	if result.RowsAffected == 0 {
		return fmt.Errorf("node not found: %s", uuid)
	}

	return nil
}

// Exists checks if a node with the given UUID exists
func (r *NodeRepository) Exists(uuid string) (bool, error) {
	if uuid == "" {
		return false, fmt.Errorf("uuid is required")
	}

	var count int64
	if err := r.db.Model(&models.Node{}).Where("uuid = ?", uuid).Count(&count).Error; err != nil {
		return false, fmt.Errorf("failed to check node existence: %w", err)
	}

	return count > 0, nil
}

// Count returns the total number of nodes
func (r *NodeRepository) Count() (int64, error) {
	var count int64
	if err := r.db.Model(&models.Node{}).Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count nodes: %w", err)
	}

	return count, nil
}

// CountByStatus returns the number of nodes with a specific status
func (r *NodeRepository) CountByStatus(status string) (int64, error) {
	if status == "" {
		return 0, fmt.Errorf("status is required")
	}

	var count int64
	if err := r.db.Model(&models.Node{}).Where("status = ?", status).Count(&count).Error; err != nil {
		return 0, fmt.Errorf("failed to count nodes by status: %w", err)
	}

	return count, nil
}

// Helper functions

func (r *NodeRepository) checkDuplicateUUID(uuid string) error {
	exists, err := r.Exists(uuid)
	if err != nil {
		return err
	}
	if exists {
		return fmt.Errorf("node with UUID %s already exists", uuid)
	}
	return nil
}

func (r *NodeRepository) checkDuplicateMAC(macAddress string) error {
	var count int64
	if err := r.db.Model(&models.Node{}).Where("mac_address = ?", macAddress).Count(&count).Error; err != nil {
		return fmt.Errorf("failed to check MAC address: %w", err)
	}
	if count > 0 {
		return fmt.Errorf("node with MAC address %s already exists", macAddress)
	}
	return nil
}

func isValidStatus(status string) bool {
	return status == models.NodeStatusActive ||
		status == models.NodeStatusDisabled ||
		status == models.NodeStatusRevoked
}
