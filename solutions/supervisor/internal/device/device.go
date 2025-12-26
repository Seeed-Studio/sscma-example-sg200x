// Package device provides device management functionality.
package device

import (
	"bufio"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"supervisor/internal/system"
	"supervisor/internal/upgrade"
)

// Configuration paths
const (
	ConfigDir       = "/etc/recamera.conf"
	UpgradeConfFile = "/etc/recamera.conf/upgrade"
	UserdataDir     = "/userdata"
	ModelDir        = "/userdata/Models"
	ModelsPreset    = "/usr/share/supervisor/models"
	PlatformInfo    = "/etc/recamera.conf/platform.info"
	WorkDir         = "/tmp/supervisor"
)

// Model files
const (
	ModelFile     = ModelDir + "/model.cvimodel"
	ModelInfoFile = ModelDir + "/model.json"
)

// DeviceInfo represents basic device information for the frontend.
type DeviceInfo struct {
	// App info
	AppName    string `json:"appName"`
	IsReCamera bool   `json:"isReCamera"`

	// Network info
	IP      string `json:"ip"`
	WifiIP  string `json:"wifiIp"`
	Mask    string `json:"mask"`
	MAC     string `json:"macAddress"`
	Gateway string `json:"gateway"`
	DNS     string `json:"dns"`

	// Device info
	DeviceName string `json:"deviceName"`
	SN         string `json:"sn"`
	Type       string `json:"type"`

	// Hardware specs
	CPU string `json:"cpu"`
	RAM string `json:"ram"`
	NPU string `json:"npu"`

	// OS info
	OSName    string `json:"osName"`
	OSVersion string `json:"osVersion"`

	// Ports
	TerminalPort string `json:"terminalPort"`

	// Update info
	Channel     int    `json:"channel"`
	ServerURL   string `json:"serverUrl"`
	OfficialURL string `json:"officialUrl"`
	NeedRestart int    `json:"needRestart"`
}

// APIDeviceInfo represents full device capabilities and configuration.
type APIDeviceInfo struct {
	CPU          string     `json:"cpu"`
	RAM          string     `json:"ram"`
	NPU          string     `json:"npu"`
	WS           string     `json:"ws"`
	TTYD         string     `json:"ttyd"`
	EMMC         int64      `json:"emmc"`
	WiFi         int        `json:"wifi"`
	CANBus       int        `json:"canbus"`
	Sensor       int        `json:"sensor"`
	Rollback     int        `json:"rollback"`
	SN           string     `json:"sn"`
	DevName      string     `json:"dev_name"`
	OS           string     `json:"os"`
	Ver          string     `json:"ver"`
	URL          string     `json:"url"`
	PlatformInfo string     `json:"platform_info"`
	Model        ModelPaths `json:"model"`
}

// ModelPaths contains paths for model files.
type ModelPaths struct {
	Dir    string `json:"dir"`
	File   string `json:"file"`
	Info   string `json:"info"`
	Preset string `json:"preset"`
}

// QueryDeviceInfo returns device information for the frontend.
func QueryDeviceInfo() *DeviceInfo {
	upgradeMgr := upgrade.NewUpgradeManager()
	channel, url := upgradeMgr.GetChannel()

	needRestart := 0
	if upgradeMgr.IsUpgradeDone() {
		needRestart = 1
	}

	// Build device type string
	deviceType := buildDeviceType()

	return &DeviceInfo{
		// App info
		AppName:    "supervisor",
		IsReCamera: true,

		// Network info
		IP:      system.GetIP("eth0"),
		WifiIP:  system.GetIP("wlan0"),
		Mask:    system.GetMask("eth0"),
		MAC:     system.GetMAC("eth0"),
		Gateway: system.GetGateway("eth0"),
		DNS:     system.GetDNS(),

		// Device info
		DeviceName: system.GetDeviceName(),
		SN:         system.GetSerialNumber(),
		Type:       deviceType,

		// Hardware specs
		CPU: "SG2002",
		RAM: "256MB",
		NPU: "1 TOPS",

		// OS info
		OSName:    system.GetOSName(),
		OSVersion: system.GetOSVersion(),

		// Ports - ttyd terminal runs on port 9090
		TerminalPort: "9090",

		// Update info
		Channel:     channel,
		ServerURL:   url,
		OfficialURL: upgrade.DefaultUpgradeURL,
		NeedRestart: needRestart,
	}
}

// buildDeviceType constructs the device type string based on hardware capabilities.
func buildDeviceType() string {
	modelType := "Basic"

	// Check for CAN bus (Gimbal variant)
	if system.HasCANBus() {
		modelType = "Gimbal"
	}

	// Check for WiFi
	if system.InterfaceExists("wlan0") {
		modelType += " WiFi"
	}

	// Check eMMC size
	emmc := system.GetEMMCSize()
	emmcGB := emmc >> 30 // Convert to GB
	if emmcGB == 0 {
		modelType += " No EMMC"
	} else if emmcGB < 8 {
		modelType += " 8G"
	} else if emmcGB < 16 {
		modelType += " 16G"
	} else if emmcGB < 32 {
		modelType += " 32G"
	} else if emmcGB < 64 {
		modelType += " 64G"
	}

	// Check sensor
	sensor := system.DetectSensor()
	switch sensor {
	case 0:
		modelType += " (No Sensor)"
	case 1:
		modelType += " (OV5647)"
	case 2:
		modelType += " (GC2053)"
	}

	return modelType
}

// GetAPIDevice returns full device capabilities and configuration.
func GetAPIDevice() *APIDeviceInfo {
	// Ensure model files exist
	go checkModels()

	wifi := 0
	if system.InterfaceExists("wlan0") {
		wifi = 1
	}

	canbus := 0
	if system.HasCANBus() {
		canbus = 1
	}

	rollback := 0
	if system.GetBootRollback() {
		rollback = 1
	}

	return &APIDeviceInfo{
		CPU:          "sg2002",
		RAM:          "256",
		NPU:          "1",
		WS:           "8000",
		TTYD:         "9090",
		EMMC:         system.GetEMMCSize(),
		WiFi:         wifi,
		CANBus:       canbus,
		Sensor:       system.DetectSensor(),
		Rollback:     rollback,
		SN:           system.GetSerialNumber(),
		DevName:      system.GetDeviceName(),
		OS:           system.GetOSName(),
		Ver:          system.GetOSVersion(),
		URL:          upgrade.DefaultUpgradeURL,
		PlatformInfo: PlatformInfo,
		Model: ModelPaths{
			Dir:    ModelDir,
			File:   ModelFile,
			Info:   ModelInfoFile,
			Preset: ModelsPreset,
		},
	}
}

// checkModels ensures model files exist, copying defaults if needed.
func checkModels() {
	if err := os.MkdirAll(ModelDir, 0755); err != nil {
		return
	}

	if _, err := os.Stat(ModelFile); os.IsNotExist(err) {
		srcModel := filepath.Join(ModelsPreset, "yolo11n_detection_cv181x_int8.cvimodel")
		srcInfo := filepath.Join(ModelsPreset, "yolo11n_detection_cv181x_int8.json")

		copyFile(srcModel, ModelFile)
		copyFile(srcInfo, ModelInfoFile)
	}
}

// copyFile copies a file from src to dst.
func copyFile(src, dst string) error {
	data, err := os.ReadFile(src)
	if err != nil {
		return err
	}
	return os.WriteFile(dst, data, 0644)
}

// UpdateDeviceName updates the device hostname and restarts avahi.
func UpdateDeviceName(name string) error {
	return system.SetDeviceName(name)
}

// DiscoveredDevice represents a device found via avahi-browse.
type DiscoveredDevice struct {
	Name      string `json:"name"`
	Type      string `json:"type"`
	Domain    string `json:"domain"`
	Hostname  string `json:"hostname"`
	IP        string `json:"ip"`
	Port      string `json:"port"`
	Interface string `json:"interface"`
}

// GetDeviceList returns discovered devices on the network via avahi.
func GetDeviceList() ([]DiscoveredDevice, error) {
	outputFile := filepath.Join(WorkDir, "getDeviceList")
	tmpFile := outputFile + ".tmp"

	// If tmp file exists, copy to output
	if data, err := os.ReadFile(tmpFile); err == nil && len(data) > 0 {
		os.WriteFile(outputFile, data, 0644)
	}

	// Start avahi-browse in background if not running
	if !system.IsProcessRunning("avahi-browse") {
		go func() {
			cmd := exec.Command("avahi-browse", "-arpt")
			out, _ := cmd.Output()
			os.WriteFile(tmpFile, out, 0644)
		}()
	}

	// Parse existing output file
	data, err := os.ReadFile(outputFile)
	if err != nil {
		return []DiscoveredDevice{}, nil
	}

	return parseAvahiBrowseOutput(string(data)), nil
}

// parseAvahiBrowseOutput parses avahi-browse -arpt output.
func parseAvahiBrowseOutput(output string) []DiscoveredDevice {
	var devices []DiscoveredDevice
	scanner := bufio.NewScanner(strings.NewReader(output))

	for scanner.Scan() {
		line := scanner.Text()
		// Format: +;interface;protocol;name;type;domain
		// or: =;interface;protocol;name;type;domain;hostname;ip;port;txt
		parts := strings.Split(line, ";")
		if len(parts) < 6 {
			continue
		}

		if parts[0] == "=" && len(parts) >= 9 {
			device := DiscoveredDevice{
				Interface: parts[1],
				Name:      parts[3],
				Type:      parts[4],
				Domain:    parts[5],
				Hostname:  parts[6],
				IP:        parts[7],
				Port:      parts[8],
			}
			devices = append(devices, device)
		}
	}

	return devices
}

// GetPlatformInfo reads the platform configuration file.
func GetPlatformInfo() string {
	data, err := os.ReadFile(PlatformInfo)
	if err != nil {
		return ""
	}
	return string(data)
}

// SavePlatformInfo saves the platform configuration.
func SavePlatformInfo(content string) error {
	return os.WriteFile(PlatformInfo, []byte(content), 0644)
}
