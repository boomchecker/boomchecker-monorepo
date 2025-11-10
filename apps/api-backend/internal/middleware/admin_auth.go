package middleware

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// TODO: Implement proper admin authentication
// Admin authentication flow:
// 1. Admin requests login via POST /admin/auth/request
//    - Provide email address
//    - System generates a JWT token valid for 24 hours
//    - Token is sent to admin's email
// 2. Admin uses the JWT token from email for subsequent requests
//    - Token is sent in Authorization header: "Bearer <token>"
//    - Middleware validates JWT signature and expiration
//    - JWT contains claims: email, role=admin, exp, iat
// 3. Token expires after 24 hours, admin must request new login
//
// Implementation files needed:
// - internal/services/admin_auth_service.go (email sending, JWT generation)
// - internal/handlers/admin_auth_handler.go (POST /admin/auth/request endpoint)
// - internal/models/admin.go (optional: admin user model if storing in DB)
// - Update this middleware to validate JWT instead of dummy check
//
// Security considerations:
// - JWT secret should be different from node JWT secrets (separate key in .env)
// - Email service configuration (SMTP or service like SendGrid/Mailgun)
// - Rate limiting on auth request endpoint to prevent email spam
// - Token should be single-use or include additional security (CSRF token, IP binding)

// AdminAuthMiddleware validates admin authentication
// TEMPORARY: This is a placeholder that allows all requests through
// In production, this MUST validate JWT tokens from email-based login
func AdminAuthMiddleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		// TODO: Replace this with proper JWT validation
		// Expected flow:
		// 1. Extract token from Authorization header
		// 2. Validate JWT signature using admin JWT secret
		// 3. Check expiration (max 24 hours)
		// 4. Verify claims (role=admin, valid email)
		// 5. If invalid, return 401 Unauthorized

		// TEMPORARY: Allow all requests (INSECURE - for development only)
		// Uncomment the following to enable placeholder auth check:
		/*
		authHeader := c.GetHeader("Authorization")
		if authHeader == "" {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "Unauthorized",
				"message": "Admin authentication required. Please request login token via email.",
			})
			c.Abort()
			return
		}

		// In production, validate JWT here
		// For now, just check if header exists
		if !strings.HasPrefix(authHeader, "Bearer ") {
			c.JSON(http.StatusUnauthorized, gin.H{
				"error":   "Unauthorized",
				"message": "Invalid authorization header format. Expected: Bearer <token>",
			})
			c.Abort()
			return
		}
		*/

		// TEMPORARY WARNING: Admin endpoints are currently UNPROTECTED
		// This allows development/testing but is INSECURE for production
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
