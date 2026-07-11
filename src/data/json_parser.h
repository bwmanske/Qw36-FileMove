#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::ordered_json;

struct Group {
    std::string id;
    std::string name;
    std::vector<std::string> destinationPaths;
    std::string createdAt;
    std::string updatedAt;
    std::string lastUsedAt;
};

struct AppSettings {
    int version = 1;
    std::string lastSelectedGroupId;
    std::string sortMode = "MostRecentlyUsed";
    std::string placementMode = "UpperLeft";
    int windowWidth = 320;
    int windowHeight = 500;
    int windowLeft = 0;
    int windowTop = 0;
    bool enableDirectoryMoves = false;
    bool preserveDirectoryStructure = false;
    bool createEmptyDirectories = false;
    bool enableSidecarFiles = false;
    bool hideQueuedSourceFiles = false;
};

struct AppData {
    AppSettings settings;
    std::vector<Group> groups;
};

// Load app data from a JSON file. Returns true on success.
// If the file is empty (0 bytes), returns a default empty AppData.
bool LoadAppData(const std::wstring& path, AppData& data);

// Save app data to a JSON file. Returns true on success.
bool SaveAppData(const std::wstring& path, const AppData& data);

// Create a new empty JSON file with default settings.
bool CreateDefaultJson(const std::wstring& path);

// Generate a unique group ID.
std::string GenerateGroupId();

// Get the current ISO 8601 timestamp.
std::string GetIsoTimestamp();
