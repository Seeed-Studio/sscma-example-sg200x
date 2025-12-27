// Package server provides the HTTP server for the supervisor.
package server

import (
	"context"
	"fmt"
	"net/http"
	"path/filepath"
	"strings"
	"time"

	"supervisor/internal/api"
	"supervisor/internal/auth"
	"supervisor/internal/config"
	"supervisor/internal/handler"
	"supervisor/internal/middleware"
	"supervisor/internal/system"
	"supervisor/internal/tls"
	"supervisor/pkg/logger"
)

// Server represents the HTTP server.
type Server struct {
	cfg         *config.Config
	httpServer  *http.Server
	httpsServer *http.Server
	authManager *auth.AuthManager
	wifiHandler *handler.WiFiHandler
	tlsManager  *tls.Manager
}

// New creates a new Server.
func New(cfg *config.Config) *Server {
	return &Server{
		cfg:         cfg,
		authManager: auth.NewAuthManager(cfg),
		tlsManager:  tls.NewManager(cfg.CertDir),
	}
}

// Start starts the HTTP server.
func (s *Server) Start() error {
	// Ensure TLS certificates exist
	if err := s.tlsManager.EnsureCertificates(); err != nil {
		return fmt.Errorf("failed to ensure TLS certificates: %w", err)
	}

	// Get TLS configuration
	tlsConfig, err := s.tlsManager.GetTLSConfig()
	if err != nil {
		return fmt.Errorf("failed to get TLS config: %w", err)
	}

	mux := s.setupRoutes()

	// Apply middleware
	handler := middleware.Chain(
		mux,
		middleware.Recovery,
		middleware.SecureHeaders,
		middleware.Logging,
		middleware.CORS,
	)

	// HTTP server - redirects all traffic to HTTPS
	if s.cfg.HTTPPort != "" {
		s.httpServer = &http.Server{
			Addr:         ":" + s.cfg.HTTPPort,
			Handler:      s.httpsRedirectHandler(),
			ReadTimeout:  30 * time.Second,
			WriteTimeout: 30 * time.Second,
			IdleTimeout:  120 * time.Second,
		}

		go func() {
			logger.Info("HTTP redirect server starting on port %s (redirecting to HTTPS)", s.cfg.HTTPPort)
			if err := s.httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
				logger.Error("HTTP server error: %v", err)
			}
		}()
	}

	// HTTPS server (required)
	s.httpsServer = &http.Server{
		Addr:         ":" + s.cfg.HTTPSPort,
		Handler:      handler,
		TLSConfig:    tlsConfig,
		ReadTimeout:  30 * time.Second,
		WriteTimeout: 30 * time.Second,
		IdleTimeout:  120 * time.Second,
	}

	go func() {
		logger.Info("HTTPS server starting on port %s", s.cfg.HTTPSPort)
		if err := s.httpsServer.ListenAndServeTLS(s.tlsManager.CertFile(), s.tlsManager.KeyFile()); err != nil && err != http.ErrServerClosed {
			logger.Error("HTTPS server error: %v", err)
		}
	}()

	return nil
}

// httpsRedirectHandler returns a handler that redirects all HTTP requests to HTTPS.
func (s *Server) httpsRedirectHandler() http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		// Build the HTTPS URL
		host := r.Host
		// Remove port if present
		if idx := strings.Index(host, ":"); idx != -1 {
			host = host[:idx]
		}

		// Add HTTPS port if not default
		var targetURL string
		if s.cfg.HTTPSPort == "443" {
			targetURL = fmt.Sprintf("https://%s%s", host, r.RequestURI)
		} else {
			targetURL = fmt.Sprintf("https://%s:%s%s", host, s.cfg.HTTPSPort, r.RequestURI)
		}

		http.Redirect(w, r, targetURL, http.StatusMovedPermanently)
	})
}

// Stop gracefully stops the server.
func (s *Server) Stop(ctx context.Context) error {
	// Stop WiFi handler
	if s.wifiHandler != nil {
		s.wifiHandler.Stop()
	}

	// Shutdown HTTP server
	if s.httpServer != nil {
		if err := s.httpServer.Shutdown(ctx); err != nil {
			logger.Error("HTTP server shutdown error: %v", err)
		}
	}

	// Shutdown HTTPS server
	if s.httpsServer != nil {
		if err := s.httpsServer.Shutdown(ctx); err != nil {
			logger.Error("HTTPS server shutdown error: %v", err)
		}
	}

	return nil
}

// setupRoutes configures the HTTP routes.
func (s *Server) setupRoutes() http.Handler {
	mux := http.NewServeMux()

	// Create handlers
	userHandler := handler.NewUserHandler(s.authManager)
	deviceHandler := handler.NewDeviceHandler()
	s.wifiHandler = handler.NewWiFiHandler()
	fileHandler := handler.NewFileHandler()
	ledHandler := handler.NewLEDHandler()

	// Paths that don't require authentication
	// Only include endpoints needed before login
	noAuthPaths := map[string]bool{
		"/api/version":                      true,
		"/api/userMgr/login":                true,
		"/api/userMgr/queryUserInfo":        true, // Needed to check firstLogin status
		"/api/userMgr/updatePassword":       true, // Needed for first login password change
		"/api/deviceMgr/queryDeviceInfo":    true, // Needed for App init (gets SN)
		"/api/deviceMgr/queryServiceStatus": true, // Needed for loading screen
	}

	// Auth middleware
	authMiddleware := middleware.Auth(s.authManager, noAuthPaths)

	// API routes - wrap with auth middleware
	apiHandler := http.NewServeMux()

	// Version endpoint
	apiHandler.HandleFunc("/api/version", s.handleVersion)

	// User management
	apiHandler.HandleFunc("/api/userMgr/login", userHandler.Login)
	apiHandler.HandleFunc("/api/userMgr/queryUserInfo", userHandler.QueryUserInfo)
	apiHandler.HandleFunc("/api/userMgr/updatePassword", userHandler.UpdatePassword)
	apiHandler.HandleFunc("/api/userMgr/setSShStatus", userHandler.SetSSHStatus)
	apiHandler.HandleFunc("/api/userMgr/addSShkey", userHandler.AddSSHKey)
	apiHandler.HandleFunc("/api/userMgr/deleteSShkey", userHandler.DeleteSSHKey)

	// Device management
	apiHandler.HandleFunc("/api/deviceMgr/queryDeviceInfo", deviceHandler.QueryDeviceInfo)
	apiHandler.HandleFunc("/api/deviceMgr/getDeviceInfo", deviceHandler.GetDeviceInfo)
	apiHandler.HandleFunc("/api/deviceMgr/getDeviceList", deviceHandler.GetDeviceList)
	apiHandler.HandleFunc("/api/deviceMgr/updateDeviceName", deviceHandler.UpdateDeviceName)
	apiHandler.HandleFunc("/api/deviceMgr/getCameraWebsocketUrl", deviceHandler.GetCameraWebsocketUrl)
	apiHandler.HandleFunc("/api/deviceMgr/queryServiceStatus", deviceHandler.QueryServiceStatus)
	apiHandler.HandleFunc("/api/deviceMgr/getSystemStatus", deviceHandler.GetSystemStatus)
	apiHandler.HandleFunc("/api/deviceMgr/setPower", deviceHandler.SetPower)
	apiHandler.HandleFunc("/api/deviceMgr/getModelList", deviceHandler.GetModelList)
	apiHandler.HandleFunc("/api/deviceMgr/getModelInfo", deviceHandler.GetModelInfo)
	apiHandler.HandleFunc("/api/deviceMgr/getModelFile", deviceHandler.GetModelFile)
	apiHandler.HandleFunc("/api/deviceMgr/uploadModel", deviceHandler.UploadModel)
	apiHandler.HandleFunc("/api/deviceMgr/setTimestamp", deviceHandler.SetTimestamp)
	apiHandler.HandleFunc("/api/deviceMgr/getTimestamp", deviceHandler.GetTimestamp)
	apiHandler.HandleFunc("/api/deviceMgr/setTimezone", deviceHandler.SetTimezone)
	apiHandler.HandleFunc("/api/deviceMgr/getTimezone", deviceHandler.GetTimezone)
	apiHandler.HandleFunc("/api/deviceMgr/getTimezoneList", deviceHandler.GetTimezoneList)
	apiHandler.HandleFunc("/api/deviceMgr/updateChannel", deviceHandler.UpdateChannel)
	apiHandler.HandleFunc("/api/deviceMgr/getSystemUpdateVersion", deviceHandler.GetSystemUpdateVersion)
	apiHandler.HandleFunc("/api/deviceMgr/updateSystem", deviceHandler.UpdateSystem)
	apiHandler.HandleFunc("/api/deviceMgr/getUpdateProgress", deviceHandler.GetUpdateProgress)
	apiHandler.HandleFunc("/api/deviceMgr/cancelUpdate", deviceHandler.CancelUpdate)
	apiHandler.HandleFunc("/api/deviceMgr/factoryReset", deviceHandler.FactoryReset)
	apiHandler.HandleFunc("/api/deviceMgr/getPlatformInfo", deviceHandler.GetPlatformInfo)
	apiHandler.HandleFunc("/api/deviceMgr/savePlatformInfo", deviceHandler.SavePlatformInfo)

	// WiFi management
	apiHandler.HandleFunc("/api/wifiMgr/getWiFiInfoList", s.wifiHandler.GetWiFiInfoList)
	apiHandler.HandleFunc("/api/wifiMgr/connectWiFi", s.wifiHandler.ConnectWiFi)
	apiHandler.HandleFunc("/api/wifiMgr/disconnectWiFi", s.wifiHandler.DisconnectWiFi)
	apiHandler.HandleFunc("/api/wifiMgr/forgetWiFi", s.wifiHandler.ForgetWiFi)
	apiHandler.HandleFunc("/api/wifiMgr/switchWiFi", s.wifiHandler.SwitchWiFi)

	// File management
	apiHandler.HandleFunc("/api/fileMgr/list", fileHandler.List)
	apiHandler.HandleFunc("/api/fileMgr/mkdir", fileHandler.Mkdir)
	apiHandler.HandleFunc("/api/fileMgr/remove", fileHandler.Remove)
	apiHandler.HandleFunc("/api/fileMgr/upload", fileHandler.Upload)
	apiHandler.HandleFunc("/api/fileMgr/download", fileHandler.Download)
	apiHandler.HandleFunc("/api/fileMgr/rename", fileHandler.Rename)
	apiHandler.HandleFunc("/api/fileMgr/info", fileHandler.Info)

	// LED management
	apiHandler.HandleFunc("/api/ledMgr/getLEDs", ledHandler.GetLEDs)
	apiHandler.HandleFunc("/api/ledMgr/getLED", ledHandler.GetLED)
	apiHandler.HandleFunc("/api/ledMgr/setLED", ledHandler.SetLED)
	apiHandler.HandleFunc("/api/ledMgr/getLEDTriggers", ledHandler.GetLEDTriggers)

	// Apply auth middleware to API routes
	mux.Handle("/api/", authMiddleware(apiHandler))

	// Static file server for web UI
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		path := r.URL.Path

		// Serve static files
		filePath := filepath.Join(s.cfg.RootDir, path)
		if path == "/" {
			filePath = filepath.Join(s.cfg.RootDir, "index.html")
		}

		http.ServeFile(w, r, filePath)
	})

	return mux
}

// handleVersion handles the version endpoint.
func (s *Server) handleVersion(w http.ResponseWriter, r *http.Request) {
	api.WriteSuccess(w, map[string]interface{}{
		"uptime":    system.GetUptime(),
		"timestamp": time.Now().Unix(),
		"version":   "1.0.0", // Will be set at build time
	})
}
