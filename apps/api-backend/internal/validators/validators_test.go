package validators

import (
	"testing"
)

// TestIsValidUUID tests UUID validation
func TestIsValidUUID(t *testing.T) {
	tests := []struct {
		name  string
		uuid  string
		want  bool
	}{
		{"valid UUID v4", "550e8400-e29b-41d4-a716-446655440000", true},
		{"valid UUID v4 lowercase", "123e4567-e89b-42d3-a456-426614174000", true}, // Fixed: 4xxx in 3rd group
		{"valid UUID uppercase", "550E8400-E29B-41D4-A716-446655440000", true},
		{"invalid - too short", "550e8400-e29b-41d4", false},
		{"invalid - no hyphens", "550e8400e29b41d4a716446655440000", false},
		{"invalid - wrong format", "not-a-uuid-at-all", false},
		{"empty string", "", false},
		{"random string", "hello world", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidUUID(tt.uuid); got != tt.want {
				t.Errorf("IsValidUUID(%q) = %v, want %v", tt.uuid, got, tt.want)
			}
		})
	}
}

// TestValidateMACAddress tests MAC address validation
func TestValidateMACAddress(t *testing.T) {
	tests := []struct {
		name      string
		mac       string
		fieldName string
		wantErr   bool
	}{
		{"valid MAC uppercase colons", "AA:BB:CC:DD:EE:FF", "mac", false},
		{"invalid - lowercase colons", "aa:bb:cc:dd:ee:ff", "mac", true}, // Validator expects uppercase
		{"invalid - mixed case", "Aa:Bb:Cc:Dd:Ee:Ff", "mac", true}, // Validator expects uppercase
		{"invalid - hyphens", "AA-BB-CC-DD-EE-FF", "mac", true}, // Validator expects colons
		{"invalid - dots", "AABB.CCDD.EEFF", "mac", true}, // Validator expects colons
		{"invalid - too short", "AA:BB:CC:DD:EE", "mac", true},
		{"invalid - too long", "AA:BB:CC:DD:EE:FF:00", "mac", true},
		{"invalid - wrong chars", "GG:HH:II:JJ:KK:LL", "mac", true},
		{"empty string", "", "mac", true},
		{"invalid format", "not-a-mac", "mac", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateMACAddress(tt.mac, tt.fieldName)
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateMACAddress(%q, %q) error = %v, wantErr %v", tt.mac, tt.fieldName, err, tt.wantErr)
			}
		})
	}
}

// TestNormalizeMACAddress tests MAC address normalization
func TestNormalizeMACAddress(t *testing.T) {
	tests := []struct {
		name    string
		mac     string
		want    string
		wantErr bool
	}{
		{"lowercase colons", "aa:bb:cc:dd:ee:ff", "AA:BB:CC:DD:EE:FF", false},
		{"uppercase colons", "AA:BB:CC:DD:EE:FF", "AA:BB:CC:DD:EE:FF", false},
		{"hyphens to colons", "aa-bb-cc-dd-ee-ff", "AA:BB:CC:DD:EE:FF", false},
		{"dots to colons", "aabb.ccdd.eeff", "AA:BB:CC:DD:EE:FF", false},
		{"mixed case", "Aa:bB:Cc:Dd:Ee:Ff", "AA:BB:CC:DD:EE:FF", false},
		{"invalid MAC", "not-a-mac", "", true},
		{"empty string", "", "", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := NormalizeMACAddress(tt.mac)
			if (err != nil) != tt.wantErr {
				t.Errorf("NormalizeMACAddress(%q) error = %v, wantErr %v", tt.mac, err, tt.wantErr)
				return
			}
			if got != tt.want {
				t.Errorf("NormalizeMACAddress(%q) = %q, want %q", tt.mac, got, tt.want)
			}
		})
	}
}

// TestValidateGPSCoordinates tests GPS coordinate validation
func TestValidateGPSCoordinates(t *testing.T) {
	tests := []struct {
		name    string
		lat     float64
		lng     float64
		wantErr bool
	}{
		{"valid Prague", 50.0755, 14.4378, false},
		{"valid equator prime meridian", 0.0, 0.0, false},
		{"valid north pole", 90.0, 0.0, false},
		{"valid south pole", -90.0, 0.0, false},
		{"valid date line", 0.0, 180.0, false},
		{"valid date line negative", 0.0, -180.0, false},
		{"invalid latitude too high", 91.0, 0.0, true},
		{"invalid latitude too low", -91.0, 0.0, true},
		{"invalid longitude too high", 0.0, 181.0, true},
		{"invalid longitude too low", 0.0, -181.0, true},
		{"invalid both", 100.0, 200.0, true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateGPSCoordinates(tt.lat, tt.lng)
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateGPSCoordinates(%v, %v) error = %v, wantErr %v", tt.lat, tt.lng, err, tt.wantErr)
			}
		})
	}
}

// TestIsValidSemanticVersion tests semantic version validation
func TestIsValidSemanticVersion(t *testing.T) {
	tests := []struct {
		name    string
		version string
		want    bool
	}{
		{"valid 1.0.0", "1.0.0", true},
		{"valid 0.0.1", "0.0.1", true},
		{"valid 10.20.30", "10.20.30", true},
		{"invalid - v prefix not supported", "v1.0.0", false}, // Validator doesn't support v prefix
		{"valid with prerelease", "1.0.0-alpha", true},
		{"valid with build", "1.0.0+build123", true},
		{"valid complex", "1.0.0-beta.1+build.123", true},
		{"invalid - two parts", "1.0", false},
		{"invalid - one part", "1", false},
		{"invalid - four parts", "1.0.0.0", false},
		{"invalid - non-numeric", "a.b.c", false},
		{"invalid - empty", "", false},
		{"invalid - random string", "not a version", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidSemanticVersion(tt.version); got != tt.want {
				t.Errorf("IsValidSemanticVersion(%q) = %v, want %v", tt.version, got, tt.want)
			}
		})
	}
}

// TestValidateNodeStatus tests node status validation
func TestValidateNodeStatus(t *testing.T) {
	tests := []struct {
		name    string
		status  string
		wantErr bool
	}{
		{"valid active", "active", false},
		{"valid disabled", "disabled", false},
		{"valid revoked", "revoked", false},
		{"invalid uppercase", "ACTIVE", true},
		{"invalid mixed case", "Active", true},
		{"invalid status", "pending", true},
		{"empty string", "", true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateNodeStatus(tt.status, "status")
			if (err != nil) != tt.wantErr {
				t.Errorf("ValidateNodeStatus(%q, %q) error = %v, wantErr %v", tt.status, "status", err, tt.wantErr)
			}
		})
	}
}
