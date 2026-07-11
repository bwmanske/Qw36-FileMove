#include "logging.h"
#include <windows.h>
#include <dwmapi.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <io.h>
#include <ctime>
#include <sys/stat.h>

static HANDLE gConsoleHandle = INVALID_HANDLE_VALUE;
static std::wstring gLogPath;
static bool gLogFileOpen = false;

bool OpenDebugConsole() {
    if (gConsoleHandle != INVALID_HANDLE_VALUE) return true;

    if (!AllocConsole()) return false;

    // Override the OS corner policy to force classic square corners on console
    HWND consoleHwnd = GetConsoleWindow();
    if (consoleHwnd) {
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(
            consoleHwnd,
            DWMWA_WINDOW_CORNER_PREFERENCE,
            &cornerPreference,
            sizeof(cornerPreference)
        );
    }

    gConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (gConsoleHandle == INVALID_HANDLE_VALUE) {
        FreeConsole();
        gConsoleHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // Redirect stdout to console
    int fileHandle = _open_osfhandle(reinterpret_cast<intptr_t>(gConsoleHandle), _O_TEXT);
    if (fileHandle != -1) {
        FILE* f = _fdopen(fileHandle, "w");
        if (f) {
            *stdout = *f;
            setvbuf(stdout, NULL, _IONBF, 0);
        }
    }

    // Redirect stderr to console
    fileHandle = _open_osfhandle(reinterpret_cast<intptr_t>(gConsoleHandle), _O_TEXT);
    if (fileHandle != -1) {
        FILE* f = _fdopen(fileHandle, "w");
        if (f) {
            *stderr = *f;
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }

    // Redirect stdin to console
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    fileHandle = _open_osfhandle(reinterpret_cast<intptr_t>(hInput), _O_TEXT);
    if (fileHandle != -1) {
        FILE* f = _fdopen(fileHandle, "r");
        if (f) {
            *stdin = *f;
            setvbuf(stdin, NULL, _IONBF, 0);
        }
    }

    return true;
}

void CloseDebugConsole() {
    if (gConsoleHandle != INVALID_HANDLE_VALUE) {
        gConsoleHandle = INVALID_HANDLE_VALUE;
        FreeConsole();
    }
}

void DebugConsoleWrite(const std::wstring& message) {
    if (gConsoleHandle == INVALID_HANDLE_VALUE) return;
    DWORD written;
    WriteConsoleW(gConsoleHandle, message.c_str(), static_cast<DWORD>(message.size()), &written, NULL);
}

void DebugConsoleWriteLine(const std::wstring& message) {
    DebugConsoleWrite(message + L"\r\n");
}

bool OpenLogFile(const std::wstring& logPath) {
    gLogPath = logPath;
    gLogFileOpen = true;
    TrimLogFileIfNeeded();
    return true;
}

void CloseLogFile() {
    gLogFileOpen = false;
    gLogPath.clear();
}

void TrimLogFileIfNeeded() {
    if (!gLogFileOpen) return;

    struct _stat64 st;
    if (_wstat64(gLogPath.c_str(), &st) != 0) return;
    if (st.st_size <= 60000) return;

    // Read all lines
    std::wifstream inFile(gLogPath);
    if (!inFile.is_open()) return;

    std::vector<std::wstring> lines;
    std::wstring line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    // Remove lines from the top until under 50KB
    while (lines.size() > 1) {
        size_t totalSize = 0;
        for (size_t i = 1; i < lines.size(); i++) {
            totalSize += (lines[i].size() + 1) * sizeof(wchar_t);
        }
        if (totalSize < 50000) break;
        lines.erase(lines.begin());
    }

    // Write back
    std::wofstream outFile(gLogPath, std::ios::trunc);
    if (!outFile.is_open()) return;
    outFile.imbue(std::locale::classic());
    for (const auto& l : lines) {
        outFile << l << L"\n";
    }
    outFile.close();
}

static std::wstring EscapeCsvField(const std::wstring& field) {
    bool needsQuote = false;
    for (wchar_t c : field) {
        if (c == L',' || c == L'"' || c == L'\n' || c == L'\r') {
            needsQuote = true;
            break;
        }
    }
    if (!needsQuote) return field;

    std::wstring escaped = L"\"";
    for (wchar_t c : field) {
        if (c == L'"') escaped += L"\"\"";
        else escaped += c;
    }
    escaped += L"\"";
    return escaped;
}

void LogInfo(const std::wstring& message) {
    if (!gLogFileOpen) return;

    std::wofstream outFile(gLogPath, std::ios::app);
    if (!outFile.is_open()) return;
    outFile.imbue(std::locale::classic());
    outFile << L"----> " << message << L"\n";
    outFile.close();

    DebugConsoleWriteLine(L"----> " + message);
}

void LogTransfer(const std::wstring& fileName,
                 const std::wstring& sourceDir,
                 const std::wstring& destDir,
                 const std::wstring& dateTime,
                 const std::wstring& result) {
    if (!gLogFileOpen) return;

    std::wofstream outFile(gLogPath, std::ios::app);
    if (!outFile.is_open()) return;
    outFile.imbue(std::locale::classic());
    outFile << EscapeCsvField(fileName) << L","
            << EscapeCsvField(sourceDir) << L","
            << EscapeCsvField(destDir) << L","
            << EscapeCsvField(dateTime) << L","
            << EscapeCsvField(result) << L"\n";
    outFile.close();
}

bool IsLogFileOpen() {
    return gLogFileOpen;
}

std::wstring GetLogFilePath() {
    return gLogPath;
}

std::wstring GetTimestamp() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::wostringstream oss;
    oss << std::put_time(tm, L"%Y-%m-%d %H:%M:%S");
    return oss.str();
}
