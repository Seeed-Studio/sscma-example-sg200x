// Package auth provides secure authentication using JWT and bcrypt.
// This replaces the insecure hardcoded AES key from the C++ implementation.
package auth

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/GehirnInc/crypt"
	_ "github.com/GehirnInc/crypt/md5_crypt"
	_ "github.com/GehirnInc/crypt/sha256_crypt"
	_ "github.com/GehirnInc/crypt/sha512_crypt"
	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"

	"supervisor/internal/config"
	"supervisor/pkg/logger"
)

var (
	ErrInvalidCredentials = errors.New("invalid credentials")
	ErrInvalidToken       = errors.New("invalid token")
	ErrTokenExpired       = errors.New("token expired")
	ErrUserNotFound       = errors.New("user not found")
	ErrAccountLocked      = errors.New("account temporarily locked due to too many failed attempts")
)

// Claims represents the JWT claims.
type Claims struct {
	Username string `json:"username"`
	jwt.RegisteredClaims
}

// LoginAttempt tracks failed login attempts for rate limiting.
type LoginAttempt struct {
	Count       int
	LastTry     time.Time
	LockedUntil time.Time
}

// AuthManager handles authentication.
type AuthManager struct {
	mu            sync.RWMutex
	cfg           *config.Config
	loginAttempts map[string]*LoginAttempt
}

// NewAuthManager creates a new AuthManager.
func NewAuthManager(cfg *config.Config) *AuthManager {
	return &AuthManager{
		cfg:           cfg,
		loginAttempts: make(map[string]*LoginAttempt),
	}
}

// Authenticate verifies user credentials using the system shadow file.
// This is more secure than the C++ implementation which used a hardcoded AES key.
func (am *AuthManager) Authenticate(username, password string) (string, error) {
	am.mu.Lock()
	defer am.mu.Unlock()

	// Check if account is locked
	if attempt, exists := am.loginAttempts[username]; exists {
		if time.Now().Before(attempt.LockedUntil) {
			remaining := time.Until(attempt.LockedUntil).Round(time.Second)
			return "", fmt.Errorf("%w: try again in %v", ErrAccountLocked, remaining)
		}
		// Reset if lockout period has passed
		if time.Now().After(attempt.LockedUntil) && attempt.Count >= am.cfg.MaxLoginAttempts {
			attempt.Count = 0
		}
	}

	// Verify credentials (password is now plaintext from frontend)
	if !am.verifySystemPassword(username, password) {
		am.recordFailedAttempt(username)
		return "", ErrInvalidCredentials
	}

	// Reset failed attempts on successful login
	delete(am.loginAttempts, username)

	// Generate JWT token
	return am.generateToken(username)
}

// verifySystemPassword verifies the password against the system shadow file.
func (am *AuthManager) verifySystemPassword(username, password string) bool {
	// Always use shadow file verification since supervisor runs as root
	// The `su` command doesn't work for verification when run as root
	// because root can switch to any user without a password
	return am.verifyShadowPassword(username, password)
}

// verifyShadowPassword reads and verifies against /etc/shadow.
func (am *AuthManager) verifyShadowPassword(username, password string) bool {
	file, err := os.Open("/etc/shadow")
	if err != nil {
		logger.Error("Failed to open shadow file: %v", err)
		return false
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.Split(line, ":")
		if len(parts) < 2 {
			continue
		}
		if parts[0] == username {
			hashedPassword := parts[1]
			if hashedPassword == "" || hashedPassword == "*" || hashedPassword == "!" || hashedPassword == "!!" {
				logger.Warn("User %s has no valid password", username)
				return false
			}
			// Use crypt to verify - we need to compare using the same algorithm
			return am.verifyHashedPassword(password, hashedPassword)
		}
	}
	return false
}

// verifyHashedPassword compares a password with a crypt-style hash.
func (am *AuthManager) verifyHashedPassword(password, hashedPassword string) bool {
	// Use the GehirnInc/crypt library to verify shadow passwords
	// This handles SHA-512 ($6$), SHA-256 ($5$), and MD5 ($1$) algorithms

	// Get the crypt implementation based on hash prefix
	crypter := crypt.NewFromHash(hashedPassword)
	if crypter == nil {
		logger.Error("Unsupported password hash algorithm for hash: %s...", hashedPassword[:min(10, len(hashedPassword))])
		return false
	}

	// Verify the password
	err := crypter.Verify(hashedPassword, []byte(password))
	return err == nil
}

// min returns the smaller of a and b
func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// recordFailedAttempt records a failed login attempt.
func (am *AuthManager) recordFailedAttempt(username string) {
	attempt, exists := am.loginAttempts[username]
	if !exists {
		attempt = &LoginAttempt{}
		am.loginAttempts[username] = attempt
	}

	attempt.Count++
	attempt.LastTry = time.Now()

	if attempt.Count >= am.cfg.MaxLoginAttempts {
		attempt.LockedUntil = time.Now().Add(time.Duration(am.cfg.LoginLockoutMinutes) * time.Minute)
		logger.Warn("Account %s locked until %v due to too many failed attempts",
			username, attempt.LockedUntil)
	}
}

// generateToken generates a JWT token for the user.
func (am *AuthManager) generateToken(username string) (string, error) {
	now := time.Now()
	claims := &Claims{
		Username: username,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(now.Add(am.cfg.TokenExpiration)),
			IssuedAt:  jwt.NewNumericDate(now),
			NotBefore: jwt.NewNumericDate(now),
			Issuer:    "supervisor",
			Subject:   username,
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString(am.cfg.JWTSecret)
}

// ValidateToken validates a JWT token and returns the claims.
func (am *AuthManager) ValidateToken(tokenString string) (*Claims, error) {
	// Remove "Bearer " prefix if present
	tokenString = strings.TrimPrefix(tokenString, "Bearer ")
	tokenString = strings.TrimSpace(tokenString)

	token, err := jwt.ParseWithClaims(tokenString, &Claims{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return am.cfg.JWTSecret, nil
	})

	if err != nil {
		if errors.Is(err, jwt.ErrTokenExpired) {
			return nil, ErrTokenExpired
		}
		return nil, ErrInvalidToken
	}

	claims, ok := token.Claims.(*Claims)
	if !ok || !token.Valid {
		return nil, ErrInvalidToken
	}

	return claims, nil
}

// HashPassword creates a bcrypt hash of a password.
// This is useful for storing custom passwords if needed.
func HashPassword(password string) (string, error) {
	cfg := config.Get()
	bytes, err := bcrypt.GenerateFromPassword([]byte(password), cfg.BCryptCost)
	return string(bytes), err
}

// CheckPasswordHash compares a password with a bcrypt hash.
func CheckPasswordHash(password, hash string) bool {
	err := bcrypt.CompareHashAndPassword([]byte(hash), []byte(password))
	return err == nil
}

// GetUsername retrieves the current system username (default recamera).
func GetUsername() string {
	// Check environment first
	if user := os.Getenv("SUPERVISOR_USER"); user != "" {
		return user
	}
	return "recamera"
}
