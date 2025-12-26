// Package system provides low-level system utilities for network, device, and process management.
package system

import (
	"bufio"
	"net"
	"os"
	"strings"
)

// GetIP returns the IPv4 address of the specified network interface.
func GetIP(ifname string) string {
	iface, err := net.InterfaceByName(ifname)
	if err != nil {
		return ""
	}
	addrs, err := iface.Addrs()
	if err != nil || len(addrs) == 0 {
		return ""
	}
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && ipnet.IP.To4() != nil {
			return ipnet.IP.String()
		}
	}
	return ""
}

// GetMask returns the subnet mask of the specified network interface.
func GetMask(ifname string) string {
	iface, err := net.InterfaceByName(ifname)
	if err != nil {
		return ""
	}
	addrs, err := iface.Addrs()
	if err != nil || len(addrs) == 0 {
		return ""
	}
	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && ipnet.IP.To4() != nil {
			mask := ipnet.Mask
			if len(mask) == 4 {
				return net.IP(mask).String()
			}
		}
	}
	return ""
}

// GetMAC returns the MAC address of the specified network interface.
func GetMAC(ifname string) string {
	iface, err := net.InterfaceByName(ifname)
	if err != nil {
		return ""
	}
	if len(iface.HardwareAddr) == 0 {
		return ""
	}
	return iface.HardwareAddr.String()
}

// GetGateway returns the default gateway for the specified network interface.
// It parses /proc/net/route to find the gateway.
func GetGateway(ifname string) string {
	file, err := os.Open("/proc/net/route")
	if err != nil {
		return ""
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	// Skip header line
	scanner.Scan()

	for scanner.Scan() {
		fields := strings.Fields(scanner.Text())
		if len(fields) < 3 {
			continue
		}
		// Fields: Iface Destination Gateway Flags RefCnt Use Metric Mask MTU Window IRTT
		if fields[0] == ifname && fields[1] == "00000000" {
			// Gateway is in hex, little-endian format
			gateway := parseHexIP(fields[2])
			if gateway != "" {
				return gateway
			}
		}
	}
	return ""
}

// parseHexIP converts a hex-encoded IP address (little-endian) to dotted decimal.
func parseHexIP(hex string) string {
	if len(hex) != 8 {
		return ""
	}
	// Parse hex bytes (little-endian)
	var ip [4]byte
	for i := 0; i < 4; i++ {
		var b byte
		for j := 0; j < 2; j++ {
			c := hex[i*2+j]
			var v byte
			if c >= '0' && c <= '9' {
				v = c - '0'
			} else if c >= 'A' && c <= 'F' {
				v = c - 'A' + 10
			} else if c >= 'a' && c <= 'f' {
				v = c - 'a' + 10
			}
			b = b*16 + v
		}
		ip[3-i] = b // Reverse for little-endian
	}
	return net.IPv4(ip[0], ip[1], ip[2], ip[3]).String()
}

// GetDNS returns the primary DNS server from /etc/resolv.conf.
func GetDNS() string {
	return getDNSServer(0)
}

// GetDNS2 returns the secondary DNS server from /etc/resolv.conf.
func GetDNS2() string {
	return getDNSServer(1)
}

// getDNSServer returns the DNS server at the specified index from /etc/resolv.conf.
func getDNSServer(index int) string {
	file, err := os.Open("/etc/resolv.conf")
	if err != nil {
		return ""
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	count := 0
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if strings.HasPrefix(line, "nameserver") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				if count == index {
					return fields[1]
				}
				count++
			}
		}
	}
	return ""
}

// InterfaceInfo contains network interface information.
type InterfaceInfo struct {
	IP         string `json:"ip"`
	SubnetMask string `json:"subnetMask"`
	MACAddress string `json:"macAddress"`
	Gateway    string `json:"gateway"`
	DNS1       string `json:"dns1"`
	DNS2       string `json:"dns2"`
}

// GetInterfaceInfo returns comprehensive information about a network interface.
func GetInterfaceInfo(ifname string) *InterfaceInfo {
	return &InterfaceInfo{
		IP:         GetIP(ifname),
		SubnetMask: GetMask(ifname),
		MACAddress: GetMAC(ifname),
		Gateway:    GetGateway(ifname),
		DNS1:       GetDNS(),
		DNS2:       GetDNS2(),
	}
}

// InterfaceExists checks if a network interface exists.
func InterfaceExists(ifname string) bool {
	_, err := net.InterfaceByName(ifname)
	return err == nil
}
