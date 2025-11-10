package handlers

import (
	"net/http"
	"strings"

	"github.com/boomchecker/api-backend/internal/services"
	"github.com/gin-gonic/gin"
)

// NodeRegistrationHandler handles HTTP requests for node registration
type NodeRegistrationHandler struct {
	registrationService *services.NodeRegistrationService
}

// NewNodeRegistrationHandler creates a new node registration handler
func NewNodeRegistrationHandler(registrationService *services.NodeRegistrationService) *NodeRegistrationHandler {
	return &NodeRegistrationHandler{
		registrationService: registrationService,
	}
}

// RegisterNode handles POST /nodes/register
// @Summary Register a new node or re-register an existing node
// @Description Registers a node using a registration token. Returns UUID and JWT token for authentication.
// @Tags nodes
// @Accept json
// @Produce json
// @Param request body services.RegistrationRequest true "Registration request"
// @Success 200 {object} services.RegistrationResponse "Successfully registered (re-registration)"
// @Success 201 {object} services.RegistrationResponse "Successfully registered (new node)"
// @Failure 400 {object} ErrorResponse "Bad request - validation failed"
// @Failure 401 {object} ErrorResponse "Unauthorized - invalid or expired token"
// @Failure 500 {object} ErrorResponse "Internal server error"
// @Router /nodes/register [post]
func (h *NodeRegistrationHandler) RegisterNode(c *gin.Context) {
	var req services.RegistrationRequest

	// Bind and validate JSON request
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, ErrorResponse{
			Error:   "Invalid request format",
			Message: err.Error(),
		})
		return
	}

	// Call registration service
	response, err := h.registrationService.RegisterNode(&req)
	if err != nil {
		// Determine appropriate status code based on error type
		statusCode := determineErrorStatusCode(err)
		c.JSON(statusCode, ErrorResponse{
			Error:   "Registration failed",
			Message: err.Error(),
		})
		return
	}

	// Return 201 Created for new nodes, 200 OK for re-registration
	statusCode := http.StatusOK
	if response.IsNewNode {
		statusCode = http.StatusCreated
	}

	c.JSON(statusCode, response)
}

// ErrorResponse represents an error response
type ErrorResponse struct {
	Error   string `json:"error"`
	Message string `json:"message"`
}

// determineErrorStatusCode maps error types to HTTP status codes
func determineErrorStatusCode(err error) int {
	errMsg := err.Error()

	// Token-related errors -> 401 Unauthorized
	if strings.Contains(errMsg, "invalid registration token") ||
		strings.Contains(errMsg, "token has expired") ||
		strings.Contains(errMsg, "token has no remaining uses") ||
		strings.Contains(errMsg, "token cannot be used for MAC address") ||
		strings.Contains(errMsg, "token not found") {
		return http.StatusUnauthorized
	}

	// Validation errors -> 400 Bad Request
	if strings.Contains(errMsg, "validation failed") ||
		strings.Contains(errMsg, "invalid MAC address") ||
		strings.Contains(errMsg, "invalid firmware version") ||
		strings.Contains(errMsg, "invalid GPS coordinates") {
		return http.StatusBadRequest
	}

	// Revoked node -> 403 Forbidden
	if strings.Contains(errMsg, "node is revoked") {
		return http.StatusForbidden
	}

	// Default to 500 Internal Server Error
	return http.StatusInternalServerError
}
