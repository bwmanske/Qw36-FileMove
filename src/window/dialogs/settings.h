#pragma once

#include <windows.h>
#include <string>
#include "data/json_parser.h"
#include "utils/cmdline_parser.h"

// Radio button IDs for sort order
enum SortRadio {
    IDM_SORT_MRU = 3001,
    IDM_SORT_LRU,
    IDM_SORT_AZ,
    IDM_SORT_ZA,
    IDM_SORT_AF,
    IDM_SORT_AL
};

// Radio button IDs for placement
enum PlacementRadio {
    IDM_PLACEMENT_UL = 3010,
    IDM_PLACEMENT_UR,
    IDM_PLACEMENT_LL,
    IDM_PLACEMENT_LR,
    IDM_PLACEMENT_LAST
};

// Checkbox IDs for options
enum OptionCheck {
    IDM_OPT_DIRECTORY_MOVES = 3020,
    IDM_OPT_PRESERVE_STRUCTURE,
    IDM_OPT_CREATE_EMPTY_DIRS,
    IDM_OPT_SIDECAR_FILES,
    IDM_OPT_HIDDEN_SOURCE
};

// Button IDs
enum SettingsButtons {
    IDM_SETTINGS_OK = 3030,
    IDM_SETTINGS_CANCEL
};

class SettingsDialog {
public:
    SettingsDialog();
    ~SettingsDialog();

    // Show the Settings dialog (modal). Returns true if OK was pressed.
    bool Show(HWND parent, AppSettings& settings);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static SettingsDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);
    void ApplySettings(AppSettings& settings);

    HWND mHWND;
    HINSTANCE mHInstance;
    RECT mClientRect;
    AppSettings mSettings;
    bool mAccepted;
    HWND mPreserveStructureHwnd;
    HWND mCreateEmptyDirsHwnd;
};
