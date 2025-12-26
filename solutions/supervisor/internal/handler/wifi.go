package handler

import (
	"fmt"
	"net/http"
	"strings"
	"sync"
	"time"

	"supervisor/internal/api"
	"supervisor/internal/network"
	"supervisor/internal/system"
	"supervisor/pkg/logger"
)

// Network status constants matching frontend enum
const (
	NetworkStatusDisconnected = 1
	NetworkStatusConnecting   = 2
	NetworkStatusConnected    = 3
)

// WiFi auth constants
const (
	WifiAuthOpen   = 0
	WifiAuthSecure = 1
)

// WiFi enable constants
const (
	WifiEnableClose   = 0 // Has WiFi, not enabled
	WifiEnableOpen    = 1 // Has WiFi, enabled
	WifiEnableDisable = 2 // No WiFi
)

// WiFiHandler handles WiFi management API requests.
type WiFiHandler struct {
	mu          sync.RWMutex
	wifiMgr     *network.WiFiManager
	staEnable   int
	apEnable    int
	networkInfo map[string]interface{}
	scanning    bool
	stopChan    chan struct{}
	selected    string // Currently selected SSID for connection
	failedCnt   int    // Failed connection counter
}

// NewWiFiHandler creates a new WiFiHandler.
func NewWiFiHandler() *WiFiHandler {
	h := &WiFiHandler{
		wifiMgr:     network.NewWiFiManager(),
		staEnable:   -1, // -1 means not initialized
		apEnable:    1,
		networkInfo: make(map[string]interface{}),
		stopChan:    make(chan struct{}),
	}

	// Start WiFi interfaces (STA and AP mode)
	h.initWiFi()

	// Start background WiFi monitoring
	go h.monitorWiFi()

	return h
}

// initWiFi starts WiFi interfaces using native Go.
func (h *WiFiHandler) initWiFi() {
	sta, ap, err := h.wifiMgr.StartWiFi()
	if err != nil {
		logger.Warning("Failed to initialize WiFi: %v", err)
		return
	}

	h.mu.Lock()
	h.staEnable = sta
	h.apEnable = ap
	h.mu.Unlock()

	logger.Info("WiFi initialized: STA=%d, AP=%d", sta, ap)
}

// Stop stops the WiFi monitoring goroutine.
func (h *WiFiHandler) Stop() {
	close(h.stopChan)
}

// monitorWiFi periodically scans for WiFi networks.
func (h *WiFiHandler) monitorWiFi() {
	ticker := time.NewTicker(10 * time.Second)
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

// getWifiEnable returns the WiFi enable status
func (h *WiFiHandler) getWifiEnable() int {
	h.mu.RLock()
	defer h.mu.RUnlock()

	if h.staEnable == -1 {
		return WifiEnableDisable // No WiFi hardware
	}
	return h.staEnable // 0=disabled, 1=enabled
}

// getEtherInfo gets ethernet interface info in frontend-expected format
func (h *WiFiHandler) getEtherInfo() map[string]interface{} {
	info := system.GetInterfaceInfo("eth0")

	ip := info.IP
	// Ignore link-local addresses
	if strings.HasPrefix(ip, "169.254") {
		ip = ""
	}

	status := NetworkStatusDisconnected
	if ip != "" {
		status = NetworkStatusConnected
	}

	return map[string]interface{}{
		"ssid":         "Ethernet",
		"status":       status,
		"ip":           ip,
		"macAddress":   info.MACAddress,
		"ipAssignment": 1, // DHCP
		"subnetMask":   info.SubnetMask,
		"auth":         0,
		"signal":       -30, // Strong signal for wired
	}
}

// getStaCurrentInfo gets current WiFi station info
func (h *WiFiHandler) getStaCurrentInfo() map[string]interface{} {
	staInfo, err := h.wifiMgr.GetStatus()
	if err != nil || staInfo == nil {
		return nil
	}

	ip := staInfo.IP
	if strings.HasPrefix(ip, "169.254") {
		ip = ""
	}

	// Determine connection status
	status := NetworkStatusConnecting
	ssid := staInfo.SSID

	h.mu.Lock()
	if ssid != "" && ip != "" && staInfo.State == "COMPLETED" {
		status = NetworkStatusConnected
	} else {
		if h.failedCnt > 0 {
			h.failedCnt--
		}
		if h.failedCnt <= 0 {
			status = NetworkStatusDisconnected
		}
	}

	if status == NetworkStatusDisconnected || status == NetworkStatusConnected {
		h.selected = ""
	}

	if ssid == "" {
		ssid = h.selected
	}
	h.mu.Unlock()

	auth := WifiAuthOpen
	if strings.Contains(staInfo.KeyMgmt, "WPA") {
		auth = WifiAuthSecure
	}

	return map[string]interface{}{
		"ssid":            ssid,
		"bssid":           staInfo.BSSID,
		"status":          status,
		"ip":              ip,
		"macAddress":      staInfo.MacAddress,
		"ipAssignment":    1, // DHCP
		"subnetMask":      "255.255.255.0",
		"auth":            auth,
		"signal":          staInfo.Signal,
		"connectedStatus": 1, // Already connected/saved
	}
}

// getConnectedNetworksList gets list of saved/connected networks
func (h *WiFiHandler) getConnectedNetworksList(current map[string]interface{}) []map[string]interface{} {
	result := []map[string]interface{}{}

	// Add current connected network first if it exists
	if current != nil && current["ssid"] != "" {
		result = append(result, current)
	}

	// Get saved networks from wpa_supplicant
	networks, err := h.wifiMgr.GetConnectedNetworks()
	if err != nil {
		return result
	}

	currentSSID := ""
	if current != nil {
		if ssid, ok := current["ssid"].(string); ok {
			currentSSID = ssid
		}
	}

	for _, n := range networks {
		// Skip if already added as current
		if n.SSID == currentSSID {
			// Update current entry with ID
			if len(result) > 0 {
				result[0]["id"] = n.ID
				result[0]["flags"] = n.Flags
			}
			continue
		}

		auth := WifiAuthOpen
		if strings.Contains(n.Flags, "WPA") || strings.Contains(n.Flags, "PSK") {
			auth = WifiAuthSecure
		}

		result = append(result, map[string]interface{}{
			"id":              n.ID,
			"ssid":            n.SSID,
			"bssid":           n.BSSID,
			"flags":           n.Flags,
			"status":          NetworkStatusDisconnected,
			"auth":            auth,
			"signal":          -80, // Default weak signal for saved networks
			"connectedStatus": 1,   // Was connected before
			"macAddress":      n.BSSID,
			"ipAssignment":    1,
			"subnetMask":      "",
			"ip":              "",
		})
	}

	return result
}

// getScanList gets list of available WiFi networks from scan
func (h *WiFiHandler) getScanList(connected []map[string]interface{}) []map[string]interface{} {
	result := []map[string]interface{}{}

	networks, err := h.wifiMgr.Scan()
	if err != nil {
		return result
	}

	// Create a map of connected/saved SSIDs for quick lookup
	connectedSSIDs := make(map[string]bool)
	for _, c := range connected {
		if ssid, ok := c["ssid"].(string); ok {
			connectedSSIDs[ssid] = true
		}
	}

	// Also update connected list with signal strength from scan
	for _, n := range networks {
		// Skip networks with empty SSIDs (hidden networks)
		if n.SSID == "" {
			continue
		}

		// Update signal strength in connected list
		for i, c := range connected {
			if cSSID, ok := c["ssid"].(string); ok && cSSID == n.SSID {
				connected[i]["signal"] = n.Signal
				connected[i]["macAddress"] = n.BSSID
				if n.Security != "" {
					connected[i]["auth"] = WifiAuthSecure
				}
			}
		}

		// Skip networks that are already in connected list
		if connectedSSIDs[n.SSID] {
			continue
		}

		auth := WifiAuthOpen
		if n.Security != "" {
			auth = WifiAuthSecure
		}

		result = append(result, map[string]interface{}{
			"ssid":            n.SSID,
			"bssid":           n.BSSID,
			"macAddress":      n.BSSID,
			"signal":          n.Signal,
			"frequency":       n.Frequency,
			"auth":            auth,
			"status":          NetworkStatusDisconnected,
			"connectedStatus": 0, // Not previously connected
			"ipAssignment":    1,
			"subnetMask":      "",
			"ip":              "",
		})
	}

	return result
}

// scanNetworks performs a WiFi scan using native Go.
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
	etherInfo := h.getEtherInfo()

	// Get current WiFi status
	staInfo := h.getStaCurrentInfo()

	// Get connected/saved networks
	connectedList := h.getConnectedNetworksList(staInfo)

	// Get scan list (excluding connected networks)
	scanList := h.getScanList(connectedList)

	// Stop AP mode if either ethernet or WiFi is connected
	ethConnected := etherInfo["status"] == NetworkStatusConnected
	wifiConnected := staInfo != nil && staInfo["status"] == NetworkStatusConnected

	h.mu.Lock()
	if (ethConnected || wifiConnected) && h.apEnable == 1 {
		go h.wifiMgr.StopAP()
		h.apEnable = 0
	}

	h.networkInfo = map[string]interface{}{
		"etherInfo":             etherInfo,
		"connectedWifiInfoList": connectedList,
		"wifiInfoList":          scanList,
	}
	h.mu.Unlock()
}

// GetWiFiInfoList returns WiFi information.
func (h *WiFiHandler) GetWiFiInfoList(w http.ResponseWriter, r *http.Request) {
	h.mu.RLock()
	info := h.networkInfo
	wifiEnable := h.getWifiEnable()
	h.mu.RUnlock()

	// Trigger a new scan
	go h.scanNetworks()

	response := map[string]interface{}{
		"wifiEnable": wifiEnable,
	}

	if etherInfo, ok := info["etherInfo"]; ok {
		response["etherInfo"] = etherInfo
	}
	if connectedList, ok := info["connectedWifiInfoList"]; ok {
		response["connectedWifiInfoList"] = connectedList
	} else {
		response["connectedWifiInfoList"] = []map[string]interface{}{}
	}
	if scanList, ok := info["wifiInfoList"]; ok {
		response["wifiInfoList"] = scanList
	} else {
		response["wifiInfoList"] = []map[string]interface{}{}
	}

	api.WriteSuccess(w, response)
}

// ConnectWiFiRequest represents a WiFi connect request.
type ConnectWiFiRequest struct {
	ID       int    `json:"id"`
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

	h.mu.Lock()
	// Check if we're already trying to connect to this network
	if h.selected == req.SSID {
		h.mu.Unlock()
		api.WriteSuccess(w, map[string]interface{}{"message": "Connecting..."})
		return
	}

	// Find network ID from connected list
	networkID := -1
	if connectedList, ok := h.networkInfo["connectedWifiInfoList"].([]map[string]interface{}); ok {
		for _, n := range connectedList {
			if ssid, ok := n["ssid"].(string); ok && ssid == req.SSID {
				if id, ok := n["id"].(int); ok {
					networkID = id
				} else if idStr, ok := n["id"].(string); ok {
					// Try to parse string ID
					fmt.Sscanf(idStr, "%d", &networkID)
				}
				break
			}
		}
	}

	h.selected = req.SSID
	h.failedCnt = 10
	h.mu.Unlock()

	// Connect to WiFi using native Go
	go func() {
		err := h.wifiMgr.Connect(req.SSID, req.Password, networkID)
		if err != nil {
			logger.Error("Failed to connect to WiFi: %v", err)
		}
		// Scan after connection attempt
		time.Sleep(3 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"message": "OK"})
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

	h.mu.Lock()
	h.failedCnt = 10
	h.mu.Unlock()

	go func() {
		h.wifiMgr.Disconnect(req.SSID)
		time.Sleep(2 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"message": "OK"})
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

	h.mu.Lock()
	h.failedCnt = 10
	h.mu.Unlock()

	go func() {
		h.wifiMgr.Forget(req.SSID)
		time.Sleep(2 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"message": "OK"})
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

	h.mu.Lock()
	if h.staEnable == -1 {
		h.mu.Unlock()
		api.WriteError(w, -1, "WiFi not supported")
		return
	}
	h.staEnable = req.Mode
	h.apEnable = req.Mode
	h.mu.Unlock()

	enable := req.Mode == 1
	if err := h.wifiMgr.SwitchWiFi(enable); err != nil {
		logger.Error("Failed to switch WiFi: %v", err)
		api.WriteError(w, -1, "Failed to switch WiFi")
		return
	}

	// Trigger scan after switch
	go func() {
		time.Sleep(3 * time.Second)
		h.scanNetworks()
	}()

	api.WriteSuccess(w, map[string]interface{}{"message": "OK"})
}
