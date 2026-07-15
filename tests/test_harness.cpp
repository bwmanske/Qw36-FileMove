#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <io.h>
#include <direct.h>

#include "utils/cmdline_parser.h"
#include "data/file_io.h"
#include "data/json_parser.h"
#include "queue/queue_manager.h"

// Simple test framework
static int gPassed = 0;
static int gFailed = 0;

#define ASSERT_TRUE(expr) do { \
    if (expr) { gPassed++; } \
    else { gFailed++; std::cerr << "  FAIL: " #expr << " at line " << __LINE__ << std::endl; } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) do { \
    auto va = (a); auto vb = (b); \
    if (va == vb) { gPassed++; } \
    else { gFailed++; std::cerr << "  FAIL: " #a " == " #b << " (" << static_cast<int>(va) << " != " << static_cast<int>(vb) << ") at line " << __LINE__ << std::endl; } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    std::string sa = (a); std::string sb = (b); \
    if (sa == sb) { gPassed++; } \
    else { gFailed++; std::cerr << "  FAIL: " #a " == " #b << " (\"" << sa << "\" != \"" << sb << "\") at line " << __LINE__ << std::endl; } \
} while(0)

#define ASSERT_WSTR_EQ(a, b) do { \
    std::wstring sa = (a); std::wstring sb = (b); \
    if (sa == sb) { gPassed++; } \
    else { \
        gFailed++; \
        std::ostringstream oss; oss << "  FAIL: " #a " == " #b << " at line " << __LINE__ << std::endl; \
        std::cerr << oss.str(); \
    } \
} while(0)

static std::string WStringToString(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &result[0], len, NULL, NULL);
    return result;
}

// Helper: create temp file with content
static bool CreateTempFile(const std::wstring& path, const std::wstring& content) {
    std::wofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

// Helper: create temp file with content (narrow)
static bool CreateTempFileNarrow(const std::wstring& path, const std::string& content) {
    std::ofstream f(WStringToString(path));
    if (!f.is_open()) return false;
    f << content;
    return true;
}

// Helper: remove file
static void RemoveFile(const std::wstring& path) {
    _wunlink(path.c_str());
}

// Helper: remove directory recursively (files + subdirs)
static void RemoveDirectoryTreeRecursive(const std::wstring& path) {
    std::wstring searchPattern = path;
    if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/') {
        searchPattern += L'\\';
    }
    searchPattern += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subDirs;

    do {
        std::wstring entryName = findData.cFileName;
        if (entryName == L"." || entryName == L"..") continue;

        std::wstring entryPath = path;
        if (!entryPath.empty() && entryPath.back() != L'\\' && entryPath.back() != L'/') {
            entryPath += L'\\';
        }
        entryPath += entryName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subDirs.push_back(entryPath);
        } else {
            _wunlink(entryPath.c_str());
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);

    for (const auto& sd : subDirs) {
        RemoveDirectoryTreeRecursive(sd);
        _wrmdir(sd.c_str());
    }
    _wrmdir(path.c_str());
}

// Helper: ensure clean test directory (remove if exists, recreate)
static void EnsureCleanTestDir(const std::wstring& path) {
    if (DirectoryExists(path)) {
        RemoveDirectoryTreeRecursive(path);
    }
    EnsureDirectoryExists(path);
}

// Helper: remove directory (legacy, files only)
static void RemoveDirectoryTree(const std::wstring& path) {
    std::vector<std::wstring> files = EnumerateDirectoryFiles(path);
    for (const auto& f : files) {
        _wunlink(f.c_str());
    }
    _wrmdir(path.c_str());
}

// ==================== cmdline_parser tests ====================

static void TestCmdlineParser() {
    std::cout << "Testing cmdline_parser..." << std::endl;

    ParsedCommandLine result;
    std::wstring error;

    // Test: empty command line (just exe path)
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe", result, error);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(result.debugModeSpecified);
        ASSERT_FALSE(result.inputFileSpecified);
        ASSERT_FALSE(result.outputFileSpecified);
        ASSERT_FALSE(result.sortModeSpecified);
        ASSERT_FALSE(result.placementModeSpecified);
    }

    // Test: /D MV
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /D MV", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.debugModeSpecified);
        ASSERT_EQ(result.debugMode, DebugMode::MV);
    }

    // Test: -d cp (case insensitive)
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe -d cp", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.debugModeSpecified);
        ASSERT_EQ(result.debugMode, DebugMode::CP);
    }

    // Test: /D invalid value
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /D INVALID", result, error);
        ASSERT_FALSE(ok);
        ASSERT_FALSE(error.empty());
    }

    // Test: /I with no extension (should accept, .json added)
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /I MyConfig", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.inputFileSpecified);
        ASSERT_WSTR_EQ(result.inputFile, L"MyConfig.json");
    }

    // Test: /I with .json extension
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /I C:\\Data\\Groups.json", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.inputFileSpecified);
        ASSERT_WSTR_EQ(result.inputFile, L"C:\\Data\\Groups.json");
    }

    // Test: /I with invalid extension
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /I Config.txt", result, error);
        ASSERT_FALSE(ok);
    }

    // Test: /O with .log extension
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /O C:\\Logs\\app.log", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.outputFileSpecified);
        ASSERT_WSTR_EQ(result.outputFile, L"C:\\Logs\\app.log");
    }

    // Test: /O with no extension (.log added)
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /O MyLog", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.outputFileSpecified);
        ASSERT_WSTR_EQ(result.outputFile, L"MyLog.log");
    }

    // Test: /O with invalid extension
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /O app.txt", result, error);
        ASSERT_FALSE(ok);
    }

    // Test: /S MRU
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S MRU", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.sortModeSpecified);
        ASSERT_EQ(result.sortMode, SortMode::MostRecentlyUsed);
    }

    // Test: /S LRU
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S LRU", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::LeastRecentlyUsed);
    }

    // Test: /S AF
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S AF", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::AddedFirst);
    }

    // Test: /S AL
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S AL", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::AddedLast);
    }

    // Test: /S AZ
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S AZ", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::AtoZ);
    }

    // Test: /S ZA
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S ZA", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::ZtoA);
    }

    // Test: /S with full name
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S AddedLast", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.sortMode, SortMode::AddedLast);
    }

    // Test: /S invalid
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /S INVALID", result, error);
        ASSERT_FALSE(ok);
    }

    // Test: /P UL
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P UL", result, error);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(result.placementModeSpecified);
        ASSERT_EQ(result.placementMode, PlacementMode::UpperLeft);
    }

    // Test: /P UR
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P UR", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.placementMode, PlacementMode::UpperRight);
    }

    // Test: /P LL
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P LL", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.placementMode, PlacementMode::LowerLeft);
    }

    // Test: /P LR
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P LR", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.placementMode, PlacementMode::LowerRight);
    }

    // Test: /P LAST
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P LAST", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.placementMode, PlacementMode::LastLocation);
    }

    // Test: /P with full name
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P LastLocation", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.placementMode, PlacementMode::LastLocation);
    }

    // Test: /P invalid
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /P INVALID", result, error);
        ASSERT_FALSE(ok);
    }

    // Test: multiple options
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /D MV /I C:\\Data\\G.json /S AZ /P UR", result, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(result.debugMode, DebugMode::MV);
        ASSERT_WSTR_EQ(result.inputFile, L"C:\\Data\\G.json");
        ASSERT_EQ(result.sortMode, SortMode::AtoZ);
        ASSERT_EQ(result.placementMode, PlacementMode::UpperRight);
    }

    // Test: unknown option
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe /X value", result, error);
        ASSERT_FALSE(ok);
    }

    // Test: option without / or - prefix (should not be treated as option)
    {
        result = {};
        error.clear();
        bool ok = ParseCommandLine(L"C:\\path\\FileMove.exe D MV", result, error);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(result.debugModeSpecified);
    }

    // Test: SortModeToString round-trip
    {
        ASSERT_STR_EQ(SortModeToString(SortMode::MostRecentlyUsed), "MostRecentlyUsed");
        ASSERT_STR_EQ(SortModeToString(SortMode::LeastRecentlyUsed), "LeastRecentlyUsed");
        ASSERT_STR_EQ(SortModeToString(SortMode::AddedFirst), "AddedFirst");
        ASSERT_STR_EQ(SortModeToString(SortMode::AddedLast), "AddedLast");
        ASSERT_STR_EQ(SortModeToString(SortMode::AtoZ), "AtoZ");
        ASSERT_STR_EQ(SortModeToString(SortMode::ZtoA), "ZtoA");
    }

    // Test: SortModeFromString round-trip
    {
        ASSERT_EQ(SortModeFromString("MostRecentlyUsed"), SortMode::MostRecentlyUsed);
        ASSERT_EQ(SortModeFromString("LeastRecentlyUsed"), SortMode::LeastRecentlyUsed);
        ASSERT_EQ(SortModeFromString("AddedFirst"), SortMode::AddedFirst);
        ASSERT_EQ(SortModeFromString("AddedLast"), SortMode::AddedLast);
        ASSERT_EQ(SortModeFromString("AtoZ"), SortMode::AtoZ);
        ASSERT_EQ(SortModeFromString("ZtoA"), SortMode::ZtoA);
    }

    // Test: PlacementModeToString round-trip
    {
        ASSERT_STR_EQ(PlacementModeToString(PlacementMode::UpperLeft), "UpperLeft");
        ASSERT_STR_EQ(PlacementModeToString(PlacementMode::UpperRight), "UpperRight");
        ASSERT_STR_EQ(PlacementModeToString(PlacementMode::LowerLeft), "LowerLeft");
        ASSERT_STR_EQ(PlacementModeToString(PlacementMode::LowerRight), "LowerRight");
        ASSERT_STR_EQ(PlacementModeToString(PlacementMode::LastLocation), "LastLocation");
    }

    // Test: PlacementModeFromString round-trip
    {
        ASSERT_EQ(PlacementModeFromString("UpperLeft"), PlacementMode::UpperLeft);
        ASSERT_EQ(PlacementModeFromString("UpperRight"), PlacementMode::UpperRight);
        ASSERT_EQ(PlacementModeFromString("LowerLeft"), PlacementMode::LowerLeft);
        ASSERT_EQ(PlacementModeFromString("LowerRight"), PlacementMode::LowerRight);
        ASSERT_EQ(PlacementModeFromString("LastLocation"), PlacementMode::LastLocation);
    }

    // Test: GetCommandLineHelp returns non-empty
    {
        std::wstring help = GetCommandLineHelp();
        ASSERT_FALSE(help.empty());
    }

    std::cout << "  cmdline_parser tests done." << std::endl;
}

// ==================== file_io tests ====================

static void TestFileIo() {
    std::cout << "Testing file_io..." << std::endl;

    // Test: GetBaseName
    {
        ASSERT_WSTR_EQ(GetBaseName(L"C:\\path\\file.txt"), L"file");
        ASSERT_WSTR_EQ(GetBaseName(L"file.json"), L"file");
        ASSERT_WSTR_EQ(GetBaseName(L"C:\\path\\noext"), L"noext");
    }

    // Test: GetDirectory (includes trailing separator)
    {
        ASSERT_WSTR_EQ(GetDirectory(L"C:\\path\\file.txt"), L"C:\\path\\");
        ASSERT_WSTR_EQ(GetDirectory(L"file.txt"), L"");
    }

    // Test: GetFileName
    {
        ASSERT_WSTR_EQ(GetFileName(L"C:\\path\\file.txt"), L"file.txt");
        ASSERT_WSTR_EQ(GetFileName(L"file.txt"), L"file.txt");
    }

    // Test: ReplaceExtension
    {
        ASSERT_WSTR_EQ(ReplaceExtension(L"C:\\path\\file.json", L".log"), L"C:\\path\\file.log");
        ASSERT_WSTR_EQ(ReplaceExtension(L"file.txt", L".log"), L"file.log");
        ASSERT_WSTR_EQ(ReplaceExtension(L"noext", L".log"), L"noext.log");
    }

    // Test: ResolveJsonPath - no directory, places in default data dir
    {
        std::wstring resolved = ResolveJsonPath(L"MyConfig");
        ASSERT_FALSE(resolved.empty());
        ASSERT_WSTR_EQ(GetFileName(resolved), L"MyConfig");
    }

    // Test: ResolveJsonPath - with directory
    {
        std::wstring resolved = ResolveJsonPath(L"C:\\Data\\Groups.json");
        ASSERT_WSTR_EQ(resolved, L"C:\\Data\\Groups.json");
    }

    // Test: ResolveJsonPath - has drive letter, returned as-is
    {
        std::wstring resolved = ResolveJsonPath(L"C:\\Data\\MyConfig");
        ASSERT_WSTR_EQ(resolved, L"C:\\Data\\MyConfig");
    }

    // Test: ResolveLogPath - derived from JSON
    {
        std::wstring logPath = ResolveLogPath(L"C:\\Data\\Groups.json", L"");
        ASSERT_WSTR_EQ(GetFileName(logPath), L"Groups.log");
    }

    // Test: ResolveLogPath - explicit path
    {
        std::wstring logPath = ResolveLogPath(L"C:\\Data\\Groups.json", L"C:\\Logs\\app.log");
        ASSERT_WSTR_EQ(logPath, L"C:\\Logs\\app.log");
    }

    // Test: ResolveLogPath - no extension gets .log
    {
        std::wstring logPath = ResolveLogPath(L"C:\\Data\\Groups.json", L"MyLog");
        ASSERT_FALSE(logPath.empty());
    }

    // Test: FileExists / DirectoryExists with temp files
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\test.txt";
        CreateTempFile(testFile, L"hello");

        ASSERT_TRUE(FileExists(testFile));
        ASSERT_TRUE(DirectoryExists(testDir));
        ASSERT_FALSE(FileExists(testDir + L"\\nonexistent.txt"));

        RemoveFile(testFile);
        ASSERT_FALSE(FileExists(testFile));
        RemoveDirectoryTree(testDir);
    }

    // Test: GetFileSize
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\sized.txt";
        CreateTempFileNarrow(testFile, "12345");

        long long size = GetFileSize(testFile);
        ASSERT_EQ(size, 5LL);

        RemoveFile(testFile);
        RemoveDirectoryTree(testDir);
    }

    // Test: EnsureDirectoryExists
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test\\nested\\deep";
        bool ok = EnsureDirectoryExists(testDir);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(DirectoryExists(testDir));

        // Cleanup
        _wrmdir(L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test\\nested\\deep");
        _wrmdir(L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test\\nested");
        _wrmdir(L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test");
    }

    // Test: EnumerateDirectoryFiles
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);
        EnsureDirectoryExists(testDir + L"\\subdir");

        CreateTempFile(testDir + L"\\file1.txt", L"a");
        CreateTempFile(testDir + L"\\file2.txt", L"b");
        CreateTempFile(testDir + L"\\subdir\\file3.txt", L"c");

        auto files = EnumerateDirectoryFiles(testDir);
        ASSERT_EQ(files.size(), 3ULL);

        RemoveFile(testDir + L"\\file1.txt");
        RemoveFile(testDir + L"\\file2.txt");
        RemoveFile(testDir + L"\\subdir\\file3.txt");
        _wrmdir((testDir + L"\\subdir").c_str());
        RemoveDirectoryTree(testDir);
    }

    // Test: ListJsonFiles
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        CreateTempFileNarrow(testDir + L"\\a.json", "{}");
        CreateTempFileNarrow(testDir + L"\\b.json", "{}");
        CreateTempFileNarrow(testDir + L"\\c.txt", "not json");

        auto jsonFiles = ListJsonFiles(testDir);
        ASSERT_EQ(jsonFiles.size(), 2ULL);

        RemoveFile(testDir + L"\\a.json");
        RemoveFile(testDir + L"\\b.json");
        RemoveFile(testDir + L"\\c.txt");
        RemoveDirectoryTree(testDir);
    }

    std::cout << "  file_io tests done." << std::endl;
}

// ==================== json_parser tests ====================

static void TestJsonParser() {
    std::cout << "Testing json_parser..." << std::endl;

    std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
    EnsureDirectoryExists(testDir);

    // Test: CreateDefaultJson
    {
        std::wstring path = testDir + L"\\default.json";
        bool ok = CreateDefaultJson(path);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(FileExists(path));

        AppData data;
        bool loaded = LoadAppData(path, data);
        ASSERT_TRUE(loaded);
        ASSERT_EQ(data.settings.version, 1);
        ASSERT_STR_EQ(data.settings.sortMode, "MostRecentlyUsed");
        ASSERT_STR_EQ(data.settings.placementMode, "UpperLeft");
        ASSERT_TRUE(data.groups.empty());

        RemoveFile(path);
    }

    // Test: Save and load round-trip
    {
        AppData data;
        data.settings.version = 1;
        data.settings.sortMode = "AddedLast";
        data.settings.placementMode = "LowerRight";
        data.settings.windowWidth = 400;
        data.settings.windowHeight = 600;
        data.settings.windowLeft = 100;
        data.settings.windowTop = 200;
        data.settings.enableDirectoryMoves = true;
        data.settings.enableSidecarFiles = true;
        data.settings.hideQueuedSourceFiles = false;

        Group g;
        g.id = "grp-001";
        g.name = "Test Group";
        g.destinationPaths.push_back("C:\\Dest1");
        g.destinationPaths.push_back("D:\\Dest2");
        g.createdAt = "2026-01-01T00:00:00";
        g.updatedAt = "2026-01-01T00:00:00";
        g.lastUsedAt = "2026-01-02T00:00:00";
        data.groups.push_back(g);

        std::wstring path = testDir + L"\\roundtrip.json";
        bool saved = SaveAppData(path, data);
        ASSERT_TRUE(saved);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_EQ(loaded.settings.version, 1);
        ASSERT_STR_EQ(loaded.settings.sortMode, "AddedLast");
        ASSERT_STR_EQ(loaded.settings.placementMode, "LowerRight");
        ASSERT_EQ(loaded.settings.windowWidth, 400);
        ASSERT_EQ(loaded.settings.windowHeight, 600);
        ASSERT_EQ(loaded.settings.windowLeft, 100);
        ASSERT_EQ(loaded.settings.windowTop, 200);
        ASSERT_TRUE(loaded.settings.enableDirectoryMoves);
        ASSERT_TRUE(loaded.settings.enableSidecarFiles);
        ASSERT_FALSE(loaded.settings.hideQueuedSourceFiles);
        ASSERT_EQ(loaded.groups.size(), 1ULL);
        ASSERT_STR_EQ(loaded.groups[0].name, "Test Group");
        ASSERT_EQ(loaded.groups[0].destinationPaths.size(), 2ULL);
        ASSERT_STR_EQ(loaded.groups[0].destinationPaths[0], "C:\\Dest1");
        ASSERT_STR_EQ(loaded.groups[0].destinationPaths[1], "D:\\Dest2");

        RemoveFile(path);
    }

    // Test: Empty file (0 bytes) loads as default
    {
        std::wstring path = testDir + L"\\empty.json";
        CreateTempFile(path, L"");

        AppData data;
        bool ok = LoadAppData(path, data);
        ASSERT_TRUE(ok);
        ASSERT_EQ(data.settings.version, 1);
        ASSERT_TRUE(data.groups.empty());

        RemoveFile(path);
    }

    // Test: Malformed JSON returns false
    {
        std::wstring path = testDir + L"\\malformed.json";
        CreateTempFileNarrow(path, "{ invalid json }");

        AppData data;
        bool ok = LoadAppData(path, data);
        ASSERT_FALSE(ok);

        RemoveFile(path);
    }

    // Test: Legacy DestinationPath migration
    {
        std::wstring path = testDir + L"\\legacy.json";
        // Write JSON with single DestinationPath instead of DestinationPaths
        std::string legacyJson = R"({
            "version": 1,
            "lastSelectedGroupId": "",
            "sortMode": "MostRecentlyUsed",
            "placementMode": "UpperLeft",
            "windowWidth": 320,
            "windowHeight": 500,
            "windowLeft": 0,
            "windowTop": 0,
            "groups": [
                {
                    "id": "grp-001",
                    "name": "Legacy Group",
                    "DestinationPath": "C:\\OldDest",
                    "createdAt": "2025-01-01T00:00:00",
                    "updatedAt": "2025-01-01T00:00:00",
                    "lastUsedAt": ""
                }
            ]
        })";
        CreateTempFileNarrow(path, legacyJson);

        AppData data;
        bool ok = LoadAppData(path, data);
        ASSERT_TRUE(ok);
        ASSERT_EQ(data.groups.size(), 1ULL);
        ASSERT_STR_EQ(data.groups[0].name, "Legacy Group");
        ASSERT_EQ(data.groups[0].destinationPaths.size(), 1ULL);
        ASSERT_STR_EQ(data.groups[0].destinationPaths[0], "C:\\OldDest");

        // Verify saved format uses destinationPaths
        AppData reloaded;
        bool ok2 = LoadAppData(path, reloaded);
        ASSERT_TRUE(ok2);
        ASSERT_EQ(reloaded.groups[0].destinationPaths.size(), 1ULL);

        RemoveFile(path);
    }

    // Test: GenerateGroupId produces unique IDs
    {
        std::string id1 = GenerateGroupId();
        std::string id2 = GenerateGroupId();
        ASSERT_FALSE(id1.empty());
        ASSERT_FALSE(id2.empty());
        // IDs start with "grp-"
        ASSERT_TRUE(id1.substr(0, 4) == "grp-");
        ASSERT_TRUE(id2.substr(0, 4) == "grp-");
    }

    // Test: GetIsoTimestamp produces valid ISO 8601 format
    {
        std::string ts = GetIsoTimestamp();
        ASSERT_FALSE(ts.empty());
        // ISO 8601 format: YYYY-MM-DDTHH:MM:SS.fff000000+HH:MM (includes ms, microseconds, tz offset)
        ASSERT_TRUE(ts.size() >= 19ULL);
        ASSERT_TRUE(ts[4] == '-' && ts[7] == '-' && ts[10] == 'T');
        ASSERT_TRUE(ts[13] == ':' && ts[16] == ':');
    }

    // Test: Multiple groups save/load
    {
        AppData data;
        for (int i = 0; i < 5; i++) {
            Group g;
            g.id = "grp-" + std::to_string(i);
            g.name = "Group " + std::to_string(i);
            g.destinationPaths.push_back("C:\\Dest" + std::to_string(i));
            g.createdAt = GetIsoTimestamp();
            g.updatedAt = g.createdAt;
            data.groups.push_back(g);
        }

        std::wstring path = testDir + L"\\multi.json";
        bool saved = SaveAppData(path, data);
        ASSERT_TRUE(saved);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_EQ(loaded.groups.size(), 5ULL);
        ASSERT_STR_EQ(loaded.groups[0].name, "Group 0");
        ASSERT_STR_EQ(loaded.groups[4].name, "Group 4");

        RemoveFile(path);
    }

    RemoveDirectoryTree(testDir);

    std::cout << "  json_parser tests done." << std::endl;
}

// ==================== queue_manager tests ====================

static void TestQueueManager() {
    std::cout << "Testing queue_manager..." << std::endl;

    QueueManager qm;

    // Test: Empty queue
    {
        ASSERT_TRUE(qm.IsEmpty());
        ASSERT_EQ(qm.GetQueuedDestinationCount(), 0);
    }

    // Test: PrepareBatch with no files
    {
        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {}, {"C:\\Dest"}, error);
        ASSERT_FALSE(ok);
        ASSERT_FALSE(error.empty());
    }

    // Test: PrepareBatch with no destinations
    {
        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {"C:\\file.txt"}, {}, error);
        ASSERT_FALSE(ok);
    }

    // Test: PrepareBatch with non-existent source
    {
        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {"C:\\nonexistent_file_xyz.txt"}, {"C:\\Dest"}, error);
        ASSERT_FALSE(ok);
    }

    // Test: PrepareBatch with non-existent destination
    {
        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {"C:\\nonexistent.txt"}, {"Z:\\NonExistentDrive\\Path"}, error);
        ASSERT_FALSE(ok);
    }

    // Test: PrepareBatch with valid file and destination
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\source.txt";
        CreateTempFile(testFile, L"test content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::wstring error;
        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(qm.IsEmpty());
        ASSERT_EQ(qm.GetQueuedDestinationCount(), 1);

        // Test release
        qm.ReleasePreparedEntries();

        // Test get next entry
        PendingMoveEntry entry;
        bool got = qm.GetNextEntry(entry);
        ASSERT_TRUE(got);
        ASSERT_STR_EQ(entry.sourceFilePath, srcFile);
        ASSERT_EQ(entry.destinationDirectories.size(), 1ULL);
        ASSERT_EQ(entry.status, QueueEntryStatus::Processing);

        // Mark completed
        qm.MarkCompleted(entry.id);

        // Cleanup
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: Deduplication - same source queued twice
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\dup.txt";
        CreateTempFile(testFile, L"dup content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error1;
        bool ok1 = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error1);
        ASSERT_TRUE(ok1);

        // Try to queue same file again
        std::wstring error2;
        bool ok2 = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error2);
        // Should fail because file is already queued
        ASSERT_FALSE(ok2);

        // Cleanup
        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: CancelPreparedEntries
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\cancel.txt";
        CreateTempFile(testFile, L"cancel content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        qm.CancelPreparedEntries();

        // Cleanup
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: Sidecar file creation
    {
        qm.SetEnableSidecarFiles(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\sidecar.txt";
        CreateTempFile(testFile, L"sidecar content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        // Check sidecar exists
        std::wstring sidecar = testFile + L".filemove-queued";
        ASSERT_TRUE(FileExists(sidecar));

        // Release and complete - sidecar should be removed
        qm.ReleasePreparedEntries();
        PendingMoveEntry entry;
        qm.GetNextEntry(entry);
        qm.MarkCompleted(entry.id);

        ASSERT_FALSE(FileExists(sidecar));

        // Cleanup
        RemoveFile(testFile);
        RemoveFile(sidecar);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);

        qm.SetEnableSidecarFiles(false);
    }

    // Test: Multiple files in batch
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::vector<std::string> sources;
        for (int i = 0; i < 3; i++) {
            std::wstring f = testDir + L"\\batch" + std::to_wstring(i) + L".txt";
            CreateTempFile(f, L"content " + std::to_wstring(i));
            sources.push_back(WStringToString(f));
        }

        std::string dest = WStringToString(destDir);
        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", sources, {dest}, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(qm.GetQueuedDestinationCount(), 3);

        // Cleanup
        qm.CancelAllEntries();
        for (int i = 0; i < 3; i++) {
            std::wstring f = testDir + L"\\batch" + std::to_wstring(i) + L".txt";
            RemoveFile(f);
        }
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: Multiple destinations per file
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\multi_dest.txt";
        CreateTempFile(testFile, L"multi dest content");

        std::wstring dest1 = testDir + L"\\dest1";
        std::wstring dest2 = testDir + L"\\dest2";
        EnsureDirectoryExists(dest1);
        EnsureDirectoryExists(dest2);

        std::string srcFile = WStringToString(testFile);
        std::string d1 = WStringToString(dest1);
        std::string d2 = WStringToString(dest2);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {d1, d2}, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(qm.GetQueuedDestinationCount(), 2);

        // Cleanup
        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(dest1);
        RemoveDirectoryTree(dest2);
        RemoveDirectoryTree(testDir);
    }

    // Test: Directory move disabled rejects directory
    {
        qm.SetEnableDirectoryMoves(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(testDir);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_FALSE(ok);

        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: Directory move enabled expands directory
    {
        qm.SetEnableDirectoryMoves(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);
        EnsureDirectoryExists(testDir + L"\\subdir");

        CreateTempFile(testDir + L"\\f1.txt", L"a");
        CreateTempFile(testDir + L"\\f2.txt", L"b");
        CreateTempFile(testDir + L"\\subdir\\f3.txt", L"c");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(testDir);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);
        ASSERT_EQ(qm.GetQueuedDestinationCount(), 3);

        // Cleanup
        qm.CancelAllEntries();
        RemoveFile(testDir + L"\\f1.txt");
        RemoveFile(testDir + L"\\f2.txt");
        RemoveFile(testDir + L"\\subdir\\f3.txt");
        _wrmdir((testDir + L"\\subdir").c_str());
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);

        qm.SetEnableDirectoryMoves(false);
    }

    std::cout << "  queue_manager tests done." << std::endl;
}

// ==================== PreserveDirectoryStructure tests ====================

static void TestPreserveDirectoryStructure() {
    std::cout << "Testing preserve_directory_structure..." << std::endl;

    QueueManager qm;
    qm.SetEnableDirectoryMoves(true);

    // Test: single file with structure disabled — dest path unchanged
    {
        qm.SetPreserveDirectoryStructure(false);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\source.txt";
        CreateTempFile(testFile, L"content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].destinationDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].destinationDirectories[0], dest);

        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: single file with structure enabled — no dir root, dest unchanged
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\source.txt";
        CreateTempFile(testFile, L"content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].destinationDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].destinationDirectories[0], dest);

        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: directory source with structure enabled — dest gets srcDirBase prefix
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\MySrc";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file1.txt", L"a");
        CreateTempFile(srcSub + L"\\file2.txt", L"b");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);
        ASSERT_EQ(entries[0].destinationDirectories.size(), 1ULL);
        ASSERT_EQ(entries[1].destinationDirectories.size(), 1ULL);

        // Dest paths should contain "MySrc"
        std::string d0 = entries[0].destinationDirectories[0];
        std::string d1 = entries[1].destinationDirectories[0];
        ASSERT_TRUE(d0.find("MySrc") != std::string::npos);
        ASSERT_TRUE(d1.find("MySrc") != std::string::npos);

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file1.txt");
        RemoveFile(srcSub + L"\\file2.txt");
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: nested subdirectory with structure enabled
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\NestedSrc";
        EnsureDirectoryExists(srcSub);
        std::wstring subA = srcSub + L"\\a";
        std::wstring subB = subA + L"\\b";
        EnsureDirectoryExists(subB);
        CreateTempFile(subB + L"\\deep.txt", L"deep");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].destinationDirectories.size(), 1ULL);

        std::string d0 = entries[0].destinationDirectories[0];
        ASSERT_TRUE(d0.find("NestedSrc") != std::string::npos);
        ASSERT_TRUE(d0.find("a") != std::string::npos);
        ASSERT_TRUE(d0.find("b") != std::string::npos);

        qm.CancelAllEntries();
        RemoveFile(subB + L"\\deep.txt");
        _wrmdir(subB.c_str());
        _wrmdir(subA.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: multiple files from same dir root — all get same prefix
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\MultiFiles";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\f1.txt", L"1");
        CreateTempFile(srcSub + L"\\f2.txt", L"2");
        CreateTempFile(srcSub + L"\\f3.txt", L"3");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 3ULL);
        for (const auto& e : entries) {
            ASSERT_EQ(e.destinationDirectories.size(), 1ULL);
            ASSERT_TRUE(e.destinationDirectories[0].find("MultiFiles") != std::string::npos);
        }

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\f1.txt");
        RemoveFile(srcSub + L"\\f2.txt");
        RemoveFile(srcSub + L"\\f3.txt");
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: structure enabled + multiple destinations
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\MultiDest";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");

        std::wstring dest1 = testDir + L"\\dest1";
        std::wstring dest2 = testDir + L"\\dest2";
        EnsureDirectoryExists(dest1);
        EnsureDirectoryExists(dest2);

        std::string srcDir = WStringToString(srcSub);
        std::string d1 = WStringToString(dest1);
        std::string d2 = WStringToString(dest2);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {d1, d2}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].destinationDirectories.size(), 2ULL);
        ASSERT_TRUE(entries[0].destinationDirectories[0].find("MultiDest") != std::string::npos);
        ASSERT_TRUE(entries[0].destinationDirectories[1].find("MultiDest") != std::string::npos);

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(dest1);
        RemoveDirectoryTree(dest2);
        RemoveDirectoryTree(testDir);
    }

    // Test: multiple directory sources — each gets own basename prefix
    {
        qm.SetPreserveDirectoryStructure(true);
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcA = testDir + L"\\DirA";
        std::wstring srcB = testDir + L"\\DirB";
        EnsureDirectoryExists(srcA);
        EnsureDirectoryExists(srcB);
        CreateTempFile(srcA + L"\\a.txt", L"a");
        CreateTempFile(srcB + L"\\b.txt", L"b");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string sA = WStringToString(srcA);
        std::string sB = WStringToString(srcB);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {sA, sB}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);

        // Find which entry came from DirA
        bool foundA = false, foundB = false;
        for (const auto& e : entries) {
            std::string d = e.destinationDirectories[0];
            if (d.find("DirA") != std::string::npos) foundA = true;
            if (d.find("DirB") != std::string::npos) foundB = true;
        }
        ASSERT_TRUE(foundA);
        ASSERT_TRUE(foundB);

        qm.CancelAllEntries();
        RemoveFile(srcA + L"\\a.txt");
        RemoveFile(srcB + L"\\b.txt");
        RemoveDirectoryTree(srcA);
        RemoveDirectoryTree(srcB);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    qm.SetPreserveDirectoryStructure(false);
    qm.SetEnableDirectoryMoves(false);

    std::cout << "  preserve_directory_structure tests done." << std::endl;
}

// Helper: directly test FindEmptyDirectories logic via Win32 API
static void FindEmptyDirsDirect(const std::wstring& dirPath, std::vector<std::wstring>& emptyDirs) {
    std::wstring searchPattern = dirPath;
    if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/') {
        searchPattern += L'\\';
    }
    searchPattern += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subDirs;
    bool hasFiles = false;

    do {
        std::wstring entryName = findData.cFileName;
        if (entryName == L"." || entryName == L"..") continue;

        std::wstring entryPath = dirPath;
        if (!entryPath.empty() && entryPath.back() != L'\\' && entryPath.back() != L'/') {
            entryPath += L'\\';
        }
        entryPath += entryName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subDirs.push_back(entryPath);
        } else {
            hasFiles = true;
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);

    // Always recurse, passing the same output vector
    for (const auto& subDir : subDirs) {
        FindEmptyDirsDirect(subDir, emptyDirs);
    }

    if (hasFiles) return;
    if (!subDirs.empty()) return;

    std::wstring relPath = dirPath;
    if (!relPath.empty() && (relPath.back() == L'\\' || relPath.back() == L'/')) {
        relPath.pop_back();
    }
    emptyDirs.push_back(relPath);
}

// ==================== CreateEmptyDirectories tests ====================

static void TestCreateEmptyDirectories() {
    std::cout << "Testing create_empty_directories..." << std::endl;

    // First, directly test FindEmptyDirectories logic
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\SrcDirectTest";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring emptyDir = srcSub + L"\\empty";
        EnsureDirectoryExists(emptyDir);

        ASSERT_TRUE(DirectoryExists(srcSub));
        ASSERT_TRUE(DirectoryExists(emptyDir));
        ASSERT_TRUE(FileExists(srcSub + L"\\file.txt"));

        std::vector<std::wstring> emptyDirs;
        FindEmptyDirsDirect(srcSub, emptyDirs);

        ASSERT_EQ(emptyDirs.size(), 1ULL);
        std::string relPath = WStringToString(emptyDirs[0]);
        std::string srcStr = WStringToString(srcSub);
        if (srcStr.size() <= relPath.size()) {
            relPath = relPath.substr(srcStr.size());
            if (!relPath.empty() && (relPath[0] == '\\' || relPath[0] == '/')) {
                relPath = relPath.substr(1);
            }
        }
        ASSERT_STR_EQ(relPath, "empty");

        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(emptyDir.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(testDir);
    }

    QueueManager qm;
    qm.SetEnableDirectoryMoves(true);
    qm.SetPreserveDirectoryStructure(true);

    // Test: leaf empty directory detected
    {
        qm.SetCreateEmptyDirectories(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\SrcWithEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring emptyDir = srcSub + L"\\empty";
        EnsureDirectoryExists(emptyDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_FALSE(entries[0].emptySourceDirectories.empty());
        ASSERT_EQ(entries[0].emptySourceDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "empty");

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(emptyDir.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: leaf empty directory detected (minimal, no structure)
    {
        qm.SetCreateEmptyDirectories(true);
        qm.SetPreserveDirectoryStructure(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\SrcMinEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring emptyDir = srcSub + L"\\empty";
        EnsureDirectoryExists(emptyDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        // With structure disabled, empty dirs should NOT be collected
        ASSERT_TRUE(entries[0].emptySourceDirectories.empty());

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(emptyDir.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);

        qm.SetPreserveDirectoryStructure(true);
    }

    // Test: nested empty directories — only leaf-most recorded
    {
        qm.SetCreateEmptyDirectories(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\NestedEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring eA = srcSub + L"\\a";
        std::wstring eB = eA + L"\\b";
        std::wstring eC = eB + L"\\c";
        EnsureDirectoryExists(eC);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_FALSE(entries[0].emptySourceDirectories.empty());
        // Only leaf-most "c" should be recorded
        ASSERT_EQ(entries[0].emptySourceDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "a\\b\\c");

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(eC.c_str());
        _wrmdir(eB.c_str());
        _wrmdir(eA.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: empty dir alongside files — recorded
    {
        qm.SetCreateEmptyDirectories(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\MixedContent";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\f1.txt", L"1");
        CreateTempFile(srcSub + L"\\f2.txt", L"2");
        std::wstring emptyDir = srcSub + L"\\emptySub";
        EnsureDirectoryExists(emptyDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);
        ASSERT_FALSE(entries[0].emptySourceDirectories.empty());
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "emptySub");

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\f1.txt");
        RemoveFile(srcSub + L"\\f2.txt");
        _wrmdir(emptyDir.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: non-empty dir not recorded
    {
        qm.SetCreateEmptyDirectories(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\NoEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring hasFiles = srcSub + L"\\hasFiles";
        EnsureDirectoryExists(hasFiles);
        CreateTempFile(hasFiles + L"\\inner.txt", L"inner");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);
        ASSERT_TRUE(entries[0].emptySourceDirectories.empty());

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        RemoveFile(hasFiles + L"\\inner.txt");
        _wrmdir(hasFiles.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: CreateEmptyDirectories disabled — no empty dirs collected
    {
        qm.SetCreateEmptyDirectories(false);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\DisabledEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring emptyDir = srcSub + L"\\empty";
        EnsureDirectoryExists(emptyDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_TRUE(entries[0].emptySourceDirectories.empty());

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(emptyDir.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: full directory tree with mixed content
    {
        qm.SetCreateEmptyDirectories(true);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\FullTree";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\root.txt", L"root");
        std::wstring subA = srcSub + L"\\subA";
        EnsureDirectoryExists(subA);
        CreateTempFile(subA + L"\\a.txt", L"a");
        std::wstring emptyD = srcSub + L"\\emptyD";
        EnsureDirectoryExists(emptyD);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);
        ASSERT_FALSE(entries[0].emptySourceDirectories.empty());
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "emptyD");

        // Verify structure paths contain "FullTree"
        for (const auto& e : entries) {
            ASSERT_TRUE(e.destinationDirectories[0].find("FullTree") != std::string::npos);
        }

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\root.txt");
        RemoveFile(subA + L"\\a.txt");
        _wrmdir(subA.c_str());
        _wrmdir(emptyD.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    qm.SetCreateEmptyDirectories(false);
    qm.SetPreserveDirectoryStructure(false);
    qm.SetEnableDirectoryMoves(false);

    std::cout << "  create_empty_directories tests done." << std::endl;
}

// ==================== Copy vs Move mode tests ====================

static void TestCopyVsMoveMode() {
    std::cout << "Testing copy_vs_move_mode..." << std::endl;

    QueueManager qm;

    // Test: CP mode entry creation
    {
        qm.SetDebugMode(DebugTransferMode::CP);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\cpfile.txt";
        CreateTempFile(testFile, L"cp content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].debugTransferMode, DebugTransferMode::CP);

        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: MV mode entry creation
    {
        qm.SetDebugMode(DebugTransferMode::MV);

        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring testFile = testDir + L"\\mvfile.txt";
        CreateTempFile(testFile, L"mv content");

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcFile = WStringToString(testFile);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcFile}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].debugTransferMode, DebugTransferMode::MV);

        qm.CancelAllEntries();
        RemoveFile(testFile);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: mode change mid-session
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string dest = WStringToString(destDir);

        // Queue one in CP mode
        std::wstring f1 = testDir + L"\\cp1.txt";
        CreateTempFile(f1, L"cp1");
        std::string s1 = WStringToString(f1);
        std::wstring error1;
        qm.SetDebugMode(DebugTransferMode::CP);
        bool ok1 = qm.PrepareBatch("grp-001", {s1}, {dest}, error1);
        ASSERT_TRUE(ok1);

        // Queue one in MV mode
        std::wstring f2 = testDir + L"\\mv1.txt";
        CreateTempFile(f2, L"mv1");
        std::string s2 = WStringToString(f2);
        std::wstring error2;
        qm.SetDebugMode(DebugTransferMode::MV);
        bool ok2 = qm.PrepareBatch("grp-001", {s2}, {dest}, error2);
        ASSERT_TRUE(ok2);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 2ULL);
        ASSERT_EQ(entries[0].debugTransferMode, DebugTransferMode::CP);
        ASSERT_EQ(entries[1].debugTransferMode, DebugTransferMode::MV);

        qm.CancelAllEntries();
        RemoveFile(f1);
        RemoveFile(f2);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);

        qm.SetDebugMode(DebugTransferMode::MV);
    }

    std::cout << "  copy_vs_move_mode tests done." << std::endl;
}

// ==================== Settings JSON round-trip tests ====================

static void TestSettingsJsonRoundTrip() {
    std::cout << "Testing settings_json_roundtrip..." << std::endl;

    std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
    EnsureDirectoryExists(testDir);

    // Test: preserveDirectoryStructure round-trip
    {
        AppData data;
        data.settings.preserveDirectoryStructure = true;
        data.settings.createEmptyDirectories = false;

        std::wstring path = testDir + L"\\preserve.json";
        bool saved = SaveAppData(path, data);
        ASSERT_TRUE(saved);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(loaded.settings.preserveDirectoryStructure);
        ASSERT_FALSE(loaded.settings.createEmptyDirectories);

        RemoveFile(path);
    }

    // Test: createEmptyDirectories round-trip
    {
        AppData data;
        data.settings.preserveDirectoryStructure = false;
        data.settings.createEmptyDirectories = true;

        std::wstring path = testDir + L"\\createempty.json";
        bool saved = SaveAppData(path, data);
        ASSERT_TRUE(saved);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(loaded.settings.preserveDirectoryStructure);
        ASSERT_TRUE(loaded.settings.createEmptyDirectories);

        RemoveFile(path);
    }

    // Test: both true round-trip
    {
        AppData data;
        data.settings.preserveDirectoryStructure = true;
        data.settings.createEmptyDirectories = true;

        std::wstring path = testDir + L"\\bothtrue.json";
        bool saved = SaveAppData(path, data);
        ASSERT_TRUE(saved);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(loaded.settings.preserveDirectoryStructure);
        ASSERT_TRUE(loaded.settings.createEmptyDirectories);

        RemoveFile(path);
    }

    // Test: both false by default on new JSON
    {
        std::wstring path = testDir + L"\\defaults.json";
        bool ok = CreateDefaultJson(path);
        ASSERT_TRUE(ok);

        AppData loaded;
        bool loadedOk = LoadAppData(path, loaded);
        ASSERT_TRUE(loadedOk);
        ASSERT_FALSE(loaded.settings.preserveDirectoryStructure);
        ASSERT_FALSE(loaded.settings.createEmptyDirectories);

        RemoveFile(path);
    }

    // Test: legacy JSON without fields — defaults to false
    {
        std::wstring path = testDir + L"\\legacy_settings.json";
        std::string legacyJson = R"({
            "version": 1,
            "lastSelectedGroupId": "",
            "sortMode": "MostRecentlyUsed",
            "placementMode": "UpperLeft",
            "windowWidth": 320,
            "windowHeight": 500,
            "windowLeft": 0,
            "windowTop": 0,
            "groups": []
        })";
        CreateTempFileNarrow(path, legacyJson);

        AppData loaded;
        bool ok = LoadAppData(path, loaded);
        ASSERT_TRUE(ok);
        ASSERT_FALSE(loaded.settings.preserveDirectoryStructure);
        ASSERT_FALSE(loaded.settings.createEmptyDirectories);

        RemoveFile(path);
    }

    RemoveDirectoryTree(testDir);

    std::cout << "  settings_json_roundtrip tests done." << std::endl;
}

// ==================== FindEmptyDirectories edge case tests ====================

static void TestFindEmptyDirectories() {
    std::cout << "Testing find_empty_directories..." << std::endl;

    QueueManager qm;
    qm.SetEnableDirectoryMoves(true);
    qm.SetPreserveDirectoryStructure(true);
    qm.SetCreateEmptyDirectories(true);

    // Test: deeply nested empty chain — only leaf-most recorded
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\DeepChain";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring eA = srcSub + L"\\a";
        std::wstring eB = eA + L"\\b";
        std::wstring eC = eB + L"\\c";
        std::wstring eD = eC + L"\\d";
        EnsureDirectoryExists(eD);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].emptySourceDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "a\\b\\c\\d");

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(eD.c_str());
        _wrmdir(eC.c_str());
        _wrmdir(eB.c_str());
        _wrmdir(eA.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: empty dir with empty subdir — only leaf recorded
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\EmptyParent";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring eA = srcSub + L"\\parent";
        std::wstring eB = eA + L"\\child";
        EnsureDirectoryExists(eB);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].emptySourceDirectories.size(), 1ULL);
        ASSERT_STR_EQ(entries[0].emptySourceDirectories[0], "parent\\child");

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(eB.c_str());
        _wrmdir(eA.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: multiple separate empty directories
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\MultiEmpty";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");
        std::wstring e1 = srcSub + L"\\empty1";
        std::wstring e2 = srcSub + L"\\empty2";
        EnsureDirectoryExists(e1);
        EnsureDirectoryExists(e2);

        std::wstring destDir = testDir + L"\\dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);
        ASSERT_EQ(entries[0].emptySourceDirectories.size(), 2ULL);

        bool found1 = false, found2 = false;
        for (const auto& ed : entries[0].emptySourceDirectories) {
            if (ed == "empty1") found1 = true;
            if (ed == "empty2") found2 = true;
        }
        ASSERT_TRUE(found1);
        ASSERT_TRUE(found2);

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        _wrmdir(e1.c_str());
        _wrmdir(e2.c_str());
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    qm.SetCreateEmptyDirectories(false);
    qm.SetPreserveDirectoryStructure(false);
    qm.SetEnableDirectoryMoves(false);

    std::cout << "  find_empty_directories tests done." << std::endl;
}

// ==================== Source-Dest Conflict with Structure tests ====================

static void TestSourceDestConflictStructure() {
    std::cout << "Testing source_dest_conflict_structure..." << std::endl;

    QueueManager qm;
    qm.SetEnableDirectoryMoves(true);
    qm.SetPreserveDirectoryStructure(true);
    qm.SetCreateEmptyDirectories(false);

    // Test: file already at structured dest path — detected as conflict
    // When source file path matches the computed structured dest path,
    // the entry should be skipped (all destinations match).
    // We test with a file that lives inside the dest tree so the
    // normalized paths match.
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        // Create dest dir, then create the structured path inside it
        std::wstring destDir = testDir + L"\\Dest";
        EnsureDirectoryExists(destDir);

        // Source is a file that already lives at the structured dest location
        std::wstring srcSub = destDir + L"\\MySrc";
        EnsureDirectoryExists(srcSub);
        std::wstring testFile = srcSub + L"\\alreadyhere.txt";
        CreateTempFile(testFile, L"already here");

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        // Should succeed but skip the file (already in all destinations)
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        // File should have been skipped — no entries remain
        ASSERT_TRUE(entries.empty());

        RemoveFile(testFile);
        _wrmdir(srcSub.c_str());
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    // Test: file not at structured dest — no conflict, entry created
    {
        std::wstring testDir = L"C:\\Users\\brad\\AppData\\Local\\Temp\\opencode\\filemove_test";
        EnsureDirectoryExists(testDir);

        std::wstring srcSub = testDir + L"\\SrcNotInDest";
        EnsureDirectoryExists(srcSub);
        CreateTempFile(srcSub + L"\\file.txt", L"data");

        std::wstring destDir = testDir + L"\\Dest";
        EnsureDirectoryExists(destDir);

        std::string srcDir = WStringToString(srcSub);
        std::string dest = WStringToString(destDir);

        std::wstring error;
        bool ok = qm.PrepareBatch("grp-001", {srcDir}, {dest}, error);
        ASSERT_TRUE(ok);

        auto entries = qm.GetQueuedEntries();
        ASSERT_EQ(entries.size(), 1ULL);

        qm.CancelAllEntries();
        RemoveFile(srcSub + L"\\file.txt");
        RemoveDirectoryTree(srcSub);
        RemoveDirectoryTree(destDir);
        RemoveDirectoryTree(testDir);
    }

    qm.SetPreserveDirectoryStructure(false);
    qm.SetEnableDirectoryMoves(false);

    std::cout << "  source_dest_conflict_structure tests done." << std::endl;
}

// ==================== Main ====================

int main() {
    std::cout << "FileMove v1.3.1 - Unit Tests" << std::endl;
    std::cout << "==============================" << std::endl;

    TestCmdlineParser();
    TestFileIo();
    TestJsonParser();
    TestQueueManager();
    TestPreserveDirectoryStructure();
    TestCreateEmptyDirectories();
    TestCopyVsMoveMode();
    TestSettingsJsonRoundTrip();
    TestFindEmptyDirectories();
    TestSourceDestConflictStructure();

    std::cout << "==============================" << std::endl;
    std::cout << "Results: " << gPassed << " passed, " << gFailed << " failed" << std::endl;

    return gFailed > 0 ? 1 : 0;
}
