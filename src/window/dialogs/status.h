#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

// Button IDs
enum StatusButtons {
    IDM_STATUS_OPEN_LOG = 4001,
    IDM_STATUS_NEW,
    IDM_STATUS_OPEN_SELECTED,
    IDM_STATUS_CLOSE,
    IDM_STATUS_JSON_LISTBOX = 4010  // Control ID for JSON file listbox
};

class StatusDialog {
public:
    StatusDialog();
    ~StatusDialog();

    // Callback for when a JSON file is selected and opened
    using JsonFileOpenCallback = std::function<bool(const std::wstring& jsonPath)>;

    // Show the Status dialog (modal). Auto-closes on successful JSON file open.
    void Show(HWND parent, const std::wstring& jsonPath, const std::wstring& logPath,
              JsonFileOpenCallback onJsonFileOpen);

    // Request close (for auto-close after JSON file open)
    void RequestClose();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static StatusDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);
    void UpdateJsonFileList();
    void SelectJsonFile(int row);

    HWND mHWND;
    HINSTANCE mHInstance;
    std::wstring mJsonPath;
    std::wstring mLogPath;
    RECT mClientRect;
    HWND mJsonListHWND;
    std::vector<std::wstring> mJsonFiles;
    int mSelectedRow;
    JsonFileOpenCallback mOnJsonFileOpen;
    bool mCloseRequested;
};
