#include "file_io.h"
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#include <io.h>
#include <string>
#include <vector>

std::wstring GetDefaultDataDirectory() {
    wchar_t* path = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path) != S_OK) {
        return L"";
    }

    std::wstring appData(path);
    CoTaskMemFree(path);

    if (!appData.empty() && appData.back() != L'\\') {
        appData += L'\\';
    }
    appData += L"FileMove";

    EnsureDirectoryExists(appData);
    return appData;
}

std::wstring GetDefaultJsonPath() {
    return GetDefaultDataDirectory() + L"\\FileMove.json";
}

std::wstring GetDefaultLogPath() {
    return GetDefaultDataDirectory() + L"\\FileMove.log";
}

std::wstring GetBaseName(const std::wstring& path) {
    std::wstring fileName = GetFileName(path);
    size_t dotPos = fileName.find_last_of(L'.');
    if (dotPos != std::wstring::npos) {
        return fileName.substr(0, dotPos);
    }
    return fileName;
}

std::wstring GetDirectory(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return L"";
    return path.substr(0, pos + 1);
}

std::wstring GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return path;
    return path.substr(pos + 1);
}

std::wstring ResolveJsonPath(const std::wstring& inputPath) {
    if (inputPath.empty()) {
        return GetDefaultJsonPath();
    }

    // Check if it has a directory component
    if (inputPath.size() >= 2 && inputPath[1] == L':') {
        // Absolute path with drive letter
        return inputPath;
    }

    size_t slashPos = inputPath.find(L'\\');
    if (slashPos != std::wstring::npos) {
        // Contains directory separator
        return inputPath;
    }

    // No directory, place in default data directory
    return GetDefaultDataDirectory() + L"\\" + inputPath;
}

std::wstring ResolveLogPath(const std::wstring& jsonPath, const std::wstring& outputOption) {
    if (!outputOption.empty()) {
        // O was specified
        if (outputOption.size() >= 2 && outputOption[1] == L':') {
            return outputOption;
        }
        size_t slashPos = outputOption.find(L'\\');
        if (slashPos != std::wstring::npos) {
            return outputOption;
        }
        // No directory in O, use JSON file's directory
        return GetDirectory(jsonPath) + outputOption;
    }

    // Derive from JSON file: same base name, .log extension, same directory
    return ReplaceExtension(jsonPath, L".log");
}

bool EnsureDirectoryExists(const std::wstring& path) {
    if (DirectoryExists(path)) return true;

    // Create parent directories recursively
    std::wstring current;
    for (size_t i = 0; i < path.size(); i++) {
        current += path[i];
        if (path[i] == L'\\' || path[i] == L'/') {
            if (_waccess(current.c_str(), 0) != 0) {
                _wmkdir(current.c_str());
            }
        }
    }

    // Final mkdir for the last component
    if (_waccess(path.c_str(), 0) != 0) {
        _wmkdir(path.c_str());
    }

    return DirectoryExists(path);
}

bool FileExists(const std::wstring& path) {
    struct _stat64 st;
    if (_wstat64(path.c_str(), &st) != 0) return false;
    return (st.st_mode & _S_IFREG) != 0;
}

bool DirectoryExists(const std::wstring& path) {
    struct _stat64 st;
    if (_wstat64(path.c_str(), &st) != 0) return false;
    return (st.st_mode & _S_IFDIR) != 0;
}

long long GetFileSize(const std::wstring& path) {
    struct _stat64 st;
    if (_wstat64(path.c_str(), &st) != 0) return -1;
    return st.st_size;
}

std::vector<std::wstring> ListJsonFiles(const std::wstring& directory) {
    std::vector<std::wstring> files;
    std::wstring searchPattern = directory + L"\\*.json";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return files;

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
    return files;
}

std::wstring ReplaceExtension(const std::wstring& path, const std::wstring& newExt) {
    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos) {
        return path + newExt;
    }
    return path.substr(0, dotPos) + newExt;
}

static void EnumerateDirectoryFilesRecursive(const std::wstring& dirPath, std::vector<std::wstring>& files) {
    std::wstring searchPattern = dirPath;
    if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/') {
        searchPattern += L'\\';
    }
    searchPattern += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring entryName = findData.cFileName;
        if (entryName == L"." || entryName == L"..") continue;

        std::wstring entryPath = dirPath;
        if (!entryPath.empty() && entryPath.back() != L'\\' && entryPath.back() != L'/') {
            entryPath += L'\\';
        }
        entryPath += entryName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recurse into subdirectory
            EnumerateDirectoryFilesRecursive(entryPath, files);
        } else {
            files.push_back(entryPath);
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);
}

std::vector<std::wstring> EnumerateDirectoryFiles(const std::wstring& dirPath) {
    std::vector<std::wstring> files;
    EnumerateDirectoryFilesRecursive(dirPath, files);
    return files;
}
