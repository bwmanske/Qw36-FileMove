#include "cmdline_parser.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <vector>
#include <optional>

static std::wstring ToUpperW(const std::wstring& s) {
    std::wstring result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return result;
}

static std::string ToUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

static std::wstring Trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Tokenize command line respecting quoted strings
static std::vector<std::wstring> TokenizeCommandLine(const std::wstring& cmdLine) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool inQuote = false;
    size_t i = 0;
    while (i < cmdLine.size()) {
        wchar_t c = cmdLine[i];
        if (c == L'"') {
            inQuote = !inQuote;
            i++;
        } else if ((c == L' ' || c == L'\t') && !inQuote) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
        } else {
            current += c;
            i++;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

static bool IsValidExtension(const std::wstring& value, const std::wstring& expectedExt) {
    size_t dotPos = value.find_last_of(L'.');
    if (dotPos == std::wstring::npos) return true; // No extension is OK
    std::wstring ext = value.substr(dotPos);
    return ToUpperW(ext) == ToUpperW(expectedExt);
}

static std::wstring EnsureExtension(const std::wstring& value, const std::wstring& ext) {
    size_t dotPos = value.find_last_of(L'.');
    if (dotPos == std::wstring::npos) {
        return value + ext;
    }
    return value;
}

static bool HasDirectory(const std::wstring& path) {
    // Check if path contains a directory separator or drive letter
    if (path.size() >= 2 && path[1] == L':') return true;
    return path.find(L'\\') != std::wstring::npos || path.find(L'/') != std::wstring::npos;
}

static std::optional<DebugMode> ParseDebugValue(const std::wstring& value) {
    std::wstring upper = ToUpperW(value);
    if (upper == L"MV") return DebugMode::MV;
    if (upper == L"CP") return DebugMode::CP;
    return std::nullopt;
}

static std::optional<SortMode> ParseSortValue(const std::wstring& value) {
    std::wstring upper = ToUpperW(value);
    if (upper == L"MRU" || upper == L"MOSTRECENTLYUSED") return SortMode::MostRecentlyUsed;
    if (upper == L"LRU" || upper == L"LEASTRECENTLYUSED") return SortMode::LeastRecentlyUsed;
    if (upper == L"AF" || upper == L"ADDEDFIRST") return SortMode::AddedFirst;
    if (upper == L"AL" || upper == L"ADDEDLAST") return SortMode::AddedLast;
    if (upper == L"AZ" || upper == L"ATOZ") return SortMode::AtoZ;
    if (upper == L"ZA" || upper == L"ZTOA") return SortMode::ZtoA;
    return std::nullopt;
}

static std::optional<PlacementMode> ParsePlacementValue(const std::wstring& value) {
    std::wstring upper = ToUpperW(value);
    if (upper == L"UL" || upper == L"UPPERLEFT") return PlacementMode::UpperLeft;
    if (upper == L"UR" || upper == L"UPPERRIGHT") return PlacementMode::UpperRight;
    if (upper == L"LL" || upper == L"LOWERLEFT") return PlacementMode::LowerLeft;
    if (upper == L"LR" || upper == L"LOWERRIGHT") return PlacementMode::LowerRight;
    if (upper == L"LAST" || upper == L"LASTLOCATION") return PlacementMode::LastLocation;
    return std::nullopt;
}

bool ParseCommandLine(const std::wstring& commandLine, ParsedCommandLine& result, std::wstring& errorMessage) {
    result.rawCommandLine = commandLine;
    auto tokens = TokenizeCommandLine(commandLine);

    // Skip the first token (executable path)
    size_t i = 1;
    while (i < tokens.size()) {
        std::wstring token = tokens[i];

        // Check for option prefix - skip tokens without / or -
        if (token.size() < 2) {
            i++;
            continue;
        }

        wchar_t prefix = token[0];
        if (prefix != L'/' && prefix != L'-') {
            // Not an option, skip
            i++;
            continue;
        }

        wchar_t option = static_cast<wchar_t>(std::towupper(token[1]));

        switch (option) {
            case L'D': {
                if (i + 1 >= tokens.size()) {
                    errorMessage = L"Missing value for D option (expected MV or CP)";
                    return false;
                }
                auto dm = ParseDebugValue(tokens[i + 1]);
                if (!dm.has_value()) {
                    errorMessage = L"Invalid D value: " + tokens[i + 1] + L" (expected MV or CP)";
                    return false;
                }
                result.debugModeSpecified = true;
                result.debugMode = dm.value();
                i += 2;
                break;
            }
            case L'I': {
                if (i + 1 >= tokens.size()) {
                    errorMessage = L"Missing value for I option (expected JSON file path)";
                    return false;
                }
                if (!IsValidExtension(tokens[i + 1], L".json")) {
                    errorMessage = L"Invalid I file extension: " + tokens[i + 1] + L" (expected .json or no extension)";
                    return false;
                }
                result.inputFileSpecified = true;
                result.inputFile = EnsureExtension(tokens[i + 1], L".json");
                i += 2;
                break;
            }
            case L'O': {
                if (i + 1 >= tokens.size()) {
                    errorMessage = L"Missing value for O option (expected log file path)";
                    return false;
                }
                if (!IsValidExtension(tokens[i + 1], L".log")) {
                    errorMessage = L"Invalid O file extension: " + tokens[i + 1] + L" (expected .log or no extension)";
                    return false;
                }
                result.outputFileSpecified = true;
                result.outputFile = EnsureExtension(tokens[i + 1], L".log");
                i += 2;
                break;
            }
            case L'S': {
                if (i + 1 >= tokens.size()) {
                    errorMessage = L"Missing value for S option (expected sort mode)";
                    return false;
                }
                auto sm = ParseSortValue(tokens[i + 1]);
                if (!sm.has_value()) {
                    errorMessage = L"Invalid S value: " + tokens[i + 1] +
                        L" (expected MRU, LRU, AF, AL, AZ, ZA)";
                    return false;
                }
                result.sortModeSpecified = true;
                result.sortMode = sm.value();
                i += 2;
                break;
            }
            case L'P': {
                if (i + 1 >= tokens.size()) {
                    errorMessage = L"Missing value for P option (expected placement mode)";
                    return false;
                }
                auto pm = ParsePlacementValue(tokens[i + 1]);
                if (!pm.has_value()) {
                    errorMessage = L"Invalid P value: " + tokens[i + 1] +
                        L" (expected UL, UR, LL, LR, LAST)";
                    return false;
                }
                result.placementModeSpecified = true;
                result.placementMode = pm.value();
                i += 2;
                break;
            }
            default:
                errorMessage = L"Unknown option: " + token;
                return false;
        }
    }

    return true;
}

std::string SortModeToString(SortMode mode) {
    switch (mode) {
        case SortMode::MostRecentlyUsed: return "MostRecentlyUsed";
        case SortMode::LeastRecentlyUsed: return "LeastRecentlyUsed";
        case SortMode::AddedFirst: return "AddedFirst";
        case SortMode::AddedLast: return "AddedLast";
        case SortMode::AtoZ: return "AtoZ";
        case SortMode::ZtoA: return "ZtoA";
    }
    return "MostRecentlyUsed";
}

std::string PlacementModeToString(PlacementMode mode) {
    switch (mode) {
        case PlacementMode::UpperLeft: return "UpperLeft";
        case PlacementMode::UpperRight: return "UpperRight";
        case PlacementMode::LowerLeft: return "LowerLeft";
        case PlacementMode::LowerRight: return "LowerRight";
        case PlacementMode::LastLocation: return "LastLocation";
    }
    return "UpperLeft";
}

std::wstring SortModeDisplayName(SortMode mode) {
    switch (mode) {
        case SortMode::MostRecentlyUsed: return L"Most Recently Used";
        case SortMode::LeastRecentlyUsed: return L"Least Recently Used";
        case SortMode::AddedFirst: return L"Added First";
        case SortMode::AddedLast: return L"Added Last";
        case SortMode::AtoZ: return L"A thru Z";
        case SortMode::ZtoA: return L"Z thru A";
    }
    return L"Most Recently Used";
}

std::wstring PlacementModeDisplayName(PlacementMode mode) {
    switch (mode) {
        case PlacementMode::UpperLeft: return L"Upper Left";
        case PlacementMode::UpperRight: return L"Upper Right";
        case PlacementMode::LowerLeft: return L"Lower Left";
        case PlacementMode::LowerRight: return L"Lower Right";
        case PlacementMode::LastLocation: return L"Last Location";
    }
    return L"Upper Left";
}

SortMode SortModeFromString(const std::string& value) {
    if (value == "MostRecentlyUsed") return SortMode::MostRecentlyUsed;
    if (value == "LeastRecentlyUsed") return SortMode::LeastRecentlyUsed;
    if (value == "AddedFirst") return SortMode::AddedFirst;
    if (value == "AddedLast") return SortMode::AddedLast;
    if (value == "AtoZ") return SortMode::AtoZ;
    if (value == "ZtoA") return SortMode::ZtoA;
    return SortMode::MostRecentlyUsed;
}

PlacementMode PlacementModeFromString(const std::string& value) {
    if (value == "UpperLeft") return PlacementMode::UpperLeft;
    if (value == "UpperRight") return PlacementMode::UpperRight;
    if (value == "LowerLeft") return PlacementMode::LowerLeft;
    if (value == "LowerRight") return PlacementMode::LowerRight;
    if (value == "LastLocation") return PlacementMode::LastLocation;
    return PlacementMode::UpperLeft;
}

std::wstring GetCommandLineHelp() {
    return
        L"Valid command-line options:\n"
        L"  /D <MV|CP>  or  -D <MV|CP>    Debug mode (MV=move, CP=copy)\n"
        L"  /I <path>   or  -I <path>     Input JSON file\n"
        L"  /O <path>   or  -O <path>     Output log file\n"
        L"  /S <value>  or  -S <value>    Sort mode (MRU, LRU, AF, AL, AZ, ZA)\n"
        L"  /P <value>  or  -P <value>    Placement mode (UL, UR, LL, LR, LAST)\n";
}
