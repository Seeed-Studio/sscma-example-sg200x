// Package logger provides structured logging for the supervisor.
package logger

import (
	"fmt"
	"io"
	"log"
	"os"
	"runtime"
	"strings"
	"sync"
	"time"
)

// Level represents a log level.
type Level int

const (
	LevelError Level = iota
	LevelWarning
	LevelInfo
	LevelDebug
	LevelVerbose
)

// String returns the string representation of the log level.
func (l Level) String() string {
	switch l {
	case LevelError:
		return "ERROR"
	case LevelWarning:
		return "WARN"
	case LevelInfo:
		return "INFO"
	case LevelDebug:
		return "DEBUG"
	case LevelVerbose:
		return "VERBOSE"
	default:
		return "UNKNOWN"
	}
}

// Logger provides structured logging.
type Logger struct {
	mu        sync.Mutex
	name      string
	level     Level
	output    io.Writer
	logger    *log.Logger
	useSyslog bool
}

var (
	defaultLogger *Logger
	once          sync.Once
)

// New creates a new logger instance.
func New(name string, level Level) *Logger {
	l := &Logger{
		name:   name,
		level:  level,
		output: os.Stderr,
	}
	l.logger = log.New(l.output, "", 0)
	return l
}

// Default returns the default logger instance.
func Default() *Logger {
	once.Do(func() {
		defaultLogger = New("supervisor", LevelWarning)
	})
	return defaultLogger
}

// SetLevel sets the log level.
func (l *Logger) SetLevel(level Level) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.level = level
}

// SetOutput sets the output writer.
func (l *Logger) SetOutput(w io.Writer) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.output = w
	l.logger = log.New(w, "", 0)
}

// log writes a log message at the specified level.
func (l *Logger) log(level Level, format string, args ...interface{}) {
	if level > l.level {
		return
	}

	l.mu.Lock()
	defer l.mu.Unlock()

	// Get caller information
	_, file, line, ok := runtime.Caller(2)
	if ok {
		// Extract just the filename
		if idx := strings.LastIndex(file, "/"); idx >= 0 {
			file = file[idx+1:]
		}
	} else {
		file = "???"
		line = 0
	}

	timestamp := time.Now().Format("2006-01-02 15:04:05.000")
	msg := fmt.Sprintf(format, args...)

	l.logger.Printf("[%s] [%s] [%s:%d] %s", timestamp, level.String(), file, line, msg)
}

// Error logs an error message.
func (l *Logger) Error(format string, args ...interface{}) {
	l.log(LevelError, format, args...)
}

// Warning logs a warning message.
func (l *Logger) Warning(format string, args ...interface{}) {
	l.log(LevelWarning, format, args...)
}

// Warn is an alias for Warning.
func (l *Logger) Warn(format string, args ...interface{}) {
	l.Warning(format, args...)
}

// Info logs an info message.
func (l *Logger) Info(format string, args ...interface{}) {
	l.log(LevelInfo, format, args...)
}

// Debug logs a debug message.
func (l *Logger) Debug(format string, args ...interface{}) {
	l.log(LevelDebug, format, args...)
}

// Verbose logs a verbose message.
func (l *Logger) Verbose(format string, args ...interface{}) {
	l.log(LevelVerbose, format, args...)
}

// Package-level convenience functions

// SetLevel sets the default logger level.
func SetLevel(level Level) {
	Default().SetLevel(level)
}

// Error logs an error message using the default logger.
func Error(format string, args ...interface{}) {
	Default().Error(format, args...)
}

// Warning logs a warning message using the default logger.
func Warning(format string, args ...interface{}) {
	Default().Warning(format, args...)
}

// Warn is an alias for Warning.
func Warn(format string, args ...interface{}) {
	Warning(format, args...)
}

// Info logs an info message using the default logger.
func Info(format string, args ...interface{}) {
	Default().Info(format, args...)
}

// Debug logs a debug message using the default logger.
func Debug(format string, args ...interface{}) {
	Default().Debug(format, args...)
}

// Verbose logs a verbose message using the default logger.
func Verbose(format string, args ...interface{}) {
	Default().Verbose(format, args...)
}
