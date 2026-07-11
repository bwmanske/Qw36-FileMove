#pragma once

#include <string>
#include <vector>

// Get the default data directory: %AppData%\Roaming\FileMove
std::wstring GetDefaultDataDirectory();

// Get the default JSON file path.
std::wstring GetDefaultJsonPath();

// Get the default log file path (derived from JSON path).
std::wstring GetDefaultLogPath();

// Resolve the JSON file path from command-line input.
// If no directory is specified, places it in the default data directory.
std::wstring ResolveJsonPath(const std::wstring& inputPath);

// Resolve the log file path.
// If O is not specified, derives from the JSON file base name.
std::wstring ResolveLogPath(const std::wstring& jsonPath, const std::wstring& outputOption);

// Get the base name without extension from a file path.
std::wstring GetBaseName(const std::wstring& path);

// Get the directory portion of a path.
std::wstring GetDirectory(const std::wstring& path);

// Get the file name from a path.
std::wstring GetFileName(const std::wstring& path);

// Create a directory if it doesn't exist. Returns true on success.
bool EnsureDirectoryExists(const std::wstring& path);

// Check if a file exists.
bool FileExists(const std::wstring& path);

// Check if a directory exists.
bool DirectoryExists(const std::wstring& path);

// Get file size in bytes. Returns -1 on error.
long long GetFileSize(const std::wstring& path);

// List all .json files in a directory.
std::vector<std::wstring> ListJsonFiles(const std::wstring& directory);

// Replace extension of a file path.
std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& newExt);

// Recursively enumerate all files in a directory.
// Returns vector of full file paths.
std::vector<std::wstring> EnumerateDirectoryFiles(const std::wstring& dirPath);
