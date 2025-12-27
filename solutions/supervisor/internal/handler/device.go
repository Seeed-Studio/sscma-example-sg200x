package handler

import (
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"supervisor/internal/api"
	"supervisor/internal/device"
	"supervisor/internal/system"
	"supervisor/internal/upgrade"
	"supervisor/pkg/logger"
)

// DeviceHandler handles device management API requests.
type DeviceHandler struct {
	modelDir    string
	modelSuffix string
	deviceInfo  *device.APIDeviceInfo
	upgradeMgr  *upgrade.UpgradeManager
}

// NewDeviceHandler creates a new DeviceHandler.
func NewDeviceHandler() *DeviceHandler {
	h := &DeviceHandler{
		modelDir:    "/usr/share/supervisor/models",
		modelSuffix: ".cvimodel",
		upgradeMgr:  upgrade.NewUpgradeManager(),
	}

	// Load device info
	h.deviceInfo = device.GetAPIDevice()
	if h.deviceInfo != nil {
		if h.deviceInfo.Model.Preset != "" {
			h.modelDir = h.deviceInfo.Model.Preset
		}
		if h.deviceInfo.Model.File != "" {
			ext := filepath.Ext(h.deviceInfo.Model.File)
			if ext != "" {
				h.modelSuffix = ext
			}
		}
	}

	return h
}

// QueryDeviceInfo returns device information.
func (h *DeviceHandler) QueryDeviceInfo(w http.ResponseWriter, r *http.Request) {
	info := device.QueryDeviceInfo()
	api.WriteSuccess(w, info)
}

// GetDeviceInfo returns detailed device information.
func (h *DeviceHandler) GetDeviceInfo(w http.ResponseWriter, r *http.Request) {
	info := device.QueryDeviceInfo()
	api.WriteSuccess(w, info)
}

// GetDeviceList returns list of devices on the network.
func (h *DeviceHandler) GetDeviceList(w http.ResponseWriter, r *http.Request) {
	devices, err := device.GetDeviceList()
	if err != nil {
		api.WriteSuccess(w, map[string]interface{}{"deviceList": []interface{}{}})
		return
	}
	api.WriteSuccess(w, map[string]interface{}{"deviceList": devices})
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

	if err := device.UpdateDeviceName(req.DeviceName); err != nil {
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
	// Since sscma-node has been removed, return success status directly
	// The frontend expects sscmaNode=0 and system=0 for "RUNNING" status
	api.WriteSuccess(w, map[string]interface{}{
		"sscmaNode": 0,
		"system":    0,
		"uptime":    system.GetUptime(),
	})
}

// GetSystemStatus returns system status information.
func (h *DeviceHandler) GetSystemStatus(w http.ResponseWriter, r *http.Request) {
	// Return system status using native Go
	api.WriteSuccess(w, map[string]interface{}{
		"uptime":     system.GetUptime(),
		"deviceName": system.GetDeviceName(),
		"osName":     system.GetOSName(),
		"osVersion":  system.GetOSVersion(),
	})
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
	// Read model info from file
	infoFile := device.ModelDir + "/model.json"
	data, err := os.ReadFile(infoFile)
	if err != nil {
		api.WriteError(w, -1, "Failed to get model info")
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write(data)
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
		Channel int    `json:"channel"`
		URL     string `json:"url"`
	}
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if err := h.upgradeMgr.UpdateChannel(req.Channel, req.URL); err != nil {
		api.WriteError(w, -1, "Failed to update channel")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"channel": req.Channel})
}

// GetSystemUpdateVersion returns available update version.
func (h *DeviceHandler) GetSystemUpdateVersion(w http.ResponseWriter, r *http.Request) {
	result, err := h.upgradeMgr.GetSystemUpdateVersion()
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
		h.upgradeMgr.UpdateSystem()
	}()

	api.WriteSuccess(w, map[string]interface{}{"status": "updating"})
}

// GetUpdateProgress returns the update progress.
func (h *DeviceHandler) GetUpdateProgress(w http.ResponseWriter, r *http.Request) {
	result, err := h.upgradeMgr.GetUpdateProgress()
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

	if err := h.upgradeMgr.CancelUpdate(); err != nil {
		api.WriteError(w, -1, "Failed to cancel update")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"status": "cancelled"})
}

// Platform Info APIs

// GetPlatformInfo returns platform configuration.
func (h *DeviceHandler) GetPlatformInfo(w http.ResponseWriter, r *http.Request) {
	info := device.GetPlatformInfo()
	api.WriteSuccess(w, map[string]interface{}{"platform_info": info})
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

	if err := device.SavePlatformInfo(req.PlatformInfo); err != nil {
		api.WriteError(w, -1, "Failed to save platform info")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{"message": "Platform info saved"})
}

// FactoryReset sets the factory reset flag for the next reboot.
// This will reset the device to factory defaults on next restart.
func (h *DeviceHandler) FactoryReset(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	// Set factory reset flag using fw_setenv
	if err := h.upgradeMgr.Recovery(); err != nil {
		logger.Error("Failed to set factory reset flag: %v", err)
		api.WriteError(w, -1, "Failed to initiate factory reset")
		return
	}

	api.WriteSuccess(w, map[string]interface{}{
		"status":  "scheduled",
		"message": "Factory reset scheduled. Please reboot the device to apply.",
	})
}
