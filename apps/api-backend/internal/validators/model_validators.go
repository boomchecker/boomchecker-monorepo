package validators

import (
	"fmt"
)

// NodeValidator provides validation for Node model fields
type NodeValidator struct{}

// ValidateNodeCreation validates all required fields for creating a new node
func (v *NodeValidator) ValidateNodeCreation(uuid, macAddress, jwtSecret, status string) []error {
	errors := []error{}

	// UUID validation
	if err := ValidateUUID(uuid, "uuid"); err != nil {
		errors = append(errors, err)
	}

	// MAC address validation
	if err := ValidateMACAddress(macAddress, "mac_address"); err != nil {
		errors = append(errors, err)
	}

	// JWT secret validation
	if err := ValidateJWTSecret(jwtSecret, "jwt_secret"); err != nil {
		errors = append(errors, err)
	}

	// Status validation
	if err := ValidateNodeStatus(status, "status"); err != nil {
		errors = append(errors, err)
	}

	return errors
}

// ValidateNodeUpdate validates fields for updating an existing node
func (v *NodeValidator) ValidateNodeUpdate(uuid string) error {
	return ValidateUUID(uuid, "uuid")
}

// ValidateOptionalFields validates optional node fields if they are provided
func (v *NodeValidator) ValidateOptionalFields(name *string, firmwareVersion *string, latitude, longitude *float64) []error {
	errors := []error{}

	// Name validation (optional)
	if name != nil {
		if err := ValidateNodeName(*name, "name"); err != nil {
			errors = append(errors, err)
		}
	}

	// Firmware version validation (optional)
	if firmwareVersion != nil {
		if err := ValidateFirmwareVersion(*firmwareVersion, "firmware_version"); err != nil {
			errors = append(errors, err)
		}
	}

	// GPS coordinates validation (both must be present if one is)
	if latitude != nil && longitude == nil {
		errors = append(errors, NewValidationError("longitude", "longitude is required when latitude is provided"))
	}
	if longitude != nil && latitude == nil {
		errors = append(errors, NewValidationError("latitude", "latitude is required when longitude is provided"))
	}
	if latitude != nil && longitude != nil {
		if err := ValidateGPSCoordinates(*latitude, *longitude); err != nil {
			errors = append(errors, err)
		}
	}

	return errors
}

// RegistrationTokenValidator provides validation for RegistrationToken model fields
type RegistrationTokenValidator struct{}

// ValidateTokenCreation validates all required fields for creating a new token
func (v *RegistrationTokenValidator) ValidateTokenCreation(id, token string) []error {
	errors := []error{}

	// ID validation (should be UUID)
	if err := ValidateUUID(id, "id"); err != nil {
		errors = append(errors, err)
	}

	// Token validation (should be UUID)
	if err := ValidateUUID(token, "token"); err != nil {
		errors = append(errors, err)
	}

	return errors
}

// ValidateUsageLimit validates usage limit constraints
func (v *RegistrationTokenValidator) ValidateUsageLimit(usageLimit *int, usedCount int) error {
	if usageLimit == nil {
		return nil // Unlimited usage is valid
	}

	if *usageLimit < 0 {
		return NewValidationError("usage_limit", "usage limit cannot be negative")
	}

	if usedCount < 0 {
		return NewValidationError("used_count", "used count cannot be negative")
	}

	if *usageLimit > 0 && usedCount > *usageLimit {
		return NewValidationError("used_count", fmt.Sprintf("used count (%d) cannot exceed usage limit (%d)", usedCount, *usageLimit))
	}

	return nil
}

// ValidatePreAuthorizedMAC validates pre-authorized MAC address if provided
func (v *RegistrationTokenValidator) ValidatePreAuthorizedMAC(macAddress *string) error {
	if macAddress == nil {
		return nil // Optional field
	}

	return ValidateMACAddress(*macAddress, "pre_authorized_mac_address")
}

// Helper function to collect and format multiple validation errors
func FormatValidationErrors(errors []error) string {
	if len(errors) == 0 {
		return ""
	}

	if len(errors) == 1 {
		return errors[0].Error()
	}

	result := "validation errors:\n"
	for i, err := range errors {
		result += fmt.Sprintf("  %d. %s\n", i+1, err.Error())
	}
	return result
}

// HasValidationErrors checks if there are any validation errors
func HasValidationErrors(errors []error) bool {
	return len(errors) > 0
}
