// Package handler provides HTTP request handlers for the supervisor API.
package handler

import (
	"net/http"
	"os"
	"os/exec"
	"strconv"
	"strings"

	"supervisor/internal/api"
	"supervisor/internal/auth"
	"supervisor/internal/script"
	"supervisor/pkg/logger"
)

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

	// Get additional user info from script
	result, err := script.Run("get_username")
	if err == nil && result != "" {
		username = result
	}

	// Check SSH status
	sshEnabled := isSSHEnabled()

	// Get SSH keys
	sshKeys := getSSHKeys(username)

	api.WriteSuccess(w, map[string]interface{}{
		"userName":   username,
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
func (h *UserHandler) UpdatePassword(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
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

	username := auth.GetUsername()

	// Verify old password first
	_, err := h.authManager.Authenticate(username, req.OldPassword)
	if err != nil {
		api.WriteError(w, -1, "Current password is incorrect")
		return
	}

	// Change password using chpasswd
	cmd := exec.Command("chpasswd")
	cmd.Stdin = strings.NewReader(username + ":" + req.NewPassword)
	if err := cmd.Run(); err != nil {
		logger.Error("Failed to change password: %v", err)
		api.WriteError(w, -1, "Failed to update password")
		return
	}

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
