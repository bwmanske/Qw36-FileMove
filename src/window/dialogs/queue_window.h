#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "data/json_parser.h"

// Button IDs
enum QueueButtons {
    IDM_QUEUE_CLOSE = 5001,
    IDM_QUEUE_PAUSE_RESUME,
    IDM_QUEUE_DELETE,
    IDM_QUEUE_EMPTY
};

// Custom message for async queue refresh from worker thread
#define WM_QUEUE_REFRESH (WM_USER + 100)

class QueueWindow {
public:
    QueueWindow();
    ~QueueWindow();

    // Create the Queue window (modeless)
    HWND Create(HWND parent, HINSTANCE hInstance);

    // Update the queue list
    void UpdateList(const std::vector<std::wstring>& destinations);

    // Get the window handle
    HWND GetHWND() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static QueueWindow* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);
    void UpdateButtonStates();

    HWND mHWND;
    HINSTANCE mHInstance;
    HWND mListHWND;
    HWND mDeleteBtnHWND;
    HWND mEmptyBtnHWND;
    HFONT mCtrlFont;
    RECT mClientRect;
    std::vector<std::wstring> mDestinations;
    bool mIsPaused;
    int mQueuedCount;
    int mProcessedCount;
    std::wstring mWorkerState;
    std::wstring mCurrentFile;
    std::wstring mCurrentDest;
    std::wstring mLastError;
};

// Global queue window instance
extern QueueWindow gQueueWindow;
