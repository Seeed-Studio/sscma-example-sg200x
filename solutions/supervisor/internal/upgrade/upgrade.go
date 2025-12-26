// Package upgrade provides system upgrade management functionality.
package upgrade

import (
	"archive/zip"
	"bufio"
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"supervisor/pkg/logger"
)

// URLs and file paths
const (
	OfficialURL  = "https://github.com/Seeed-Studio/reCamera-OS/releases/latest"
	OfficialURL2 = "https://files.seeedstudio.com/reCamera"
	MD5FileName  = "sg2002_recamera_emmc_md5sum.txt"
	URLFileName  = "url.txt"

	DefaultUpgradeURL = "https://github.com/Seeed-Studio/reCamera-OS/releases/latest"
)

// Partitions
const (
	FIPPart     = "/dev/mmcblk0boot0"
	BootPart    = "/dev/mmcblk0p1"
	RecvPart    = "/dev/mmcblk0p5"
	RootFS      = "/dev/mmcblk0p3"
	RootFSB     = "/dev/mmcblk0p4"
	ForceROPath = "/sys/block/mmcblk0boot0/force_ro"
)

// Directories
const (
	ConfigDir       = "/etc/recamera.conf"
	UpgradeConfFile = "/etc/recamera.conf/upgrade"
	UpgradeTmpDir   = "/userdata/.upgrade"
	UpgradeFilesDir = "/tmp/upgrade"
	WorkDir         = "/tmp/supervisor"
)

// Work directory files
const (
	UpgradeCancelFile = WorkDir + "/upgrade.cancel"
	UpgradeDoneFile   = WorkDir + "/upgrade.done"
	UpgradeMutexFile  = WorkDir + "/upgrade.mutex"
	UpgradeProgFile   = WorkDir + "/upgrade.prog"
	UpgradeProgTmp    = WorkDir + "/upgrade.prog.tmp"
)

// UpdateStatus represents the status of update availability.
type UpdateStatus int

const (
	UpdateStatusAvailable UpdateStatus = 1
	UpdateStatusUpgrading UpdateStatus = 2
	UpdateStatusQuerying  UpdateStatus = 3
)

// UpdateVersion represents available update information.
type UpdateVersion struct {
	OSName    string       `json:"osName"`
	OSVersion string       `json:"osVersion"`
	Status    UpdateStatus `json:"status"`
}

// UpdateProgress represents upgrade progress.
type UpdateProgress struct {
	Progress int    `json:"progress"`
	Status   string `json:"status"` // download, upgrade, idle, cancelled
}

// VersionInfo from MD5 file
type VersionInfo struct {
	FileName string
	MD5Sum   string
	OSName   string
	Version  string
}

// UpgradeManager handles system upgrades.
type UpgradeManager struct {
	mu          sync.RWMutex
	mountPath   string
	downloading bool
	upgrading   bool
	cancelled   bool
}

// NewUpgradeManager creates a new upgrade manager.
func NewUpgradeManager() *UpgradeManager {
	// Ensure directories exist
	os.MkdirAll(WorkDir, 0700)
	os.MkdirAll(ConfigDir, 0755)
	os.MkdirAll(UpgradeTmpDir, 0755)
	os.MkdirAll(UpgradeFilesDir, 0755)
	return &UpgradeManager{}
}

// IsUpgrading checks if an upgrade is in progress.
func (m *UpgradeManager) IsUpgrading() bool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.upgrading || m.downloading
}

// IsUpgradeDone checks if an upgrade has completed and requires restart.
func (m *UpgradeManager) IsUpgradeDone() bool {
	_, err := os.Stat(UpgradeDoneFile)
	return err == nil
}

// GetChannel returns the current upgrade channel and server URL.
func (m *UpgradeManager) GetChannel() (channel int, url string) {
	data, err := os.ReadFile(UpgradeConfFile)
	if err != nil {
		return 0, ""
	}
	parts := strings.SplitN(strings.TrimSpace(string(data)), ",", 2)
	if len(parts) >= 1 {
		channel, _ = strconv.Atoi(parts[0])
	}
	if len(parts) >= 2 {
		url = parts[1]
	}
	return
}

// UpdateChannel sets the upgrade channel and server URL.
func (m *UpgradeManager) UpdateChannel(channel int, url string) error {
	if m.IsUpgrading() {
		return fmt.Errorf("upgrade in progress")
	}

	content := strconv.Itoa(channel)
	if url != "" {
		content += "," + url
	}
	if err := os.WriteFile(UpgradeConfFile, []byte(content), 0644); err != nil {
		return err
	}

	// Clean up upgrade state
	m.Clean()
	return nil
}

// Clean removes all upgrade files.
func (m *UpgradeManager) Clean() {
	os.RemoveAll(UpgradeFilesDir)
	os.MkdirAll(UpgradeFilesDir, 0755)
	files, _ := filepath.Glob(UpgradeTmpDir + "/*")
	for _, f := range files {
		os.RemoveAll(f)
	}
}

// GetSystemUpdateVersion checks for available updates.
func (m *UpgradeManager) GetSystemUpdateVersion() (*UpdateVersion, error) {
	result := &UpdateVersion{Status: UpdateStatusQuerying}

	if m.IsUpgrading() {
		result.Status = UpdateStatusUpgrading
		return result, nil
	}

	// Check for cached version info
	versionFile := filepath.Join(UpgradeFilesDir, "version.json")
	if data, err := os.ReadFile(versionFile); err == nil {
		var cached UpdateVersion
		if json.Unmarshal(data, &cached) == nil && cached.OSName != "" {
			result.Status = UpdateStatusAvailable
			result.OSName = cached.OSName
			result.OSVersion = cached.OSVersion
			return result, nil
		}
	}

	// Start background query if not running
	go m.QueryLatestVersion()

	return result, nil
}

// QueryLatestVersion queries for the latest version in background.
func (m *UpgradeManager) QueryLatestVersion() error {
	channel, customURL := m.GetChannel()

	var md5URL string
	if channel != 0 && customURL != "" {
		md5URL = customURL
	} else {
		// Parse GitHub releases URL
		url, err := m.parseGitHubReleasesURL(OfficialURL)
		if err != nil {
			// Fallback to Seeed URL
			url, err = m.getSeeedLatestURL()
			if err != nil {
				return err
			}
		}
		md5URL = url
	}

	// Download MD5 file
	md5Path := filepath.Join(UpgradeFilesDir, MD5FileName)
	if err := m.downloadFile(md5URL, md5Path); err != nil {
		return err
	}

	// Parse version info
	info, err := m.parseVersionInfo(md5Path)
	if err != nil {
		return err
	}

	// Save version info
	versionFile := filepath.Join(UpgradeFilesDir, "version.json")
	versionData := UpdateVersion{
		OSName:    info.OSName,
		OSVersion: info.Version,
		Status:    UpdateStatusAvailable,
	}
	data, _ := json.Marshal(versionData)
	os.WriteFile(versionFile, data, 0644)

	// Save URL for download
	baseURL := strings.TrimSuffix(md5URL, "/"+MD5FileName)
	os.WriteFile(filepath.Join(UpgradeFilesDir, URLFileName), []byte(baseURL), 0644)

	return nil
}

// parseGitHubReleasesURL parses GitHub releases URL to get MD5 file URL.
func (m *UpgradeManager) parseGitHubReleasesURL(url string) (string, error) {
	client := &http.Client{
		Timeout: 60 * time.Second,
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			return http.ErrUseLastResponse
		},
	}

	resp, err := client.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	// Get the Location header for redirect
	location := resp.Header.Get("Location")
	if location == "" {
		return "", fmt.Errorf("no redirect location found")
	}

	// Convert tag URL to download URL
	// https://github.com/.../releases/tag/v1.0.0 -> https://github.com/.../releases/download/v1.0.0
	location = strings.Replace(location, "/tag/", "/download/", 1)
	return location + "/" + MD5FileName, nil
}

// getSeeedLatestURL gets the latest version URL from Seeed server.
func (m *UpgradeManager) getSeeedLatestURL() (string, error) {
	url := OfficialURL2 + "/latest"
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	version := strings.TrimSpace(string(body))
	if version == "" {
		return "", fmt.Errorf("empty version from Seeed server")
	}

	return OfficialURL2 + "/" + version + "/" + MD5FileName, nil
}

// parseVersionInfo parses the MD5 file to extract version information.
func (m *UpgradeManager) parseVersionInfo(md5Path string) (*VersionInfo, error) {
	data, err := os.ReadFile(md5Path)
	if err != nil {
		return nil, err
	}

	// Parse line: <md5sum>  <filename>_<os>_<version>_ota.zip
	re := regexp.MustCompile(`([a-f0-9]+)\s+(\S+ota\.zip)`)
	matches := re.FindStringSubmatch(string(data))
	if len(matches) < 3 {
		return nil, fmt.Errorf("invalid MD5 file format")
	}

	info := &VersionInfo{
		MD5Sum:   matches[1],
		FileName: matches[2],
	}

	// Parse filename: sg2002_reCamera_1.0.0_ota.zip
	parts := strings.Split(info.FileName, "_")
	if len(parts) >= 3 {
		info.OSName = parts[1]
		info.Version = parts[2]
	}

	return info, nil
}

// GetUpdateProgress returns the current upgrade progress.
func (m *UpgradeManager) GetUpdateProgress() (*UpdateProgress, error) {
	// Check if cancelled
	if _, err := os.Stat(UpgradeCancelFile); err == nil {
		return &UpdateProgress{Progress: 0, Status: "cancelled"}, nil
	}

	// Read progress file
	data, err := os.ReadFile(UpgradeProgFile)
	if err != nil || len(data) == 0 {
		return &UpdateProgress{Progress: 0, Status: "idle"}, nil
	}

	var progress UpdateProgress
	if err := json.Unmarshal(data, &progress); err != nil {
		return &UpdateProgress{Progress: 0, Status: "idle"}, nil
	}

	return &progress, nil
}

// UpdateSystem initiates a system update.
func (m *UpgradeManager) UpdateSystem() error {
	// Clear state files
	os.Remove(UpgradeCancelFile)
	os.Remove(UpgradeDoneFile)
	os.Remove(UpgradeProgFile)

	m.cancelled = false

	// Start download and upgrade in background
	go m.performUpgrade()

	return nil
}

// performUpgrade performs the full upgrade process.
func (m *UpgradeManager) performUpgrade() {
	m.mu.Lock()
	m.downloading = true
	m.mu.Unlock()

	defer func() {
		m.mu.Lock()
		m.downloading = false
		m.upgrading = false
		m.mu.Unlock()
	}()

	// Download phase
	m.updateProgress(0, "download")
	if err := m.downloadOTA(); err != nil {
		logger.Error("Download failed: %v", err)
		m.updateProgress(0, "failed")
		return
	}

	// Check for cancellation
	if m.cancelled {
		return
	}

	m.mu.Lock()
	m.downloading = false
	m.upgrading = true
	m.mu.Unlock()

	// Upgrade phase
	m.updateProgress(50, "upgrade")
	if err := m.performOTAUpgrade(); err != nil {
		logger.Error("Upgrade failed: %v", err)
		m.updateProgress(50, "failed")
		return
	}

	m.updateProgress(100, "upgrade")
	os.WriteFile(UpgradeDoneFile, []byte("1"), 0644)
}

// downloadOTA downloads the OTA package.
func (m *UpgradeManager) downloadOTA() error {
	// Mount recovery partition
	if err := m.mountRecovery(); err != nil {
		return err
	}
	defer m.unmountRecovery()

	// Get version info
	md5Path := filepath.Join(UpgradeFilesDir, MD5FileName)
	info, err := m.parseVersionInfo(md5Path)
	if err != nil {
		// Try to query latest first
		if err := m.QueryLatestVersion(); err != nil {
			return err
		}
		info, err = m.parseVersionInfo(md5Path)
		if err != nil {
			return err
		}
	}

	// Get download URL
	urlData, err := os.ReadFile(filepath.Join(UpgradeFilesDir, URLFileName))
	if err != nil {
		return fmt.Errorf("no URL file found")
	}
	baseURL := strings.TrimSpace(string(urlData))
	downloadURL := baseURL + "/" + info.FileName

	// Download to temp directory
	tmpPath := filepath.Join(UpgradeTmpDir, info.FileName)
	if err := m.downloadFileWithProgress(downloadURL, tmpPath, 0, 50); err != nil {
		return err
	}

	// Verify MD5
	if err := m.verifyMD5(tmpPath, info.MD5Sum); err != nil {
		os.Remove(tmpPath)
		return err
	}

	// Copy to recovery partition
	destPath := filepath.Join(m.mountPath, info.FileName)
	if err := copyFile(tmpPath, destPath); err != nil {
		return err
	}

	// Copy MD5 file
	copyFile(md5Path, filepath.Join(m.mountPath, MD5FileName))

	// Clean temp
	os.Remove(tmpPath)

	return nil
}

// performOTAUpgrade writes the OTA update to partitions.
func (m *UpgradeManager) performOTAUpgrade() error {
	// Mount recovery partition
	if err := m.mountRecovery(); err != nil {
		return err
	}
	defer m.unmountRecovery()

	// Find OTA file
	md5Path := filepath.Join(m.mountPath, MD5FileName)
	info, err := m.parseVersionInfo(md5Path)
	if err != nil {
		return err
	}

	zipPath := filepath.Join(m.mountPath, info.FileName)
	if _, err := os.Stat(zipPath); err != nil {
		return fmt.Errorf("OTA file not found: %s", zipPath)
	}

	// Open zip file
	zipReader, err := zip.OpenReader(zipPath)
	if err != nil {
		return err
	}
	defer zipReader.Close()

	// Read MD5 sums from zip
	md5Map, err := m.readZipMD5(zipReader)
	if err != nil {
		return err
	}

	// Update FIP partition
	m.updateProgress(55, "upgrade")
	if err := m.updatePartition(zipReader, FIPPart, "fip.bin", md5Map, true); err != nil {
		logger.Warning("FIP update skipped: %v", err)
	}

	// Update boot partition
	m.updateProgress(60, "upgrade")
	if err := m.updatePartition(zipReader, BootPart, "boot.emmc", md5Map, false); err != nil {
		logger.Warning("Boot update skipped: %v", err)
	}

	// Determine target rootfs partition (opposite of current)
	targetRootFS := RootFSB
	if m.isRootFSB() {
		targetRootFS = RootFS
	}

	// Update rootfs
	m.updateProgress(65, "upgrade")
	if err := m.updatePartition(zipReader, targetRootFS, "rootfs_ext4.emmc", md5Map, false); err != nil {
		return fmt.Errorf("rootfs update failed: %v", err)
	}

	// Switch boot partition
	m.updateProgress(95, "upgrade")
	if err := m.switchPartition(); err != nil {
		return err
	}

	return nil
}

// mountRecovery mounts the recovery partition.
func (m *UpgradeManager) mountRecovery() error {
	// Check if already ext4
	out, _ := exec.Command("blkid", "-o", "value", "-s", "TYPE", RecvPart).Output()
	if strings.TrimSpace(string(out)) != "ext4" {
		// Format as ext4
		if err := exec.Command("mkfs.ext4", "-F", RecvPart).Run(); err != nil {
			return fmt.Errorf("format recovery partition failed: %v", err)
		}
	}

	// Create mount point
	m.mountPath = filepath.Join("/tmp", "recovery_mount")
	os.MkdirAll(m.mountPath, 0755)

	// Mount
	if err := exec.Command("mount", RecvPart, m.mountPath).Run(); err != nil {
		return fmt.Errorf("mount recovery partition failed: %v", err)
	}

	return nil
}

// unmountRecovery unmounts the recovery partition.
func (m *UpgradeManager) unmountRecovery() {
	if m.mountPath != "" {
		exec.Command("umount", m.mountPath).Run()
		os.RemoveAll(m.mountPath)
		m.mountPath = ""
	}
}

// isRootFSB checks if current root is on partition B.
func (m *UpgradeManager) isRootFSB() bool {
	out, err := exec.Command("mountpoint", "-n", "/").Output()
	if err != nil {
		return false
	}
	device := strings.Fields(string(out))[0]
	// Resolve symlinks
	resolved, _ := filepath.EvalSymlinks(device)
	resolvedB, _ := filepath.EvalSymlinks(RootFSB)
	return resolved == resolvedB
}

// switchPartition switches the boot partition.
func (m *UpgradeManager) switchPartition() error {
	partB := 0
	if m.isRootFSB() {
		partB = 1
	}
	partB = 1 - partB // Switch to opposite

	// Set U-Boot environment
	if err := exec.Command("fw_setenv", "use_part_b", strconv.Itoa(partB)).Run(); err != nil {
		return err
	}
	if err := exec.Command("fw_setenv", "boot_cnt", "0").Run(); err != nil {
		return err
	}
	if err := exec.Command("fw_setenv", "boot_failed_limits", "5").Run(); err != nil {
		return err
	}
	exec.Command("fw_setenv", "boot_rollback").Run() // Clear rollback flag

	return nil
}

// readZipMD5 reads MD5 sums from md5sum.txt inside the zip.
func (m *UpgradeManager) readZipMD5(zipReader *zip.ReadCloser) (map[string]string, error) {
	md5Map := make(map[string]string)

	for _, f := range zipReader.File {
		if f.Name == "md5sum.txt" {
			rc, err := f.Open()
			if err != nil {
				return nil, err
			}
			defer rc.Close()

			scanner := bufio.NewScanner(rc)
			for scanner.Scan() {
				fields := strings.Fields(scanner.Text())
				if len(fields) >= 2 {
					md5Map[fields[1]] = fields[0]
				}
			}
			break
		}
	}

	return md5Map, nil
}

// updatePartition updates a partition from the zip file.
func (m *UpgradeManager) updatePartition(zipReader *zip.ReadCloser, partition, filename string, md5Map map[string]string, isFIP bool) error {
	// Find file in zip
	var zipFile *zip.File
	for _, f := range zipReader.File {
		if f.Name == filename {
			zipFile = f
			break
		}
	}
	if zipFile == nil {
		return fmt.Errorf("file not found in zip: %s", filename)
	}

	expectedMD5 := md5Map[filename]
	if expectedMD5 == "" {
		return fmt.Errorf("no MD5 for: %s", filename)
	}

	// For FIP partition, we need to disable write protection
	if isFIP {
		os.WriteFile(ForceROPath, []byte("0"), 0644)
		defer os.WriteFile(ForceROPath, []byte("1"), 0644)
	}

	// Open zip file
	rc, err := zipFile.Open()
	if err != nil {
		return err
	}
	defer rc.Close()

	// Open partition for writing
	part, err := os.OpenFile(partition, os.O_WRONLY, 0)
	if err != nil {
		return err
	}
	defer part.Close()

	// Create MD5 hash writer
	hash := md5.New()
	writer := io.MultiWriter(part, hash)

	// Copy data
	written, err := io.Copy(writer, rc)
	if err != nil {
		return err
	}

	// Verify MD5
	actualMD5 := hex.EncodeToString(hash.Sum(nil))
	if actualMD5 != expectedMD5 {
		return fmt.Errorf("MD5 mismatch for %s: expected %s, got %s", filename, expectedMD5, actualMD5)
	}

	logger.Info("Updated %s with %s (%d bytes)", partition, filename, written)
	return nil
}

// downloadFile downloads a file from URL.
func (m *UpgradeManager) downloadFile(url, destPath string) error {
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("HTTP error: %d", resp.StatusCode)
	}

	out, err := os.Create(destPath)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, resp.Body)
	return err
}

// downloadFileWithProgress downloads a file with progress tracking.
func (m *UpgradeManager) downloadFileWithProgress(url, destPath string, progressStart, progressEnd int) error {
	resp, err := http.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("HTTP error: %d", resp.StatusCode)
	}

	totalSize := resp.ContentLength
	out, err := os.Create(destPath)
	if err != nil {
		return err
	}
	defer out.Close()

	var downloaded int64
	buf := make([]byte, 32*1024)
	for {
		if m.cancelled {
			return fmt.Errorf("cancelled")
		}

		n, err := resp.Body.Read(buf)
		if n > 0 {
			out.Write(buf[:n])
			downloaded += int64(n)

			if totalSize > 0 {
				pct := progressStart + int(float64(downloaded)/float64(totalSize)*float64(progressEnd-progressStart))
				m.updateProgress(pct, "download")
			}
		}
		if err == io.EOF {
			break
		}
		if err != nil {
			return err
		}
	}

	return nil
}

// verifyMD5 verifies the MD5 checksum of a file.
func (m *UpgradeManager) verifyMD5(filePath, expectedMD5 string) error {
	file, err := os.Open(filePath)
	if err != nil {
		return err
	}
	defer file.Close()

	hash := md5.New()
	if _, err := io.Copy(hash, file); err != nil {
		return err
	}

	actualMD5 := hex.EncodeToString(hash.Sum(nil))
	if actualMD5 != expectedMD5 {
		return fmt.Errorf("MD5 mismatch: expected %s, got %s", expectedMD5, actualMD5)
	}

	return nil
}

// updateProgress updates the progress file.
func (m *UpgradeManager) updateProgress(progress int, status string) {
	data, _ := json.Marshal(UpdateProgress{Progress: progress, Status: status})
	os.WriteFile(UpgradeProgFile, data, 0644)
}

// CancelUpdate cancels an ongoing update.
func (m *UpgradeManager) CancelUpdate() error {
	m.cancelled = true
	os.WriteFile(UpgradeCancelFile, []byte("1"), 0644)
	os.Remove(UpgradeProgFile)
	return nil
}

// copyFile copies a file from src to dst.
func copyFile(src, dst string) error {
	sourceFile, err := os.Open(src)
	if err != nil {
		return err
	}
	defer sourceFile.Close()

	destFile, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer destFile.Close()

	_, err = io.Copy(destFile, sourceFile)
	return err
}

// Recovery sets the factory reset flag.
func (m *UpgradeManager) Recovery() error {
	return exec.Command("fw_setenv", "factory_reset", "1").Run()
}
