// Package api provides API response helpers and common types.
package api

import (
	"encoding/json"
	"net/http"
)

// Response represents a standard API response.
type Response struct {
	Code    int         `json:"code"`
	Message string      `json:"msg"`
	Data    interface{} `json:"data,omitempty"`
}

// OK creates a successful response.
func OK(data interface{}) *Response {
	return &Response{
		Code:    0,
		Message: "OK",
		Data:    data,
	}
}

// OKWithMessage creates a successful response with a custom message.
func OKWithMessage(message string, data interface{}) *Response {
	return &Response{
		Code:    0,
		Message: message,
		Data:    data,
	}
}

// Error creates an error response.
func Error(code int, message string) *Response {
	return &Response{
		Code:    code,
		Message: message,
	}
}

// ErrorWithData creates an error response with data.
func ErrorWithData(code int, message string, data interface{}) *Response {
	return &Response{
		Code:    code,
		Message: message,
		Data:    data,
	}
}

// Write writes a Response to the http.ResponseWriter.
func (r *Response) Write(w http.ResponseWriter) error {
	w.Header().Set("Content-Type", "application/json")
	return json.NewEncoder(w).Encode(r)
}

// WriteJSON writes JSON data to the response.
func WriteJSON(w http.ResponseWriter, statusCode int, data interface{}) error {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	return json.NewEncoder(w).Encode(data)
}

// WriteSuccess writes a successful API response.
func WriteSuccess(w http.ResponseWriter, data interface{}) error {
	return OK(data).Write(w)
}

// WriteSuccessMessage writes a successful API response with a message.
func WriteSuccessMessage(w http.ResponseWriter, message string, data interface{}) error {
	return OKWithMessage(message, data).Write(w)
}

// WriteError writes an error API response.
func WriteError(w http.ResponseWriter, code int, message string) error {
	return Error(code, message).Write(w)
}

// WriteErrorWithData writes an error API response with additional data.
func WriteErrorWithData(w http.ResponseWriter, code int, message string, data interface{}) error {
	return ErrorWithData(code, message, data).Write(w)
}

// WriteHTTPError writes an HTTP error response.
func WriteHTTPError(w http.ResponseWriter, statusCode int, message string) {
	http.Error(w, message, statusCode)
}

// ParseJSONBody parses JSON from request body into the target.
func ParseJSONBody(r *http.Request, target interface{}) error {
	return json.NewDecoder(r.Body).Decode(target)
}
