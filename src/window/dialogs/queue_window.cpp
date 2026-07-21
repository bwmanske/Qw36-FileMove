#include "queue_window.h"
#include "utils/logging.h"
#include "resources/resource_loader.h"
#include "queue/worker_thread.h"
#include "queue/queue_manager.h"
#include "window/main_window.h"
#include <dwmapi.h>
#include <commctrl.h>
#include <algorithm>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#ifndef LB_RESETCONTENT
#define LB_RESETCONTENT 379
#endif

#ifndef LB_ADDSTRING
#define LB_ADDSTRING (WM_USER + 180)
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

QueueWindow gQueueWindow;

QueueWindow::QueueWindow()
    : mHWND(NULL), mListHWND(NULL), mDeleteBtnHWND(NULL), mEmptyBtnHWND(NULL), mCtrlFont(NULL), mIsPaused(false),
      mQueuedCount(0), mProcessedCount(0)
{
}

QueueWindow::~QueueWindow() {
}

HWND QueueWindow::Create(HWND parent, HINSTANCE hInstance) {
    mHInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveQueueClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveQueueClass",
        L"Queue",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 480,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    } else {
        return NULL;
    }

    GetClientRect(mHWND, &mClientRect);

    // Create normal font for all controls
    mCtrlFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Create listbox for destinations
    mListHWND = CreateWindowExW(
        0,
        WC_LISTBOXW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_EXTENDEDSEL,
        5, 125, mClientRect.right - 10, mClientRect.bottom - 175,
        mHWND, NULL, hInstance, NULL
    );
    SendMessageW(mListHWND, WM_SETFONT, reinterpret_cast<WPARAM>(mCtrlFont), MAKELPARAM(TRUE, 0));

    // Delete button
    int btnY = mClientRect.bottom - 45;
    mDeleteBtnHWND = CreateWindowExW(0, WC_BUTTONW, L"Delete",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, btnY, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_QUEUE_DELETE), hInstance, NULL);
    SendMessageW(mDeleteBtnHWND, WM_SETFONT, reinterpret_cast<WPARAM>(mCtrlFont), MAKELPARAM(TRUE, 0));
    EnableWindow(mDeleteBtnHWND, FALSE);

    // Empty button
    mEmptyBtnHWND = CreateWindowExW(0, WC_BUTTONW, L"Empty",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        85, btnY, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_QUEUE_EMPTY), hInstance, NULL);
    SendMessageW(mEmptyBtnHWND, WM_SETFONT, reinterpret_cast<WPARAM>(mCtrlFont), MAKELPARAM(TRUE, 0));
    EnableWindow(mEmptyBtnHWND, FALSE);

    // Close button
    int cancelX = mClientRect.right - 75;
    HWND hCloseBtn = CreateWindowExW(0, WC_BUTTONW, L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cancelX, btnY+15, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_QUEUE_CLOSE), hInstance, NULL);
    SendMessageW(hCloseBtn, WM_SETFONT, reinterpret_cast<WPARAM>(mCtrlFont), MAKELPARAM(TRUE, 0));

    return mHWND;
}

void QueueWindow::UpdateList(const std::vector<std::wstring>& destinations) {
    if (!mListHWND) return;

    mDestinations = destinations;

    // Sync pause state from worker
    mIsPaused = (gWorkerThread.GetState() == WorkerState::ManualPause);

    // Initialize queue status data
    mQueuedCount = gQueueManager.GetQueuedDestinationCount();
    mProcessedCount = gWorkerThread.GetProcessedCount();
    WorkerState ws = gWorkerThread.GetState();
    switch (ws) {
        case WorkerState::Idle: mWorkerState = L"Idle"; break;
        case WorkerState::Moving: mWorkerState = L"Moving"; break;
        case WorkerState::ManualPause: mWorkerState = L"Manual Pause"; break;
        case WorkerState::PausedError: mWorkerState = L"Paused - Error"; break;
    }
    std::string fileStr = gWorkerThread.GetCurrentFile();
    mCurrentFile = std::wstring(fileStr.begin(), fileStr.end());
    std::string destStr = gWorkerThread.GetCurrentDest();
    mCurrentDest = std::wstring(destStr.begin(), destStr.end());
    std::string errorStr = gWorkerThread.GetLastError();
    mLastError = std::wstring(errorStr.begin(), errorStr.end());

    // Update title with count
    std::wstring title = L"Queue (";
    title += std::to_wstring(destinations.size());
    title += L")";
    SetWindowTextW(mHWND, title.c_str());

    // Update heading
    InvalidateRect(mHWND, NULL, FALSE);

    // Update listbox
    SendMessageW(mListHWND, LB_RESETCONTENT, 0, 0);
    for (const auto& dest : destinations) {
        SendMessageW(mListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(dest.c_str()));
    }
}

HWND QueueWindow::GetHWND() const {
    return mHWND;
}

QueueWindow* QueueWindow::GetInstance(HWND hwnd) {
    return reinterpret_cast<QueueWindow*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK QueueWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    QueueWindow* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<QueueWindow*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"Queue");
            SetTimer(hwnd, 1, 500, NULL);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                // Periodic refresh to ensure queue stays in sync
                PostMessageW(hwnd, WM_QUEUE_REFRESH, 0, 0);

                // Refresh queue status data
                int newQueued = gQueueManager.GetQueuedDestinationCount();
                int newProcessed = gWorkerThread.GetProcessedCount();
                WorkerState ws = gWorkerThread.GetState();
                std::wstring newState;
                switch (ws) {
                    case WorkerState::Idle: newState = L"Idle"; break;
                    case WorkerState::Moving: newState = L"Moving"; break;
                    case WorkerState::ManualPause: newState = L"Manual Pause"; break;
                    case WorkerState::PausedError: newState = L"Paused - Error"; break;
                }
                std::string fileStr = gWorkerThread.GetCurrentFile();
                std::wstring newFile(fileStr.begin(), fileStr.end());
                std::string destStr = gWorkerThread.GetCurrentDest();
                std::wstring newDest(destStr.begin(), destStr.end());
                std::string errorStr = gWorkerThread.GetLastError();
                std::wstring newError(errorStr.begin(), errorStr.end());
                if (newQueued != instance->mQueuedCount ||
                    newProcessed != instance->mProcessedCount ||
                    newState != instance->mWorkerState ||
                    newFile != instance->mCurrentFile ||
                    newDest != instance->mCurrentDest ||
                    newError != instance->mLastError) {
                    instance->mQueuedCount = newQueued;
                    instance->mProcessedCount = newProcessed;
                    instance->mWorkerState = newState;
                    instance->mCurrentFile = newFile;
                    instance->mCurrentDest = newDest;
                    instance->mLastError = newError;
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
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
            short code = static_cast<short>(HIWORD(wParam));
            if (code == LBN_SELCHANGE && reinterpret_cast<HWND>(lParam) == instance->mListHWND) {
                instance->UpdateButtonStates();
                break;
            }
            instance->OnCommand(id);
            break;
        }

        case WM_QUEUE_REFRESH: {
            auto entries = gQueueManager.GetQueuedEntries();
            std::vector<std::wstring> items;
            for (const auto& e : entries) {
                std::wstring src(e.sourceFilePath.begin(), e.sourceFilePath.end());
                for (const auto& d : e.destinationDirectories) {
                    std::wstring dest(d.begin(), d.end());
                    items.push_back(src + L" -> " + dest);
                }
            }
            // Only update if data actually changed
            if (items != instance->mDestinations) {
                instance->UpdateList(items);
            }
            instance->UpdateButtonStates();
            break;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            GetClientRect(hwnd, &instance->mClientRect);
            RECT pauseRect = { instance->mClientRect.right - 80, 2, instance->mClientRect.right - 10, 22 };
            if (PtInRect(&pauseRect, pt)) {
                instance->OnCommand(IDM_QUEUE_PAUSE_RESUME);
                break;
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            instance->mHWND = NULL;
            if (instance->mCtrlFont) {
                DeleteObject(instance->mCtrlFont);
                instance->mCtrlFont = NULL;
            }
            break;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void QueueWindow::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);

    if (mListHWND) {
        MoveWindow(mListHWND, 5, 125, mClientRect.right - 10, mClientRect.bottom - 175, TRUE);
    }

    int btnY = mClientRect.bottom - 45;
    int cancelX = mClientRect.right - 75;
    MoveWindow(mDeleteBtnHWND, 15, btnY, 65, 24, TRUE);
    MoveWindow(mEmptyBtnHWND, 85, btnY, 65, 24, TRUE);
    MoveWindow(GetDlgItem(mHWND, IDM_QUEUE_CLOSE), cancelX, btnY+15, 65, 24, TRUE);
}

void QueueWindow::OnCommand(int id) {
    switch (id) {
        case IDM_QUEUE_CLOSE: {
            DestroyWindow(mHWND);
            break;
        }
        case IDM_QUEUE_PAUSE_RESUME: {
            if (mIsPaused) {
                gWorkerThread.Resume();
                mIsPaused = false;
            } else {
                gWorkerThread.Pause();
                mIsPaused = true;
            }
            int queued = gQueueManager.GetQueuedDestinationCount();
            int processed = gWorkerThread.GetProcessedCount();
            std::wstring state = mIsPaused ? L"Manual Pause" : L"Idle";
            gMainWindow.UpdateStatusBar(queued, state);
            InvalidateRect(mHWND, NULL, TRUE);
            UpdateButtonStates();
            break;
        }
        case IDM_QUEUE_DELETE: {
            int selCount = static_cast<int>(SendMessageW(mListHWND, LB_GETSELCOUNT, 0, 0));
            if (selCount <= 0) break;

            std::vector<int> selItems(selCount);
            SendMessageW(mListHWND, LB_GETSELITEMS, selCount, reinterpret_cast<LPARAM>(selItems.data()));

            // Build a map from listbox index -> (entry, dest index) by iterating entries
            std::vector<std::pair<std::string, int>> indexToEntry;
            auto entries = gQueueManager.GetQueuedEntries();
            for (const auto& e : entries) {
                for (const auto& d : e.destinationDirectories) {
                    indexToEntry.push_back({ e.id, static_cast<int>(indexToEntry.size()) });
                }
            }

            // Collect unique entry IDs to cancel
            std::vector<std::string> idsToCancel;
            for (int idx : selItems) {
                if (idx >= 0 && idx < static_cast<int>(indexToEntry.size())) {
                    std::string entryId = indexToEntry[idx].first;
                    if (std::find(idsToCancel.begin(), idsToCancel.end(), entryId) == idsToCancel.end()) {
                        idsToCancel.push_back(entryId);
                    }
                }
            }

            for (const auto& entryId : idsToCancel) {
                gQueueManager.MarkCanceled(entryId);
            }

            PostMessageW(mHWND, WM_QUEUE_REFRESH, 0, 0);
            break;
        }
        case IDM_QUEUE_EMPTY: {
            int result = MessageBoxW(mHWND,
                L"Are you sure you want to empty the entire queue?",
                L"Empty Queue",
                MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
            if (result == IDYES) {
                gQueueManager.CancelAllEntries();
                PostMessageW(mHWND, WM_QUEUE_REFRESH, 0, 0);
            }
            break;
        }
    }
}

void QueueWindow::UpdateButtonStates() {
    bool paused = (gWorkerThread.GetState() == WorkerState::ManualPause);
    int queued = gQueueManager.GetQueuedDestinationCount();
    EnableWindow(mEmptyBtnHWND, paused && queued > 0);

    int selCount = static_cast<int>(SendMessageW(mListHWND, LB_GETSELCOUNT, 0, 0));
    EnableWindow(mDeleteBtnHWND, paused && selCount > 0);
}

void QueueWindow::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    RECT clientRect = mClientRect;

    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 250));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT hBoldFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hNormalFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hBoldFont);

    RECT headerRect = { 5, 2, clientRect.right - 90, 22 };
    std::wstring heading = L"Queued Destinations (" + std::to_wstring(mDestinations.size()) + L")";
    DrawTextW(hdc, heading.c_str(), -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Pause/Resume button
    RECT pauseRect = { clientRect.right - 80, 2, clientRect.right - 10, 22 };
    HBRUSH btnBrush = CreateSolidBrush(RGB(220, 220, 230));
    FillRect(hdc, &pauseRect, btnBrush);
    DeleteObject(btnBrush);
    FrameRect(hdc, &pauseRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SelectObject(hdc, hNormalFont);
    DrawTextW(hdc, mIsPaused ? L"Resume" : L"Pause", -1, &pauseRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Queue Status section
    SelectObject(hdc, hBoldFont);
    RECT sectionRect = { 5, 28, 200, 48 };
    DrawTextW(hdc, L"Queue Status", -1, &sectionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hNormalFont);

    // Queued / Processed / Worker State
    RECT qpRect = { 10, 52, clientRect.right - 10, 72 };
    std::wstring qpText = L"Queued / Processed: " + std::to_wstring(mQueuedCount) +
        L" / " + std::to_wstring(mProcessedCount) + L"    Worker State: " + mWorkerState;
    DrawTextW(hdc, qpText.c_str(), -1, &qpRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Current File
    RECT cfRect = { 10, 72, clientRect.right - 10, 92 };
    std::wstring cfText = L"Current File:  " + (mCurrentFile.empty() ? L"None" : mCurrentFile);
    DrawTextW(hdc, cfText.c_str(), -1, &cfRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Current Destination
    RECT cdRect = { 10, 92, clientRect.right - 10, 112 };
    std::wstring cdText = L"Current Destination: " + (mCurrentDest.empty() ? L"None" : mCurrentDest);
    DrawTextW(hdc, cdText.c_str(), -1, &cdRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Last Queue Error
    RECT leRect = { 10, 112, clientRect.right - 10, 132 };
    std::wstring leText = L"Last Queue Error: " + (mLastError.empty() ? L"None" : mLastError);
    DrawTextW(hdc, leText.c_str(), -1, &leRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hBoldFont);
    DeleteObject(hNormalFont);
}
