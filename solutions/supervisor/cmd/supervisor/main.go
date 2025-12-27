// Package main provides the entry point for the supervisor.
package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"golang.org/x/sys/unix"

	"supervisor/internal/config"
	"supervisor/internal/server"
	"supervisor/pkg/logger"
)

// Build-time variables
var (
	Version   = "dev"
	BuildTime = "unknown"
	GitCommit = "unknown"
)

func main() {
	// Parse command-line flags
	var (
		showVersion = flag.Bool("version", false, "Show version information")
		daemon      = flag.Bool("d", false, "Run as daemon")
		httpPort    = flag.String("p", "", "HTTP port for redirect (default: 80)")
		httpsPort   = flag.String("P", "", "HTTPS port (default: 443)")
		rootDir     = flag.String("r", "", "Web root directory")
		scriptPath  = flag.String("s", "", "Script path")
		certDir     = flag.String("c", "", "TLS certificate directory")
		noAuth      = flag.Bool("n", false, "Disable authentication")
		logLevel    = flag.Int("v", -1, "Log level (0-4)")
	)
	flag.Parse()

	// Show version
	if *showVersion {
		fmt.Printf("Supervisor version %s\n", Version)
		fmt.Printf("Build time: %s\n", BuildTime)
		fmt.Printf("Git commit: %s\n", GitCommit)
		os.Exit(0)
	}

	// Get configuration
	cfg := config.Get()

	// Apply command-line overrides
	if *httpPort != "" {
		cfg.HTTPPort = *httpPort
	}
	if *httpsPort != "" {
		cfg.HTTPSPort = *httpsPort
	}
	if *rootDir != "" {
		cfg.RootDir = *rootDir
	}
	if *scriptPath != "" {
		cfg.ScriptPath = *scriptPath
	}
	if *certDir != "" {
		cfg.CertDir = *certDir
	}
	if *noAuth {
		cfg.NoAuth = true
	}
	if *logLevel >= 0 {
		cfg.LogLevel = *logLevel
	}
	if *daemon {
		cfg.DaemonMode = true
	}

	// Initialize logger with appropriate level
	logger.SetLevel(logger.Level(cfg.LogLevel))

	// Daemonize if requested
	if cfg.DaemonMode {
		if err := daemonize(); err != nil {
			logger.Error("Failed to daemonize: %v", err)
			os.Exit(1)
		}
	}

	// Create and start server
	srv := server.New(cfg)
	if err := srv.Start(); err != nil {
		logger.Error("Failed to start server: %v", err)
		os.Exit(1)
	}

	logger.Info("Supervisor started (version %s)", Version)

	// Wait for shutdown signal
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("Shutting down...")

	// Graceful shutdown with timeout
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	if err := srv.Stop(ctx); err != nil {
		logger.Error("Error during shutdown: %v", err)
	}

	logger.Info("Supervisor stopped")
}

// daemonize forks the process and redirects stdout/stderr to /dev/null.
// Uses unix.Dup2 for riscv64 compatibility (syscall.Dup2 is not available on riscv64).
func daemonize() error {
	// Fork
	pid, err := syscall.ForkExec(os.Args[0], os.Args, &syscall.ProcAttr{
		Dir: "/",
		Env: os.Environ(),
		Sys: &syscall.SysProcAttr{
			Setsid: true,
		},
		Files: []uintptr{0, 1, 2},
	})
	if err != nil {
		return fmt.Errorf("fork failed: %w", err)
	}

	if pid > 0 {
		// Parent exits
		os.Exit(0)
	}

	// Child continues
	// Redirect stdin, stdout, stderr to /dev/null
	nullFile, err := os.OpenFile("/dev/null", os.O_RDWR, 0)
	if err != nil {
		return fmt.Errorf("failed to open /dev/null: %w", err)
	}

	// Use unix.Dup2 instead of syscall.Dup2 for riscv64 compatibility
	unix.Dup2(int(nullFile.Fd()), int(os.Stdin.Fd()))
	unix.Dup2(int(nullFile.Fd()), int(os.Stdout.Fd()))
	unix.Dup2(int(nullFile.Fd()), int(os.Stderr.Fd()))

	return nil
}
