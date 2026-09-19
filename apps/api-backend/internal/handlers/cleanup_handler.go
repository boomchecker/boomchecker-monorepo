package handlers

import (
	"net/http"

	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
)

// CleanupHandler handles HTTP requests for token cleanup
type CleanupHandler struct {
	cleanupService *services.CleanupService
}

// NewCleanupHandler creates a new cleanup handler
func NewCleanupHandler(cleanupService *services.CleanupService) *CleanupHandler {
	return &CleanupHandler{
		cleanupService: cleanupService,
	}
}

// CleanupResponse represents the response from cleanup operation
type CleanupResponse struct {
	Message string `json:"message" example:"Token cleanup completed successfully"`
}

// CleanupAllExpiredTokens handles POST /admin/tokens/cleanup
// @Summary Cleanup all expired tokens
// @Description Manually trigger cleanup of expired admin tokens and registration tokens
// @Tags admin-maintenance
// @Security AdminAuth
// @Produce json
// @Success 200 {object} CleanupResponse "Cleanup completed successfully"
// @Failure 401 {object} ErrorResponse "Unauthorized"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/tokens/cleanup [post]
func (h *CleanupHandler) CleanupAllExpiredTokens(c *gin.Context) {
	// Trigger immediate cleanup
	h.cleanupService.RunCleanupNow()

	c.JSON(http.StatusOK, CleanupResponse{
		Message: "Token cleanup completed successfully",
	})
}
