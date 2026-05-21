package main

import (
	"encoding/base64"
	"fmt"
	"log"
	"os"

	"github.com/boomchecker/api-backend/internal/crypto"
)

func main() {
	fmt.Println("BoomChecker - Encryption Key Generator")
	fmt.Println("=========================================")
	fmt.Println()

	// Generate encryption key
	key, err := crypto.GenerateEncryptionKey()
	if err != nil {
		log.Fatalf("Failed to generate encryption key: %v", err)
	}

	fmt.Println("Successfully generated encryption key!")
	fmt.Println()
	fmt.Println("Add this to your .env file:")
	fmt.Println("----------------------------")
	fmt.Printf("JWT_ENCRYPTION_KEY=%s\n", key)
	fmt.Println()
	fmt.Println("SECURITY WARNING:")
	fmt.Println("   - Keep this key SECRET and SECURE")
	fmt.Println("   - Never commit this key to version control")
	fmt.Println("   - If the key is leaked, all JWT secrets are compromised")
	fmt.Println("   - Use different keys for development/staging/production")
	fmt.Println()

	// Test the key
	fmt.Println("Testing encryption/decryption...")
	testSecret := "test-jwt-secret-12345"
	
	// Decode the generated key to bytes for testing
	keyBytes, err := base64.StdEncoding.DecodeString(key)
	if err != nil {
		log.Fatalf("Failed to decode key: %v", err)
	}
	
	// Temporarily set env var for GetEncryptionKey to work
	os.Setenv(crypto.EnvKeyName, key)
	
	encrypted, err := crypto.Encrypt(testSecret, keyBytes)
	if err != nil {
		log.Fatalf("Encryption test failed: %v", err)
	}

	decrypted, err := crypto.Decrypt(encrypted, keyBytes)
	if err != nil {
		log.Fatalf("Decryption test failed: %v", err)
	}

	if decrypted != testSecret {
		log.Fatalf("Test failed: decrypted value doesn't match original")
	}

	fmt.Println("Encryption test passed!")
	fmt.Println()
}
