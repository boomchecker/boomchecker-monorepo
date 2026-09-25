package handlers

import (
	"net/http"
	"strings"

	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
)

// AdminAuthHandler handles HTTP requests for admin authentication
type AdminAuthHandler struct {
	adminAuthService *services.AdminAuthService
}

// NewAdminAuthHandler creates a new admin authentication handler
func NewAdminAuthHandler(adminAuthService *services.AdminAuthService) *AdminAuthHandler {
	return &AdminAuthHandler{
		adminAuthService: adminAuthService,
	}
}

// RequestToken handles POST /admin/auth/request
// @Summary Request admin authentication token
// @Description Request a JWT token for admin access. Token is sent via email and is valid for 24 hours. Rate limited to 1 request per 24 hours.
// @Tags admin-auth
// @Accept json
// @Produce json
// @Param request body services.TokenRequest true "Email address"
// @Success 200 {object} services.TokenResponse "Token request successful, email sent"
// @Failure 400 {object} ErrorResponse "Invalid request format"
// @Failure 401 {object} ErrorResponse "Unauthorized email"
// @Failure 429 {object} ErrorResponse "Rate limit exceeded"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/auth/request [post]
func (h *AdminAuthHandler) RequestToken(c *gin.Context) {
	var req services.TokenRequest

	// Bind and validate JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, ErrorResponse{
			Error:   "Invalid request format",
			Message: err.Error(),
		})
		return
	}

	// Call admin auth service to request token
	response, err := h.adminAuthService.RequestToken(c.Request.Context(), &req)
	if err != nil {
		// Determine appropriate status code based on error type
		statusCode := determineAdminAuthErrorStatusCode(err)
		c.JSON(statusCode, ErrorResponse{
			Error:   "Token request failed",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, response)
}

// determineAdminAuthErrorStatusCode maps error types to HTTP status codes
func determineAdminAuthErrorStatusCode(err error) int {
	errMsg := strings.ToLower(err.Error())

	// Unauthorized errors
	if strings.Contains(errMsg, "unauthorized") {
		return http.StatusUnauthorized
	}

	// Rate limit errors
	if strings.Contains(errMsg, "rate limit") {
		return http.StatusTooManyRequests
	}

	// Validation errors
	if strings.Contains(errMsg, "validation") ||
		strings.Contains(errMsg, "invalid") ||
		strings.Contains(errMsg, "required") {
		return http.StatusBadRequest
	}

	// Default to internal server error
	return http.StatusInternalServerError
}
