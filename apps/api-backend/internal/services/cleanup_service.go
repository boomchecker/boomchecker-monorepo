package services

import (
	"log"
	"time"

	"github.com/boomchecker/api-backend/internal/repositories"
)

// CleanupService handles periodic cleanup of expired tokens
type CleanupService struct {
	adminTokenRepo        *repositories.AdminTokenRepository
	registrationTokenRepo *repositories.RegistrationTokenRepository
	ticker                *time.Ticker
	done                  chan bool
}

// NewCleanupService creates a new cleanup service
func NewCleanupService(
	adminTokenRepo *repositories.AdminTokenRepository,
	registrationTokenRepo *repositories.RegistrationTokenRepository,
) *CleanupService {
	return &CleanupService{
		adminTokenRepo:        adminTokenRepo,
		registrationTokenRepo: registrationTokenRepo,
		done:                  make(chan bool),
	}
}

// Start begins the periodic cleanup process
// Runs cleanup every 24 hours
func (s *CleanupService) Start() {
	// Run cleanup immediately on startup
	s.runCleanup()

	// Setup ticker for daily cleanup (every 24 hours)
	s.ticker = time.NewTicker(24 * time.Hour)

	go func() {
		for {
			select {
			case <-s.ticker.C:
				s.runCleanup()
			case <-s.done:
				log.Println("Cleanup service stopped")
				return
			}
		}
	}()

	log.Println("Cleanup service started - will run every 24 hours")
}

// Stop stops the cleanup service
func (s *CleanupService) Stop() {
	if s.ticker != nil {
		s.ticker.Stop()
	}
	s.done <- true
}

// runCleanup performs the actual cleanup of expired tokens
func (s *CleanupService) runCleanup() {
	log.Println("Starting scheduled token cleanup...")

	// Cleanup expired admin tokens
	adminCount, err := s.adminTokenRepo.CleanupExpired()
	if err != nil {
		log.Printf("ERROR: Failed to cleanup admin tokens: %v", err)
	} else {
		log.Printf("Cleaned up %d expired admin token(s)", adminCount)
	}

	// Cleanup expired registration tokens
	regCount, err := s.registrationTokenRepo.CleanupExpired()
	if err != nil {
		log.Printf("ERROR: Failed to cleanup registration tokens: %v", err)
	} else {
		log.Printf("Cleaned up %d expired registration token(s)", regCount)
	}

	log.Printf("Token cleanup completed: %d admin + %d registration tokens removed", adminCount, regCount)
}

// RunCleanupNow triggers an immediate cleanup (useful for testing or manual trigger)
func (s *CleanupService) RunCleanupNow() {
	s.runCleanup()
}
