package validators

import (
	"fmt"
	"strings"
	"time"
)

// UTC timestamp formats
const (
	// ISO8601 format with Z suffix (recommended)
	ISO8601UTC = "2006-01-02T15:04:05Z"
	
	// ISO8601 with milliseconds
	ISO8601UTCMillis = "2006-01-02T15:04:05.000Z"
	
	// RFC3339 (also valid UTC format)
	RFC3339UTC = time.RFC3339
)

// IsValidUTCTimestamp checks if the timestamp string is valid UTC format
// Accepts: 2025-11-10T14:30:00Z or 2025-11-10T14:30:00.123Z
func IsValidUTCTimestamp(timestamp string) bool {
	if timestamp == "" {
		return false
	}

	// Must end with 'Z' to indicate UTC
	if !strings.HasSuffix(timestamp, "Z") {
		return false
	}

	// Try parsing with different formats
	formats := []string{
		ISO8601UTC,
		ISO8601UTCMillis,
		time.RFC3339,
	}

	for _, format := range formats {
		if _, err := time.Parse(format, timestamp); err == nil {
			return true
		}
	}

	return false
}

// ValidateUTCTimestamp validates timestamp and returns an error if invalid
func ValidateUTCTimestamp(timestamp string, fieldName string) error {
	if timestamp == "" {
		return NewValidationError(fieldName, "timestamp is required")
	}
	if !IsValidUTCTimestamp(timestamp) {
		return NewValidationError(fieldName, "invalid UTC timestamp format (expected: 2025-11-10T14:30:00Z)")
	}
	return nil
}

// ParseUTCTimestamp parses UTC timestamp string to time.Time
// Returns time in UTC timezone
func ParseUTCTimestamp(timestamp string) (time.Time, error) {
	if timestamp == "" {
		return time.Time{}, NewValidationError("timestamp", "timestamp is required")
	}

	formats := []string{
		ISO8601UTC,
		ISO8601UTCMillis,
		time.RFC3339,
	}

	for _, format := range formats {
		if t, err := time.Parse(format, timestamp); err == nil {
			return t.UTC(), nil
		}
	}

	return time.Time{}, NewValidationError("timestamp", fmt.Sprintf("invalid UTC timestamp format: %s", timestamp))
}

// FormatUTCTimestamp formats time.Time to UTC ISO 8601 string
// Always returns format: 2025-11-10T14:30:00Z
func FormatUTCTimestamp(t time.Time) string {
	return t.UTC().Format(ISO8601UTC)
}

// IsInFuture checks if the timestamp is in the future
func IsInFuture(t time.Time) bool {
	return t.After(time.Now().UTC())
}

// IsInPast checks if the timestamp is in the past
func IsInPast(t time.Time) bool {
	return t.Before(time.Now().UTC())
}

// ValidateFutureTimestamp validates that timestamp is in the future
func ValidateFutureTimestamp(t time.Time, fieldName string) error {
	if !IsInFuture(t) {
		return NewValidationError(fieldName, fmt.Sprintf("timestamp must be in the future (got: %s)", FormatUTCTimestamp(t)))
	}
	return nil
}

// ValidatePastTimestamp validates that timestamp is in the past
func ValidatePastTimestamp(t time.Time, fieldName string) error {
	if !IsInPast(t) {
		return NewValidationError(fieldName, fmt.Sprintf("timestamp must be in the past (got: %s)", FormatUTCTimestamp(t)))
	}
	return nil
}

// EnsureUTC ensures time is in UTC timezone
// If time is in different timezone, converts it to UTC
func EnsureUTC(t time.Time) time.Time {
	return t.UTC()
}
