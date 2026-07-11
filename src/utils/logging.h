#pragma once

#include <string>
#include <vector>

// Open a debug console window. Returns true if console was opened.
bool OpenDebugConsole();

// Close the debug console window.
void CloseDebugConsole();

// Write a message to the debug console (if open).
void DebugConsoleWrite(const std::wstring& message);

// Write a message to the debug console with newline.
void DebugConsoleWriteLine(const std::wstring& message);

// Open a log file for appending. Returns true on success.
bool OpenLogFile(const std::wstring& logPath);

// Close the log file.
void CloseLogFile();

// Trim log file if it exceeds 60KB, removing oldest lines until below 50KB.
void TrimLogFileIfNeeded();

// Write a non-transfer record (----> format) to the log file.
void LogInfo(const std::wstring& message);

// Write a transfer record (CSV format) to the log file.
void LogTransfer(const std::wstring& fileName,
                 const std::wstring& sourceDir,
                 const std::wstring& destDir,
                 const std::wstring& dateTime,
                 const std::wstring& result);

// Check if the log file is currently open.
bool IsLogFileOpen();

// Get the current log file path.
std::wstring GetLogFilePath();

// Get the current timestamp as a formatted string.
std::wstring GetTimestamp();
