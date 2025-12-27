// Package tls provides TLS certificate management for the supervisor.
package tls

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"math/big"
	"net"
	"os"
	"path/filepath"
	"time"

	"supervisor/pkg/logger"
)

const (
	// DefaultCertDir is the default directory for storing certificates
	DefaultCertDir = "/etc/recamera.conf/certs"

	// CertFileName is the certificate file name
	CertFileName = "server.crt"

	// KeyFileName is the private key file name
	KeyFileName = "server.key"

	// CertValidityDays is the number of days the certificate is valid
	CertValidityDays = 3650 // 10 years
)

// Manager handles TLS certificate operations.
type Manager struct {
	certDir  string
	certFile string
	keyFile  string
}

// NewManager creates a new TLS manager.
func NewManager(certDir string) *Manager {
	if certDir == "" {
		certDir = DefaultCertDir
	}
	return &Manager{
		certDir:  certDir,
		certFile: filepath.Join(certDir, CertFileName),
		keyFile:  filepath.Join(certDir, KeyFileName),
	}
}

// EnsureCertificates ensures that valid certificates exist, generating them if necessary.
func (m *Manager) EnsureCertificates() error {
	// Check if certificates already exist and are valid
	if m.certificatesExist() {
		if m.certificatesValid() {
			logger.Info("Using existing TLS certificates from %s", m.certDir)
			return nil
		}
		logger.Warning("Existing certificates are invalid or expired, regenerating...")
	}

	// Create certificate directory if it doesn't exist
	if err := os.MkdirAll(m.certDir, 0700); err != nil {
		return fmt.Errorf("failed to create certificate directory: %w", err)
	}

	// Generate new certificates
	logger.Info("Generating new self-signed TLS certificates...")
	return m.generateCertificates()
}

// certificatesExist checks if certificate files exist.
func (m *Manager) certificatesExist() bool {
	_, certErr := os.Stat(m.certFile)
	_, keyErr := os.Stat(m.keyFile)
	return certErr == nil && keyErr == nil
}

// certificatesValid checks if the existing certificates are valid.
func (m *Manager) certificatesValid() bool {
	// Load the certificate
	certPEM, err := os.ReadFile(m.certFile)
	if err != nil {
		return false
	}

	block, _ := pem.Decode(certPEM)
	if block == nil {
		return false
	}

	cert, err := x509.ParseCertificate(block.Bytes)
	if err != nil {
		return false
	}

	// Check if certificate is not expired (with 30 day buffer)
	return time.Now().Add(30 * 24 * time.Hour).Before(cert.NotAfter)
}

// generateCertificates generates a new self-signed certificate and private key.
func (m *Manager) generateCertificates() error {
	// Generate private key using ECDSA P-384 (stronger than P-256)
	privateKey, err := ecdsa.GenerateKey(elliptic.P384(), rand.Reader)
	if err != nil {
		return fmt.Errorf("failed to generate private key: %w", err)
	}

	// Generate serial number
	serialNumber, err := rand.Int(rand.Reader, new(big.Int).Lsh(big.NewInt(1), 128))
	if err != nil {
		return fmt.Errorf("failed to generate serial number: %w", err)
	}

	// Get all local IP addresses for SANs
	ipAddresses := getLocalIPAddresses()

	// Certificate template with strong signature algorithm (SHA-384)
	template := x509.Certificate{
		SerialNumber: serialNumber,
		Subject: pkix.Name{
			Organization:  []string{"reCamera"},
			Country:       []string{"US"},
			Province:      []string{""},
			Locality:      []string{""},
			StreetAddress: []string{""},
			PostalCode:    []string{""},
			CommonName:    "reCamera Supervisor",
		},
		NotBefore:             time.Now(),
		NotAfter:              time.Now().AddDate(0, 0, CertValidityDays),
		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		BasicConstraintsValid: true,
		IsCA:                  false,

		// Use SHA-384 for signature (stronger than SHA-256)
		SignatureAlgorithm: x509.ECDSAWithSHA384,

		// Subject Alternative Names
		DNSNames:    []string{"localhost", "recamera", "recamera.local"},
		IPAddresses: ipAddresses,
	}

	// Create self-signed certificate
	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, &privateKey.PublicKey, privateKey)
	if err != nil {
		return fmt.Errorf("failed to create certificate: %w", err)
	}

	// Write certificate to file
	certOut, err := os.OpenFile(m.certFile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if err != nil {
		return fmt.Errorf("failed to open cert file for writing: %w", err)
	}
	defer certOut.Close()

	if err := pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: derBytes}); err != nil {
		return fmt.Errorf("failed to write certificate: %w", err)
	}

	// Write private key to file
	keyOut, err := os.OpenFile(m.keyFile, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		return fmt.Errorf("failed to open key file for writing: %w", err)
	}
	defer keyOut.Close()

	privateKeyBytes, err := x509.MarshalECPrivateKey(privateKey)
	if err != nil {
		return fmt.Errorf("failed to marshal private key: %w", err)
	}

	if err := pem.Encode(keyOut, &pem.Block{Type: "EC PRIVATE KEY", Bytes: privateKeyBytes}); err != nil {
		return fmt.Errorf("failed to write private key: %w", err)
	}

	logger.Info("TLS certificates generated successfully (ECDSA P-384 + SHA-384)")
	logger.Info("  Certificate: %s", m.certFile)
	logger.Info("  Private Key: %s", m.keyFile)

	return nil
}

// GetTLSConfig returns a secure TLS configuration for the server.
// Security features:
// - TLS 1.3 preferred (falls back to TLS 1.2 minimum)
// - Strong ECDHE cipher suites with AEAD (GCM/ChaCha20-Poly1305)
// - Secure curve preferences (X25519, P-384, P-256)
// - Session tickets disabled (prevents forward secrecy bypass)
// - Renegotiation disabled (prevents security issues)
func (m *Manager) GetTLSConfig() (*tls.Config, error) {
	cert, err := tls.LoadX509KeyPair(m.certFile, m.keyFile)
	if err != nil {
		return nil, fmt.Errorf("failed to load TLS certificates: %w", err)
	}

	return &tls.Config{
		Certificates: []tls.Certificate{cert},

		// Minimum TLS 1.2, prefer TLS 1.3
		MinVersion: tls.VersionTLS12,
		MaxVersion: tls.VersionTLS13,

		// Prefer server cipher suite order
		PreferServerCipherSuites: true,

		// Strong cipher suites only (TLS 1.2)
		// TLS 1.3 cipher suites are not configurable and are always secure
		CipherSuites: []uint16{
			// TLS 1.3 cipher suites (automatically used when TLS 1.3 is negotiated)
			// TLS_AES_256_GCM_SHA384
			// TLS_CHACHA20_POLY1305_SHA256
			// TLS_AES_128_GCM_SHA256

			// TLS 1.2 cipher suites (ordered by preference)
			tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
			tls.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
			tls.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
			tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
			tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
			tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		},

		// Secure curve preferences
		CurvePreferences: []tls.CurveID{
			tls.X25519,    // Modern, fast, constant-time
			tls.CurveP384, // NIST P-384
			tls.CurveP256, // NIST P-256
		},

		// Disable session tickets (ensures forward secrecy)
		SessionTicketsDisabled: true,

		// Disable renegotiation (prevents DoS and other attacks)
		Renegotiation: tls.RenegotiateNever,
	}, nil
}

// CertFile returns the path to the certificate file.
func (m *Manager) CertFile() string {
	return m.certFile
}

// KeyFile returns the path to the private key file.
func (m *Manager) KeyFile() string {
	return m.keyFile
}

// getLocalIPAddresses returns all local IP addresses for certificate SANs.
func getLocalIPAddresses() []net.IP {
	var ips []net.IP

	// Always include loopback
	ips = append(ips, net.ParseIP("127.0.0.1"))
	ips = append(ips, net.ParseIP("::1"))

	// Always include the AP mode IP
	ips = append(ips, net.ParseIP("192.168.16.1"))

	// Get all network interfaces
	interfaces, err := net.Interfaces()
	if err != nil {
		logger.Warning("Failed to get network interfaces: %v", err)
		return ips
	}

	for _, iface := range interfaces {
		// Skip loopback and down interfaces
		if iface.Flags&net.FlagLoopback != 0 || iface.Flags&net.FlagUp == 0 {
			continue
		}

		addrs, err := iface.Addrs()
		if err != nil {
			continue
		}

		for _, addr := range addrs {
			var ip net.IP
			switch v := addr.(type) {
			case *net.IPNet:
				ip = v.IP
			case *net.IPAddr:
				ip = v.IP
			}

			if ip != nil && !ip.IsLoopback() {
				ips = append(ips, ip)
			}
		}
	}

	return ips
}
