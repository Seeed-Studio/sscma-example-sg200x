package system

import (
	"os"
	"os/exec"
	"strconv"
	"strings"
	"syscall"
)

// StopProcessByName stops all processes with the given name.
// signal: 9 (SIGKILL) for immediate termination, 15 (SIGTERM) for graceful shutdown
func StopProcessByName(name string, signal int) error {
	pids, err := FindProcessByName(name)
	if err != nil {
		return err
	}

	for _, pid := range pids {
		process, err := os.FindProcess(pid)
		if err != nil {
			continue
		}
		process.Signal(syscall.Signal(signal))
	}
	return nil
}

// FindProcessByName returns PIDs of all processes with the given name.
func FindProcessByName(name string) ([]int, error) {
	cmd := exec.Command("pidof", name)
	out, err := cmd.Output()
	if err != nil {
		// pidof returns error if no process found
		return nil, nil
	}

	var pids []int
	for _, pidStr := range strings.Fields(string(out)) {
		pid, err := strconv.Atoi(pidStr)
		if err == nil {
			pids = append(pids, pid)
		}
	}
	return pids, nil
}

// IsProcessRunning checks if a process with the given name is running.
func IsProcessRunning(name string) bool {
	pids, _ := FindProcessByName(name)
	return len(pids) > 0
}

// RunService executes a service init script with the given action.
func RunService(service, action string) error {
	return exec.Command("/etc/init.d/"+service, action).Run()
}

// RunServiceAsync executes a service init script asynchronously.
func RunServiceAsync(service, action string) {
	go exec.Command("/etc/init.d/"+service, action).Run()
}
