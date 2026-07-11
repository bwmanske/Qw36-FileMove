#pragma once

#include <string>
#include <optional>

enum class DebugMode {
    None,
    MV,  // Normal move
    CP   // Debug copy (do not remove source)
};

enum class SortMode {
    MostRecentlyUsed,
    LeastRecentlyUsed,
    AddedFirst,
    AddedLast,
    AtoZ,
    ZtoA
};

enum class PlacementMode {
    UpperLeft,
    UpperRight,
    LowerLeft,
    LowerRight,
    LastLocation
};

struct ParsedCommandLine {
    bool debugModeSpecified = false;
    DebugMode debugMode = DebugMode::None;

    bool inputFileSpecified = false;
    std::wstring inputFile;

    bool outputFileSpecified = false;
    std::wstring outputFile;

    bool sortModeSpecified = false;
    SortMode sortMode = SortMode::MostRecentlyUsed;

    bool placementModeSpecified = false;
    PlacementMode placementMode = PlacementMode::UpperLeft;

    // Raw original command line for logging/About
    std::wstring rawCommandLine;
};

// Parse the command line. Returns true on success.
// On failure, sets errorMessage and returns false.
bool ParseCommandLine(const std::wstring& commandLine, ParsedCommandLine& result, std::wstring& errorMessage);

// Convert SortMode to its JSON string representation
std::string SortModeToString(SortMode mode);

// Convert PlacementMode to its JSON string representation
std::string PlacementModeToString(PlacementMode mode);

// Get the display name for SortMode
std::wstring SortModeDisplayName(SortMode mode);

// Get the display name for PlacementMode
std::wstring PlacementModeDisplayName(PlacementMode mode);

// Convert a JSON string back to SortMode
SortMode SortModeFromString(const std::string& value);

// Convert a JSON string back to PlacementMode
PlacementMode PlacementModeFromString(const std::string& value);

// Get the help text listing all valid options
std::wstring GetCommandLineHelp();
