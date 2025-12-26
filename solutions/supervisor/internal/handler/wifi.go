package handler

import (
	"encoding/json"
	"net/http"
	"sync"
	"time"

	"supervisor/internal/api"
	"supervisor/internal/script"
	"supervisor/pkg/logger"
)

// WiFiHandler handles WiFi management API requests.
type WiFiHandler struct {
	mu          sync.RWMutex
	staEnable   int
	apEnable    int
	networkInfo map[string]interface{}
	scanning    bool
	stopChan    chan struct{}
}

// NewWiFiHandler creates a new WiFiHandler.
func NewWiFiHandler() *WiFiHandler {
	h := &WiFiHandler{
		staEnable:   1,
		apEnable:    1,
		networkInfo: make(map[string]interface{}),
		stopChan:    make(chan struct{}),
	}

	// Start background WiFi monitoring
	go h.monitorWiFi()

	return h
}

// Stop stops the WiFi monitoring goroutine.
func (h *WiFiHandler) Stop() {
	close(h.stopChan)
}

// monitorWiFi periodically scans for WiFi networks.
func (h *WiFiHandler) monitorWiFi() {
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	// Initial scan
	h.scanNetworks()

	for {
		select {
		case <-ticker.C:
			h.scanNetworks()
		case <-h.stopChan:
			return
		}
	}
}

// scanNetworks performs a WiFi scan.
func (h *WiFiHandler) scanNetworks() {
	h.mu.Lock()
	if h.scanning {
		h.mu.Unlock()
		return
	}
	h.scanning = true
	h.mu.Unlock()

	defer func() {
		h.mu.Lock()
		h.scanning = false
		h.mu.Unlock()
	}()

	// Get ethernet info
	ethInfo := h.getEthernet()

	// Get current WiFi info
	staInfo := h.getStaCurrent()

	// Get connected networks
	connectedInfo := h.getStaConnected(staInfo)

	// Get scan list
	scanList := h.getScanList(connectedInfo)

	h.mu.Lock()
	h.networkInfo = map[string]interface{}{
		"eth":       ethInfo,
		"sta":       staInfo,
		"connected": connectedInfo,
		"scanList":  scanList,
	}
	h.mu.Unlock()
}

// getEthernet gets ethernet interface info.
func (h *WiFiHandler) getEthernet() map[string]interface{} {
	result, err := script.RunJSON("get_eth")
	if err != nil {
		return map[string]interface{}{}
	}
	return result
}

// getStaCurrent gets current WiFi station info.
func (h *WiFiHandler) getStaCurrent() map[string]interface{} {
	result, err := script.RunJSON("get_sta_current")
	if err != nil {
		return map[string]interface{}{}
	}
	return result
}

// getStaConnected gets connected WiFi networks.
func (h *WiFiHandler) getStaConnected(current map[string]interface{}) []map[string]interface{} {
	result, err := script.Run("get_sta_connected")
	if err != nil {
		return []map[string]interface{}{}
	}

	var connected []map[string]interface{}
	if err := json.Unmarshal([]byte(result), &connected); err != nil {
		return []map[string]interface{}{}
	}
	return connected
}

// getScanList gets WiFi scan results.
func (h *WiFiHandler) getScanList(connected []map[string]interface{}) []map[string]interface{} {
	result, err := script.Run("get_scan_list")
	if err != nil {
		return []map[string]interface{}{}
	}

	var scanList []map[string]interface{}
	if err := json.Unmarshal([]byte(result), &scanList); err != nil {
		return []map[string]interface{}{}
	}
	return scanList
}

// GetWiFiInfoList returns WiFi information.
func (h *WiFiHandler) GetWiFiInfoList(w http.ResponseWriter, r *http.Request) {
	h.mu.RLock()
	info := h.networkInfo
	h.mu.RUnlock()

	// Trigger a new scan
	go h.scanNetworks()

	response := map[string]interface{}{
		"wifiEnable": h.staEnable,
		"apEnable":   h.apEnable,
	}

	if eth, ok := info["eth"].(map[string]interface{}); ok {
		response["eth"] = eth
	}
	if sta, ok := info["sta"].(map[string]interface{}); ok {
		response["sta"] = sta
	}
	if connected, ok := info["connected"].([]map[string]interface{}); ok {
		response["connectedList"] = connected
	}
	if scanList, ok := info["scanList"].([]map[string]interface{}); ok {
		response["scanList"] = scanList
	}

	api.WriteSuccess(w, response)
}

// ConnectWiFiRequest represents a WiFi connect request.
type ConnectWiFiRequest struct {
	SSID     string `json:"ssid"`
	Password string `json:"password"`
	Security string `json:"security"`
}

// ConnectWiFi connects to a WiFi network.
func (h *WiFiHandler) ConnectWiFi(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req ConnectWiFiRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.SSID == "" {
		api.WriteError(w, -1, "SSID required")
		return
	}

	// Connect to WiFi using script
	result, err := script.Run("connectWiFi", req.SSID, req.Password, req.Security)
	if err != nil {
		logger.Error("Failed to connect to WiFi: %v", err)
		api.WriteError(w, -1, "Failed to connect to WiFi")
		return
	}

	// Parse result
	var response map[string]interface{}
	if err := json.Unmarshal([]byte(result), &response); err != nil {
		response = map[string]interface{}{"status": "connecting"}
	}

	// Trigger scan after connection attempt
	go func() {
		time.Sleep(5 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, response)
}

// DisconnectWiFiRequest represents a WiFi disconnect request.
type DisconnectWiFiRequest struct {
	SSID string `json:"ssid"`
}

// DisconnectWiFi disconnects from a WiFi network.
func (h *WiFiHandler) DisconnectWiFi(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req DisconnectWiFiRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.SSID == "" {
		api.WriteError(w, -1, "SSID required")
		return
	}

	_, err := script.Run("disconnectWiFi", req.SSID)
	if err != nil {
		logger.Error("Failed to disconnect from WiFi: %v", err)
		api.WriteError(w, -1, "Failed to disconnect")
		return
	}

	// Trigger scan after disconnect
	go func() {
		time.Sleep(2 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"status": "disconnected"})
}

// ForgetWiFiRequest represents a WiFi forget request.
type ForgetWiFiRequest struct {
	SSID string `json:"ssid"`
}

// ForgetWiFi removes a saved WiFi network.
func (h *WiFiHandler) ForgetWiFi(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req ForgetWiFiRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	if req.SSID == "" {
		api.WriteError(w, -1, "SSID required")
		return
	}

	_, err := script.Run("forgetWiFi", req.SSID)
	if err != nil {
		logger.Error("Failed to forget WiFi network: %v", err)
		api.WriteError(w, -1, "Failed to forget network")
		return
	}

	// Trigger scan after forget
	go func() {
		time.Sleep(2 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"status": "forgotten"})
}

// SwitchWiFiRequest represents a WiFi switch request.
type SwitchWiFiRequest struct {
	Mode int `json:"mode"` // 0: disable, 1: enable
}

// SwitchWiFi enables or disables WiFi.
func (h *WiFiHandler) SwitchWiFi(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	var req SwitchWiFiRequest
	if err := api.ParseJSONBody(r, &req); err != nil {
		api.WriteError(w, -1, "Invalid request body")
		return
	}

	action := "disable"
	if req.Mode == 1 {
		action = "enable"
	}

	_, err := script.Run("switchWiFi", action)
	if err != nil {
		logger.Error("Failed to switch WiFi: %v", err)
		api.WriteError(w, -1, "Failed to switch WiFi")
		return
	}

	h.mu.Lock()
	h.staEnable = req.Mode
	h.mu.Unlock()

	// Trigger scan after switch
	go func() {
		time.Sleep(3 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"mode": req.Mode})
}
