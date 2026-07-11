#include "status.h"
#include "data/file_io.h"
#include "utils/logging.h"
#include "queue/queue_manager.h"
#include "queue/worker_thread.h"
#include "resources/resource_loader.h"
#include "new_file.h"
#include "window/main_window.h"
#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#ifndef LB_RESETCONTENT
#define LB_RESETCONTENT 379
#endif

#ifndef LB_ADDSTRING
#define LB_ADDSTRING (WM_USER + 180)
#endif

#ifndef LB_GETCURSEL
#define LB_GETCURSEL 386
#endif

#ifndef LB_SETCURSEL
#define LB_SETCURSEL 387
#endif

#ifndef LB_GETCOUNT
#define LB_GETCOUNT 395
#endif

#ifndef LBS_NOTIFY
#define LBS_NOTIFY 0x0004L
#endif

#ifndef LBN_SELCHANGE
#define LBN_SELCHANGE 0
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

StatusDialog::StatusDialog()
    : mHWND(NULL), mJsonListHWND(NULL), mSelectedRow(-1), mCloseRequested(false),
      mQueuedCount(0), mProcessedCount(0)
{
}

StatusDialog::~StatusDialog() {
}

void StatusDialog::Show(HWND parent, const std::wstring& jsonPath, const std::wstring& logPath,
                        int queuedCount, int processedCount, const std::wstring& workerState,
                        const std::wstring& currentFile, const std::wstring& currentDest,
                        const std::wstring& lastError, bool isPaused,
                        JsonFileOpenCallback onJsonFileOpen) {
    mJsonPath = jsonPath;
    mLogPath = logPath;
    mQueuedCount = queuedCount;
    mProcessedCount = processedCount;
    mWorkerState = workerState;
    mCurrentFile = currentFile;
    mCurrentDest = currentDest;
    mLastError = lastError;
    mIsPaused = isPaused;
    mOnJsonFileOpen = onJsonFileOpen;
    mCloseRequested = false;

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(StatusDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveStatusClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveStatusClass",
        L"Status",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 420,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        // Set non-bold font on parent so child controls inherit it
        {
            HFONT hParentFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
            SendMessageW(mHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hParentFont), MAKELPARAM(TRUE, 0));
            DeleteObject(hParentFont);
        }
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    }

    GetClientRect(mHWND, &mClientRect);

    // Create JSON file list (without WS_VISIBLE initially)
    mJsonListHWND = CreateWindowExW(
        0,
        WC_LISTBOXW,
        NULL,
        WS_CHILD | WS_BORDER | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_NOTIFY,
        10, 230, mClientRect.right - 20, mClientRect.bottom - 285,
        mHWND, reinterpret_cast<HMENU>(IDM_STATUS_JSON_LISTBOX), hInstance, NULL
    );

    UpdateJsonFileList();

    // Set non-bold font for listbox entries
    {
        HFONT hListFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(mJsonListHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hListFont), MAKELPARAM(TRUE, 1));
    }

    ShowWindow(mJsonListHWND, SW_SHOW);

    // Create non-bold font for buttons
    HFONT hBtnFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Create buttons
    int btnY = mClientRect.bottom - 45;

    HWND hNewBtn = CreateWindowExW(0, WC_BUTTONW, L"New",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, btnY, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_STATUS_NEW), hInstance, NULL);
    SendMessageW(hNewBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    HWND hOpenBtn = CreateWindowExW(0, WC_BUTTONW, L"Open Selected",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        85, btnY, 95, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_STATUS_OPEN_SELECTED), hInstance, NULL);
    SendMessageW(hOpenBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    HWND hCloseBtn = CreateWindowExW(0, WC_BUTTONW, L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 75, btnY+15, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_STATUS_CLOSE), hInstance, NULL);
    SendMessageW(hCloseBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    InvalidateRect(mHWND, NULL, TRUE);

    MSG msg;
    while (IsWindow(mHWND) && !mCloseRequested && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.hwnd == mHWND) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    DeleteObject(hBtnFont);
    UnregisterClassW(L"FileMoveStatusClass", hInstance);
}

void StatusDialog::RequestClose() {
    mCloseRequested = true;
    if (mHWND) {
        DestroyWindow(mHWND);
    }
}

StatusDialog* StatusDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<StatusDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK StatusDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    StatusDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<StatusDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"Status");
            SetTimer(hwnd, 1, 500, NULL);
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            instance->OnSize(width, height);
            break;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                // Refresh status data - only update if state changed
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

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            instance->OnPaint(hdc);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == IDM_STATUS_JSON_LISTBOX && code == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(instance->mJsonListHWND, LB_GETCURSEL, 0, 0));
                instance->mSelectedRow = sel;
                break;
            }
            instance->OnCommand(id);
            break;
        }

        case WM_NOTIFY: {
            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->code == LBN_SELCHANGE) {
                int sel = static_cast<int>(SendMessageW(instance->mJsonListHWND, LB_GETCURSEL, 0, 0));
                instance->mSelectedRow = sel;
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            GetClientRect(hwnd, &instance->mClientRect);
            RECT openRect = { instance->mClientRect.right - 50, 50, instance->mClientRect.right - 10, 70 };
            if (PtInRect(&openRect, pt)) {
                instance->OnCommand(IDM_STATUS_OPEN_LOG);
                break;
            }
            RECT pauseRect = { instance->mClientRect.right - 80, 185, instance->mClientRect.right - 10, 208 };
            if (PtInRect(&pauseRect, pt)) {
                instance->OnCommand(IDM_STATUS_PAUSE_RESUME);
                break;
            }
            break;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            instance->mHWND = NULL;
            break;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void StatusDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);

    if (mJsonListHWND) {
        MoveWindow(mJsonListHWND, 10, 230, mClientRect.right - 20, mClientRect.bottom - 285, TRUE);
    }

    int btnY = mClientRect.bottom - 45;
    MoveWindow(GetDlgItem(mHWND, IDM_STATUS_NEW), 15, btnY, 65, 24, TRUE);
    MoveWindow(GetDlgItem(mHWND, IDM_STATUS_OPEN_SELECTED), 85, btnY, 95, 24, TRUE);
    MoveWindow(GetDlgItem(mHWND, IDM_STATUS_CLOSE), mClientRect.right - 75, btnY+15, 65, 24, TRUE);
}

void StatusDialog::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    RECT clientRect = mClientRect;

    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 250));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hNormal = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Active Files section
    RECT sectionRect = { 10, 8, 200, 28 };
    DrawTextW(hdc, L"Active Files", -1, &sectionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hNormal);

    // JSON path
    RECT jsonRect = { 15, 30, clientRect.right - 60, 50 };
    std::wstring jsonText = L"JSON: " + mJsonPath;
    DrawTextW(hdc, jsonText.c_str(), -1, &jsonRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // LOG path
    RECT logRect = { 15, 50, clientRect.right - 60, 70 };
    std::wstring logText = L"LOG:  " + mLogPath;
    DrawTextW(hdc, logText.c_str(), -1, &logRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Open button for LOG
    RECT openRect = { clientRect.right - 50, 50, clientRect.right - 10, 70 };
    HBRUSH btnBrush = CreateSolidBrush(RGB(220, 220, 230));
    FillRect(hdc, &openRect, btnBrush);
    DeleteObject(btnBrush);
    FrameRect(hdc, &openRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    DrawTextW(hdc, L"Open", -1, &openRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Queue Status section
    SelectObject(hdc, hFont);
    sectionRect = { 10, 80, 200, 100 };
    DrawTextW(hdc, L"Queue Status", -1, &sectionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hNormal);

    // Queued / Processed
    RECT qpRect = { 15, 102, clientRect.right - 10, 122 };
    std::wstring qpText = L"Queued / Processed: " + std::to_wstring(mQueuedCount) +
        L" / " + std::to_wstring(mProcessedCount) + L"    Worker State: " + mWorkerState;
    DrawTextW(hdc, qpText.c_str(), -1, &qpRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Current File
    RECT cfRect = { 15, 122, clientRect.right - 10, 142 };
    std::wstring cfText = L"Current File:  " + (mCurrentFile.empty() ? L"None" : mCurrentFile);
    DrawTextW(hdc, cfText.c_str(), -1, &cfRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Current Destination
    RECT cdRect = { 15, 142, clientRect.right - 10, 162 };
    std::wstring cdText = L"Current Destination: " + (mCurrentDest.empty() ? L"None" : mCurrentDest);
    DrawTextW(hdc, cdText.c_str(), -1, &cdRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Last Queue Error
    RECT leRect = { 15, 162, clientRect.right - 10, 182 };
    std::wstring leText = L"Last Queue Error: " + (mLastError.empty() ? L"None" : mLastError);
    DrawTextW(hdc, leText.c_str(), -1, &leRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Pause/Resume button
    RECT pauseRect = { clientRect.right - 80, 185, clientRect.right - 10, 208 };
    btnBrush = CreateSolidBrush(RGB(220, 220, 230));
    FillRect(hdc, &pauseRect, btnBrush);
    DeleteObject(btnBrush);
    FrameRect(hdc, &pauseRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    DrawTextW(hdc, mIsPaused ? L"Resume" : L"Pause", -1, &pauseRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // JSON Files section
    SelectObject(hdc, hFont);
    sectionRect = { 10, 210, 400, 230 };
    DrawTextW(hdc, L"JSON Files In Default Data Directory", -1, &sectionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    DeleteObject(hNormal);
}

void StatusDialog::OnCommand(int id) {
    switch (id) {
        case IDM_STATUS_OPEN_LOG: {
            ShellExecuteW(mHWND, L"open", mLogPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        case IDM_STATUS_PAUSE_RESUME: {
            if (mIsPaused) {
                gWorkerThread.Resume();
                mIsPaused = false;
                mWorkerState = L"Idle";
            } else {
                gWorkerThread.Pause();
                mIsPaused = true;
                mWorkerState = L"Manual Pause";
            }
            int queued = gQueueManager.GetQueuedDestinationCount();
            int processed = gWorkerThread.GetProcessedCount();
            gMainWindow.UpdateStatusBar(queued, mWorkerState);
            InvalidateRect(mHWND, NULL, TRUE);
            break;
        }
        case IDM_STATUS_NEW: {
            NewFileDialog newFileDlg;
            NewFileResult result = newFileDlg.Show(mHWND);
            if (result.accepted && mOnJsonFileOpen) {
                std::wstring newJsonPath = GetDefaultDataDirectory() + L"\\" + result.baseName + L".json";
                if (mOnJsonFileOpen(newJsonPath)) {
                    mCloseRequested = true;
                    DestroyWindow(mHWND);
                }
            }
            break;
        }
        case IDM_STATUS_OPEN_SELECTED: {
            if (mSelectedRow >= 0 && mSelectedRow < static_cast<int>(mJsonFiles.size())) {
                std::wstring selectedPath = GetDefaultDataDirectory() + L"\\" + mJsonFiles[mSelectedRow];
                if (mOnJsonFileOpen) {
                    if (mOnJsonFileOpen(selectedPath)) {
                        mCloseRequested = true;
                        DestroyWindow(mHWND);
                    }
                }
            }
            break;
        }
        case IDM_STATUS_CLOSE: {
            mCloseRequested = true;
            DestroyWindow(mHWND);
            break;
        }
    }
}

void StatusDialog::UpdateJsonFileList() {
    if (!mJsonListHWND) return;

    mJsonFiles = ListJsonFiles(GetDefaultDataDirectory());
    SendMessageW(mJsonListHWND, LB_RESETCONTENT, 0, 0);

    for (const auto& file : mJsonFiles) {
        SendMessageW(mJsonListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(file.c_str()));
    }

    // Select current JSON file
    std::wstring currentFile = GetFileName(mJsonPath);
    for (int i = 0; i < static_cast<int>(mJsonFiles.size()); i++) {
        if (mJsonFiles[i] == currentFile) {
            SendMessageW(mJsonListHWND, LB_SETCURSEL, i, 0);
            mSelectedRow = i;
            break;
        }
    }
}

void StatusDialog::SelectJsonFile(int row) {
    if (row < 0 || row >= static_cast<int>(mJsonFiles.size())) return;
    mSelectedRow = row;
}
