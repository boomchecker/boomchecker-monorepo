package models

import "time"

// HealthResponse represents the response structure for health check endpoints
type HealthResponse struct {
	Status    string    `json:"status"`
	Timestamp time.Time `json:"timestamp"`
	Service   string    `json:"service"`
}
