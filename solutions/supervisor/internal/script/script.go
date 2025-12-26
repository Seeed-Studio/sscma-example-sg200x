// Package script provides secure shell script execution.
package script

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"strings"
	"time"

	"supervisor/internal/config"
	"supervisor/pkg/logger"
)

// DefaultTimeout is the default script execution timeout.
const DefaultTimeout = 30 * time.Second

// Runner executes shell scripts.
type Runner struct {
	scriptPath string
	timeout    time.Duration
}

// NewRunner creates a new script runner.
func NewRunner() *Runner {
	cfg := config.Get()
	return &Runner{
		scriptPath: cfg.ScriptPath,
		timeout:    DefaultTimeout,
	}
}

// SetTimeout sets the script execution timeout.
func (r *Runner) SetTimeout(timeout time.Duration) {
	r.timeout = timeout
}

// Run executes a script command with the given arguments.
func (r *Runner) Run(command string, args ...string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), r.timeout)
	defer cancel()

	return r.RunContext(ctx, command, args...)
}

// RunContext executes a script command with a context.
func (r *Runner) RunContext(ctx context.Context, command string, args ...string) (string, error) {
	// Build the full command
	cmdArgs := make([]string, 0, len(args)+2)
	cmdArgs = append(cmdArgs, r.scriptPath, command)

	// Safely quote arguments to prevent injection
	for _, arg := range args {
		cmdArgs = append(cmdArgs, shellescape(arg))
	}

	logger.Debug("Executing script: %s %s", r.scriptPath, strings.Join(cmdArgs[1:], " "))

	cmd := exec.CommandContext(ctx, "/bin/sh", "-c", strings.Join(cmdArgs, " "))

	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	if err := cmd.Run(); err != nil {
		if ctx.Err() == context.DeadlineExceeded {
			return "", fmt.Errorf("script timed out after %v", r.timeout)
		}
		logger.Error("Script execution failed: %v, stderr: %s", err, stderr.String())
		return "", fmt.Errorf("script failed: %v", err)
	}

	result := strings.TrimRight(stdout.String(), "\r\n")
	logger.Debug("Script result: %s", result)
	return result, nil
}

// RunJSON executes a script and parses JSON output.
func (r *Runner) RunJSON(command string, args ...string) (map[string]interface{}, error) {
	output, err := r.Run(command, args...)
	if err != nil {
		return nil, err
	}

	var result map[string]interface{}
	if err := json.Unmarshal([]byte(output), &result); err != nil {
		logger.Error("Failed to parse JSON output: %v", err)
		return nil, fmt.Errorf("invalid JSON output: %v", err)
	}

	return result, nil
}

// shellescape escapes a string for safe use in shell commands.
// This prevents command injection attacks.
func shellescape(s string) string {
	// If string is empty, return quoted empty string
	if s == "" {
		return "''"
	}

	// Check if escaping is needed
	needsEscape := false
	for _, c := range s {
		if !isShellSafe(c) {
			needsEscape = true
			break
		}
	}

	if !needsEscape {
		return s
	}

	// Use single quotes and escape any single quotes within
	var builder strings.Builder
	builder.WriteByte('\'')
	for _, c := range s {
		if c == '\'' {
			builder.WriteString("'\"'\"'") // End single quote, add escaped quote, start single quote
		} else {
			builder.WriteRune(c)
		}
	}
	builder.WriteByte('\'')
	return builder.String()
}

// isShellSafe returns true if the character is safe in shell without quoting.
func isShellSafe(c rune) bool {
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= '0' && c <= '9') ||
		c == '-' || c == '_' || c == '.' || c == '/' || c == ':'
}

// Default runner instance
var defaultRunner *Runner

// GetRunner returns the default script runner.
func GetRunner() *Runner {
	if defaultRunner == nil {
		defaultRunner = NewRunner()
	}
	return defaultRunner
}

// Run executes a script using the default runner.
func Run(command string, args ...string) (string, error) {
	return GetRunner().Run(command, args...)
}

// RunJSON executes a script and parses JSON using the default runner.
func RunJSON(command string, args ...string) (map[string]interface{}, error) {
	return GetRunner().RunJSON(command, args...)
}
