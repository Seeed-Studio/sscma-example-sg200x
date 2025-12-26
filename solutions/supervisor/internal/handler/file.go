package handler

import (
	"encoding/json"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"

	"supervisor/internal/api"
	"supervisor/internal/config"
)

// FileHandler handles file management API requests.
type FileHandler struct {
	localDir string
	sdDir    string
}

// NewFileHandler creates a new FileHandler.
func NewFileHandler() *FileHandler {
	cfg := config.Get()
	return &FileHandler{
		localDir: cfg.LocalDir,
		sdDir:    cfg.SDDir,
	}
}

// isSDAvailable checks if SD card is available.
func (h *FileHandler) isSDAvailable() bool {
	info, err := os.Stat(h.sdDir)
	return err == nil && info.IsDir()
}

// isValidStorage validates the storage parameter.
func (h *FileHandler) isValidStorage(storage string) bool {
	return storage == "" || storage == "local" || storage == "sd"
}

// decodePath URL-decodes a path.
func decodePath(path string) string {
	decoded, err := url.QueryUnescape(path)
	if err != nil {
		return path
	}
	return decoded
}

// isValidPath validates a path to prevent directory traversal and hidden files.
func isValidPath(path string) bool {
	if path == "" {
		return true
	}
	if path == "." || path == ".." {
		return false
	}
	if strings.Contains(path, "../") || strings.Contains(path, "..\\") {
		return false
	}

	// Check for hidden files or directories (starting with '.')
	cleanPath := strings.TrimPrefix(path, "/")
	for _, component := range strings.Split(cleanPath, "/") {
		if component != "" && strings.HasPrefix(component, ".") {
			return false
		}
	}
	return true
}

// isValidFilename validates a filename.
func isValidFilename(filename string) bool {
	if filename == "" || filename == "." || filename == ".." {
		return false
	}
	invalidChars := "<>:\"|?*\\"
	for _, c := range invalidChars {
		if strings.ContainsRune(filename, c) {
			return false
		}
	}
	return true
}

// getFullPath constructs the full path based on storage type.
func (h *FileHandler) getFullPath(relativePath, storage string) string {
	baseDir := h.localDir
	if storage == "sd" {
		baseDir = h.sdDir
	}
	if relativePath == "" {
		return baseDir
	}
	return filepath.Join(baseDir, relativePath)
}

// jsonBodyCache stores parsed JSON body for a request
type jsonBodyCache struct {
	mu    sync.RWMutex
	cache map[*http.Request]map[string]interface{}
}

var jsonCache = &jsonBodyCache{
	cache: make(map[*http.Request]map[string]interface{}),
}

// getJSONBody parses and caches the JSON body of a request
func getJSONBody(r *http.Request) map[string]interface{} {
	jsonCache.mu.RLock()
	if cached, ok := jsonCache.cache[r]; ok {
		jsonCache.mu.RUnlock()
		return cached
	}
	jsonCache.mu.RUnlock()

	// Check if content type is JSON
	contentType := r.Header.Get("Content-Type")
	if !strings.Contains(contentType, "application/json") {
		return nil
	}

	// Read and parse body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		return nil
	}
	r.Body.Close()

	var data map[string]interface{}
	if err := json.Unmarshal(body, &data); err != nil {
		return nil
	}

	// Cache the result
	jsonCache.mu.Lock()
	jsonCache.cache[r] = data
	jsonCache.mu.Unlock()

	return data
}

// clearJSONCache removes the cached JSON body for a request (call at end of handler)
func clearJSONCache(r *http.Request) {
	jsonCache.mu.Lock()
	delete(jsonCache.cache, r)
	jsonCache.mu.Unlock()
}

// getParam gets a parameter from URL query, form body, or JSON body.
func getParam(r *http.Request, name string) string {
	// Try URL query first
	val := r.URL.Query().Get(name)
	if val != "" {
		return val
	}

	// Try JSON body
	if jsonBody := getJSONBody(r); jsonBody != nil {
		if v, ok := jsonBody[name]; ok {
			switch vt := v.(type) {
			case string:
				return vt
			case float64:
				return strconv.FormatFloat(vt, 'f', -1, 64)
			case int:
				return strconv.Itoa(vt)
			case bool:
				return strconv.FormatBool(vt)
			}
		}
	}

	// For POST requests, try form value
	if r.Method == http.MethodPost {
		return r.FormValue(name)
	}
	return ""
}

// List lists directory contents.
func (h *FileHandler) List(w http.ResponseWriter, r *http.Request) {
	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter. Use 'local' or 'sd'.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	fullPath := h.getFullPath(path, effectiveStorage)

	info, err := os.Stat(fullPath)
	if err != nil {
		api.WriteError(w, -1, "Path does not exist.")
		return
	}

	if !info.IsDir() {
		api.WriteError(w, -1, "Path is not a directory.")
		return
	}

	data := map[string]interface{}{
		"path":        path,
		"storage":     effectiveStorage,
		"directories": []interface{}{},
		"files":       []interface{}{},
	}

	// Add storage info for root path
	if path == "" || path == "/" {
		var stat syscall.Statfs_t
		if err := syscall.Statfs(fullPath, &stat); err == nil {
			data["total"] = stat.Blocks * uint64(stat.Bsize)
			data["free"] = stat.Bavail * uint64(stat.Bsize)
			data["used"] = (stat.Blocks - stat.Bavail) * uint64(stat.Bsize)
		}
	}

	entries, err := os.ReadDir(fullPath)
	if err != nil {
		api.WriteError(w, -1, "Failed to read directory: "+err.Error())
		return
	}

	dirs := []map[string]interface{}{}
	files := []map[string]interface{}{}

	for _, entry := range entries {
		name := entry.Name()
		// Skip hidden files
		if strings.HasPrefix(name, ".") {
			continue
		}

		info, err := entry.Info()
		if err != nil {
			continue
		}

		item := map[string]interface{}{
			"name":     name,
			"modified": info.ModTime().Unix(),
		}

		if entry.IsDir() {
			item["type"] = "directory"
			item["size"] = 0
			dirs = append(dirs, item)
		} else {
			item["type"] = "file"
			item["size"] = info.Size()
			files = append(files, item)
		}
	}

	data["directories"] = dirs
	data["files"] = files

	api.WriteSuccessMessage(w, "Success", data)
}

// Mkdir creates a directory.
func (h *FileHandler) Mkdir(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if path == "" {
		api.WriteError(w, -1, "Path is empty.")
		return
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	fullPath := h.getFullPath(path, effectiveStorage)

	if _, err := os.Stat(fullPath); err == nil {
		api.WriteError(w, -1, "Path already exists.")
		return
	}

	if err := os.MkdirAll(fullPath, 0755); err != nil {
		api.WriteError(w, -1, "Failed to create directory: "+err.Error())
		return
	}

	api.WriteSuccessMessage(w, "Directory created.", map[string]interface{}{
		"path":    path,
		"storage": effectiveStorage,
	})
}

// Remove deletes a file or directory.
func (h *FileHandler) Remove(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if path == "" {
		api.WriteError(w, -1, "Path is empty.")
		return
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	fullPath := h.getFullPath(path, effectiveStorage)

	info, err := os.Stat(fullPath)
	if err != nil {
		api.WriteError(w, -1, "Path does not exist.")
		return
	}

	fileType := "file"
	if info.IsDir() {
		fileType = "directory"
		// Check if directory is empty
		entries, err := os.ReadDir(fullPath)
		if err != nil {
			api.WriteError(w, -1, "Failed to read directory.")
			return
		}
		if len(entries) > 0 {
			api.WriteError(w, -1, "Directory is not empty.")
			return
		}
	}

	if err := os.Remove(fullPath); err != nil {
		api.WriteError(w, -1, "Failed to remove "+fileType+": "+err.Error())
		return
	}

	api.WriteSuccessMessage(w, fileType+" removed.", map[string]interface{}{
		"path":    path,
		"type":    fileType,
		"storage": effectiveStorage,
	})
}

// Upload handles file uploads.
func (h *FileHandler) Upload(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")
	offsetStr := getParam(r, "offset")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if path != "" && !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	uploadDir := h.getFullPath(path, effectiveStorage)

	if path != "" {
		if info, err := os.Stat(uploadDir); err != nil || !info.IsDir() {
			api.WriteError(w, -1, "Upload directory does not exist.")
			return
		}
	}

	// Parse offset
	var offset int64
	if offsetStr != "" {
		var err error
		offset, err = strconv.ParseInt(offsetStr, 10, 64)
		if err != nil || offset < 0 {
			api.WriteError(w, -1, "Invalid offset parameter.")
			return
		}
	}

	// Parse multipart form (100MB max)
	if err := r.ParseMultipartForm(100 << 20); err != nil {
		api.WriteError(w, -1, "Failed to parse form: "+err.Error())
		return
	}

	results := []map[string]interface{}{}
	successCount := 0

	files := r.MultipartForm.File["file"]
	for _, fh := range files {
		if fh.Filename == "" {
			continue
		}

		if !isValidFilename(fh.Filename) {
			results = append(results, map[string]interface{}{
				"filename": fh.Filename,
				"status":   "failed",
				"message":  "Invalid filename",
			})
			continue
		}

		fullPath := filepath.Join(uploadDir, fh.Filename)

		// Open source file
		src, err := fh.Open()
		if err != nil {
			results = append(results, map[string]interface{}{
				"filename": fh.Filename,
				"status":   "failed",
				"message":  "Cannot open uploaded file",
			})
			continue
		}

		// Create/open destination file
		var dst *os.File
		if offset > 0 {
			dst, err = os.OpenFile(fullPath, os.O_RDWR|os.O_CREATE, 0644)
		} else {
			dst, err = os.Create(fullPath)
		}
		if err != nil {
			src.Close()
			results = append(results, map[string]interface{}{
				"filename": fh.Filename,
				"status":   "failed",
				"message":  "Cannot create file",
			})
			continue
		}

		if offset > 0 {
			if _, err := dst.Seek(offset, 0); err != nil {
				src.Close()
				dst.Close()
				results = append(results, map[string]interface{}{
					"filename": fh.Filename,
					"status":   "failed",
					"message":  "Cannot seek to offset",
				})
				continue
			}
		}

		written, err := io.Copy(dst, src)
		src.Close()
		dst.Close()

		if err != nil {
			results = append(results, map[string]interface{}{
				"filename": fh.Filename,
				"status":   "failed",
				"message":  "Upload error: " + err.Error(),
			})
			continue
		}

		results = append(results, map[string]interface{}{
			"filename": fh.Filename,
			"status":   "success",
			"size":     written,
			"offset":   offset,
			"storage":  effectiveStorage,
		})
		successCount++
	}

	data := map[string]interface{}{
		"uploads": results,
		"count":   successCount,
		"storage": effectiveStorage,
	}

	if successCount > 0 {
		api.WriteSuccessMessage(w, "Upload completed.", data)
	} else {
		api.WriteErrorWithData(w, -1, "Upload failed.", data)
	}
}

// Download serves a file for download.
func (h *FileHandler) Download(w http.ResponseWriter, r *http.Request) {
	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if path == "" {
		api.WriteError(w, -1, "Path is empty.")
		return
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	fullPath := h.getFullPath(path, effectiveStorage)

	info, err := os.Stat(fullPath)
	if err != nil {
		api.WriteError(w, -1, "File does not exist.")
		return
	}

	if info.IsDir() {
		api.WriteError(w, -1, "Path is not a file.")
		return
	}

	http.ServeFile(w, r, fullPath)
}

// Rename renames a file or directory.
func (h *FileHandler) Rename(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		api.WriteError(w, -1, "Method not allowed")
		return
	}

	oldPath := getParam(r, "old_path")
	newPath := getParam(r, "new_path")
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if oldPath == "" || newPath == "" {
		api.WriteError(w, -1, "Paths are empty.")
		return
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(oldPath) || !isValidPath(newPath) {
		api.WriteError(w, -1, "Invalid paths.")
		return
	}

	fullOldPath := h.getFullPath(oldPath, effectiveStorage)
	fullNewPath := h.getFullPath(newPath, effectiveStorage)

	if _, err := os.Stat(fullOldPath); err != nil {
		api.WriteError(w, -1, "Source does not exist.")
		return
	}

	if _, err := os.Stat(fullNewPath); err == nil {
		api.WriteError(w, -1, "Destination already exists.")
		return
	}

	if err := os.Rename(fullOldPath, fullNewPath); err != nil {
		api.WriteError(w, -1, "Failed to rename: "+err.Error())
		return
	}

	api.WriteSuccessMessage(w, "Renamed successfully.", map[string]interface{}{
		"old_path": oldPath,
		"new_path": newPath,
		"storage":  effectiveStorage,
	})
}

// Info returns file or directory info.
func (h *FileHandler) Info(w http.ResponseWriter, r *http.Request) {
	path := decodePath(getParam(r, "path"))
	storage := getParam(r, "storage")

	effectiveStorage := storage
	if effectiveStorage == "" {
		effectiveStorage = "local"
	}

	if path == "" {
		api.WriteError(w, -1, "Path is empty.")
		return
	}

	if !h.isValidStorage(effectiveStorage) {
		api.WriteError(w, -1, "Invalid storage parameter.")
		return
	}

	if effectiveStorage == "sd" && !h.isSDAvailable() {
		api.WriteError(w, -1, "SD card not available.")
		return
	}

	if !isValidPath(path) {
		api.WriteError(w, -1, "Invalid path.")
		return
	}

	fullPath := h.getFullPath(path, effectiveStorage)

	info, err := os.Stat(fullPath)
	if err != nil {
		api.WriteError(w, -1, "Path does not exist.")
		return
	}

	data := map[string]interface{}{
		"path":         path,
		"is_directory": info.IsDir(),
		"is_file":      info.Mode().IsRegular(),
		"storage":      effectiveStorage,
		"modified":     info.ModTime().Unix(),
	}

	if info.Mode().IsRegular() {
		data["size"] = info.Size()
	} else {
		data["size"] = 0
	}

	api.WriteSuccessMessage(w, "Info retrieved.", data)
}
