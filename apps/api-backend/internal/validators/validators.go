package validators

import (
	"fmt"
	"regexp"
	"strings"
)

// UUID validation regex (RFC 4122 v4)
var uuidRegex = regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$`)

// MAC address validation regex (uppercase with colons)
var macRegex = regexp.MustCompile(`^([0-9A-F]{2}:){5}[0-9A-F]{2}$`)

// Semantic versioning regex (basic)
var semverRegex = regexp.MustCompile(`^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$`)

// ValidationError represents a validation error with field context
type ValidationError struct {
	Field   string
	Message string
}

func (e *ValidationError) Error() string {
	return fmt.Sprintf("%s: %s", e.Field, e.Message)
}

// NewValidationError creates a new validation error
func NewValidationError(field, message string) *ValidationError {
	return &ValidationError{
		Field:   field,
		Message: message,
	}
}

// IsValidUUID checks if the string is a valid RFC 4122 v4 UUID
// Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
// where x is any hex digit and y is one of 8, 9, A, or B
func IsValidUUID(uuid string) bool {
	if uuid == "" {
		return false
	}
	// Convert to lowercase for validation
	return uuidRegex.MatchString(strings.ToLower(uuid))
}

// ValidateUUID validates and returns an error if invalid
func ValidateUUID(uuid string, fieldName string) error {
	if uuid == "" {
		return NewValidationError(fieldName, "UUID is required")
	}
	if !IsValidUUID(uuid) {
		return NewValidationError(fieldName, "invalid UUID format (expected: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx)")
	}
	return nil
}

// IsValidMACAddress checks if the string is a valid MAC address
// Expected format: AA:BB:CC:DD:EE:FF (uppercase, colon-separated)
func IsValidMACAddress(mac string) bool {
	if mac == "" {
		return false
	}
	return macRegex.MatchString(mac)
}

// ValidateMACAddress validates and returns an error if invalid
func ValidateMACAddress(mac string, fieldName string) error {
	if mac == "" {
		return NewValidationError(fieldName, "MAC address is required")
	}
	if !IsValidMACAddress(mac) {
		return NewValidationError(fieldName, "invalid MAC address format (expected: AA:BB:CC:DD:EE:FF, uppercase with colons)")
	}
	return nil
}

// NormalizeMACAddress converts MAC address to uppercase with colons
// Handles formats: aa:bb:cc:dd:ee:ff, aa-bb-cc-dd-ee-ff, aabbccddeeff
func NormalizeMACAddress(mac string) (string, error) {
	if mac == "" {
		return "", NewValidationError("mac_address", "MAC address is required")
	}

	// Remove common separators
	mac = strings.ReplaceAll(mac, "-", ":")
	mac = strings.ReplaceAll(mac, ".", ":")
	mac = strings.ReplaceAll(mac, " ", "")

	// If no colons, add them (for format aabbccddeeff)
	if !strings.Contains(mac, ":") && len(mac) == 12 {
		parts := []string{}
		for i := 0; i < len(mac); i += 2 {
			parts = append(parts, mac[i:i+2])
		}
		mac = strings.Join(parts, ":")
	}

	// Convert to uppercase
	mac = strings.ToUpper(mac)

	// Validate final format
	if !IsValidMACAddress(mac) {
		return "", NewValidationError("mac_address", "invalid MAC address format after normalization")
	}

	return mac, nil
}

// IsValidLatitude checks if the value is a valid GPS latitude
// Valid range: -90.0 to 90.0
func IsValidLatitude(lat float64) bool {
	return lat >= -90.0 && lat <= 90.0
}

// ValidateLatitude validates latitude and returns an error if invalid
func ValidateLatitude(lat float64, fieldName string) error {
	if !IsValidLatitude(lat) {
		return NewValidationError(fieldName, fmt.Sprintf("latitude must be between -90.0 and 90.0 (got: %f)", lat))
	}
	return nil
}

// IsValidLongitude checks if the value is a valid GPS longitude
// Valid range: -180.0 to 180.0
func IsValidLongitude(lng float64) bool {
	return lng >= -180.0 && lng <= 180.0
}

// ValidateLongitude validates longitude and returns an error if invalid
func ValidateLongitude(lng float64, fieldName string) error {
	if !IsValidLongitude(lng) {
		return NewValidationError(fieldName, fmt.Sprintf("longitude must be between -180.0 and 180.0 (got: %f)", lng))
	}
	return nil
}

// ValidateGPSCoordinates validates both latitude and longitude
func ValidateGPSCoordinates(lat, lng float64) error {
	if err := ValidateLatitude(lat, "latitude"); err != nil {
		return err
	}
	if err := ValidateLongitude(lng, "longitude"); err != nil {
		return err
	}
	return nil
}

// IsValidSemanticVersion checks if the string follows semantic versioning
// Format: MAJOR.MINOR.PATCH or MAJOR.MINOR.PATCH-prerelease+build
// Examples: 1.0.0, 2.1.3-beta, 1.0.0-alpha+001
func IsValidSemanticVersion(version string) bool {
	if version == "" {
		return false
	}
	return semverRegex.MatchString(version)
}

// ValidateFirmwareVersion validates firmware version string
func ValidateFirmwareVersion(version string, fieldName string) error {
	if version == "" {
		return nil // Firmware version is optional
	}
	if !IsValidSemanticVersion(version) {
		return NewValidationError(fieldName, "invalid semantic version format (expected: MAJOR.MINOR.PATCH)")
	}
	return nil
}

// IsValidNodeStatus checks if the status is a valid node status
func IsValidNodeStatus(status string) bool {
	switch status {
	case "active", "disabled", "revoked":
		return true
	default:
		return false
	}
}

// ValidateNodeStatus validates node status
func ValidateNodeStatus(status string, fieldName string) error {
	if status == "" {
		return NewValidationError(fieldName, "status is required")
	}
	if !IsValidNodeStatus(status) {
		return NewValidationError(fieldName, "invalid status (allowed: active, disabled, revoked)")
	}
	return nil
}

// ValidateStringLength validates string length constraints
func ValidateStringLength(value string, fieldName string, minLength, maxLength int) error {
	length := len(value)
	if minLength > 0 && length < minLength {
		return NewValidationError(fieldName, fmt.Sprintf("must be at least %d characters (got: %d)", minLength, length))
	}
	if maxLength > 0 && length > maxLength {
		return NewValidationError(fieldName, fmt.Sprintf("must be at most %d characters (got: %d)", maxLength, length))
	}
	return nil
}

// ValidateNodeName validates node name constraints
func ValidateNodeName(name string, fieldName string) error {
	if name == "" {
		return nil // Name is optional
	}
	return ValidateStringLength(name, fieldName, 1, 100)
}

// IsValidBase64JWTSecret checks if the JWT secret is properly base64 encoded
// and has minimum length (44 characters for 32-byte secret)
func IsValidBase64JWTSecret(secret string) bool {
	if secret == "" {
		return false
	}
	// Base64 encoded 32 bytes should be 44 characters minimum (without padding)
	// With padding it's typically 44-45 characters
	return len(secret) >= 44
}

// ValidateJWTSecret validates encrypted JWT secret format
func ValidateJWTSecret(secret string, fieldName string) error {
	if secret == "" {
		return NewValidationError(fieldName, "JWT secret is required")
	}
	if !IsValidBase64JWTSecret(secret) {
		return NewValidationError(fieldName, "JWT secret must be at least 44 characters (base64-encoded 32 bytes)")
	}
	return nil
}
