package middleware

import (
	"net/http"
	"strings"

	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
)

// AdminAuthMiddleware validates admin authentication using JWT tokens
// Tokens are obtained via POST /admin/auth/request and sent via email
// They are valid for 24 hours and must be included in the Authorization header
func AdminAuthMiddleware(adminAuthService *services.AdminAuthService) gin.HandlerFunc {
	return func(c *gin.Context) {
		// Step 1: Extract token from Authorization header
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			unauthorizedResponse(c, "Admin authentication required. Please request a login token via POST /admin/auth/request")
			return
		}

		// Step 2: Validate Bearer token format
		if !strings.HasPrefix(authHeader, "Bearer ") {
			unauthorizedResponse(c, "Invalid authorization header format. Expected: Bearer <token>")
			return
		}

		// Step 3: Extract token string
		tokenString := strings.TrimPrefix(authHeader, "Bearer ")
		if tokenString == "" {
			unauthorizedResponse(c, "Token is required in Authorization header")
			return
		}

		// Step 4: Validate JWT token using admin auth service
		claims, err := adminAuthService.ValidateToken(tokenString)
		if err != nil {
			unauthorizedResponse(c, "Invalid or expired token: "+err.Error())
			return
		}

		// Step 5: Store claims in context for use by handlers
		c.Set("admin_email", claims.Email)
		c.Set("admin_claims", claims)

		// Token is valid, continue to next handler
		c.Next()
	}
}

// unauthorizedResponse is a helper to return 401 responses
func unauthorizedResponse(c *gin.Context, message string) {
	c.JSON(http.StatusUnauthorized, gin.H{
		"error":   "Unauthorized",
		"message": message,
	})
	c.Abort()
}
