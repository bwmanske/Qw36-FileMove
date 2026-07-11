#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "window/group_list.h"
#include "data/json_parser.h"
#include "utils/cmdline_parser.h"
#include "drag_drop/drop_target.h"

// Menu item IDs for header context menu
enum HeaderMenu {
    IDM_QUEUE_WINDOW = 1001,
    IDM_STATUS,
    IDM_SETTINGS,
    IDM_SEARCH,
    IDM_ABOUT
};

// Menu item IDs for group context menu
enum GroupMenu {
    IDM_USE_CLIPBOARD = 2001,
    IDM_EDIT,
    IDM_DELETE
};

// Shutdown dialog result
enum ShutdownResult {
    ShutdownCancel,
    ShutdownFinishCurrent,
    ShutdownCancelImmediate
};

// Custom messages
enum CustomMessages {
    WM_UPDATE_STATUSBAR = WM_USER + 1,
    WM_REFRESH_STATUS = WM_USER + 2
};

class MainWindow;

// Global window procedure (must be free function for Win32)
LRESULT CALLBACK MainWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

class MainWindow {
    friend LRESULT CALLBACK MainWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
    MainWindow();
    ~MainWindow();

    // Create the main window
    HWND Create(HINSTANCE hInstance, int nCmdShow);

    // Update the group list from app data
    void UpdateGroups(const std::vector<Group>& groups, SortMode sortMode);

    // Update status bar
    void UpdateStatusBar(int queuedCount, const std::wstring& workerState);

    // Get the window handle
    HWND GetHWND() const;

    // Get the group list
    GroupList& GetGroupList();

    // Show shutdown prompt, returns the user's choice
    ShutdownResult ShowShutdownPrompt();

    // Set the JSON base name for the title
    void SetJsonBaseName(const std::wstring& baseName);

    // Apply window placement
    void ApplyPlacement(PlacementMode mode, int savedWidth, int savedHeight,
                       int savedLeft, int savedTop);

    // Handle drop on a group row
    void OnDrop(int row, const std::vector<std::string>& files);

    // Handle clipboard paste for selected group
    void OnUseClipboard();

private:
    // Instance data accessor
    static MainWindow* GetInstance(HWND hwnd);

    // Handle header button click
    void OnHeaderClick();

    // Handle header right-click
    void OnHeaderRightClick(POINT pt);

    // Handle group right-click
    void OnGroupRightClick(const std::string& groupId, POINT pt);

    // Handle paint
    void OnPaint(HDC hdc);

    // Handle size change
    void OnSize(int width, int height);

    // Handle command from menu
    void OnCommand(int id);

    // Draw header
    void DrawHeader(HDC hdc);

    // Draw status bar
    void DrawStatusBar(HDC hdc);

    // ListView subclass procedure for tooltip mouse tracking
    static LRESULT CALLBACK ListViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Custom tooltip window procedure
    static LRESULT CALLBACK TooltipWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Show/hide tracked tooltip
    void ShowTrackedTooltip(int row);
    void HideTrackedTooltip();
    void AutoHideTooltip();

    // Handle tooltip timer
    void OnTooltipTimer();

    // Handle drag highlight poll timer
    void OnDragHighlightTimer();

    // Show/hide custom tooltip
    void ShowCustomTooltip(const std::wstring& text, POINT screenPos);

    HWND mHWND;
    HINSTANCE mHInstance;

    GroupList mGroupList;
    std::wstring mJsonBaseName;
    HWND mCustomTooltipHWND;
    std::wstring mCustomTooltipText;
    int mPendingTooltipRow;
    std::wstring mPendingTooltipText;
    UINT_PTR mTooltipTimerId;
    UINT_PTR mAutoHideTimerId;
    WNDPROC mOriginalListViewProc;

    // Layout
    int mHeaderHeight;
    int mStatusBarHeight;
    RECT mClientRect;

    // Status bar state
    int mQueuedCount;
    std::wstring mWorkerState;

    // Window state for placement
    int mWindowWidth;
    int mWindowHeight;
    int mWindowLeft;
    int mWindowTop;

    // Drag-and-drop
    DropTarget* mDropTarget;
    DWORD mDropCookie;
    int mDragHighlightRow;
    POINT mLastClickPt;
    bool mLeftButtonDown;
};

// Global main window instance
extern MainWindow gMainWindow;
