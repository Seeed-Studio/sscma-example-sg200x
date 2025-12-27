// Package handler provides HTTP request handlers for the supervisor API.
package handler

import (
	"bufio"
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"supervisor/internal/api"
	"supervisor/internal/auth"
	"supervisor/pkg/logger"

	"github.com/GehirnInc/crypt"
)

// Default username for the system
const DefaultUsername = "recamera"

// UserHandler handles user management API requests.
type UserHandler struct {
	authManager *auth.AuthManager
}

// NewUserHandler creates a new UserHandler.
func NewUserHandler(am *auth.AuthManager) *UserHandler {
	return &UserHandler{
		authManager: am,
	}
}

// LoginRequest represents a login request.
type LoginRequest struct {
	Username string `json:"userName"`
	Password string `json:"password"`
}

// LoginResponse represents a login response.
type LoginResponse struct {
	Token      string `json:"token"`
	RetryCount int    `json:"retryCount,omitempty"`
}

// Login handles user login.
func (h *UserHandler) Login(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req LoginRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.Username == "" || req.Password == "" {
		api.WriteError(w, -1, "Username and password required")
		return
	}

	token, err := h.authManager.Authenticate(req.Username, req.Password)
	if err != nil {
		logger.Warn("Login failed for user %s: %v", req.Username, err)
		api.WriteError(w, -1, "Authentication failed")
		return
	}

	logger.Info("User %s logged in successfully", req.Username)
	api.WriteSuccess(w, LoginResponse{Token: token})
}

// QueryUserInfo returns user information.
func (h *UserHandler) QueryUserInfo(w http.ResponseWriter, r *http.Request) {
	username := auth.GetUsername()

	// Use default username if not set
	if username == "" {
		username = DefaultUsername
	}

	// Check SSH status
	sshEnabled := isSSHEnabled()

	// Get SSH keys
	sshKeys := getSSHKeys(username)

	// Check if this is first login (password is not set or default)
	firstLogin := isFirstLogin(username)

	api.WriteSuccess(w, map[string]interface{}{
		"userName":   username,
		"firstLogin": firstLogin,
		"sshEnabled": sshEnabled,
		"sshKeys":    sshKeys,
	})
}

// UpdatePasswordRequest represents a password update request.
type UpdatePasswordRequest struct {
	OldPassword string `json:"oldPassword"`
	NewPassword string `json:"newPassword"`
}

// UpdatePassword handles password updates.
// This endpoint allows unauthenticated access ONLY during first login.
// After first login, authentication is required.
func (h *UserHandler) UpdatePassword(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	username := auth.GetUsername()

	// Check if this is first login - only allow unauthenticated access for first login
	if !isFirstLogin(username) {
		// Not first login - require authentication
		authHeader := r.Header.Get("Authorization")
		if authHeader == "" {
			w.WriteHeader(http.StatusUnauthorized)
			api.WriteError(w, -1, "Authentication required")
			return
		}
		// Validate the token
		_, err := h.authManager.ValidateToken(authHeader)
		if err != nil {
			w.WriteHeader(http.StatusUnauthorized)
			api.WriteError(w, -1, "Invalid or expired token")
			return
		}
	}

	var req UpdatePasswordRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.OldPassword == "" || req.NewPassword == "" {
		api.WriteError(w, -1, "Old and new passwords required")
		return
	}

	// Validate password strength
	if len(req.NewPassword) < 8 {
		api.WriteError(w, -1, "Password must be at least 8 characters")
		return
	}

	// Verify old password first
	_, err := h.authManager.Authenticate(username, req.OldPassword)
	if err != nil {
		api.WriteError(w, -1, "Current password is incorrect")
		return
	}

	// Change password using passwd command (same as original shell script)
	// The passwd command expects the new password twice via stdin
	cmd := exec.Command("passwd", username)
	cmd.Stdin = strings.NewReader(req.NewPassword + "\n" + req.NewPassword + "\n")
	output, err := cmd.CombinedOutput()
	if err != nil {
		logger.Error("Failed to change password: %v, output: %s", err, string(output))
		api.WriteError(w, -1, "Failed to update password")
		return
	}

	// Clear the first login flag after successful password change
	clearFirstLoginFlag()

	logger.Info("Password updated for user %s", username)
	api.WriteSuccess(w, map[string]interface{}{"message": "Password updated successfully"})
}

// SetSSHStatusRequest represents an SSH status update request.
type SetSSHStatusRequest struct {
	Enable bool `json:"enable"`
}

// SetSSHStatus enables or disables SSH.
func (h *UserHandler) SetSSHStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SetSSHStatusRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	var err error
	if req.Enable {
		err = exec.Command("systemctl", "start", "dropbear").Run()
		if err == nil {
			err = exec.Command("systemctl", "enable", "dropbear").Run()
		}
	} else {
		err = exec.Command("systemctl", "stop", "dropbear").Run()
		if err == nil {
			err = exec.Command("systemctl", "disable", "dropbear").Run()
		}
	}

	if err != nil {
		logger.Error("Failed to change SSH status: %v", err)
		api.WriteError(w, -1, "Failed to update SSH status")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"enabled": req.Enable})
}

// AddSSHKeyRequest represents an SSH key addition request.
type AddSSHKeyRequest struct {
	Key  string `json:"key"`
	Name string `json:"name"`
}

// AddSSHKey adds an SSH public key.
func (h *UserHandler) AddSSHKey(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req AddSSHKeyRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.Key == "" {
		api.WriteError(w, -1, "SSH key required")
		return
	}

	// Validate SSH key format
	if !isValidSSHKey(req.Key) {
		api.WriteError(w, -1, "Invalid SSH key format")
		return
	}

	username := auth.GetUsername()
	sshDir := "/home/" + username + "/.ssh"
	authKeysFile := sshDir + "/authorized_keys"

	// Ensure .ssh directory exists
	if err := os.MkdirAll(sshDir, 0700); err != nil {
		logger.Error("Failed to create .ssh directory: %v", err)
		api.WriteError(w, -1, "Failed to add SSH key")
		return
	}

	// Append key to authorized_keys
	f, err := os.OpenFile(authKeysFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0600)
	if err != nil {
		logger.Error("Failed to open authorized_keys: %v", err)
		api.WriteError(w, -1, "Failed to add SSH key")
		return
	}
	defer f.Close()

	keyLine := req.Key
	if req.Name != "" {
		keyLine = keyLine + " " + req.Name
	}
	if _, err := f.WriteString(keyLine + "\n"); err != nil {
		logger.Error("Failed to write SSH key: %v", err)
		api.WriteError(w, -1, "Failed to add SSH key")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"message": "SSH key added successfully"})
}

// DeleteSSHKeyRequest represents an SSH key deletion request.
type DeleteSSHKeyRequest struct {
	ID string `json:"id"`
}

// DeleteSSHKey removes an SSH public key.
func (h *UserHandler) DeleteSSHKey(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req DeleteSSHKeyRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.ID == "" {
		api.WriteError(w, -1, "Key ID required")
		return
	}

	username := auth.GetUsername()
	authKeysFile := "/home/" + username + "/.ssh/authorized_keys"

	// Read current keys
	data, err := os.ReadFile(authKeysFile)
	if err != nil {
		api.WriteError(w, -1, "No SSH keys found")
		return
	}

	lines := strings.Split(string(data), "\n")
	var newLines []string
	found := false
	for i, line := range lines {
		if strings.TrimSpace(line) == "" {
			continue
		}
		// ID is the line number (1-indexed)
		if strconv.Itoa(i+1) != req.ID {
			newLines = append(newLines, line)
		} else {
			found = true
		}
	}

	if !found {
		api.WriteError(w, -1, "SSH key not found")
		return
	}

	// Write back
	if err := os.WriteFile(authKeysFile, []byte(strings.Join(newLines, "\n")+"\n"), 0600); err != nil {
		logger.Error("Failed to write authorized_keys: %v", err)
		api.WriteError(w, -1, "Failed to delete SSH key")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"message": "SSH key deleted successfully"})
}

// Helper functions

func isSSHEnabled() bool {
	cmd := exec.Command("systemctl", "is-active", "dropbear")
	if err := cmd.Run(); err == nil {
		return true
	}
	return false
}

func getSSHKeys(username string) []map[string]interface{} {
	authKeysFile := "/home/" + username + "/.ssh/authorized_keys"
	data, err := os.ReadFile(authKeysFile)
	if err != nil {
		return []map[string]interface{}{}
	}

	var keys []map[string]interface{}
	lines := strings.Split(string(data), "\n")
	for i, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.Fields(line)
		key := map[string]interface{}{
			"id":   strconv.Itoa(i + 1),
			"type": "",
			"key":  "",
			"name": "",
		}
		if len(parts) >= 1 {
			key["type"] = parts[0]
		}
		if len(parts) >= 2 {
			// Truncate key for display
			if len(parts[1]) > 20 {
				key["key"] = parts[1][:10] + "..." + parts[1][len(parts[1])-10:]
			} else {
				key["key"] = parts[1]
			}
		}
		if len(parts) >= 3 {
			key["name"] = strings.Join(parts[2:], " ")
		}
		keys = append(keys, key)
	}
	return keys
}

func isValidSSHKey(key string) bool {
	parts := strings.Fields(key)
	if len(parts) < 2 {
		return false
	}
	validTypes := []string{"ssh-rsa", "ssh-ed25519", "ssh-dss", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384", "ecdsa-sha2-nistp521"}
	for _, t := range validTypes {
		if parts[0] == t {
			return true
		}
	}
	return false
}

// isFirstLogin checks if the user needs to set a password.
// Returns true if the password is default or the first login flag is set.
func isFirstLogin(username string) bool {
	// Check first login flag file
	firstLoginFile := "/etc/recamera.conf/first_login"
	if _, err := os.Stat(firstLoginFile); err == nil {
		// First login flag exists
		data, _ := os.ReadFile(firstLoginFile)
		if string(data) == "1" || strings.TrimSpace(string(data)) == "true" {
			return true
		}
	}

	// Check if user has a valid password in /etc/shadow
	// If password field is empty, !, *, or !!, then first login is true
	shadowFile, err := os.Open("/etc/shadow")
	if err != nil {
		// Can't read shadow, assume not first login
		return false
	}
	defer shadowFile.Close()

	scanner := bufio.NewScanner(shadowFile)
	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.Split(line, ":")
		if len(parts) >= 2 && parts[0] == username {
			passwd := parts[1]
			// Check if password is locked or empty
			if passwd == "" || passwd == "*" || passwd == "!" || passwd == "!!" || passwd == "!*" {
				return true
			}
			// Check for default password "recamera" - verify using crypt
			if verifyDefaultPassword(passwd) {
				return true
			}
			return false
		}
	}

	return false
}

// verifyDefaultPassword checks if the password hash matches the default password "recamera"
func verifyDefaultPassword(hashedPassword string) bool {
	// Use the GehirnInc/crypt library to verify if the hash matches "recamera"
	crypter := crypt.NewFromHash(hashedPassword)
	if crypter == nil {
		return false
	}
	err := crypter.Verify(hashedPassword, []byte("recamera"))
	return err == nil
}

// clearFirstLoginFlag removes the first login flag file
func clearFirstLoginFlag() {
	firstLoginFile := "/etc/recamera.conf/first_login"
	os.Remove(firstLoginFile)
}
