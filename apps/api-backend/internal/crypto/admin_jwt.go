package crypto

import (
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// AdminClaims represents JWT claims for admin authentication
type AdminClaims struct {
	Email string `json:"email"` // Admin email
	jwt.RegisteredClaims
}

const (
	// AdminJWTExpiration is the admin token expiration (24 hours)
	AdminJWTExpiration = 24 * time.Hour
)

// GenerateAdminJWT generates a JWT token for admin authentication
// Returns the JWT token string and expiration timestamp
func GenerateAdminJWT(email string, jwtSecretBase64 string) (token string, expiresAt time.Time, err error) {
	if email == "" {
		return "", time.Time{}, fmt.Errorf("email is required")
	}
	if jwtSecretBase64 == "" {
		return "", time.Time{}, fmt.Errorf("JWT secret is required")
	}

	// Decode JWT secret from base64
	jwtSecret, err := base64.StdEncoding.DecodeString(jwtSecretBase64)
	if err != nil {
		return "", time.Time{}, fmt.Errorf("failed to decode JWT secret: %w", err)
	}

	now := time.Now().UTC()
	expiresAtTime := now.Add(AdminJWTExpiration)

	// Create claims
	claims := AdminClaims{
		Email: email,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    JWTIssuer,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(expiresAtTime),
			Subject:   email,
		},
	}

	// Create token with claims
	jwtToken := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)

	// Sign token with secret
	tokenString, err := jwtToken.SignedString(jwtSecret)
	if err != nil {
		return "", time.Time{}, fmt.Errorf("failed to sign JWT token: %w", err)
	}

	return tokenString, expiresAtTime, nil
}

// VerifyAdminJWT verifies a JWT token and returns the claims
// Returns error if token is invalid, expired, or signature doesn't match
func VerifyAdminJWT(tokenString string, jwtSecretBase64 string) (*AdminClaims, error) {
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
	token, err := jwt.ParseWithClaims(tokenString, &AdminClaims{}, func(token *jwt.Token) (interface{}, error) {
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
	claims, ok := token.Claims.(*AdminClaims)
	if !ok || !token.Valid {
		return nil, fmt.Errorf("invalid token claims")
	}

	return claims, nil
}

// IsAdminTokenExpired checks if a token is expired without full verification
func IsAdminTokenExpired(tokenString string) (bool, error) {
	// Parse without verification (only check expiration)
	token, _, err := jwt.NewParser().ParseUnverified(tokenString, &AdminClaims{})
	if err != nil {
		return false, fmt.Errorf("failed to parse token: %w", err)
	}

	claims, ok := token.Claims.(*AdminClaims)
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

// GetEmailFromToken extracts email from token without verification
// WARNING: This does NOT verify the token signature! Use only for logging/debugging
func GetEmailFromToken(tokenString string) (string, error) {
	// Parse without verification
	token, _, err := jwt.NewParser().ParseUnverified(tokenString, &AdminClaims{})
	if err != nil {
		return "", fmt.Errorf("failed to parse token: %w", err)
	}

	claims, ok := token.Claims.(*AdminClaims)
	if !ok {
		return "", fmt.Errorf("invalid token claims")
	}

	return claims.Email, nil
}

// HashToken creates a SHA-256 hash of the JWT token for storage in database
// This prevents token theft from database dumps
func HashToken(token string) string {
	hash := sha256.Sum256([]byte(token))
	return hex.EncodeToString(hash[:])
}
