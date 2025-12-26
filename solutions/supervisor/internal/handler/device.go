package handler

import (
	"encoding/json"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"supervisor/internal/api"
	"supervisor/internal/script"
	"supervisor/pkg/logger"
)

// DeviceHandler handles device management API requests.
type DeviceHandler struct {
	modelDir    string
	modelSuffix string
	deviceInfo  map[string]interface{}
}

// NewDeviceHandler creates a new DeviceHandler.
func NewDeviceHandler() *DeviceHandler {
	h := &DeviceHandler{
		modelDir:    "/usr/share/supervisor/models",
		modelSuffix: ".cvimodel",
	}

	// Load device info from script
	result, err := script.RunJSON("api_device")
	if err == nil {
		h.deviceInfo = result
		if model, ok := result["model"].(map[string]interface{}); ok {
			if preset, ok := model["preset"].(string); ok {
				h.modelDir = preset
			}
			if file, ok := model["file"].(string); ok {
				ext := filepath.Ext(file)
				if ext != "" {
					h.modelSuffix = ext
				}
			}
		}
	}

	return h
}

// QueryDeviceInfo returns device information.
func (h *DeviceHandler) QueryDeviceInfo(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("queryDeviceInfo")
	if err != nil {
		api.WriteError(w, -1, "Failed to query device info")
		return
	}
	api.WriteSuccess(w, result)
}

// GetDeviceInfo returns detailed device information.
func (h *DeviceHandler) GetDeviceInfo(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("getDeviceInfo")
	if err != nil {
		api.WriteError(w, -1, "Failed to get device info")
		return
	}
	api.WriteSuccess(w, result)
}

// GetDeviceList returns list of devices on the network.
func (h *DeviceHandler) GetDeviceList(w http.ResponseWriter, r *http.Request) {
	result, err := script.Run("getDeviceList")
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"deviceList": []interface{}{}})
		return
	}

	var deviceList []interface{}
	if err := json.Unmarshal([]byte(result), &deviceList); err != nil {
		deviceList = []interface{}{}
	}

	api.WriteSuccess(w, map[string]interface{}{"deviceList": deviceList})
}

// UpdateDeviceNameRequest represents a device name update request.
type UpdateDeviceNameRequest struct {
	DeviceName string `json:"deviceName"`
}

// UpdateDeviceName updates the device name.
func (h *DeviceHandler) UpdateDeviceName(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req UpdateDeviceNameRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.DeviceName == "" {
		api.WriteError(w, -1, "Device name required")
		return
	}

	_, err := script.Run("updateDeviceName", req.DeviceName)
	if err != nil {
		api.WriteError(w, -1, "Failed to update device name")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"deviceName": req.DeviceName})
}

// GetCameraWebsocketUrl returns the camera WebSocket URL.
func (h *DeviceHandler) GetCameraWebsocketUrl(w http.ResponseWriter, r *http.Request) {
	host := r.Host
	if idx := strings.Index(host, ":"); idx != -1 {
		host = host[:idx]
	}

	// Get port from query or use default
	port := r.URL.Query().Get("port")
	if port == "" {
		port = "8765"
	}

	wsURL := "ws://" + host + ":" + port
	api.WriteSuccess(w, map[string]interface{}{"websocketUrl": wsURL})
}

// QueryServiceStatus returns the status of services.
func (h *DeviceHandler) QueryServiceStatus(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("queryServiceStatus")
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{})
		return
	}
	api.WriteSuccess(w, result)
}

// GetSystemStatus returns system status information.
func (h *DeviceHandler) GetSystemStatus(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("getSystemStatus")
	if err != nil {
		api.WriteError(w, -1, "Failed to get system status")
		return
	}
	api.WriteSuccess(w, result)
}

// SetPowerRequest represents a power mode request.
type SetPowerRequest struct {
	Mode int `json:"mode"` // 0: shutdown, 1: reboot, 2: suspend
}

// SetPower sets the power mode.
func (h *DeviceHandler) SetPower(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SetPowerRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	var cmd string
	switch req.Mode {
	case 0:
		cmd = "poweroff"
	case 1:
		cmd = "reboot"
	case 2:
		cmd = "suspend"
	default:
		api.WriteError(w, -1, "Invalid power mode")
		return
	}

	// Send response before executing power command
	api.WriteSuccess(w, map[string]interface{}{"mode": req.Mode})

	// Execute power command in background
	go func() {
		time.Sleep(1 * time.Second)
		exec.Command(cmd).Run()
	}()
}

// GetModelList returns the list of available models.
func (h *DeviceHandler) GetModelList(w http.ResponseWriter, r *http.Request) {
	models := []map[string]interface{}{}

	files, err := os.ReadDir(h.modelDir)
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"models": models})
		return
	}

	for _, file := range files {
		if file.IsDir() {
			continue
		}
		name := file.Name()
		if !strings.HasSuffix(name, h.modelSuffix) {
			continue
		}

		info, err := file.Info()
		if err != nil {
			continue
		}

		models = append(models, map[string]interface{}{
			"name":     name,
			"path":     filepath.Join(h.modelDir, name),
			"size":     info.Size(),
			"modified": info.ModTime().Unix(),
		})
	}

	api.WriteSuccess(w, map[string]interface{}{"models": models})
}

// GetModelInfo returns information about the current model.
func (h *DeviceHandler) GetModelInfo(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("getModelInfo")
	if err != nil {
		api.WriteError(w, -1, "Failed to get model info")
		return
	}
	api.WriteSuccess(w, result)
}

// GetModelFile serves a model file for download.
func (h *DeviceHandler) GetModelFile(w http.ResponseWriter, r *http.Request) {
	name := r.URL.Query().Get("name")
	if name == "" {
		api.WriteError(w, -1, "Model name required")
		return
	}

	// Sanitize path to prevent directory traversal
	name = filepath.Base(name)
	filePath := filepath.Join(h.modelDir, name)

	// Verify file exists and is under model directory
	absPath, err := filepath.Abs(filePath)
	if err != nil || !strings.HasPrefix(absPath, h.modelDir) {
		api.WriteError(w, -1, "Invalid model path")
		return
	}

	http.ServeFile(w, r, filePath)
}

// UploadModel handles model file uploads.
func (h *DeviceHandler) UploadModel(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	// Parse multipart form
	if err := r.ParseMultipartForm(100 << 20); err != nil { // 100MB max
		api.WriteError(w, -1, "Failed to parse form")
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		api.WriteError(w, -1, "File required")
		return
	}
	defer file.Close()

	// Validate filename
	filename := filepath.Base(header.Filename)
	if !strings.HasSuffix(filename, h.modelSuffix) {
		api.WriteError(w, -1, "Invalid model file type")
		return
	}

	// Create destination file
	dstPath := filepath.Join(h.modelDir, filename)
	dst, err := os.Create(dstPath)
	if err != nil {
		logger.Error("Failed to create model file: %v", err)
		api.WriteError(w, -1, "Failed to save model")
		return
	}
	defer dst.Close()

	// Copy file
	if _, err := io.Copy(dst, file); err != nil {
		logger.Error("Failed to copy model file: %v", err)
		os.Remove(dstPath)
		api.WriteError(w, -1, "Failed to save model")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{
		"name": filename,
		"path": dstPath,
		"size": header.Size,
	})
}

// Timestamp APIs

// SetTimestampRequest represents a timestamp set request.
type SetTimestampRequest struct {
	Timestamp int64 `json:"timestamp"`
}

// SetTimestamp sets the system timestamp.
func (h *DeviceHandler) SetTimestamp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SetTimestampRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	// Set system time using date command
	t := time.Unix(req.Timestamp, 0)
	dateStr := t.Format("2006-01-02 15:04:05")
	if err := exec.Command("date", "-s", dateStr).Run(); err != nil {
		logger.Error("Failed to set timestamp: %v", err)
		api.WriteError(w, -1, "Failed to set timestamp")
		return
	}

	// Sync to hardware clock
	exec.Command("hwclock", "-w").Run()

	api.WriteSuccess(w, map[string]interface{}{"timestamp": req.Timestamp})
}

// GetTimestamp returns the current system timestamp.
func (h *DeviceHandler) GetTimestamp(w http.ResponseWriter, r *http.Request) {
	api.WriteSuccess(w, map[string]interface{}{"timestamp": time.Now().Unix()})
}

// SetTimezoneRequest represents a timezone set request.
type SetTimezoneRequest struct {
	Timezone string `json:"timezone"`
}

// SetTimezone sets the system timezone.
func (h *DeviceHandler) SetTimezone(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SetTimezoneRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.Timezone == "" {
		api.WriteError(w, -1, "Timezone required")
		return
	}

	// Verify timezone exists
	tzFile := "/usr/share/zoneinfo/" + req.Timezone
	if _, err := os.Stat(tzFile); err != nil {
		api.WriteError(w, -1, "Invalid timezone")
		return
	}

	// Create symlink
	localtime := "/etc/localtime"
	os.Remove(localtime)
	if err := os.Symlink(tzFile, localtime); err != nil {
		logger.Error("Failed to set timezone: %v", err)
		api.WriteError(w, -1, "Failed to set timezone")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"timezone": req.Timezone})
}

// GetTimezone returns the current timezone.
func (h *DeviceHandler) GetTimezone(w http.ResponseWriter, r *http.Request) {
	link, err := os.Readlink("/etc/localtime")
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"timezone": "UTC"})
		return
	}

	tz := strings.TrimPrefix(link, "/usr/share/zoneinfo/")
	api.WriteSuccess(w, map[string]interface{}{"timezone": tz})
}

// GetTimezoneList returns a list of available timezones.
func (h *DeviceHandler) GetTimezoneList(w http.ResponseWriter, r *http.Request) {
	timezones := []string{}

	// Read common timezones
	commonTZ := []string{
		"UTC", "America/New_York", "America/Los_Angeles", "America/Chicago",
		"Europe/London", "Europe/Paris", "Europe/Berlin",
		"Asia/Tokyo", "Asia/Shanghai", "Asia/Singapore",
		"Australia/Sydney", "Pacific/Auckland",
	}

	for _, tz := range commonTZ {
		tzFile := "/usr/share/zoneinfo/" + tz
		if _, err := os.Stat(tzFile); err == nil {
			timezones = append(timezones, tz)
		}
	}

	api.WriteSuccess(w, map[string]interface{}{"timezones": timezones})
}

// System Update APIs

// UpdateChannel sets the update channel.
func (h *DeviceHandler) UpdateChannel(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req struct {
		Channel string `json:"channel"`
	}
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	_, err := script.Run("updateChannel", req.Channel)
	if err != nil {
		api.WriteError(w, -1, "Failed to update channel")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"channel": req.Channel})
}

// GetSystemUpdateVersion returns available update version.
func (h *DeviceHandler) GetSystemUpdateVersion(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("getSystemUpdateVersion")
	if err != nil {
		api.WriteError(w, -1, "Failed to get update version")
		return
	}
	api.WriteSuccess(w, result)
}

// UpdateSystem initiates a system update.
func (h *DeviceHandler) UpdateSystem(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	// Start update in background
	go func() {
		script.Run("updateSystem")
	}()

	api.WriteSuccess(w, map[string]interface{}{"status": "updating"})
}

// GetUpdateProgress returns the update progress.
func (h *DeviceHandler) GetUpdateProgress(w http.ResponseWriter, r *http.Request) {
	result, err := script.RunJSON("getUpdateProgress")
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"progress": 0, "status": "idle"})
		return
	}
	api.WriteSuccess(w, result)
}

// CancelUpdate cancels an ongoing update.
func (h *DeviceHandler) CancelUpdate(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	_, err := script.Run("cancelUpdate")
	if err != nil {
		api.WriteError(w, -1, "Failed to cancel update")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"status": "cancelled"})
}

// Platform Info APIs

// GetPlatformInfo returns platform configuration.
func (h *DeviceHandler) GetPlatformInfo(w http.ResponseWriter, r *http.Request) {
	result, err := script.Run("getPlatformInfo")
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"platform_info": ""})
		return
	}
	api.WriteSuccess(w, map[string]interface{}{"platform_info": result})
}

// SavePlatformInfo saves platform configuration.
func (h *DeviceHandler) SavePlatformInfo(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req struct {
		PlatformInfo string `json:"platform_info"`
	}
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	_, err := script.Run("savePlatformInfo", req.PlatformInfo)
	if err != nil {
		api.WriteError(w, -1, "Failed to save platform info")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"message": "Platform info saved"})
}

// Uptime returns the system uptime in milliseconds.
func Uptime() int64 {
	data, err := os.ReadFile("/proc/uptime")
	if err != nil {
		return 0
	}
	fields := strings.Fields(string(data))
	if len(fields) < 1 {
		return 0
	}
	seconds, err := strconv.ParseFloat(fields[0], 64)
	if err != nil {
		return 0
	}
	return int64(seconds * 1000)
}
