package network

import (
	"bufio"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"supervisor/internal/system"
)

// Configuration paths
const (
	ConfigDir     = "/etc/recamera.conf"
	STAConfigFile = "/etc/recamera.conf/sta"
	APConfigFile  = "/etc/recamera.conf/ap"
	WPAConfFile   = "/etc/wpa_supplicant.conf"
	HostAPDConf   = "/etc/hostapd_2g4.conf"
)

// WiFiManager handles WiFi operations.
type WiFiManager struct {
	mu       sync.RWMutex
	wpa      *WPAClient
	scanning bool
	scanData *ScanData
}

// ScanData holds the results of the most recent WiFi scan.
type ScanData struct {
	Networks  []WiFiNetwork
	Timestamp time.Time
}

// WiFiNetwork represents a WiFi network from scan results.
type WiFiNetwork struct {
	SSID      string `json:"ssid"`
	BSSID     string `json:"bssid"`
	Signal    int    `json:"signal"`
	Frequency string `json:"frequency"`
	Security  string `json:"security"`
	Connected bool   `json:"connected"`
	Saved     bool   `json:"saved"`
}

// WiFiStatus represents the current WiFi connection status.
type WiFiStatus struct {
	SSID       string `json:"ssid"`
	BSSID      string `json:"bssid"`
	IP         string `json:"ip"`
	State      string `json:"wpa_state"`
	Connected  bool   `json:"connected"`
	KeyMgmt    string `json:"key_mgmt"`
	MacAddress string `json:"address"`
	Signal     int    `json:"signal"`
}

// NewWiFiManager creates a new WiFi manager.
func NewWiFiManager() *WiFiManager {
	return &WiFiManager{
		wpa: NewWPAClient("wlan0"),
	}
}

// HasWiFi checks if WiFi interface exists.
func HasWiFi() bool {
	return system.InterfaceExists("wlan0")
}

// StartSTA starts the WiFi station (client) mode.
func (m *WiFiManager) StartSTA() error {
	if !HasWiFi() {
		return nil
	}

	if system.IsProcessRunning("wpa_supplicant") {
		return nil
	}

	// Bring interface down and up
	exec.Command("ifconfig", "wlan0", "down").Run()
	exec.Command("ifconfig", "wlan0", "up").Run()

	// Start wpa_supplicant
	return exec.Command("wpa_supplicant", "-B", "-i", "wlan0", "-c", WPAConfFile).Run()
}

// StopSTA stops the WiFi station mode.
func (m *WiFiManager) StopSTA() error {
	return system.StopProcessByName("wpa_supplicant", 9)
}

// StartAP starts the WiFi access point mode.
func (m *WiFiManager) StartAP() error {
	if !HasWiFi() {
		return nil
	}

	if system.IsProcessRunning("hostapd") {
		return nil
	}

	// Update AP SSID if needed
	m.updateAPSSID()

	// Bring interface down and up
	exec.Command("ifconfig", "wlan1", "down").Run()
	exec.Command("ifconfig", "wlan1", "up").Run()

	// Start hostapd
	return exec.Command("hostapd", "-B", HostAPDConf).Run()
}

// StopAP stops the WiFi access point mode.
func (m *WiFiManager) StopAP() error {
	system.StopProcessByName("hostapd", 9)
	exec.Command("ifconfig", "wlan1", "0").Run()
	exec.Command("ifconfig", "wlan1", "down").Run()
	time.Sleep(500 * time.Millisecond)
	system.RunServiceAsync("S80dnsmasq", "restart")
	system.RunServiceAsync("S49ntp", "restart")
	return nil
}

// updateAPSSID updates the AP SSID based on MAC address if needed.
func (m *WiFiManager) updateAPSSID() {
	data, err := os.ReadFile(HostAPDConf)
	if err != nil {
		return
	}

	content := string(data)
	if !strings.Contains(content, "ssid=AUOK") {
		return
	}

	// Get MAC suffix for unique SSID
	mac := system.GetMAC("wlan1")
	if mac == "" {
		return
	}
	parts := strings.Split(mac, ":")
	if len(parts) >= 6 {
		suffix := parts[3] + parts[4] + parts[5]
		newSSID := "reCamera_" + suffix
		content = regexp.MustCompile(`ssid=.*`).ReplaceAllString(content, "ssid="+newSSID)
		os.WriteFile(HostAPDConf, []byte(content), 0644)
	}
}

// StartWiFi initializes WiFi based on saved configuration.
// Returns STA enable state and AP enable state.
func (m *WiFiManager) StartWiFi() (sta int, ap int, err error) {
	sta, ap = -1, -1

	if !HasWiFi() {
		return sta, ap, nil
	}

	// Ensure config directory exists
	os.MkdirAll(ConfigDir, 0755)

	// Read STA config
	staData, err := os.ReadFile(STAConfigFile)
	if err != nil {
		sta = 1
		os.WriteFile(STAConfigFile, []byte("1"), 0644)
	} else {
		sta, _ = strconv.Atoi(strings.TrimSpace(string(staData)))
	}

	// Start STA if enabled
	if sta == 1 {
		go m.StartSTA()
	}

	// Read AP config
	apData, err := os.ReadFile(APConfigFile)
	if err != nil {
		ap = 1
		os.WriteFile(APConfigFile, []byte("1"), 0644)
	} else {
		ap, _ = strconv.Atoi(strings.TrimSpace(string(apData)))
	}

	// Start AP if enabled
	if ap == 1 {
		go m.StartAP()
	}

	return sta, ap, nil
}

// StopWiFi stops all WiFi services.
func (m *WiFiManager) StopWiFi() error {
	go m.StopSTA()
	go m.StopAP()
	return nil
}

// SwitchWiFi enables or disables WiFi STA mode.
func (m *WiFiManager) SwitchWiFi(enable bool) error {
	if enable {
		os.WriteFile(STAConfigFile, []byte("1"), 0644)
		return m.StartSTA()
	}
	os.WriteFile(STAConfigFile, []byte("0"), 0644)
	return m.StopSTA()
}

// GetStatus returns the current WiFi connection status.
func (m *WiFiManager) GetStatus() (*WiFiStatus, error) {
	status, err := m.wpa.Status()
	if err != nil {
		return nil, err
	}

	signal := -80 // Default weak signal
	if sigStr, ok := status["signal"]; ok {
		signal, _ = strconv.Atoi(sigStr)
	}

	wifiStatus := &WiFiStatus{
		SSID:       status["ssid"],
		BSSID:      status["bssid"],
		IP:         status["ip_address"],
		State:      status["wpa_state"],
		KeyMgmt:    status["key_mgmt"],
		MacAddress: status["address"],
		Signal:     signal,
	}
	wifiStatus.Connected = wifiStatus.State == "COMPLETED"

	return wifiStatus, nil
}

// GetConnectedNetworks returns all configured networks.
func (m *WiFiManager) GetConnectedNetworks() ([]WPANetwork, error) {
	m.wpa.Reconfigure()
	return m.wpa.ListNetworks()
}

// Scan triggers a WiFi scan and returns results.
func (m *WiFiManager) Scan() ([]WiFiNetwork, error) {
	m.mu.Lock()
	if m.scanning {
		m.mu.Unlock()
		// Return cached data if available
		if m.scanData != nil {
			return m.scanData.Networks, nil
		}
		return nil, nil
	}
	m.scanning = true
	m.mu.Unlock()

	defer func() {
		m.mu.Lock()
		m.scanning = false
		m.mu.Unlock()
	}()

	// Use wpa_cli for scanning (works while connected, unlike iw)
	networks, err := m.scanWithWPACli()
	if err != nil {
		// If wpa_cli fails, return cached data
		if m.scanData != nil {
			return m.scanData.Networks, nil
		}
		return nil, err
	}

	// Get saved networks for marking
	savedNetworks, _ := m.wpa.ListNetworks()
	savedSSIDs := make(map[string]bool)
	for _, n := range savedNetworks {
		savedSSIDs[n.SSID] = true
	}

	// Get current connection
	status, _ := m.GetStatus()

	// Mark saved and connected networks
	for i := range networks {
		networks[i].Saved = savedSSIDs[networks[i].SSID]
		if status != nil && networks[i].SSID == status.SSID {
			networks[i].Connected = status.Connected
		}
	}

	// Cache results
	m.mu.Lock()
	m.scanData = &ScanData{
		Networks:  networks,
		Timestamp: time.Now(),
	}
	m.mu.Unlock()

	return networks, nil
}

// scanWithWPACli performs a WiFi scan using wpa_cli (works while connected).
func (m *WiFiManager) scanWithWPACli() ([]WiFiNetwork, error) {
	// Trigger a new scan
	m.wpa.Scan()

	// Wait a bit for scan to complete
	time.Sleep(2 * time.Second)

	// Get scan results
	results, err := m.wpa.ScanResults()
	if err != nil {
		return nil, err
	}

	var networks []WiFiNetwork
	for _, r := range results {
		// Skip networks with empty SSIDs (hidden networks)
		if r.SSID == "" {
			continue
		}

		security := ""
		if strings.Contains(r.Flags, "WPA2") || strings.Contains(r.Flags, "RSN") {
			security = "WPA2"
		} else if strings.Contains(r.Flags, "WPA") {
			security = "WPA"
		} else if strings.Contains(r.Flags, "WEP") {
			security = "WEP"
		}

		networks = append(networks, WiFiNetwork{
			SSID:      r.SSID,
			BSSID:     r.BSSID,
			Signal:    r.Signal,
			Frequency: r.Frequency,
			Security:  security,
		})
	}

	return networks, nil
}

// parseIWScanOutput parses the output of 'iw dev wlan0 scan'.
func parseIWScanOutput(output string) []WiFiNetwork {
	var networks []WiFiNetwork
	var current *WiFiNetwork

	scanner := bufio.NewScanner(strings.NewReader(output))
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())

		if strings.HasPrefix(line, "BSS ") {
			// New network entry - save previous one if it has a non-empty SSID
			if current != nil && current.SSID != "" {
				networks = append(networks, *current)
			}
			current = &WiFiNetwork{}
			// Extract BSSID
			parts := strings.Fields(line)
			if len(parts) >= 2 {
				current.BSSID = strings.TrimSuffix(parts[1], "(on")
			}
		} else if current != nil {
			if strings.HasPrefix(line, "SSID:") {
				current.SSID = strings.TrimPrefix(line, "SSID: ")
			} else if strings.HasPrefix(line, "signal:") {
				// Parse signal strength (e.g., "signal: -65.00 dBm")
				parts := strings.Fields(line)
				if len(parts) >= 2 {
					signal, _ := strconv.ParseFloat(parts[1], 64)
					current.Signal = int(signal)
				}
			} else if strings.HasPrefix(line, "freq:") {
				current.Frequency = strings.TrimPrefix(line, "freq: ")
			} else if strings.Contains(line, "WPA") || strings.Contains(line, "RSN") {
				if current.Security == "" {
					current.Security = "WPA"
				}
				if strings.Contains(line, "RSN") {
					current.Security = "WPA2"
				}
			} else if strings.Contains(line, "WEP") {
				current.Security = "WEP"
			}
		}
	}

	// Add last network only if it has a non-empty SSID
	if current != nil && current.SSID != "" {
		networks = append(networks, *current)
	}

	return networks
}

// Connect connects to a WiFi network.
func (m *WiFiManager) Connect(ssid, password string, networkID int) error {
	m.wpa.Reconfigure()

	// Check if creating new network or using existing
	if networkID < 0 {
		var err error
		networkID, err = m.wpa.AddNetwork()
		if err != nil {
			return err
		}

		// Set SSID
		ok, err := m.wpa.SetNetworkSSID(networkID, ssid)
		if err != nil || !ok {
			m.wpa.RemoveNetwork(networkID)
			return err
		}

		// Set password or open network
		if password == "" {
			m.wpa.SetNetworkKeyMgmt(networkID, "NONE")
		} else {
			ok, err = m.wpa.SetNetworkPSK(networkID, password)
			if err != nil || !ok {
				m.wpa.RemoveNetwork(networkID)
				return err
			}
		}
	}

	// Set priority for this network (highest)
	networks, _ := m.wpa.ListNetworks()
	for _, n := range networks {
		priority := 0
		if n.ID == networkID {
			priority = 100
		}
		m.wpa.SetNetworkPriority(n.ID, priority)
	}

	// Enable and connect
	m.wpa.EnableNetwork(networkID)
	m.wpa.SaveConfig()
	m.wpa.Disconnect()
	err := m.wpa.SelectNetwork(networkID)
	if err != nil {
		return err
	}

	// Start DHCP renewal in background after a short delay
	// to allow wpa_supplicant to complete association
	go m.renewDHCP()

	return nil
}

// renewDHCP waits for WiFi association and then obtains a DHCP lease
func (m *WiFiManager) renewDHCP() {
	// Wait for wpa_state to become COMPLETED (up to 15 seconds)
	for i := 0; i < 30; i++ {
		time.Sleep(500 * time.Millisecond)
		status, err := m.GetStatus()
		if err == nil && status != nil && status.State == "COMPLETED" {
			// Association complete, now get DHCP lease
			// Use udhcpc since dhcpcd has issues on some devices
			exec.Command("udhcpc", "-i", "wlan0", "-n", "-q").Run()
			return
		}
	}
}

// Disconnect disconnects from a WiFi network.
func (m *WiFiManager) Disconnect(ssid string) error {
	id, err := m.wpa.FindNetworkBySSID(ssid)
	if err != nil || id < 0 {
		return err
	}
	m.wpa.DisableNetwork(id)
	return m.wpa.SaveConfig()
}

// Forget removes a saved WiFi network.
func (m *WiFiManager) Forget(ssid string) error {
	id, err := m.wpa.FindNetworkBySSID(ssid)
	if err != nil || id < 0 {
		return err
	}
	m.wpa.RemoveNetwork(id)
	return m.wpa.SaveConfig()
}
