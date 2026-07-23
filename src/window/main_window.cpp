#include "main_window.h"
#include "data/file_io.h"
#include "utils/logging.h"
#include "queue/queue_manager.h"
#include "queue/worker_thread.h"
#include "dialogs/about.h"
#include "dialogs/status.h"
#include "dialogs/settings.h"
#include "dialogs/group_editor.h"
#include "dialogs/queue_window.h"
#include "dialogs/search.h"
#include "dialogs/new_file.h"
#include "resources/resource_loader.h"
#include "resources/resource_ids.h"
#include <objbase.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <algorithm>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

// External globals from main.cpp
extern std::wstring gCommandLine;
extern AppData gAppData;
extern std::wstring gJsonPath;
extern std::wstring gLogPath;

// Define missing macros excluded by WIN32_LEAN_AND_MEAN
#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#ifndef GWL_WNDPROC
#define GWL_WNDPROC (-4)
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#ifndef TD_INITIAL
#define TD_INITIAL 0
#endif

using namespace Gdiplus;

MainWindow gMainWindow;

MainWindow::MainWindow()
    : mHWND(NULL)
    , mHeaderHeight(28)
    , mStatusBarHeight(22)
    , mQueuedCount(0)
    , mWorkerState(L"Idle")
    , mWindowWidth(320)
    , mWindowHeight(500)
    , mWindowLeft(0)
    , mWindowTop(0)
    , mCustomTooltipHWND(NULL)
    , mPendingTooltipRow(-1)
    , mTooltipTimerId(0)
    , mAutoHideTimerId(0)
    , mOriginalListViewProc(NULL)
    , mDropTarget(NULL)
    , mDropCookie(0)
    , mDragHighlightRow(-1)
    , mLeftButtonDown(false)
    , mGearImage(NULL)
    , mRightClickedGroupId()
{
    mLastClickPt.x = 0;
    mLastClickPt.y = 0;
}

MainWindow::~MainWindow() {
    if (mGroupList.GetHWND() && mOriginalListViewProc) {
        SetWindowLongPtrW(mGroupList.GetHWND(), GWL_WNDPROC, (LONG_PTR)mOriginalListViewProc);
    }
    if (mGearImage) {
        delete static_cast<Image*>(mGearImage);
        mGearImage = NULL;
    }
}

HWND MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    mHInstance = hInstance;
    DebugConsoleWriteLine(L"MainWindow::Create entered");
    LogInfo(L"MainWindow::Create entered");

    // Load application icon from embedded resource
    HICON hIcon = LoadEmbeddedIcon(hInstance, 32);
    HICON hIconSm = LoadEmbeddedIcon(hInstance, 16);

    // Load gear image from embedded resource
    mGearImage = LoadEmbeddedPng(hInstance, IDR_GEAR_IMAGE);
    DebugConsoleWriteLine(L"MainWindow::Create: icons loaded (hIcon=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(hIcon)) + L", hIconSm=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(hIconSm)) + L")");

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindowWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveMainClass";
    wc.hIcon = hIcon;
    wc.hIconSm = hIconSm;

    if (!RegisterClassExW(&wc)) {
        DebugConsoleWriteLine(L"MainWindow::Create: RegisterClassExW FAILED");
        LogInfo(L"MainWindow::Create: RegisterClassExW FAILED");
        return NULL;
    }
    DebugConsoleWriteLine(L"MainWindow::Create: RegisterClassExW succeeded");

    mHWND = CreateWindowExW(
        0,
        L"FileMoveMainClass",
        L"FileMove",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        mWindowWidth, mWindowHeight,
        NULL, NULL, hInstance, this
    );

    if (mHWND) {
        DebugConsoleWriteLine(L"MainWindow::Create: CreateWindowExW succeeded, HWND=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(mHWND)));

        // Override the OS corner policy to force classic square corners
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        HRESULT hr = DwmSetWindowAttribute(
            mHWND,
            DWMWA_WINDOW_CORNER_PREFERENCE,
            &cornerPreference,
            sizeof(cornerPreference)
        );
        if (FAILED(hr)) {
            std::wstring hrMsg = L"MainWindow::Create: DwmSetWindowAttribute FAILED hr=0x" + std::to_wstring(hr);
            DebugConsoleWriteLine(hrMsg);
            LogInfo(hrMsg);
        } else {
            DebugConsoleWriteLine(L"MainWindow::Create: DwmSetWindowAttribute succeeded");
        }
    } else {
        DWORD err = GetLastError();
        wchar_t errBuf[256];
        swprintf_s(errBuf, L"MainWindow::Create: CreateWindowExW returned NULL! GetLastError=%lu", err);
        DebugConsoleWriteLine(errBuf);
        LogInfo(errBuf);
        return NULL;
    }

    // Store this pointer in window GWL_USERDATA
    SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Set window icons (taskbar, alt-tab)
    if (hIcon) SendMessageW(mHWND, WM_SETICON, ICON_BIG, reinterpret_cast<WPARAM>(hIcon));
    if (hIconSm) SendMessageW(mHWND, WM_SETICON, ICON_SMALL, reinterpret_cast<WPARAM>(hIconSm));

    // Create group list
    RECT clientRect;
    GetClientRect(mHWND, &clientRect);
    RECT listRect;
    listRect.left = 4;
    listRect.top = mHeaderHeight + 2;
    listRect.right = clientRect.right - 4;
    listRect.bottom = clientRect.bottom - mStatusBarHeight - 4;
    mGroupList.Create(mHWND, listRect);
    mGroupList.SetIconDirectory(L"");
    // Ensure column width matches list width
    ListView_SetColumnWidth(mGroupList.GetHWND(), 0, listRect.right - listRect.left);
    // Subclass ListView to catch WM_MOUSEMOVE for manual tooltip
    mOriginalListViewProc = (WNDPROC)SetWindowLongPtrW(mGroupList.GetHWND(), GWL_WNDPROC, (LONG_PTR)ListViewWndProc);

    // Register custom tooltip window class
    WNDCLASSEXW tc = {};
    tc.cbSize = sizeof(WNDCLASSEXW);
    tc.style = CS_HREDRAW | CS_VREDRAW;
    tc.lpfnWndProc = TooltipWndProc;
    tc.hInstance = hInstance;
    tc.hCursor = LoadCursor(NULL, IDC_ARROW);
    tc.hbrBackground = (HBRUSH)(COLOR_INFOBK + 1);
    tc.lpszClassName = L"FileMoveTooltipClass";
    RegisterClassExW(&tc);

    DebugConsoleWriteLine(L"MainWindow::Create: calling ShowWindow with nCmdShow=" + std::to_wstring(nCmdShow));
    ShowWindow(mHWND, nCmdShow);
    BOOL visibleAfter = IsWindowVisible(mHWND);
    DebugConsoleWriteLine(L"MainWindow::Create: after ShowWindow, IsWindowVisible=" + std::to_wstring(visibleAfter));

    DebugConsoleWriteLine(L"MainWindow::Create: calling UpdateWindow");
    UpdateWindow(mHWND);

    // Enable drag-and-drop on main window
    DragAcceptFiles(mHWND, TRUE);

    // Start a timer to poll for drag highlight (100ms interval)
    SetTimer(mHWND, 3, 100, NULL);

    // Start status bar refresh timer (500ms interval)
    SetTimer(mHWND, 4, 500, NULL);

    DebugConsoleWriteLine(L"MainWindow::Create: returning HWND=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(mHWND)));
    return mHWND;
}

void MainWindow::UpdateGroups(const std::vector<Group>& groups, SortMode sortMode) {
    mGroupList.UpdateFromData(groups, sortMode);
}

void MainWindow::UpdateStatusBar(int queuedCount, const std::wstring& workerState) {
    if (queuedCount != mQueuedCount || workerState != mWorkerState) {
        mQueuedCount = queuedCount;
        mWorkerState = workerState;

        if (mHWND && mStatusBarHeight > 0) {
            RECT clientRect;
            GetClientRect(mHWND, &clientRect);
            RECT statusRect;
            statusRect.left = 0;
            statusRect.top = clientRect.bottom - mStatusBarHeight;
            statusRect.right = clientRect.right;
            statusRect.bottom = clientRect.bottom;
            InvalidateRect(mHWND, &statusRect, FALSE);
        }
    }
}

HWND MainWindow::GetHWND() const {
    return mHWND;
}

GroupList& MainWindow::GetGroupList() {
    return mGroupList;
}

void MainWindow::SetJsonBaseName(const std::wstring& baseName) {
    mJsonBaseName = baseName;

    std::wstring title = L"FileMove (";
    title += baseName;
    title += L")";

    if (mHWND) {
        SetWindowTextW(mHWND, title.c_str());
    }
}

void MainWindow::ApplyPlacement(PlacementMode mode, int savedWidth, int savedHeight,
                                int savedLeft, int savedTop) {
    mWindowWidth = savedWidth;
    mWindowHeight = savedHeight;
    mWindowLeft = savedLeft;
    mWindowTop = savedTop;

    if (!mHWND) return;

    // Get screen dimensions
    HMONITOR monitor = MonitorFromWindow(mHWND, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(monitor, &mi);

    int screenWidth = mi.rcWork.right - mi.rcWork.left;
    int screenHeight = mi.rcWork.bottom - mi.rcWork.top;

    // Ensure minimum size
    int width = std::max(mWindowWidth, 280);
    int height = std::max(mWindowHeight, 200);

    // Log startup placement info
    std::wstring placementModeStr;
    switch (mode) {
        case PlacementMode::UpperLeft: placementModeStr = L"UpperLeft"; break;
        case PlacementMode::UpperRight: placementModeStr = L"UpperRight"; break;
        case PlacementMode::LowerLeft: placementModeStr = L"LowerLeft"; break;
        case PlacementMode::LowerRight: placementModeStr = L"LowerRight"; break;
        case PlacementMode::LastLocation: placementModeStr = L"LastLocation"; break;
    }
    LogInfo(L"Startup placement: mode=" + placementModeStr +
        L", size=" + std::to_wstring(width) + L"x" + std::to_wstring(height) +
        L", savedPos=(" + std::to_wstring(savedLeft) + L"," + std::to_wstring(savedTop) + L")");
    DebugConsoleWriteLine(L"Startup placement: mode=" + placementModeStr +
        L", size=" + std::to_wstring(width) + L"x" + std::to_wstring(height) +
        L", savedPos=(" + std::to_wstring(savedLeft) + L"," + std::to_wstring(savedTop) + L")");

    int x, y;
    bool wasClamped = false;

    switch (mode) {
        case PlacementMode::UpperLeft:
            x = mi.rcWork.left;
            y = mi.rcWork.top;
            break;
        case PlacementMode::UpperRight:
            x = mi.rcWork.right - width;
            y = mi.rcWork.top;
            break;
        case PlacementMode::LowerLeft:
            x = mi.rcWork.left;
            y = mi.rcWork.bottom - height;
            break;
        case PlacementMode::LowerRight:
            x = mi.rcWork.right - width;
            y = mi.rcWork.bottom - height;
            break;
        case PlacementMode::LastLocation:
            x = savedLeft;
            y = savedTop;
            // Clamp to screen bounds
            if (x < mi.rcWork.left) { x = mi.rcWork.left; wasClamped = true; }
            if (y < mi.rcWork.top) { y = mi.rcWork.top; wasClamped = true; }
            if (x + width > mi.rcWork.right) { x = mi.rcWork.right - width; wasClamped = true; }
            if (y + height > mi.rcWork.bottom) { y = mi.rcWork.bottom - height; wasClamped = true; }
            break;
    }

    if (wasClamped) {
        LogInfo(L"Window was off-screen, restored within visible bounds: initial=(" +
            std::to_wstring(savedLeft) + L"," + std::to_wstring(savedTop) +
            L"), adjusted=(" + std::to_wstring(x) + L"," + std::to_wstring(y) + L")");
        DebugConsoleWriteLine(L"Window was off-screen, restored within visible bounds: initial=(" +
            std::to_wstring(savedLeft) + L"," + std::to_wstring(savedTop) +
            L"), adjusted=(" + std::to_wstring(x) + L"," + std::to_wstring(y) + L")");
    }

    SetWindowPos(mHWND, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

ShutdownResult MainWindow::ShowShutdownPrompt() {
    // Use MessageBox with custom text
    int result = MessageBoxW(
        mHWND,
        L"File transfers are still active.\n\n"
        L"Finish the current file across all destinations, or stop\n"
        L"immediately and leave the source file in place.\n\n"
        L"Click Yes to finish current file.\n"
        L"Click No to cancel immediately.\n"
        L"Click Cancel to return to the app.",
        L"Shutdown",
        MB_YESNOCANCEL | MB_ICONWARNING | MB_DEFBUTTON1
    );

    switch (result) {
        case IDYES: return ShutdownFinishCurrent;
        case IDNO: return ShutdownCancelImmediate;
        default: return ShutdownCancel;
    }
}

// Static instance accessor
MainWindow* MainWindow::GetInstance(HWND hwnd) {
    return reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

// Global window procedure (must be static/global)
LRESULT CALLBACK MainWindowWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* instance = MainWindow::GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<MainWindow*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            return TRUE;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            instance->OnSize(width, height);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            instance->OnPaint(hdc);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            instance->OnCommand(id);
            break;
        }

        case WM_NOTIFY: {
            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == 0 && nmhdr->code == NM_RCLICK) {
                // Right-click on group list
                NMLISTVIEW* nmlv = reinterpret_cast<NMLISTVIEW*>(nmhdr);
                if (nmlv->iItem >= 0) {
                    std::string groupId = instance->mGroupList.GetGroupIdAtRow(nmlv->iItem);
                    POINT pt;
                    GetCursorPos(&pt);
                    ScreenToClient(hwnd, &pt);
                    instance->OnGroupRightClick(groupId, pt);
                }
            } else if (nmhdr->idFrom == 0 && nmhdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* nmlv = reinterpret_cast<NMLISTVIEW*>(nmhdr);
                if (nmlv->uNewState & LVIS_SELECTED) {
                    instance->mGroupList.ClearHighlight();
                }
            } else if (nmhdr->idFrom == 0 && nmhdr->code == NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW* nmcd = reinterpret_cast<NMLVCUSTOMDRAW*>(nmhdr);
                switch (nmcd->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT: {
                        return CDRF_NOTIFYITEMDRAW;
                    }
                    case CDDS_ITEMPREPAINT: {
                        if (!instance->mGroupList.GetHighlightedGroup().empty()) {
                            std::string groupName = instance->mGroupList.GetGroupNameAtRow(static_cast<int>(nmcd->nmcd.dwItemSpec));
                            if (groupName == instance->mGroupList.GetHighlightedGroup()) {
                                nmcd->clrTextBk = GetSysColor(COLOR_HIGHLIGHT);
                                nmcd->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
                                return CDRF_DODEFAULT;
                            }
                        }
                        return CDRF_DODEFAULT;
                    }
                }
                return CDRF_DODEFAULT;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                instance->OnTooltipTimer();
            } else if (wParam == 2) {
                // Auto-hide tooltip (don't reset pending state)
                instance->AutoHideTooltip();
            } else if (wParam == 3) {
                // Drag highlight poll timer
                instance->OnDragHighlightTimer();
            } else if (wParam == 4) {
                // Status bar refresh timer - only update if state changed
                int queued = gQueueManager.GetQueuedDestinationCount();
                WorkerState ws = gWorkerThread.GetState();
                std::wstring stateStr;
                switch (ws) {
                    case WorkerState::Idle: stateStr = L"Idle"; break;
                    case WorkerState::Moving: stateStr = L"Moving"; break;
                    case WorkerState::ManualPause: stateStr = L"Manual Pause"; break;
                    case WorkerState::PausedError: stateStr = L"Paused - Error"; break;
                }
                if (queued != instance->mQueuedCount || stateStr != instance->mWorkerState) {
                    instance->UpdateStatusBar(queued, stateStr);
                }
            }
            break;
        }

        case WM_UPDATE_STATUSBAR: {
            int queued = static_cast<int>(wParam);
            WorkerState ws = static_cast<WorkerState>(lParam);
            std::wstring stateStr;
            switch (ws) {
                case WorkerState::Idle: stateStr = L"Idle"; break;
                case WorkerState::Moving: stateStr = L"Moving"; break;
                case WorkerState::ManualPause: stateStr = L"Manual Pause"; break;
                case WorkerState::PausedError: stateStr = L"Paused - Error"; break;
            }
            if (queued != instance->mQueuedCount || stateStr != instance->mWorkerState) {
                instance->mQueuedCount = queued;
                instance->mWorkerState = stateStr;
                // Trigger immediate repaint of status bar
                if (instance->mHWND && instance->mStatusBarHeight > 0) {
                    RECT clientRect;
                    GetClientRect(instance->mHWND, &clientRect);
                    RECT statusRect;
                    statusRect.left = 0;
                    statusRect.top = clientRect.bottom - instance->mStatusBarHeight;
                    statusRect.right = clientRect.right;
                    statusRect.bottom = clientRect.bottom;
                    InvalidateRect(instance->mHWND, &statusRect, FALSE);
                }
            }
            break;
        }

        case WM_CONTEXTMENU: {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            // Convert screen coords to client coords
            ScreenToClient(hwnd, &pt);

            if (pt.y < instance->mHeaderHeight) {
                int gearSize = instance->mHeaderHeight;
                int gearRight = instance->mClientRect.right - 5;
                int gearLeft = gearRight - gearSize;
                if (pt.x >= gearLeft && pt.x <= gearRight) {
                    instance->OnHeaderRightClick(pt);
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);

            if (pt.y < instance->mHeaderHeight) {
                int gearSize = instance->mHeaderHeight;
                int gearRight = instance->mClientRect.right - 5;
                int gearLeft = gearRight - gearSize;
                if (pt.x >= gearLeft && pt.x <= gearRight) {
                    instance->OnHeaderRightClick(pt);
                } else {
                    instance->OnHeaderClick();
                }
            }
            break;
        }

        case WM_RBUTTONDOWN: {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);

            if (pt.y < instance->mHeaderHeight) {
                int gearSize = instance->mHeaderHeight;
                int gearRight = instance->mClientRect.right - 5;
                int gearLeft = gearRight - gearSize;
                if (pt.x >= gearLeft && pt.x <= gearRight) {
                    instance->OnHeaderRightClick(pt);
                }
            }
            break;
        }

        case WM_CLOSE: {
            // Check if worker is active or queue has items
            bool workerActive = gWorkerThread.IsBusy();
            bool queueHasItems = gQueueManager.GetQueuedDestinationCount() > 0;

            if (workerActive || queueHasItems) {
                ShutdownResult result = instance->ShowShutdownPrompt();
                switch (result) {
                    case ShutdownResult::ShutdownCancel:
                        // User chose to cancel shutdown
                        break;
                    case ShutdownResult::ShutdownFinishCurrent:
                        gWorkerThread.RequestShutdown(ShutdownAction::FinishCurrentAndExit);
                        PostQuitMessage(0);
                        break;
                    case ShutdownResult::ShutdownCancelImmediate:
                        gWorkerThread.RequestShutdown(ShutdownAction::CancelAndExit);
                        PostQuitMessage(0);
                        break;
                }
            } else {
                PostQuitMessage(0);
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 3);
            KillTimer(hwnd, 4);
            PostQuitMessage(0);
            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            if (fileCount > 0) {
                POINT pt;
                DragQueryPoint(hDrop, &pt);
                MapWindowPoints(hwnd, instance->mGroupList.GetHWND(), &pt, 1);

                LVHITTESTINFO hti = {};
                hti.pt = pt;
                int row = ListView_HitTest(instance->mGroupList.GetHWND(), &hti);

                if (row >= 0) {
                    std::vector<std::string> files;
                    for (UINT i = 0; i < fileCount; i++) {
                        wchar_t path[MAX_PATH];
                        UINT len = DragQueryFileW(hDrop, i, path, _countof(path));
                        if (len > 0 && len < _countof(path)) {
                            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, path, (int)len, NULL, 0, NULL, NULL);
                            if (utf8Len <= 0) continue;
                            std::string narrow;
                            narrow.resize(utf8Len);
                            WideCharToMultiByte(CP_UTF8, 0, path, (int)len, &narrow[0], utf8Len, NULL, NULL);
                            files.push_back(narrow);
                        }
                    }
                    if (!files.empty()) {
                        instance->OnDrop(row, files);
                    }
                }
            }
            DragFinish(hDrop);
            break;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void MainWindow::OnHeaderClick() {
    GroupEditorDialog editorDlg;
    auto onGroupSaved = [this](const Group& group) {
        gAppData.groups.push_back(group);
        SaveAppData(gJsonPath, gAppData);
        SortMode sortMode = SortModeFromString(gAppData.settings.sortMode);
        UpdateGroups(gAppData.groups, sortMode);
    };

    editorDlg.Show(mHWND, nullptr, gAppData.groups, onGroupSaved);
}

void MainWindow::OnHeaderRightClick(POINT pt) {
    HMENU hMenu = CreatePopupMenu();

    int queuedCount = gQueueManager.GetQueuedDestinationCount();
    std::wstring queueLabel = L"Queue Window (";
    queueLabel += std::to_wstring(queuedCount);
    queueLabel += L")";

    AppendMenuW(hMenu, MF_STRING, IDM_STATUS, L"Active JSON");
    AppendMenuW(hMenu, MF_STRING, IDM_QUEUE_WINDOW, queueLabel.c_str());
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings");
    AppendMenuW(hMenu, MF_STRING, IDM_SEARCH, L"Search");
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"About");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, mHWND, NULL);
    DestroyMenu(hMenu);

    mGroupList.ClearHighlight();
}

void MainWindow::OnGroupRightClick(const std::string& groupId, POINT pt) {
    mRightClickedGroupId = groupId;

    HMENU hMenu = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_USE_CLIPBOARD, L"Use Clipboard");
    AppendMenuW(hMenu, MF_STRING, IDM_EDIT, L"Edit");
    AppendMenuW(hMenu, MF_STRING, IDM_DELETE, L"Delete...");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, mHWND, NULL);
    DestroyMenu(hMenu);
}

    void MainWindow::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    DrawHeader(hdc);
    DrawStatusBar(hdc);
}

void MainWindow::OnSize(int width, int height) {
    // Update client rect
    GetClientRect(mHWND, &mClientRect);

    // Resize group list
    if (mGroupList.GetHWND()) {
        RECT listRect;
        listRect.left = 4;
        listRect.top = mHeaderHeight + 2;
        listRect.right = width - 4;
        listRect.bottom = height - mStatusBarHeight - 4;
        MoveWindow(mGroupList.GetHWND(), listRect.left, listRect.top,
                   listRect.right - listRect.left, listRect.bottom - listRect.top, TRUE);
        // Update column width to match new list width
        ListView_SetColumnWidth(mGroupList.GetHWND(), 0, listRect.right - listRect.left);
    }
}

void MainWindow::OnCommand(int id) {
    switch (id) {
        case IDM_QUEUE_WINDOW: {
            if (!gQueueWindow.GetHWND()) {
                gQueueWindow.Create(mHWND, mHInstance);
            }
            // Refresh queue list
            {
                auto entries = gQueueManager.GetQueuedEntries();
                std::vector<std::wstring> items;
                for (const auto& e : entries) {
                    std::wstring src(e.sourceFilePath.begin(), e.sourceFilePath.end());
                    for (const auto& d : e.destinationDirectories) {
                        std::wstring dest(d.begin(), d.end());
                        items.push_back(src + L" -> " + dest);
                    }
                }
                gQueueWindow.UpdateList(items);
            }
            SetForegroundWindow(gQueueWindow.GetHWND());
            break;
        }
        case IDM_STATUS: {
            auto onJsonFileOpen = [this](const std::wstring& jsonPath) -> bool {
                AppData newData;
                if (!LoadAppData(jsonPath, newData)) {
                    MessageBoxW(mHWND, L"Failed to load JSON file (malformed).",
                        L"Error", MB_ICONERROR);
                    return false;
                }

                gAppData = newData;
                gJsonPath = jsonPath;
                gLogPath = GetDefaultDataDirectory() + L"\\" + GetBaseName(jsonPath) + L".log";
                OpenLogFile(gLogPath);

                LogInfo(L"JSON file switched: " + GetTimestamp() + L" (" + jsonPath + L")");
                LogInfo(L"LOG file opened: " + GetTimestamp() + L" (" + gLogPath + L")");
                LogInfo(L"Sort mode: " + std::wstring(gAppData.settings.sortMode.begin(), gAppData.settings.sortMode.end()));
                LogInfo(L"Placement mode: " + std::wstring(gAppData.settings.placementMode.begin(), gAppData.settings.placementMode.end()));
                LogInfo(L"Window size: " + std::to_wstring(gAppData.settings.windowWidth) + L" x " + std::to_wstring(gAppData.settings.windowHeight));
                LogInfo(L"Window position: Left " + std::to_wstring(gAppData.settings.windowLeft) + L", Top " + std::to_wstring(gAppData.settings.windowTop));

                gQueueManager.SetEnableSidecarFiles(gAppData.settings.enableSidecarFiles);
                gQueueManager.SetHideQueuedSourceFiles(gAppData.settings.hideQueuedSourceFiles);
                gQueueManager.SetEnableDirectoryMoves(gAppData.settings.enableDirectoryMoves);
                gQueueManager.SetPreserveDirectoryStructure(gAppData.settings.preserveDirectoryStructure);
                gQueueManager.SetCreateEmptyDirectories(gAppData.settings.createEmptyDirectories);

                SetJsonBaseName(GetBaseName(jsonPath));
                SortMode sortMode = SortModeFromString(gAppData.settings.sortMode);
                UpdateGroups(gAppData.groups, sortMode);

                return true;
            };

            StatusDialog statusDlg;
            statusDlg.Show(mHWND, gJsonPath, gLogPath, onJsonFileOpen);
            break;
        }
        case IDM_SETTINGS: {
            SettingsDialog settingsDlg;
            bool accepted = settingsDlg.Show(mHWND, gAppData.settings);
            if (accepted) {
                SaveAppData(gJsonPath, gAppData);
                gQueueManager.SetEnableSidecarFiles(gAppData.settings.enableSidecarFiles);
                gQueueManager.SetHideQueuedSourceFiles(gAppData.settings.hideQueuedSourceFiles);
                gQueueManager.SetEnableDirectoryMoves(gAppData.settings.enableDirectoryMoves);
                gQueueManager.SetPreserveDirectoryStructure(gAppData.settings.preserveDirectoryStructure);
                gQueueManager.SetCreateEmptyDirectories(gAppData.settings.createEmptyDirectories);

                SortMode sortMode = SortModeFromString(gAppData.settings.sortMode);
                UpdateGroups(gAppData.groups, sortMode);
            }
            break;
        }
        case IDM_SEARCH: {
            mGroupList.ClearHighlight();
            SearchDialog searchDlg;
            std::vector<std::string> groupNames;
            for (const auto& group : gAppData.groups) {
                groupNames.push_back(group.name);
            }
            std::wstring selectedName = searchDlg.Show(mHWND, groupNames);
            if (!selectedName.empty()) {
                int len = WideCharToMultiByte(CP_UTF8, 0, selectedName.c_str(), -1, NULL, 0, NULL, NULL);
                if (len > 1) {
                    std::string selectedNameStr(len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, selectedName.c_str(), -1, &selectedNameStr[0], len, NULL, NULL);
                    mGroupList.ScrollToGroup(selectedNameStr);
                    mGroupList.HighlightGroup(selectedNameStr);
                }
            }
            break;
        }
        case IDM_ABOUT: {
            AboutDialog aboutDlg;
            aboutDlg.Show(mHWND, gCommandLine);
            break;
        }
        case IDM_USE_CLIPBOARD: {
            OnUseClipboard();
            break;
        }
        case IDM_EDIT: {
            std::string selectedId = mRightClickedGroupId;
            const Group* targetGroup = nullptr;
            for (const auto& group : gAppData.groups) {
                if (group.id == selectedId) {
                    targetGroup = &group;
                    break;
                }
            }

            if (targetGroup) {
                GroupEditorDialog editorDlg;
                auto onGroupSaved = [this](const Group& group) {
                    bool found = false;
                    for (auto& g : gAppData.groups) {
                        if (g.id == group.id) {
                            g = group;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        gAppData.groups.push_back(group);
                    }
                    SaveAppData(gJsonPath, gAppData);
                    SortMode sortMode = SortModeFromString(gAppData.settings.sortMode);
                    UpdateGroups(gAppData.groups, sortMode);
                };

                editorDlg.Show(mHWND, targetGroup, gAppData.groups, onGroupSaved);
            }
            break;
        }
        case IDM_DELETE: {
            std::string selectedId = mRightClickedGroupId;
            std::string selectedName;

            for (const auto& group : gAppData.groups) {
                if (group.id == selectedId) {
                    selectedName = group.name;
                    break;
                }
            }

            int result = MessageBoxW(mHWND,
                (L"Are you sure you want to delete group \"" +
                std::wstring(selectedName.begin(), selectedName.end()) +
                L"\"?").c_str(),
                L"Delete Group", MB_YESNO | MB_ICONWARNING);

            if (result == IDYES) {
                gAppData.groups.erase(
                    std::remove_if(gAppData.groups.begin(), gAppData.groups.end(),
                        [&selectedId](const Group& g) { return g.id == selectedId; }),
                    gAppData.groups.end());
                SaveAppData(gJsonPath, gAppData);
                SortMode sortMode = SortModeFromString(gAppData.settings.sortMode);
                UpdateGroups(gAppData.groups, sortMode);
            }
            break;
        }
    }
}

void MainWindow::DrawHeader(HDC hdc) {
    RECT headerRect;
    headerRect.left = 0;
    headerRect.top = 0;
    headerRect.right = mClientRect.right;
    headerRect.bottom = mHeaderHeight;

    // Draw header background
    HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 245));
    FillRect(hdc, &headerRect, bgBrush);
    DeleteObject(bgBrush);

    int clientWidth = mClientRect.right;
    int gearSize = mHeaderHeight;
    int gearRight = clientWidth - 5;
    int gearLeft = gearRight - gearSize;
    int newGroupRight = gearLeft;

    // Draw "New Group" button (5px left, right = gearLeft - 5, flat)
    RECT newGroupRect;
    newGroupRect.left = 5;
    newGroupRect.top = 0;
    newGroupRect.right = newGroupRight;
    newGroupRect.bottom = mHeaderHeight;

    HBRUSH btnBrush = CreateSolidBrush(RGB(220, 220, 230));
    FillRect(hdc, &newGroupRect, btnBrush);
    DeleteObject(btnBrush);

    // Draw gear button (flat, right-aligned at 10px from edge)
    RECT gearRect;
    gearRect.left = gearLeft;
    gearRect.top = 0;
    gearRect.right = gearRight;
    gearRect.bottom = mHeaderHeight;

    FillRect(hdc, &gearRect, btnBrush);
    DeleteObject(btnBrush);

    // Draw separator line (after button fill so it's visible)
    MoveToEx(hdc, 0, mHeaderHeight - 1, NULL);
    LineTo(hdc, clientWidth, mHeaderHeight - 1);

    // Draw "New Group" text (non-bold for compactness)
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    ncm.lfSmCaptionFont.lfWeight = FW_NORMAL;
    HFONT hFont = CreateFontIndirectW(&ncm.lfSmCaptionFont);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, L"New Group", -1, &newGroupRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    // Draw gear image centered in gear button, scaled to header height
    if (mGearImage) {
        Graphics graphics(hdc);
        int imgMargin = 2;
        int imgSize = gearSize - imgMargin * 2;
        int imgX = gearLeft + imgMargin;
        int imgY = imgMargin;
        graphics.DrawImage(static_cast<Image*>(mGearImage), imgX, imgY, imgSize, imgSize);
    }
}

LRESULT CALLBACK MainWindow::ListViewWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* instance = &gMainWindow;
    if (instance && instance->mOriginalListViewProc) {
        if (msg == WM_MOUSEMOVE) {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            LVHITTESTINFO hti = {};
            hti.pt = pt;
            int row = ListView_HitTest(hwnd, &hti);
            if (row >= 0) {
                instance->ShowTrackedTooltip(row);
            } else {
                instance->HideTrackedTooltip();
            }
        } else if (msg == WM_MOUSELEAVE) {
            instance->HideTrackedTooltip();
        }
        return CallWindowProcW(instance->mOriginalListViewProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::ShowTrackedTooltip(int row) {
    // If same row already pending or shown, do nothing
    if (mPendingTooltipRow == row) return;

    // Hide any existing tooltip and kill timer
    HideTrackedTooltip();

    // Store pending row and text
    mPendingTooltipRow = row;
    mPendingTooltipText = mGroupList.GetTooltipForRow(row);
    if (mPendingTooltipText.empty()) {
        mPendingTooltipRow = -1;
        return;
    }

    // Start 500ms timer before showing
    mTooltipTimerId = SetTimer(mHWND, 1, 500, NULL);
}

void MainWindow::HideTrackedTooltip() {
    // Kill pending timer
    if (mTooltipTimerId != 0) {
        KillTimer(mHWND, mTooltipTimerId);
        mTooltipTimerId = 0;
    }

    // Kill auto-hide timer
    if (mAutoHideTimerId != 0) {
        KillTimer(mHWND, mAutoHideTimerId);
        mAutoHideTimerId = 0;
    }

    // Destroy custom tooltip window
    if (mCustomTooltipHWND) {
        DestroyWindow(mCustomTooltipHWND);
        mCustomTooltipHWND = NULL;
    }

    mPendingTooltipRow = -1;
    mPendingTooltipText.clear();
    mCustomTooltipText.clear();
}

void MainWindow::AutoHideTooltip() {
    // Kill auto-hide timer
    if (mAutoHideTimerId != 0) {
        KillTimer(mHWND, mAutoHideTimerId);
        mAutoHideTimerId = 0;
    }

    // Destroy custom tooltip window but keep pending state
    if (mCustomTooltipHWND) {
        DestroyWindow(mCustomTooltipHWND);
        mCustomTooltipHWND = NULL;
    }
}

void MainWindow::ShowCustomTooltip(const std::wstring& text, POINT screenPos) {
    mCustomTooltipText = text;

    // Measure text to determine window size (max width 500px)
    HDC hdc = GetDC(NULL);
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);

    // Bold font for first line
    ncm.lfMessageFont.lfWeight = FW_BOLD;
    HFONT hBoldFont = CreateFontIndirectW(&ncm.lfMessageFont);

    // Normal font for remaining lines
    ncm.lfMessageFont.lfWeight = FW_NORMAL;
    HFONT hNormalFont = CreateFontIndirectW(&ncm.lfMessageFont);

    HFONT hOldFont = (HFONT)SelectObject(hdc, hBoldFont);

    // Measure each line separately to get accurate height
    int maxLineWidth = 0;
    int totalHeight = 0;
    int lineCount = 0;
    const wchar_t* start = text.c_str();
    const wchar_t* p = start;
    int maxWidth = 500;
    while (*p) {
        const wchar_t* lineStart = p;
        while (*p && *p != L'\r' && *p != L'\n') p++;
        int lineLen = static_cast<int>(p - lineStart);

        HFONT hMeasureFont = (lineCount == 0) ? hBoldFont : hNormalFont;
        SelectObject(hdc, hMeasureFont);
        RECT lr = { 0, 0, maxWidth, 0 };
        DrawTextW(hdc, lineStart, lineLen, &lr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        if (lr.right - lr.left > maxLineWidth) maxLineWidth = lr.right - lr.left;
        totalHeight += lr.bottom - lr.top;
        lineCount++;

        // Skip \r\n or \n
        if (*p == L'\r') { p++; }
        if (*p == L'\n') { p++; }
    }
    int w = maxLineWidth + 16;
    int h = totalHeight + 12;

    SelectObject(hdc, hOldFont);
    DeleteObject(hBoldFont);
    DeleteObject(hNormalFont);
    ReleaseDC(NULL, hdc);

    int x = screenPos.x + 16;
    int y = screenPos.y + 16;

    // Ensure tooltip stays within screen bounds
    HMONITOR monitor = MonitorFromPoint(screenPos, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(monitor, &mi);
    if (x + w > mi.rcWork.right) x = screenPos.x - w - 8;
    if (y + h > mi.rcWork.bottom) y = screenPos.y - h - 8;
    if (x < mi.rcWork.left) x = mi.rcWork.left;
    if (y < mi.rcWork.top) y = mi.rcWork.top;

    mCustomTooltipHWND = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"FileMoveTooltipClass",
        NULL,
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        mHWND,
        NULL,
        mHInstance,
        NULL
    );

    if (mCustomTooltipHWND) {
        // Auto-hide after 5 seconds
        mAutoHideTimerId = SetTimer(mHWND, 2, 10000, NULL);
    }
}

LRESULT CALLBACK MainWindow::TooltipWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Fill background with tooltip color
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_INFOBK + 1));

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_INFOTEXT));
            NONCLIENTMETRICSW ncm = {};
            ncm.cbSize = sizeof(NONCLIENTMETRICSW);
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);

            // Bold font for first line
            ncm.lfMessageFont.lfWeight = FW_BOLD;
            HFONT hBoldFont = CreateFontIndirectW(&ncm.lfMessageFont);

            // Normal font for remaining lines
            ncm.lfMessageFont.lfWeight = FW_NORMAL;
            HFONT hNormalFont = CreateFontIndirectW(&ncm.lfMessageFont);

            HFONT hOldFont = (HFONT)SelectObject(hdc, hBoldFont);

            RECT textRect = ps.rcPaint;
            textRect.left += 8;
            textRect.top += 6;
            textRect.right -= 8;
            textRect.bottom -= 6;

            // Split text by \r\n and draw each line
            const std::wstring& text = gMainWindow.mCustomTooltipText;
            const wchar_t* start = text.c_str();
            const wchar_t* p = start;
            int y = textRect.top;
            int lineIndex = 0;

            while (*p) {
                const wchar_t* lineStart = p;
                while (*p && *p != L'\r' && *p != L'\n') p++;
                int lineLen = static_cast<int>(p - lineStart);

                // Switch font: bold for line 0, normal for rest
                if (lineIndex > 0) {
                    SelectObject(hdc, hNormalFont);
                }

                RECT lineRect = textRect;
                lineRect.top = y;
                lineRect.bottom = y + 999;
                DrawTextW(hdc, lineStart, lineLen, &lineRect, DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);

                // Measure this line's height for Y offset
                RECT mRect = { textRect.left, 0, textRect.right, 0 };
                HFONT hMeasureFont = (lineIndex == 0) ? hBoldFont : hNormalFont;
                SelectObject(hdc, hMeasureFont);
                DrawTextW(hdc, lineStart, lineLen, &mRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
                SelectObject(hdc, (lineIndex == 0) ? hBoldFont : hNormalFont);
                y += mRect.bottom - mRect.top;

                // Skip \r\n
                if (*p == L'\r') p++;
                if (*p == L'\n') p++;
                lineIndex++;
            }

            SelectObject(hdc, hOldFont);
            DeleteObject(hBoldFont);
            DeleteObject(hNormalFont);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_NCPAINT:
            return 0;
        case WM_NCACTIVATE:
            return TRUE;
        case WM_NCDESTROY:
            gMainWindow.mCustomTooltipHWND = NULL;
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void MainWindow::OnTooltipTimer() {
    KillTimer(mHWND, 1);
    mTooltipTimerId = 0;

    if (mPendingTooltipRow < 0 || mPendingTooltipText.empty()) return;

    // Verify cursor is still over the same row
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(mGroupList.GetHWND(), &pt);
    LVHITTESTINFO hti = {};
    hti.pt = pt;
    int row = ListView_HitTest(mGroupList.GetHWND(), &hti);
    if (row != mPendingTooltipRow) return;

    // Get fresh screen coordinates for tooltip positioning
    GetCursorPos(&pt);
    ShowCustomTooltip(mPendingTooltipText, pt);
}

void MainWindow::OnDragHighlightTimer() {
    HWND lvHWND = mGroupList.GetHWND();
    if (!lvHWND) return;

    // Check if left mouse button is down (drag in progress)
    if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        // Button released - clear highlight
        if (mDragHighlightRow >= 0) {
            LVITEMW lvi = {};
            lvi.iItem = mDragHighlightRow;
            lvi.iSubItem = 0;
            lvi.mask = LVIF_STATE;
            lvi.state = 0;
            lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItem(lvHWND, &lvi);
            mDragHighlightRow = -1;
        }
        return;
    }

    // Get cursor position in screen coords
    POINT pt;
    GetCursorPos(&pt);

    // Check if cursor is over the ListView
    RECT lvRect;
    GetWindowRect(lvHWND, &lvRect);
    if (!PtInRect(&lvRect, pt)) {
        if (mDragHighlightRow >= 0) {
            LVITEMW lvi = {};
            lvi.iItem = mDragHighlightRow;
            lvi.iSubItem = 0;
            lvi.mask = LVIF_STATE;
            lvi.state = 0;
            lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItem(lvHWND, &lvi);
            mDragHighlightRow = -1;
        }
        return;
    }

    // Convert to ListView client coords
    POINT clientPt = { pt.x - lvRect.left, pt.y - lvRect.top };
    LVHITTESTINFO hti = {};
    hti.pt = clientPt;
    int row = ListView_HitTest(lvHWND, &hti);

    if (row >= 0 && row != mDragHighlightRow) {
        // Clear previous highlight
        if (mDragHighlightRow >= 0) {
            LVITEMW lvi = {};
            lvi.iItem = mDragHighlightRow;
            lvi.iSubItem = 0;
            lvi.mask = LVIF_STATE;
            lvi.state = 0;
            lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItem(lvHWND, &lvi);
        }
        // Highlight new row
        LVITEMW lvi = {};
        lvi.iItem = row;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_STATE;
        lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItem(lvHWND, &lvi);
        ListView_EnsureVisible(lvHWND, row, FALSE);
        mDragHighlightRow = row;
    }
}

void MainWindow::DrawStatusBar(HDC hdc) {
    RECT statusRect;
    statusRect.left = 0;
    statusRect.top = mClientRect.bottom - mStatusBarHeight;
    statusRect.right = mClientRect.right;
    statusRect.bottom = mClientRect.bottom;

    // Draw status bar background
    HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 245));
    FillRect(hdc, &statusRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw separator line
    MoveToEx(hdc, 0, statusRect.top, NULL);
    LineTo(hdc, statusRect.right, statusRect.top);

    // Draw "Queued: X" left justified (non-bold for compactness)
    std::wstring queuedText = L"Queued: ";
    queuedText += std::to_wstring(mQueuedCount);
    RECT leftRect = statusRect;
    leftRect.left += 5;
    leftRect.right = statusRect.right / 2;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    NONCLIENTMETRICSW ncm = {};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    ncm.lfSmCaptionFont.lfWeight = FW_NORMAL;
    HFONT hFont = CreateFontIndirectW(&ncm.lfSmCaptionFont);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    DrawTextW(hdc, queuedText.c_str(), -1, &leftRect, DT_VCENTER | DT_SINGLELINE);

    // Draw "Status: X" right justified
    std::wstring statusText = L"Status: ";
    statusText += mWorkerState;
    RECT rightRect = statusRect;
    rightRect.left = statusRect.right / 2;
    rightRect.right -= 5;
    DrawTextW(hdc, statusText.c_str(), -1, &rightRect, DT_VCENTER | DT_SINGLELINE | DT_RIGHT);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void MainWindow::OnDrop(int row, const std::vector<std::string>& files) {
    // Clear drag highlight
    if (mDragHighlightRow >= 0 && mGroupList.GetHWND()) {
        LVITEMW lvi = {};
        lvi.iItem = mDragHighlightRow;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_STATE;
        lvi.state = 0;
        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItem(mGroupList.GetHWND(), &lvi);
        mDragHighlightRow = -1;
    }

    if (row < 0 || row >= mGroupList.GetRowCount()) return;

    std::string groupId = mGroupList.GetGroupIdAtRow(row);
    if (groupId.empty()) return;

    // Find the group to get destinations
    const Group* group = nullptr;
    for (const auto& g : gAppData.groups) {
        if (g.id == groupId) {
            group = &g;
            break;
        }
    }
    if (!group) return;

    // Prepare batch
    std::wstring errorMsg;
    bool prepared = gQueueManager.PrepareBatch(groupId, files, group->destinationPaths, errorMsg);

    if (!prepared) {
        MessageBoxW(mHWND, errorMsg.c_str(), L"Queue Error", MB_ICONERROR | MB_OK | MB_TOPMOST);
        return;
    }

    // Check for unavailable destinations and warn
    // (PrepareBatch already handles this, but we may want to show a warning)

    // Release to worker
    gQueueManager.ReleasePreparedEntries();
    gWorkerThread.Notify();

    // Update group lastUsedAt
    for (auto& g : gAppData.groups) {
        if (g.id == groupId) {
            g.lastUsedAt = GetIsoTimestamp();
            break;
        }
    }
    SaveAppData(gJsonPath, gAppData);
}

void MainWindow::OnUseClipboard() {
    if (!OpenClipboard(mHWND)) {
        MessageBoxW(mHWND, L"Could not open clipboard.", L"Clipboard Error", MB_ICONERROR);
        return;
    }

    // Check for file list
    if (!IsClipboardFormatAvailable(CF_HDROP)) {
        CloseClipboard();
        MessageBoxW(mHWND, L"Clipboard does not contain file paths.",
            L"Clipboard Error", MB_ICONERROR);
        return;
    }

    HDROP hDrop = reinterpret_cast<HDROP>(GetClipboardData(CF_HDROP));
    CloseClipboard();

    if (!hDrop) {
        MessageBoxW(mHWND, L"Could not read clipboard data.",
            L"Clipboard Error", MB_ICONERROR);
        return;
    }

    // Get file list
    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    std::vector<std::string> files;
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t path[MAX_PATH];
        UINT len = DragQueryFileW(hDrop, i, path, _countof(path));
        if (len > 0 && len < _countof(path)) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, path, (int)len, NULL, 0, NULL, NULL);
            if (utf8Len <= 0) continue;
            std::string narrow;
            narrow.resize(utf8Len);
            WideCharToMultiByte(CP_UTF8, 0, path, (int)len, &narrow[0], utf8Len, NULL, NULL);
            files.push_back(narrow);
        }
    }

    if (files.empty()) {
        MessageBoxW(mHWND, L"No valid files found in clipboard.",
            L"Clipboard Error", MB_ICONERROR);
        return;
    }

    // Get right-clicked group
    std::string groupId = mRightClickedGroupId;
    if (groupId.empty()) {
        MessageBoxW(mHWND, L"No group selected. Select a group first.",
            L"Clipboard Error", MB_ICONERROR);
        return;
    }

    // Find the group
    const Group* group = nullptr;
    for (const auto& g : gAppData.groups) {
        if (g.id == groupId) {
            group = &g;
            break;
        }
    }
    if (!group) return;

    // Prepare batch
    std::wstring errorMsg;
    bool prepared = gQueueManager.PrepareBatch(groupId, files, group->destinationPaths, errorMsg);

    if (!prepared) {
        MessageBoxW(mHWND, errorMsg.c_str(), L"Queue Error", MB_ICONERROR | MB_TOPMOST);
        return;
    }

    // Release to worker
    gQueueManager.ReleasePreparedEntries();
    gWorkerThread.Notify();

    // Clear clipboard after successful use
    if (OpenClipboard(mHWND)) {
        EmptyClipboard();
        CloseClipboard();
    }

    // Update group lastUsedAt
    for (auto& g : gAppData.groups) {
        if (g.id == groupId) {
            g.lastUsedAt = GetIsoTimestamp();
            break;
        }
    }
    SaveAppData(gJsonPath, gAppData);
}
