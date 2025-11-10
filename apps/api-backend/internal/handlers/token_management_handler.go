package handlers

import (
	"net/http"

	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
)

// TODO: Admin Authentication Implementation Required
// These endpoints currently use AdminAuthMiddleware which is a placeholder.
// 
// Required implementation:
// 1. Admin Login Flow (POST /admin/auth/request):
//    - Admin provides email address
//    - System generates JWT token valid for 24 hours
//    - Token contains claims: { email, role: "admin", exp, iat }
//    - Token is sent to admin's email address
//    - Admin uses this token for subsequent API calls
//
// 2. JWT Token Structure:
//    - Use separate signing key from node JWTs (ADMIN_JWT_SECRET in .env)
//    - Include: email, role=admin, iat (issued at), exp (expires 24h)
//    - Sign with HS256 or RS256
//
// 3. Middleware Updates:
//    - internal/middleware/admin_auth.go needs to validate JWT
//    - Extract token from Authorization: Bearer <token>
//    - Verify signature, expiration, and admin role claim
//
// 4. Email Service:
//    - Configure SMTP or use service (SendGrid, Mailgun, AWS SES)
//    - Template for login email with token
//    - Rate limiting to prevent email spam
//
// 5. Security Considerations:
//    - Store admin emails in config or database
//    - Consider single-use tokens or token revocation
//    - Add IP binding or additional security measures
//    - Log all admin actions for audit trail

// TokenManagementHandler handles HTTP requests for registration token management
type TokenManagementHandler struct {
	tokenService *services.TokenManagementService
}

// NewTokenManagementHandler creates a new token management handler
func NewTokenManagementHandler(tokenService *services.TokenManagementService) *TokenManagementHandler {
	return &TokenManagementHandler{
		tokenService: tokenService,
	}
}

// CreateToken handles POST /admin/registration-node-tokens
// @Summary Create registration token
// @Description Create new registration token with optional expiration, usage limit, and MAC authorization
// @Tags admin
// @Accept json
// @Produce json
// @Security AdminAuth
// @Param request body services.CreateTokenRequest true "Token configuration"
// @Success 201 {object} services.CreateTokenResponse "Token created"
// @Failure 400 {object} ErrorResponse "Invalid request or validation error"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens [post]
func (h *TokenManagementHandler) CreateToken(c *gin.Context) {
	var req services.CreateTokenRequest

	// Bind and validate JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, ErrorResponse{
			Error:   "Invalid request format",
			Message: err.Error(),
		})
		return
	}

	// Call token service
	response, err := h.tokenService.CreateToken(&req)
	if err != nil {
		statusCode := http.StatusInternalServerError
		if isValidationError(err) {
			statusCode = http.StatusBadRequest
		}

		c.JSON(statusCode, ErrorResponse{
			Error:   "Failed to create token",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusCreated, response)
}

// ListAllTokens handles GET /admin/registration-node-tokens
// @Summary List all tokens
// @Description Return all registration tokens (active, expired, used)
// @Tags admin
// @Produce json
// @Security AdminAuth
// @Success 200 {object} map[string]interface{} "List with tokens array and count"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens [get]
func (h *TokenManagementHandler) ListAllTokens(c *gin.Context) {
	tokens, err := h.tokenService.ListAllTokens()
	if err != nil {
		c.JSON(http.StatusInternalServerError, ErrorResponse{
			Error:   "Failed to list tokens",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"tokens": tokens,
		"count":  len(tokens),
	})
}

// ListActiveTokens handles GET /admin/registration-node-tokens/active
// @Summary List active tokens
// @Description Return only non-expired tokens with remaining uses
// @Tags admin
// @Produce json
// @Security AdminAuth
// @Success 200 {object} map[string]interface{} "List with tokens array and count"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens/active [get]
func (h *TokenManagementHandler) ListActiveTokens(c *gin.Context) {
	tokens, err := h.tokenService.ListActiveTokens()
	if err != nil {
		c.JSON(http.StatusInternalServerError, ErrorResponse{
			Error:   "Failed to list active tokens",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"tokens": tokens,
		"count":  len(tokens),
	})
}

// GetToken handles GET /admin/registration-node-tokens/:token
// @Summary Get token details
// @Description Return details of specific registration token
// @Tags admin
// @Produce json
// @Security AdminAuth
// @Param token path string true "Token value"
// @Success 200 {object} services.TokenListResponse "Token details"
// @Failure 404 {object} ErrorResponse "Token not found"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens/{token} [get]
func (h *TokenManagementHandler) GetToken(c *gin.Context) {
	tokenValue := c.Param("token")

	token, err := h.tokenService.GetToken(tokenValue)
	if err != nil {
		c.JSON(http.StatusNotFound, ErrorResponse{
			Error:   "Token not found",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, token)
}

// DeleteToken handles DELETE /admin/registration-node-tokens/:token
// @Summary Delete token
// @Description Permanently remove registration token
// @Tags admin
// @Security AdminAuth
// @Param token path string true "Token value"
// @Success 204 "Token deleted"
// @Failure 404 {object} ErrorResponse "Token not found"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens/{token} [delete]
func (h *TokenManagementHandler) DeleteToken(c *gin.Context) {
	tokenValue := c.Param("token")

	if err := h.tokenService.DeleteToken(tokenValue); err != nil {
		c.JSON(http.StatusNotFound, ErrorResponse{
			Error:   "Failed to delete token",
			Message: err.Error(),
		})
		return
	}

	c.Status(http.StatusNoContent)
}

// CleanupExpiredTokens handles POST /admin/registration-node-tokens/cleanup
// @Summary Cleanup expired tokens
// @Description Remove all expired tokens from database
// @Tags admin
// @Produce json
// @Security AdminAuth
// @Success 200 {object} map[string]interface{} "Cleanup results with deleted count"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens/cleanup [post]
func (h *TokenManagementHandler) CleanupExpiredTokens(c *gin.Context) {
	count, err := h.tokenService.CleanupExpiredTokens()
	if err != nil {
		c.JSON(http.StatusInternalServerError, ErrorResponse{
			Error:   "Failed to cleanup expired tokens",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message":        "Expired tokens cleaned up successfully",
		"deleted_tokens": count,
	})
}

// GetStatistics handles GET /admin/registration-node-tokens/statistics
// @Summary Get token statistics
// @Description Return statistics about registration tokens (total, active, expired counts)
// @Tags admin
// @Produce json
// @Security AdminAuth
// @Success 200 {object} map[string]interface{} "Token statistics"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /admin/registration-node-tokens/statistics [get]
func (h *TokenManagementHandler) GetStatistics(c *gin.Context) {
	stats, err := h.tokenService.GetStatistics()
	if err != nil {
		c.JSON(http.StatusInternalServerError, ErrorResponse{
			Error:   "Failed to get statistics",
			Message: err.Error(),
		})
		return
	}

	c.JSON(http.StatusOK, stats)
}

// isValidationError checks if an error is a validation error
func isValidationError(err error) bool {
	msg := err.Error()
	return len(msg) > 0 && (msg[:10] == "validation" || msg[:7] == "invalid")
}
