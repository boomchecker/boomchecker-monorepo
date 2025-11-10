package services

import (
	"fmt"
	"time"

	"github.com/boomchecker/api-backend/internal/crypto"
	"github.com/boomchecker/api-backend/internal/models"
	"github.com/boomchecker/api-backend/internal/repositories"
	"github.com/boomchecker/api-backend/internal/validators"
	"github.com/google/uuid"
)

// NodeRegistrationService handles the business logic for node registration
type NodeRegistrationService struct {
	nodeRepo  *repositories.NodeRepository
	tokenRepo *repositories.RegistrationTokenRepository
}

// NewNodeRegistrationService creates a new node registration service instance
func NewNodeRegistrationService(
	nodeRepo *repositories.NodeRepository,
	tokenRepo *repositories.RegistrationTokenRepository,
) *NodeRegistrationService {
	return &NodeRegistrationService{
		nodeRepo:  nodeRepo,
		tokenRepo: tokenRepo,
	}
}

// RegistrationRequest contains the data needed to register a node
type RegistrationRequest struct {
	RegistrationToken string   `json:"registration_token" binding:"required" example:"a1b2c3d4-e5f6-7890-abcd-ef1234567890"`
	MacAddress        string   `json:"mac_address" binding:"required" example:"AA:BB:CC:DD:EE:FF"`
	FirmwareVersion   *string  `json:"firmware_version,omitempty" example:"1.0.0"`
	Latitude          *float64 `json:"latitude,omitempty" example:"50.0755"`
	Longitude         *float64 `json:"longitude,omitempty" example:"14.4378"`
}

// RegistrationResponse contains the data returned after successful registration
type RegistrationResponse struct {
	UUID       string `json:"uuid" example:"550e8400-e29b-41d4-a716-446655440000"`
	JWTToken   string `json:"jwt_token" example:"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."`
	ExpiresAt  string `json:"expires_at" example:"2025-12-10T14:30:00Z"` // UTC timestamp when JWT expires (RFC3339 format)
	IsNewNode  bool   `json:"is_new_node" example:"true"`
	MacAddress string `json:"mac_address" example:"AA:BB:CC:DD:EE:FF"`
}

// RegisterNode handles the complete node registration flow
// This includes:
// 1. Validating the registration token
// 2. Validating input data (MAC address, GPS coordinates, firmware version)
// 3. Checking if node already exists (re-registration case)
// 4. Generating UUID and JWT secret for new nodes
// 5. Creating/updating node in database
// 6. Incrementing token usage count
// 7. Generating JWT token for the node
func (s *NodeRegistrationService) RegisterNode(req *RegistrationRequest) (*RegistrationResponse, error) {
	// Step 1: Validate input data
	if err := s.validateRegistrationRequest(req); err != nil {
		return nil, fmt.Errorf("validation failed: %w", err)
	}

	// Step 2: Normalize MAC address
	normalizedMAC, err := validators.NormalizeMACAddress(req.MacAddress)
	if err != nil {
		return nil, fmt.Errorf("invalid MAC address: %w", err)
	}
	req.MacAddress = normalizedMAC

	// Step 3: Validate registration token
	token, err := s.tokenRepo.ValidateToken(req.RegistrationToken, &req.MacAddress)
	if err != nil {
		return nil, fmt.Errorf("invalid registration token: %w", err)
	}

	// Step 4: Check if node already exists (re-registration case)
	existingNode, err := s.nodeRepo.FindByMAC(req.MacAddress)
	if err == nil {
		// Node exists - handle re-registration
		return s.handleReRegistration(existingNode, req, token)
	}

	// Step 5: Node doesn't exist - create new node
	return s.handleNewRegistration(req, token)
}

// handleNewRegistration creates a new node in the database
func (s *NodeRegistrationService) handleNewRegistration(
	req *RegistrationRequest,
	token *models.RegistrationToken,
) (*RegistrationResponse, error) {
	// Generate new UUID for the node
	nodeUUID := uuid.New().String()

	// Generate and encrypt JWT secret
	jwtSecret, encryptedSecret, err := crypto.EncryptJWTSecret()
	if err != nil {
		return nil, fmt.Errorf("failed to generate and encrypt JWT secret: %w", err)
	}

	// Create node model
	node := &models.Node{
		UUID:            nodeUUID,
		MacAddress:      req.MacAddress,
		JWTSecret:       encryptedSecret,
		Status:          models.NodeStatusActive,
		FirmwareVersion: req.FirmwareVersion,
		Latitude:        req.Latitude,
		Longitude:       req.Longitude,
		LastSeenAt:      timePtr(time.Now().UTC()),
	}

	// Save node to database
	if err := s.nodeRepo.Create(node); err != nil {
		return nil, fmt.Errorf("failed to create node: %w", err)
	}

	// Increment token usage count
	if err := s.tokenRepo.IncrementUsedCount(req.RegistrationToken); err != nil {
		// Log error but don't fail the registration
		// The node is already created at this point
		fmt.Printf("Warning: failed to increment token usage: %v\n", err)
	}

	// Generate JWT token for the node
	jwtToken, expiresAt, err := s.generateNodeJWT(nodeUUID, jwtSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to generate JWT: %w", err)
	}

	return &RegistrationResponse{
		UUID:       nodeUUID,
		JWTToken:   jwtToken,
		ExpiresAt:  expiresAt,
		IsNewNode:  true,
		MacAddress: req.MacAddress,
	}, nil
}

// handleReRegistration handles the case when a node with the same MAC already exists
func (s *NodeRegistrationService) handleReRegistration(
	existingNode *models.Node,
	req *RegistrationRequest,
	token *models.RegistrationToken,
) (*RegistrationResponse, error) {
	// Check if node is revoked
	if existingNode.IsRevoked() {
		return nil, fmt.Errorf("node is revoked and cannot be re-registered")
	}

	// Update node information
	if req.FirmwareVersion != nil {
		existingNode.FirmwareVersion = req.FirmwareVersion
	}
	if req.Latitude != nil && req.Longitude != nil {
		existingNode.Latitude = req.Latitude
		existingNode.Longitude = req.Longitude
	}

	// Set status to active if it was disabled
	if existingNode.IsDisabled() {
		existingNode.Status = models.NodeStatusActive
	}

	// Update last seen timestamp
	now := time.Now().UTC()
	existingNode.LastSeenAt = &now

	// Save updates
	if err := s.nodeRepo.Update(existingNode); err != nil {
		return nil, fmt.Errorf("failed to update node: %w", err)
	}

	// Increment token usage count
	if err := s.tokenRepo.IncrementUsedCount(req.RegistrationToken); err != nil {
		fmt.Printf("Warning: failed to increment token usage: %v\n", err)
	}

	// Decrypt existing JWT secret
	jwtSecret, err := crypto.DecryptJWTSecret(existingNode.JWTSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to decrypt JWT secret: %w", err)
	}

	// Generate new JWT token with existing secret
	jwtToken, expiresAt, err := s.generateNodeJWT(existingNode.UUID, jwtSecret)
	if err != nil {
		return nil, fmt.Errorf("failed to generate JWT: %w", err)
	}

	return &RegistrationResponse{
		UUID:       existingNode.UUID,
		JWTToken:   jwtToken,
		ExpiresAt:  expiresAt,
		IsNewNode:  false,
		MacAddress: req.MacAddress,
	}, nil
}

// validateRegistrationRequest validates all input data
func (s *NodeRegistrationService) validateRegistrationRequest(req *RegistrationRequest) error {
	// Validate registration token
	if req.RegistrationToken == "" {
		return fmt.Errorf("registration token is required")
	}

	// Validate MAC address
	if err := validators.ValidateMACAddress(req.MacAddress, "mac_address"); err != nil {
		return err
	}

	// Validate firmware version if provided
	if req.FirmwareVersion != nil && *req.FirmwareVersion != "" {
		if !validators.IsValidSemanticVersion(*req.FirmwareVersion) {
			return fmt.Errorf("invalid firmware version format: %s", *req.FirmwareVersion)
		}
	}

	// Validate GPS coordinates if provided
	if req.Latitude != nil || req.Longitude != nil {
		if req.Latitude == nil || req.Longitude == nil {
			return fmt.Errorf("both latitude and longitude must be provided")
		}
		if err := validators.ValidateGPSCoordinates(*req.Latitude, *req.Longitude); err != nil {
			return err
		}
	}

	return nil
}

// generateNodeJWT creates a JWT token for a node
// Returns the token string, expiration time as UTC string (RFC3339), and any error
func (s *NodeRegistrationService) generateNodeJWT(nodeUUID string, jwtSecret string) (string, string, error) {
	// JWT expires in 30 days
	expiresIn := int64(30 * 24 * 60 * 60) // 30 days in seconds

	token, expiresAtUnix, err := crypto.GenerateNodeJWT(nodeUUID, jwtSecret, time.Duration(expiresIn)*time.Second)
	if err != nil {
		return "", "", err
	}

	// Convert Unix timestamp to UTC RFC3339 string
	expiresAt := time.Unix(expiresAtUnix, 0).UTC().Format(time.RFC3339)

	return token, expiresAt, nil
}

// Helper function to create a pointer to a time value
func timePtr(t time.Time) *time.Time {
	return &t
}
