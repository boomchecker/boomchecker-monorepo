package validators

import (
	"testing"
	"time"
)

// TestIsValidUTCTimestamp tests UTC timestamp validation
func TestIsValidUTCTimestamp(t *testing.T) {
	tests := []struct {
		name      string
		timestamp string
		want      bool
	}{
		{"valid RFC3339", "2025-11-10T14:30:00Z", true},
		{"valid with milliseconds", "2025-11-10T14:30:00.123Z", true},
		{"valid with microseconds", "2025-11-10T14:30:00.123456Z", true},
		{"invalid - missing Z", "2025-11-10T14:30:00", false},
		{"invalid - wrong timezone", "2025-11-10T14:30:00+01:00", false},
		{"invalid - wrong format", "2025/11/10 14:30:00", false},
		{"invalid - date only", "2025-11-10", false},
		{"empty string", "", false},
		{"random string", "not-a-timestamp", false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsValidUTCTimestamp(tt.timestamp); got != tt.want {
				t.Errorf("IsValidUTCTimestamp(%q) = %v, want %v", tt.timestamp, got, tt.want)
			}
		})
	}
}

// TestParseUTCTimestamp tests timestamp parsing
func TestParseUTCTimestamp(t *testing.T) {
	tests := []struct {
		name      string
		timestamp string
		wantErr   bool
		checkUTC  bool
	}{
		{"valid RFC3339", "2025-11-10T14:30:00Z", false, true},
		{"valid with milliseconds", "2025-11-10T14:30:00.123Z", false, true},
		{"invalid format", "2025-11-10 14:30:00", true, false},
		{"empty string", "", true, false},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ParseUTCTimestamp(tt.timestamp)
			if (err != nil) != tt.wantErr {
				t.Errorf("ParseUTCTimestamp(%q) error = %v, wantErr %v", tt.timestamp, err, tt.wantErr)
				return
			}
			if !tt.wantErr && tt.checkUTC {
				if got.Location() != time.UTC {
					t.Errorf("ParseUTCTimestamp(%q) location = %v, want UTC", tt.timestamp, got.Location())
				}
			}
		})
	}
}

// TestFormatUTCTimestamp tests timestamp formatting
func TestFormatUTCTimestamp(t *testing.T) {
	// Create a specific time in UTC
	testTime := time.Date(2025, 11, 10, 14, 30, 0, 0, time.UTC)
	
	got := FormatUTCTimestamp(testTime)
	want := "2025-11-10T14:30:00Z"
	
	if got != want {
		t.Errorf("FormatUTCTimestamp() = %q, want %q", got, want)
	}
}

// TestIsInFuture tests future timestamp detection
func TestIsInFuture(t *testing.T) {
	now := time.Now().UTC()
	future := now.Add(1 * time.Hour)
	past := now.Add(-1 * time.Hour)
	
	tests := []struct {
		name      string
		timestamp time.Time
		want      bool
	}{
		{"future time", future, true},
		{"past time", past, false},
		{"current time (approximately)", now, false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsInFuture(tt.timestamp); got != tt.want {
				t.Errorf("IsInFuture() = %v, want %v", got, tt.want)
			}
		})
	}
}

// TestIsInPast tests past timestamp detection
func TestIsInPast(t *testing.T) {
	now := time.Now().UTC()
	future := now.Add(1 * time.Hour)
	past := now.Add(-1 * time.Hour)
	
	tests := []struct {
		name      string
		timestamp time.Time
		want      bool
	}{
		{"past time", past, true},
		{"future time", future, false},
		{"current time (approximately)", now, false},
	}
	
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := IsInPast(tt.timestamp); got != tt.want {
				t.Errorf("IsInPast() = %v, want %v", got, tt.want)
			}
		})
	}
}

// TestTimestampRoundtrip tests parsing and formatting roundtrip
func TestTimestampRoundtrip(t *testing.T) {
	original := "2025-11-10T14:30:00Z"
	
	parsed, err := ParseUTCTimestamp(original)
	if err != nil {
		t.Fatalf("ParseUTCTimestamp() error = %v", err)
	}
	
	formatted := FormatUTCTimestamp(parsed)
	
	if formatted != original {
		t.Errorf("Roundtrip failed: got %q, want %q", formatted, original)
	}
}
