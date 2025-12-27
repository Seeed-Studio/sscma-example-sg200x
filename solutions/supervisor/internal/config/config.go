// Package config provides configuration management for the supervisor.
package config

import (
	"crypto/rand"
	"encoding/hex"
	"os"
	"strconv"
	"sync"
	"time"
)

// Config holds all configuration for the supervisor.
type Config struct {
	// Server settings
	HTTPPort   string // Optional HTTP port (for redirect to HTTPS only)
	HTTPSPort  string // HTTPS port (required)
	RootDir    string
	ScriptPath string

	// TLS settings
	CertDir string // Directory for TLS certificates

	// Security settings
	NoAuth              bool
	JWTSecret           []byte
	TokenExpiration     time.Duration
	BCryptCost          int
	MaxLoginAttempts    int
	LoginLockoutMinutes int

	// Storage settings
	LocalDir string
	SDDir    string

	// Logging
	LogLevel int

	// Runtime settings
	DaemonMode bool
}

// LogLevels
const (
	LogError   = 0
	LogWarning = 1
	LogInfo    = 2
	LogDebug   = 3
	LogVerbose = 4
)

var (
	instance *Config
	once     sync.Once
)

// DefaultConfig returns a new Config with default values.
func DefaultConfig() *Config {
	return &Config{
		HTTPPort:   "80",  // HTTP for redirect only
		HTTPSPort:  "443", // HTTPS is required
		RootDir:    "/usr/share/supervisor/www/",
		ScriptPath: "/usr/share/supervisor/scripts/main.sh",

		CertDir: "/etc/recamera.conf/certs",

		NoAuth:              false,
		JWTSecret:           nil, // Will be generated
		TokenExpiration:     72 * time.Hour,
		BCryptCost:          12,
		MaxLoginAttempts:    5,
		LoginLockoutMinutes: 15,

		LocalDir: "/userdata",
		SDDir:    "/mnt/sd",

		LogLevel: LogWarning,

		DaemonMode: false,
	}
}

// Get returns the singleton configuration instance.
func Get() *Config {
	once.Do(func() {
		instance = DefaultConfig()
		instance.loadFromEnv()
		if instance.JWTSecret == nil {
			instance.JWTSecret = generateSecureKey(32)
		}
	})
	return instance
}

// loadFromEnv loads configuration from environment variables.
func (c *Config) loadFromEnv() {
	if port := os.Getenv("SUPERVISOR_HTTP_PORT"); port != "" {
		c.HTTPPort = port
	}
	if port := os.Getenv("SUPERVISOR_HTTPS_PORT"); port != "" {
		c.HTTPSPort = port
	}
	if dir := os.Getenv("SUPERVISOR_ROOT_DIR"); dir != "" {
		c.RootDir = dir
	}
	if script := os.Getenv("SUPERVISOR_SCRIPT_PATH"); script != "" {
		c.ScriptPath = script
	}
	if certDir := os.Getenv("SUPERVISOR_CERT_DIR"); certDir != "" {
		c.CertDir = certDir
	}
	if noAuth := os.Getenv("SUPERVISOR_NO_AUTH"); noAuth == "true" || noAuth == "1" {
		c.NoAuth = true
	}
	if secret := os.Getenv("SUPERVISOR_JWT_SECRET"); secret != "" {
		c.JWTSecret = []byte(secret)
	}
	if level := os.Getenv("SUPERVISOR_LOG_LEVEL"); level != "" {
		if l, err := strconv.Atoi(level); err == nil {
			c.LogLevel = l
		}
	}
	if localDir := os.Getenv("SUPERVISOR_LOCAL_DIR"); localDir != "" {
		c.LocalDir = localDir
	}
	if sdDir := os.Getenv("SUPERVISOR_SD_DIR"); sdDir != "" {
		c.SDDir = sdDir
	}
}

// generateSecureKey generates a cryptographically secure random key.
func generateSecureKey(length int) []byte {
	key := make([]byte, length)
	if _, err := rand.Read(key); err != nil {
		// Fallback: this should never happen
		panic("failed to generate secure key: " + err.Error())
	}
	return key
}

// GenerateSecureToken generates a cryptographically secure random token.
func GenerateSecureToken(length int) (string, error) {
	bytes := make([]byte, length)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return hex.EncodeToString(bytes), nil
}

// SetJWTSecret sets the JWT secret key (for testing or loading from secure storage).
func (c *Config) SetJWTSecret(secret []byte) {
	c.JWTSecret = secret
}
