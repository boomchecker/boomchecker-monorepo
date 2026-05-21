package crypto

import (
	"encoding/base64"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// NodeClaims represents JWT claims for node authentication
type NodeClaims struct {
	NodeUUID string `json:"node_uuid"` // Node UUID
	jwt.RegisteredClaims
}

const (
	// JWTIssuer is the issuer name
	JWTIssuer = "boomchecker-api"

	// DefaultJWTExpiration is the default token expiration (1 year)
	DefaultJWTExpiration = 365 * 24 * time.Hour
)

// GenerateNodeJWT generates a JWT token for a node using golang-jwt/jwt
// Returns the JWT token string and expiration timestamp
func GenerateNodeJWT(nodeUUID string, jwtSecretBase64 string, expirationDuration time.Duration) (token string, expiresAt int64, err error) {
	if nodeUUID == "" {
		return "", 0, fmt.Errorf("node UUID is required")
	}
	if jwtSecretBase64 == "" {
		return "", 0, fmt.Errorf("JWT secret is required")
	}

	// Decode JWT secret from base64
	jwtSecret, err := base64.StdEncoding.DecodeString(jwtSecretBase64)
	if err != nil {
		return "", 0, fmt.Errorf("failed to decode JWT secret: %w", err)
	}

	// Use default expiration if not specified
	if expirationDuration == 0 {
		expirationDuration = DefaultJWTExpiration
	}

	now := time.Now().UTC()
	expiresAtTime := now.Add(expirationDuration)
	expiresAt = expiresAtTime.Unix()

	// Create claims
	claims := NodeClaims{
		NodeUUID: nodeUUID,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    JWTIssuer,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(expiresAtTime),
		},
	}

	// Create token with claims
	jwtToken := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)

	// Sign token with secret
	tokenString, err := jwtToken.SignedString(jwtSecret)
	if err != nil {
		return "", 0, fmt.Errorf("failed to sign JWT token: %w", err)
	}

	return tokenString, expiresAt, nil
}

// VerifyNodeJWT verifies a JWT token and returns the claims
// Returns error if token is invalid, expired, or signature doesn't match
func VerifyNodeJWT(tokenString string, jwtSecretBase64 string) (*NodeClaims, error) {
	if tokenString == "" {
		return nil, fmt.Errorf("token is required")
	}
	if jwtSecretBase64 == "" {
		return nil, fmt.Errorf("JWT secret is required")
	}

	// Decode JWT secret from base64
	jwtSecret, err := base64.StdEncoding.DecodeString(jwtSecretBase64)
	if err != nil {
		return nil, fmt.Errorf("failed to decode JWT secret: %w", err)
	}

	// Parse and validate token
	token, err := jwt.ParseWithClaims(tokenString, &NodeClaims{}, func(token *jwt.Token) (interface{}, error) {
		// Verify signing method
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return jwtSecret, nil
	})

	if err != nil {
		return nil, fmt.Errorf("failed to parse token: %w", err)
	}

	// Extract claims
	claims, ok := token.Claims.(*NodeClaims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid token claims")
	}

	return claims, nil
}

// IsTokenExpired checks if a token is expired without full verification
func IsTokenExpired(tokenString string) (bool, error) {
	// Parse without verification (only check expiration)
	token, _, err := jwt.NewParser().ParseUnverified(tokenString, &NodeClaims{})
	if err != nil {
		return false, fmt.Errorf("failed to parse token: %w", err)
	}

	claims, ok := token.Claims.(*NodeClaims)
	if !ok {
		return false, fmt.Errorf("invalid token claims")
	}

	// Check if expired
	now := time.Now().UTC()
	if claims.ExpiresAt != nil && claims.ExpiresAt.Before(now) {
		return true, nil
	}

	return false, nil
}

// GetNodeUUIDFromToken extracts node UUID from token without verification
// WARNING: This does NOT verify the token signature! Use only for logging/debugging
func GetNodeUUIDFromToken(tokenString string) (string, error) {
	// Parse without verification
	token, _, err := jwt.NewParser().ParseUnverified(tokenString, &NodeClaims{})
	if err != nil {
		return "", fmt.Errorf("failed to parse token: %w", err)
	}

	claims, ok := token.Claims.(*NodeClaims)
	if !ok {
		return "", fmt.Errorf("invalid token claims")
	}

	return claims.NodeUUID, nil
}
