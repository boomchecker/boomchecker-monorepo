package handlers

import (
	"net/http"
	"time"

	"github.com/boomchecker/api-backend/internal/models"
	"github.com/gin-gonic/gin"
)

// PingHandler handles the /ping endpoint for health checks
func PingHandler(c *gin.Context) {
	response := models.HealthResponse{
		Status:    "ok",
		Timestamp: time.Now(),
		Service:   "api-backend",
	}

	c.JSON(http.StatusOK, response)
}
