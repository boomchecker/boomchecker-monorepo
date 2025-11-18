package handlers

import (
	"net/http"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"github.com/gin-gonic/gin"
)

// PingHandler handles the /ping endpoint for health checks
// @Summary Health check
// @Description Simple health check endpoint
// @Tags health
// @Produce json
// @Success 200 {object} models.HealthResponse "Service is healthy"
// @Router /ping [get]
func PingHandler(c *gin.Context) {
	response := models.HealthResponse{
		Status:    "ok",
		Timestamp: time.Now(),
		Service:   "api-backend",
	}

	c.JSON(http.StatusOK, response)
}
