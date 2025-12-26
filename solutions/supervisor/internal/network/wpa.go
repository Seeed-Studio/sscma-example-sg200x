// Package network provides WiFi and network management functionality.
package network

import (
	"os/exec"
	"strconv"
	"strings"
)

// WPAClient wraps wpa_cli commands for a specific interface.
type WPAClient struct {
	iface string
}

// NewWPAClient creates a new WPA client for the specified interface.
func NewWPAClient(iface string) *WPAClient {
	return &WPAClient{iface: iface}
}

// run executes a wpa_cli command and returns the output.
func (w *WPAClient) run(args ...string) (string, error) {
	cmdArgs := append([]string{"-i", w.iface}, args...)
	cmd := exec.Command("wpa_cli", cmdArgs...)
	out, err := cmd.Output()
	return strings.TrimSpace(string(out)), err
}

// Status returns the current WPA status as a map.
func (w *WPAClient) Status() (map[string]string, error) {
	out, err := w.run("status")
	if err != nil {
		return nil, err
	}

	result := make(map[string]string)
	for _, line := range strings.Split(out, "\n") {
		parts := strings.SplitN(line, "=", 2)
		if len(parts) == 2 {
			result[parts[0]] = parts[1]
		}
	}
	return result, nil
}

// WPANetwork represents a configured WiFi network.
type WPANetwork struct {
	ID      int
	SSID    string
	BSSID   string
	Flags   string
	Current bool
}

// ListNetworks returns all configured networks.
func (w *WPAClient) ListNetworks() ([]WPANetwork, error) {
	out, err := w.run("list_networks")
	if err != nil {
		return nil, err
	}

	var networks []WPANetwork
	lines := strings.Split(out, "\n")
	// Skip header line
	for i := 1; i < len(lines); i++ {
		line := lines[i]
		if line == "" {
			continue
		}
		fields := strings.Split(line, "\t")
		if len(fields) < 2 {
			continue
		}

		id, _ := strconv.Atoi(fields[0])
		network := WPANetwork{
			ID:   id,
			SSID: fields[1],
		}
		if len(fields) >= 3 {
			network.BSSID = fields[2]
		}
		if len(fields) >= 4 {
			network.Flags = fields[3]
			network.Current = strings.Contains(fields[3], "CURRENT")
		}
		networks = append(networks, network)
	}
	return networks, nil
}

// AddNetwork adds a new network and returns its ID.
func (w *WPAClient) AddNetwork() (int, error) {
	out, err := w.run("add_network")
	if err != nil {
		return -1, err
	}
	return strconv.Atoi(out)
}

// SetNetwork sets a network parameter.
func (w *WPAClient) SetNetwork(id int, key, value string) (bool, error) {
	out, err := w.run("set_network", strconv.Itoa(id), key, value)
	if err != nil {
		return false, err
	}
	return out == "OK", nil
}

// SetNetworkSSID sets the SSID for a network (properly quoted).
func (w *WPAClient) SetNetworkSSID(id int, ssid string) (bool, error) {
	return w.SetNetwork(id, "ssid", "\""+ssid+"\"")
}

// SetNetworkPSK sets the PSK for a network (properly quoted).
func (w *WPAClient) SetNetworkPSK(id int, psk string) (bool, error) {
	return w.SetNetwork(id, "psk", "\""+psk+"\"")
}

// SetNetworkKeyMgmt sets the key management for a network.
func (w *WPAClient) SetNetworkKeyMgmt(id int, keyMgmt string) (bool, error) {
	return w.SetNetwork(id, "key_mgmt", keyMgmt)
}

// SetNetworkPriority sets the priority for a network.
func (w *WPAClient) SetNetworkPriority(id int, priority int) (bool, error) {
	return w.SetNetwork(id, "priority", strconv.Itoa(priority))
}

// EnableNetwork enables a network.
func (w *WPAClient) EnableNetwork(id int) error {
	_, err := w.run("enable_network", strconv.Itoa(id))
	return err
}

// DisableNetwork disables a network.
func (w *WPAClient) DisableNetwork(id int) error {
	_, err := w.run("disable_network", strconv.Itoa(id))
	return err
}

// RemoveNetwork removes a network.
func (w *WPAClient) RemoveNetwork(id int) error {
	_, err := w.run("remove_network", strconv.Itoa(id))
	return err
}

// SelectNetwork selects a network for connection.
func (w *WPAClient) SelectNetwork(id int) error {
	_, err := w.run("select_network", strconv.Itoa(id))
	return err
}

// Reconfigure reloads the configuration file.
func (w *WPAClient) Reconfigure() error {
	_, err := w.run("reconfigure")
	return err
}

// SaveConfig saves the current configuration.
func (w *WPAClient) SaveConfig() error {
	_, err := w.run("save_config")
	return err
}

// Disconnect disconnects from the current network.
func (w *WPAClient) Disconnect() error {
	_, err := w.run("disconnect")
	return err
}

// Reconnect attempts to reconnect to the last connected network.
func (w *WPAClient) Reconnect() error {
	_, err := w.run("reconnect")
	return err
}

// Scan triggers a WiFi scan.
func (w *WPAClient) Scan() error {
	_, err := w.run("scan")
	return err
}

// ScanResults returns the results of the last scan.
func (w *WPAClient) ScanResults() ([]WPAScanResult, error) {
	out, err := w.run("scan_results")
	if err != nil {
		return nil, err
	}

	var results []WPAScanResult
	lines := strings.Split(out, "\n")
	// Skip header line
	for i := 1; i < len(lines); i++ {
		line := lines[i]
		if line == "" {
			continue
		}
		fields := strings.Split(line, "\t")
		if len(fields) < 5 {
			continue
		}

		signal, _ := strconv.Atoi(fields[2])
		result := WPAScanResult{
			BSSID:     fields[0],
			Frequency: fields[1],
			Signal:    signal,
			Flags:     fields[3],
			SSID:      fields[4],
		}
		results = append(results, result)
	}
	return results, nil
}

// WPAScanResult represents a WiFi scan result.
type WPAScanResult struct {
	BSSID     string
	Frequency string
	Signal    int
	Flags     string
	SSID      string
}

// FindNetworkBySSID finds a network by SSID.
func (w *WPAClient) FindNetworkBySSID(ssid string) (int, error) {
	networks, err := w.ListNetworks()
	if err != nil {
		return -1, err
	}
	for _, n := range networks {
		if n.SSID == ssid {
			return n.ID, nil
		}
	}
	return -1, nil
}
