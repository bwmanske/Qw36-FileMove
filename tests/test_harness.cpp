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

// Helper: remove directory
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

    // Test: GetIsoTimestamp produces valid format
    {
        std::string ts = GetIsoTimestamp();
        ASSERT_FALSE(ts.empty());
        // ISO 8601 format: YYYY-MM-DDTHH:MM:SS
        ASSERT_EQ(ts.size(), 19ULL);
        ASSERT_TRUE(ts[4] == '-' && ts[7] == '-' && ts[10] == 'T');
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

// ==================== Main ====================

int main() {
    std::cout << "FileMove v1.3.0 - Unit Tests" << std::endl;
    std::cout << "==============================" << std::endl;

    TestCmdlineParser();
    TestFileIo();
    TestJsonParser();
    TestQueueManager();

    std::cout << "==============================" << std::endl;
    std::cout << "Results: " << gPassed << " passed, " << gFailed << " failed" << std::endl;

    return gFailed > 0 ? 1 : 0;
}
