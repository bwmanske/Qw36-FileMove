#include "json_parser.h"
#include "file_io.h"
#include "utils/logging.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <random>
#include <algorithm>

std::string GenerateGroupId() {
    auto now = std::chrono::system_clock::now();
    auto count = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::mt19937 gen(static_cast<unsigned>(count));
    std::uniform_int_distribution<> dis(0, 0xFFFFFF);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "grp-%06x", dis(gen));
    return std::string(buf);
}

std::string GetIsoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    auto secTp = std::chrono::time_point<std::chrono::system_clock,
        std::chrono::seconds>(std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()));
    std::time_t tt = std::chrono::system_clock::to_time_t(secTp);
    struct tm lt;
    localtime_s(&lt, &tt);

    long timezoneOffset = 0;
    _get_timezone(&timezoneOffset);
    int offsetMinutes = -static_cast<int>(timezoneOffset / 60);
    char tzBuf[8];
    std::snprintf(tzBuf, sizeof(tzBuf), "%c%02d:%02d",
        offsetMinutes >= 0 ? '+' : '-',
        std::abs(offsetMinutes) / 60,
        std::abs(offsetMinutes) % 60);

    std::ostringstream oss;
    oss << std::put_time(&lt, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << "00000" << tzBuf;
    return oss.str();
}

static int SortModeStringToInt(const std::string& val) {
    if (val == "MostRecentlyUsed") return 0;
    if (val == "LeastRecentlyUsed") return 1;
    if (val == "AddedFirst") return 2;
    if (val == "AddedLast") return 3;
    if (val == "AtoZ") return 4;
    if (val == "ZtoA") return 5;
    return 0;
}

static int PlacementModeStringToInt(const std::string& val) {
    if (val == "UpperLeft") return 0;
    if (val == "UpperRight") return 1;
    if (val == "LowerLeft") return 2;
    if (val == "LowerRight") return 3;
    if (val == "LastLocation") return 4;
    return 0;
}

static json AppDataToJson(const AppData& data) {
    json j;
    j["Version"] = data.settings.version;
    j["LastSelectedGroupId"] = data.settings.lastSelectedGroupId;
    j["SortMode"] = SortModeStringToInt(data.settings.sortMode);
    j["PlacementMode"] = PlacementModeStringToInt(data.settings.placementMode);
    j["EnableDirectoryMoves"] = data.settings.enableDirectoryMoves;
    j["PreserveDirectoryStructure"] = data.settings.preserveDirectoryStructure;
    j["CreateEmptyDirectories"] = data.settings.createEmptyDirectories;
    j["EnableSidecarFiles"] = data.settings.enableSidecarFiles;
    j["HideQueuedSourceFiles"] = data.settings.hideQueuedSourceFiles;
    j["WindowWidth"] = data.settings.windowWidth;
    j["WindowHeight"] = data.settings.windowHeight;
    j["WindowLeft"] = data.settings.windowLeft;
    j["WindowTop"] = data.settings.windowTop;

    json groupsArr = json::array();
    for (const auto& g : data.groups) {
        json gj;
        gj["Id"] = g.id;
        gj["Name"] = g.name;
        gj["DestinationPaths"] = g.destinationPaths;
        gj["CreatedAt"] = g.createdAt;
        gj["UpdatedAt"] = g.updatedAt;
        gj["LastUsedAt"] = g.lastUsedAt;
        groupsArr.push_back(gj);
    }
    j["Groups"] = groupsArr;

    return j;
}

static std::string GetStringSafe(const json& j, const std::string& key, const std::string& defaultValue) {
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    return defaultValue;
}

static int GetIntSafe(const json& j, const std::string& key, int defaultValue) {
    if (j.contains(key) && j[key].is_number_integer()) return j[key].get<int>();
    return defaultValue;
}

static bool GetBoolSafe(const json& j, const std::string& key, bool defaultValue) {
    if (j.contains(key) && j[key].is_boolean()) return j[key].get<bool>();
    return defaultValue;
}

static std::string GetStringBoth(const json& j, const std::string& camelKey, const std::string& pascalKey, const std::string& defaultValue) {
    std::string val = GetStringSafe(j, camelKey, std::string(""));
    if (!val.empty()) return val;
    return GetStringSafe(j, pascalKey, defaultValue);
}

static int GetIntBoth(const json& j, const std::string& camelKey, const std::string& pascalKey, int defaultValue) {
    if (j.contains(camelKey) && j[camelKey].is_number_integer()) return j[camelKey].get<int>();
    return GetIntSafe(j, pascalKey, defaultValue);
}

static bool GetBoolBoth(const json& j, const std::string& camelKey, const std::string& pascalKey, bool defaultValue) {
    if (j.contains(camelKey) && j[camelKey].is_boolean()) return j[camelKey].get<bool>();
    return GetBoolSafe(j, pascalKey, defaultValue);
}

static bool IsIntBoth(const json& j, const std::string& camelKey, const std::string& pascalKey) {
    if (j.contains(camelKey) && j[camelKey].is_number_integer()) return true;
    if (j.contains(pascalKey) && j[pascalKey].is_number_integer()) return true;
    return false;
}

static int GetIntBothRaw(const json& j, const std::string& camelKey, const std::string& pascalKey, int defaultValue) {
    if (j.contains(camelKey) && j[camelKey].is_number_integer()) return j[camelKey].get<int>();
    return GetIntSafe(j, pascalKey, defaultValue);
}

static std::string SortModeIntToString(int val) {
    switch (val) {
        case 0: return "MostRecentlyUsed";
        case 1: return "LeastRecentlyUsed";
        case 2: return "AddedFirst";
        case 3: return "AddedLast";
        case 4: return "AtoZ";
        case 5: return "ZtoA";
        default: return "MostRecentlyUsed";
    }
}

static std::string PlacementModeIntToString(int val) {
    switch (val) {
        case 0: return "UpperLeft";
        case 1: return "UpperRight";
        case 2: return "LowerLeft";
        case 3: return "LowerRight";
        case 4: return "LastLocation";
        default: return "UpperLeft";
    }
}

static void JsonToAppData(const json& j, AppData& data) {
    // Handle both camelCase and PascalCase keys, and integer/string for SortMode/PlacementMode
    data.settings.version = GetIntBoth(j, "version", "Version", 1);
    data.settings.lastSelectedGroupId = GetStringBoth(j, "lastSelectedGroupId", "LastSelectedGroupId", std::string(""));

    // SortMode: may be string or integer
    if (IsIntBoth(j, "sortMode", "SortMode")) {
        data.settings.sortMode = SortModeIntToString(GetIntBothRaw(j, "sortMode", "SortMode", 0));
    } else {
        data.settings.sortMode = GetStringBoth(j, "sortMode", "SortMode", std::string("MostRecentlyUsed"));
    }

    // PlacementMode: may be string or integer
    if (IsIntBoth(j, "placementMode", "PlacementMode")) {
        data.settings.placementMode = PlacementModeIntToString(GetIntBothRaw(j, "placementMode", "PlacementMode", 0));
    } else {
        data.settings.placementMode = GetStringBoth(j, "placementMode", "PlacementMode", std::string("UpperLeft"));
    }

    data.settings.windowWidth = GetIntBoth(j, "windowWidth", "WindowWidth", 320);
    data.settings.windowHeight = GetIntBoth(j, "windowHeight", "WindowHeight", 500);
    data.settings.windowLeft = GetIntBoth(j, "windowLeft", "WindowLeft", 0);
    data.settings.windowTop = GetIntBoth(j, "windowTop", "WindowTop", 0);
    data.settings.enableDirectoryMoves = GetBoolBoth(j, "enableDirectoryMoves", "EnableDirectoryMoves", false);
    data.settings.preserveDirectoryStructure = GetBoolBoth(j, "preserveDirectoryStructure", "PreserveDirectoryStructure", false);
    data.settings.createEmptyDirectories = GetBoolBoth(j, "createEmptyDirectories", "CreateEmptyDirectories", false);
    data.settings.enableSidecarFiles = GetBoolBoth(j, "enableSidecarFiles", "EnableSidecarFiles", false);
    data.settings.hideQueuedSourceFiles = GetBoolBoth(j, "hideQueuedSourceFiles", "HideQueuedSourceFiles", false);

    data.groups.clear();
    LogInfo(L"JSON: parsing groups array");
    DebugConsoleWriteLine(L"JSON: parsing groups array");
    // Debug: dump all top-level keys
    {
        std::wstring keysDebug = L"JSON: top-level keys: ";
        for (auto it = j.begin(); it != j.end(); ++it) {
            keysDebug += L"[" + std::wstring(it.key().begin(), it.key().end()) + L"] ";
        }
        LogInfo(keysDebug);
        DebugConsoleWriteLine(keysDebug);
    }
    // Handle both "groups" and "Groups"
    const json* groupsArr = nullptr;
    if (j.contains("groups") && j["groups"].is_array()) {
        groupsArr = &j["groups"];
    } else if (j.contains("Groups") && j["Groups"].is_array()) {
        groupsArr = &j["Groups"];
    }

    if (groupsArr) {
        int groupIdx = 0;
        for (const auto& gj : *groupsArr) {
            Group g;
            g.id = GetStringBoth(gj, "id", "Id", std::string(""));
            g.name = GetStringBoth(gj, "name", "Name", std::string(""));

            std::wstring groupInfo = L"JSON: Group " + std::to_wstring(groupIdx + 1) +
                L" id=[" + std::wstring(g.id.begin(), g.id.end()) + L"] name=[" +
                std::wstring(g.name.begin(), g.name.end()) + L"]";

            // Handle both "destinationPaths" and "DestinationPaths"
            if (gj.contains("destinationPaths") && gj["destinationPaths"].is_array()) {
                for (const auto& dp : gj["destinationPaths"]) {
                    std::string destPath = dp.get<std::string>();
                    g.destinationPaths.push_back(destPath);
                    groupInfo += L" dest=[" + std::wstring(destPath.begin(), destPath.end()) + L"]";
                }
            } else if (gj.contains("DestinationPaths") && gj["DestinationPaths"].is_array()) {
                for (const auto& dp : gj["DestinationPaths"]) {
                    std::string destPath = dp.get<std::string>();
                    g.destinationPaths.push_back(destPath);
                    groupInfo += L" dest=[" + std::wstring(destPath.begin(), destPath.end()) + L"]";
                }
            } else if (gj.contains("DestinationPath")) {
                // Legacy migration: single DestinationPath -> one-item list
                std::string destPath = gj["DestinationPath"].get<std::string>();
                g.destinationPaths.push_back(destPath);
                groupInfo += L" dest(legacy)[" + std::wstring(destPath.begin(), destPath.end()) + L"]";
            }

            g.createdAt = GetStringBoth(gj, "createdAt", "CreatedAt", std::string(""));
            g.updatedAt = GetStringBoth(gj, "updatedAt", "UpdatedAt", std::string(""));
            g.lastUsedAt = GetStringBoth(gj, "lastUsedAt", "LastUsedAt", std::string(""));
            data.groups.push_back(g);
            LogInfo(groupInfo);
            DebugConsoleWriteLine(groupInfo);
            groupIdx++;
        }
        LogInfo(L"JSON: total groups read: " + std::to_wstring(groupIdx));
        DebugConsoleWriteLine(L"JSON: total groups read: " + std::to_wstring(groupIdx));
    } else {
        LogInfo(L"JSON: no groups array found in JSON");
        DebugConsoleWriteLine(L"JSON: no groups array found in JSON");
    }
}

bool LoadAppData(const std::wstring& path, AppData& data) {
    LogInfo(L"LoadAppData: attempting to load " + path);
    DebugConsoleWriteLine(L"LoadAppData: attempting to load " + path);

    long long size = GetFileSize(path);
    LogInfo(L"LoadAppData: file size = " + std::to_wstring(size));
    DebugConsoleWriteLine(L"LoadAppData: file size = " + std::to_wstring(size));
    if (size == -1) {
        LogInfo(L"LoadAppData: GetFileSize failed");
        DebugConsoleWriteLine(L"LoadAppData: GetFileSize failed");
        return false;
    }

    // Empty file: return default
    if (size == 0) {
        LogInfo(L"LoadAppData: file is empty, returning default AppData");
        DebugConsoleWriteLine(L"LoadAppData: file is empty, returning default AppData");
        data = AppData();
        return true;
    }

    std::ifstream inFile(path, std::ios::binary);
    if (!inFile.is_open()) {
        LogInfo(L"LoadAppData: failed to open file");
        DebugConsoleWriteLine(L"LoadAppData: failed to open file");
        return false;
    }

    std::stringstream ss;
    ss << inFile.rdbuf();
    inFile.close();

    std::string content = ss.str();

    LogInfo(L"LoadAppData: content length = " + std::to_wstring(content.size()));
    DebugConsoleWriteLine(L"LoadAppData: content length = " + std::to_wstring(content.size()));

    try {
        json j = json::parse(content);
        LogInfo(L"LoadAppData: JSON parse succeeded");
        DebugConsoleWriteLine(L"LoadAppData: JSON parse succeeded");
        JsonToAppData(j, data);
        LogInfo(L"LoadAppData: completed successfully, groups count = " + std::to_wstring(data.groups.size()));
        DebugConsoleWriteLine(L"LoadAppData: completed successfully, groups count = " + std::to_wstring(data.groups.size()));
        return true;
    } catch (const json::parse_error& e) {
        std::string whatStr(e.what());
        LogInfo(L"LoadAppData: JSON parse error - " + std::wstring(whatStr.begin(), whatStr.end()));
        DebugConsoleWriteLine(L"LoadAppData: JSON parse error - " + std::wstring(whatStr.begin(), whatStr.end()));
        return false;
    }
}

bool SaveAppData(const std::wstring& path, const AppData& data) {
    json j = AppDataToJson(data);

    // Convert wide path to UTF-8 for std::ofstream
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) return false;
    std::string utf8Path(utf8Len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &utf8Path[0], utf8Len, NULL, NULL);

    std::ofstream outFile(utf8Path, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) return false;

    std::string pretty = j.dump(4);
    outFile << pretty;
    outFile.close();

    return true;
}

bool CreateDefaultJson(const std::wstring& path) {
    AppData data;
    return SaveAppData(path, data);
}
