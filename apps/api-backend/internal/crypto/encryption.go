package crypto

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"os"
)

var (
	// ErrInvalidKeySize is returned when encryption key has invalid size
	ErrInvalidKeySize = errors.New("encryption key must be 32 bytes for AES-256")

	// ErrEncryptionKeyNotSet is returned when encryption key is not configured
	ErrEncryptionKeyNotSet = errors.New("encryption key not set in environment variable JWT_ENCRYPTION_KEY")

	// ErrInvalidCiphertext is returned when ciphertext cannot be decrypted
	ErrInvalidCiphertext = errors.New("invalid ciphertext: cannot decrypt")

	// ErrCiphertextTooShort is returned when ciphertext is too short
	ErrCiphertextTooShort = errors.New("ciphertext too short")
)

const (
	// AES256KeySize is the required key size for AES-256 (32 bytes)
	AES256KeySize = 32

	// JWTSecretSize is the size of generated JWT secrets (32 bytes)
	JWTSecretSize = 32

	// EnvKeyName is the environment variable name for encryption key
	EnvKeyName = "JWT_ENCRYPTION_KEY"
)

// GetEncryptionKey retrieves the encryption key from environment variable
// Returns error if key is not set or has invalid size
func GetEncryptionKey() ([]byte, error) {
	keyBase64 := os.Getenv(EnvKeyName)
	if keyBase64 == "" {
		return nil, ErrEncryptionKeyNotSet
	}

	// Decode from base64
	key, err := base64.StdEncoding.DecodeString(keyBase64)
	if err != nil {
		return nil, fmt.Errorf("failed to decode encryption key: %w", err)
	}

	// Validate key size
	if len(key) != AES256KeySize {
		return nil, fmt.Errorf("%w: got %d bytes, expected %d", ErrInvalidKeySize, len(key), AES256KeySize)
	}

	return key, nil
}

// GenerateEncryptionKey generates a new 32-byte encryption key
// This should be called once during initial setup and stored securely
func GenerateEncryptionKey() (string, error) {
	key := make([]byte, AES256KeySize)
	if _, err := io.ReadFull(rand.Reader, key); err != nil {
		return "", fmt.Errorf("failed to generate encryption key: %w", err)
	}

	// Return as base64 for easy storage in environment variable
	return base64.StdEncoding.EncodeToString(key), nil
}

// Encrypt encrypts plaintext using AES-256-GCM
// Returns base64-encoded ciphertext with nonce prepended
// Format: [nonce(12 bytes)][ciphertext][auth_tag(16 bytes)]
func Encrypt(plaintext string, key []byte) (string, error) {
	if len(key) != AES256KeySize {
		return "", ErrInvalidKeySize
	}

	// Create AES cipher block
	block, err := aes.NewCipher(key)
	if err != nil {
		return "", fmt.Errorf("failed to create cipher: %w", err)
	}

	// Create GCM mode (Galois/Counter Mode)
	aesGCM, err := cipher.NewGCM(block)
	if err != nil {
		return "", fmt.Errorf("failed to create GCM: %w", err)
	}

	// Generate a random nonce (number used once)
	// GCM standard nonce size is 12 bytes
	nonce := make([]byte, aesGCM.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return "", fmt.Errorf("failed to generate nonce: %w", err)
	}

	// Encrypt and authenticate
	// aesGCM.Seal appends the ciphertext and auth tag to nonce
	ciphertext := aesGCM.Seal(nonce, nonce, []byte(plaintext), nil)

	// Encode to base64 for storage in database
	return base64.StdEncoding.EncodeToString(ciphertext), nil
}

// Decrypt decrypts base64-encoded ciphertext using AES-256-GCM
// Returns original plaintext
func Decrypt(ciphertextBase64 string, key []byte) (string, error) {
	if len(key) != AES256KeySize {
		return "", ErrInvalidKeySize
	}

	// Decode from base64
	ciphertext, err := base64.StdEncoding.DecodeString(ciphertextBase64)
	if err != nil {
		return "", fmt.Errorf("failed to decode ciphertext: %w", err)
	}

	// Create AES cipher block
	block, err := aes.NewCipher(key)
	if err != nil {
		return "", fmt.Errorf("failed to create cipher: %w", err)
	}

	// Create GCM mode
	aesGCM, err := cipher.NewGCM(block)
	if err != nil {
		return "", fmt.Errorf("failed to create GCM: %w", err)
	}

	// Check minimum ciphertext length
	nonceSize := aesGCM.NonceSize()
	if len(ciphertext) < nonceSize {
		return "", ErrCiphertextTooShort
	}

	// Extract nonce and ciphertext
	nonce, ciphertextData := ciphertext[:nonceSize], ciphertext[nonceSize:]

	// Decrypt and verify authentication tag
	plaintext, err := aesGCM.Open(nil, nonce, ciphertextData, nil)
	if err != nil {
		return "", fmt.Errorf("%w: %v", ErrInvalidCiphertext, err)
	}

	return string(plaintext), nil
}

// GenerateJWTSecret generates a cryptographically secure random JWT secret
// Returns base64-encoded 32-byte secret
func GenerateJWTSecret() (string, error) {
	secret := make([]byte, JWTSecretSize)
	if _, err := io.ReadFull(rand.Reader, secret); err != nil {
		return "", fmt.Errorf("failed to generate JWT secret: %w", err)
	}

	return base64.StdEncoding.EncodeToString(secret), nil
}

// EncryptJWTSecret generates a new JWT secret and encrypts it
// Returns encrypted JWT secret ready for database storage
func EncryptJWTSecret() (plainSecret, encryptedSecret string, err error) {
	// Get encryption key from environment
	key, err := GetEncryptionKey()
	if err != nil {
		return "", "", err
	}

	// Generate random JWT secret
	plainSecret, err = GenerateJWTSecret()
	if err != nil {
		return "", "", err
	}

	// Encrypt the secret
	encryptedSecret, err = Encrypt(plainSecret, key)
	if err != nil {
		return "", "", fmt.Errorf("failed to encrypt JWT secret: %w", err)
	}

	return plainSecret, encryptedSecret, nil
}

// DecryptJWTSecret decrypts an encrypted JWT secret from database
// Returns the original plaintext JWT secret
func DecryptJWTSecret(encryptedSecret string) (string, error) {
	// Get encryption key from environment
	key, err := GetEncryptionKey()
	if err != nil {
		return "", err
	}

	// Decrypt the secret
	plainSecret, err := Decrypt(encryptedSecret, key)
	if err != nil {
		return "", fmt.Errorf("failed to decrypt JWT secret: %w", err)
	}

	return plainSecret, nil
}

// ValidateEncryptionKey checks if the encryption key is properly configured
func ValidateEncryptionKey() error {
	_, err := GetEncryptionKey()
	return err
}
