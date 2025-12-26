package handler

import (
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"supervisor/internal/api"
	"supervisor/pkg/logger"
)

// LEDHandler handles LED control API requests.
type LEDHandler struct {
	ledBasePath string
}

// LEDInfo represents LED information.
type LEDInfo struct {
	Name          string `json:"name"`
	Brightness    int    `json:"brightness"`
	MaxBrightness int    `json:"max_brightness"`
	Trigger       string `json:"trigger"`
}

// NewLEDHandler creates a new LEDHandler.
func NewLEDHandler() *LEDHandler {
	return &LEDHandler{
		ledBasePath: "/sys/class/leds",
	}
}

// GetLEDs returns information about all LEDs.
func (h *LEDHandler) GetLEDs(w http.ResponseWriter, r *http.Request) {
	leds := []LEDInfo{}

	entries, err := os.ReadDir(h.ledBasePath)
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"leds": leds})
		return
	}

	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}

		name := entry.Name()
		ledPath := filepath.Join(h.ledBasePath, name)

		info := LEDInfo{Name: name}

		// Read brightness
		if data, err := os.ReadFile(filepath.Join(ledPath, "brightness")); err == nil {
			val, _ := strconv.Atoi(strings.TrimSpace(string(data)))
			info.Brightness = val
		}

		// Read max brightness
		if data, err := os.ReadFile(filepath.Join(ledPath, "max_brightness")); err == nil {
			val, _ := strconv.Atoi(strings.TrimSpace(string(data)))
			info.MaxBrightness = val
		}

		// Read trigger
		if data, err := os.ReadFile(filepath.Join(ledPath, "trigger")); err == nil {
			// Parse trigger - the current trigger is in brackets
			triggerStr := strings.TrimSpace(string(data))
			for _, t := range strings.Fields(triggerStr) {
				if strings.HasPrefix(t, "[") && strings.HasSuffix(t, "]") {
					info.Trigger = strings.Trim(t, "[]")
					break
				}
			}
		}

		leds = append(leds, info)
	}

	api.WriteSuccess(w, map[string]interface{}{"leds": leds})
}

// GetLED returns information about a specific LED.
func (h *LEDHandler) GetLED(w http.ResponseWriter, r *http.Request) {
	name := r.URL.Query().Get("name")
	if name == "" {
		api.WriteError(w, -1, "LED name required")
		return
	}

	// Sanitize name to prevent path traversal
	name = filepath.Base(name)
	ledPath := filepath.Join(h.ledBasePath, name)

	if _, err := os.Stat(ledPath); err != nil {
		api.WriteError(w, -1, "LED not found")
		return
	}

	info := LEDInfo{Name: name}

	if data, err := os.ReadFile(filepath.Join(ledPath, "brightness")); err == nil {
		val, _ := strconv.Atoi(strings.TrimSpace(string(data)))
		info.Brightness = val
	}

	if data, err := os.ReadFile(filepath.Join(ledPath, "max_brightness")); err == nil {
		val, _ := strconv.Atoi(strings.TrimSpace(string(data)))
		info.MaxBrightness = val
	}

	if data, err := os.ReadFile(filepath.Join(ledPath, "trigger")); err == nil {
		triggerStr := strings.TrimSpace(string(data))
		for _, t := range strings.Fields(triggerStr) {
			if strings.HasPrefix(t, "[") && strings.HasSuffix(t, "]") {
				info.Trigger = strings.Trim(t, "[]")
				break
			}
		}
	}

	api.WriteSuccess(w, info)
}

// SetLEDRequest represents an LED set request.
type SetLEDRequest struct {
	Name       string `json:"name"`
	Brightness *int   `json:"brightness,omitempty"`
	Trigger    string `json:"trigger,omitempty"`
}

// SetLED sets LED brightness or trigger.
func (h *LEDHandler) SetLED(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SetLEDRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.Name == "" {
		api.WriteError(w, -1, "LED name required")
		return
	}

	// Sanitize name
	name := filepath.Base(req.Name)
	ledPath := filepath.Join(h.ledBasePath, name)

	if _, err := os.Stat(ledPath); err != nil {
		api.WriteError(w, -1, "LED not found")
		return
	}

	// Set trigger if specified
	if req.Trigger != "" {
		triggerPath := filepath.Join(ledPath, "trigger")
		if err := os.WriteFile(triggerPath, []byte(req.Trigger), 0644); err != nil {
			logger.Error("Failed to set LED trigger: %v", err)
			api.WriteError(w, -1, "Failed to set trigger")
			return
		}
	}

	// Set brightness if specified
	if req.Brightness != nil {
		brightnessPath := filepath.Join(ledPath, "brightness")
		if err := os.WriteFile(brightnessPath, []byte(strconv.Itoa(*req.Brightness)), 0644); err != nil {
			logger.Error("Failed to set LED brightness: %v", err)
			api.WriteError(w, -1, "Failed to set brightness")
			return
		}
	}

	api.WriteSuccess(w, map[string]interface{}{"name": name, "status": "updated"})
}

// GetLEDTriggers returns available LED triggers.
func (h *LEDHandler) GetLEDTriggers(w http.ResponseWriter, r *http.Request) {
	name := r.URL.Query().Get("name")
	if name == "" {
		api.WriteError(w, -1, "LED name required")
		return
	}

	name = filepath.Base(name)
	triggerPath := filepath.Join(h.ledBasePath, name, "trigger")

	data, err := os.ReadFile(triggerPath)
	if err != nil {
		api.WriteError(w, -1, "Failed to read triggers")
		return
	}

	triggers := []string{}
	current := ""
	for _, t := range strings.Fields(string(data)) {
		if strings.HasPrefix(t, "[") && strings.HasSuffix(t, "]") {
			current = strings.Trim(t, "[]")
			triggers = append(triggers, current)
		} else {
			triggers = append(triggers, t)
		}
	}

	api.WriteSuccess(w, map[string]interface{}{
		"triggers": triggers,
		"current":  current,
	})
}
