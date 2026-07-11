#include "settings.h"
#include "data/json_parser.h"
#include "resources/resource_loader.h"
#include <dwmapi.h>
#include <commctrl.h>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#ifndef BST_CHECKED
#define BST_CHECKED 1
#endif

#ifndef BST_UNCHECKED
#define BST_UNCHECKED 0
#endif

#ifndef BM_SETCHECK
#define BM_SETCHECK 241
#endif

#ifndef BM_GETCHECK
#define BM_GETCHECK 240
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

SettingsDialog::SettingsDialog()
    : mHWND(NULL), mAccepted(false), mPreserveStructureHwnd(NULL), mCreateEmptyDirsHwnd(NULL)
{
}

SettingsDialog::~SettingsDialog() {
}

bool SettingsDialog::Show(HWND parent, AppSettings& settings) {
    mSettings = settings;
    mAccepted = false;

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(SettingsDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveSettingsClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveSettingsClass",
        L"Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 415, 375,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    } else {
        return false;
    }

    GetClientRect(mHWND, &mClientRect);
    int y = 10;

    // Create non-bold font for all controls (explicit, not from system metrics)
    HFONT hCtrlFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Sort Order section header (static text)
    CreateWindowExW(0, WC_STATICW, L"Sort Order",
        WS_CHILD | WS_VISIBLE,
        10, y, 380, 16,
        mHWND, NULL, hInstance, NULL);
    y += 20;

    // Sort radio buttons (2 columns)
    const wchar_t* sortLabels[] = {
        L"Most Recently Used", L"Least Recently Used",
        L"A thru Z", L"Z thru A",
        L"Added First", L"Added Last"
    };
    int sortIds[] = { IDM_SORT_MRU, IDM_SORT_LRU, IDM_SORT_AZ, IDM_SORT_ZA, IDM_SORT_AF, IDM_SORT_AL };

    std::string sortStr = settings.sortMode;
    for (int i = 0; i < 6; i++) {
        int col = (i % 2) == 0 ? 15 : 200;
        int row = y + (i / 2) * 20;
        DWORD btnStyle = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
        if (i == 0) btnStyle |= WS_GROUP;
        HWND hBtn = CreateWindowExW(0, WC_BUTTONW, sortLabels[i],
            btnStyle,
            col, row, 160, 18,
            mHWND, reinterpret_cast<HMENU>(sortIds[i]), hInstance, NULL);
        SendMessageW(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

        bool check = false;
        if (sortIds[i] == IDM_SORT_MRU && sortStr == "MostRecentlyUsed") check = true;
        if (sortIds[i] == IDM_SORT_LRU && sortStr == "LeastRecentlyUsed") check = true;
        if (sortIds[i] == IDM_SORT_AZ && sortStr == "AtoZ") check = true;
        if (sortIds[i] == IDM_SORT_ZA && sortStr == "ZtoA") check = true;
        if (sortIds[i] == IDM_SORT_AF && sortStr == "AddedFirst") check = true;
        if (sortIds[i] == IDM_SORT_AL && sortStr == "AddedLast") check = true;
        if (check) {
            SendMessageW(hBtn, BM_SETCHECK, BST_CHECKED, 0);
        }
    }
    y += 65;

    // Placement section header
    CreateWindowExW(0, WC_STATICW, L"Placement",
        WS_CHILD | WS_VISIBLE,
        10, y, 380, 16,
        mHWND, NULL, hInstance, NULL);
    y += 20;

    // Placement radio buttons
    const wchar_t* placementLabels[] = {
        L"Upper Left", L"Upper Right",
        L"Last Location",
        L"Lower Left", L"Lower Right"
    };
    int placementIds[] = { IDM_PLACEMENT_UL, IDM_PLACEMENT_UR, IDM_PLACEMENT_LAST, IDM_PLACEMENT_LL, IDM_PLACEMENT_LR };

    std::string placementStr = settings.placementMode;
    for (int i = 0; i < 5; i++) {
        int col, row;
        if (i == 0) { col = 15; row = y; }
        else if (i == 1) { col = 200; row = y; }
        else if (i == 2) { col = 120; row = y + 20; }
        else if (i == 3) { col = 15; row = y + 40; }
        else { col = 200; row = y + 40; }

        DWORD btnStyle = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
        if (i == 0) btnStyle |= WS_GROUP;
        HWND hBtn = CreateWindowExW(0, WC_BUTTONW, placementLabels[i],
            btnStyle,
            col, row, 160, 18,
            mHWND, reinterpret_cast<HMENU>(placementIds[i]), hInstance, NULL);
        SendMessageW(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

        bool check = false;
        if (placementIds[i] == IDM_PLACEMENT_UL && placementStr == "UpperLeft") check = true;
        if (placementIds[i] == IDM_PLACEMENT_UR && placementStr == "UpperRight") check = true;
        if (placementIds[i] == IDM_PLACEMENT_LL && placementStr == "LowerLeft") check = true;
        if (placementIds[i] == IDM_PLACEMENT_LR && placementStr == "LowerRight") check = true;
        if (placementIds[i] == IDM_PLACEMENT_LAST && placementStr == "LastLocation") check = true;
        if (check) {
            SendMessageW(hBtn, BM_SETCHECK, BST_CHECKED, 0);
        }
    }
    y += 65;

    // Options section header
    CreateWindowExW(0, WC_STATICW, L"Options",
        WS_CHILD | WS_VISIBLE,
        10, y, 380, 16,
        mHWND, NULL, hInstance, NULL);
    y += 20;

    // Checkboxes
    HWND hCb1 = CreateWindowExW(0, WC_BUTTONW, L"Move directories with subdirectories and files",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        15, y, 345, 18,
        mHWND, reinterpret_cast<HMENU>(IDM_OPT_DIRECTORY_MOVES), hInstance, NULL);
    SendMessageW(hCb1, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));
    if (settings.enableDirectoryMoves) SendMessageW(hCb1, BM_SETCHECK, BST_CHECKED, 0);

    y += 20;
    mPreserveStructureHwnd = CreateWindowExW(0, WC_BUTTONW, L"Preserve directory structure at destination",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, y, 325, 18,
        mHWND, reinterpret_cast<HMENU>(IDM_OPT_PRESERVE_STRUCTURE), hInstance, NULL);
    SendMessageW(mPreserveStructureHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));
    if (settings.preserveDirectoryStructure) SendMessageW(mPreserveStructureHwnd, BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(mPreserveStructureHwnd, settings.enableDirectoryMoves);

    y += 20;
    mCreateEmptyDirsHwnd = CreateWindowExW(0, WC_BUTTONW, L"Create empty directories",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        55, y, 305, 18,
        mHWND, reinterpret_cast<HMENU>(IDM_OPT_CREATE_EMPTY_DIRS), hInstance, NULL);
    SendMessageW(mCreateEmptyDirsHwnd, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));
    if (settings.createEmptyDirectories) SendMessageW(mCreateEmptyDirsHwnd, BM_SETCHECK, BST_CHECKED, 0);
    EnableWindow(mCreateEmptyDirsHwnd, settings.preserveDirectoryStructure);

    y += 20;
    HWND hCb2 = CreateWindowExW(0, WC_BUTTONW, L"Create .filemove-queued sidecar files",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        15, y, 345, 18,
        mHWND, reinterpret_cast<HMENU>(IDM_OPT_SIDECAR_FILES), hInstance, NULL);
    SendMessageW(hCb2, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));
    if (settings.enableSidecarFiles) SendMessageW(hCb2, BM_SETCHECK, BST_CHECKED, 0);

    y += 20;
    HWND hCb3 = CreateWindowExW(0, WC_BUTTONW, L"Experimental: mark queued source files as hidden",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        15, y, 345, 18,
        mHWND, reinterpret_cast<HMENU>(IDM_OPT_HIDDEN_SOURCE), hInstance, NULL);
    SendMessageW(hCb3, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));
    if (settings.hideQueuedSourceFiles) SendMessageW(hCb3, BM_SETCHECK, BST_CHECKED, 0);
    y += 25;

    // OK and Cancel buttons
    int btnY = mClientRect.bottom - 30;
    int cancelX = mClientRect.right - 80;
    int okX = cancelX - 80;
    HWND hOkBtn = CreateWindowExW(0, WC_BUTTONW, L"OK",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        okX, btnY, 70, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_SETTINGS_OK), hInstance, NULL);
    SendMessageW(hOkBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

    HWND hCancelBtn = CreateWindowExW(0, WC_BUTTONW, L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cancelX, btnY, 70, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_SETTINGS_CANCEL), hInstance, NULL);
    SendMessageW(hCancelBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

    // Don't delete hCtrlFont - controls need it for their lifetime

    InvalidateRect(mHWND, NULL, TRUE);

    MSG msg;
    while (IsWindow(mHWND) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.hwnd == mHWND) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (mAccepted) {
        settings = mSettings;
    }

    UnregisterClassW(L"FileMoveSettingsClass", hInstance);
    return mAccepted;
}

SettingsDialog* SettingsDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<SettingsDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"Settings");
            return 0;
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

        case WM_DESTROY: {
            instance->mHWND = NULL;
            break;
        }

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void SettingsDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);
}

void SettingsDialog::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    RECT clientRect = mClientRect;

    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 250));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);
}

void SettingsDialog::OnCommand(int id) {
    switch (id) {
        case IDM_SETTINGS_OK: {
            mAccepted = true;
            ApplySettings(mSettings);
            DestroyWindow(mHWND);
            break;
        }
        case IDM_SETTINGS_CANCEL: {
            mAccepted = false;
            DestroyWindow(mHWND);
            break;
        }
        case IDM_OPT_DIRECTORY_MOVES: {
            bool checked = (SendMessageW(GetDlgItem(mHWND, IDM_OPT_DIRECTORY_MOVES), BM_GETCHECK, 0, 0) == BST_CHECKED);
            EnableWindow(mPreserveStructureHwnd, checked);
            break;
        }
        case IDM_OPT_PRESERVE_STRUCTURE: {
            bool checked = (SendMessageW(mPreserveStructureHwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (checked) {
                SendMessageW(GetDlgItem(mHWND, IDM_OPT_DIRECTORY_MOVES), BM_SETCHECK, BST_CHECKED, 0);
            }
            EnableWindow(mCreateEmptyDirsHwnd, checked);
            break;
        }
        case IDM_OPT_CREATE_EMPTY_DIRS: {
            break;
        }
    }
}

void SettingsDialog::ApplySettings(AppSettings& settings) {
    // Sort mode
    if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_MRU), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "MostRecentlyUsed";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_LRU), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "LeastRecentlyUsed";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_AZ), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "AtoZ";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_ZA), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "ZtoA";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_AF), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "AddedFirst";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_SORT_AL), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.sortMode = "AddedLast";

    // Placement mode
    if (SendMessageW(GetDlgItem(mHWND, IDM_PLACEMENT_UL), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.placementMode = "UpperLeft";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_PLACEMENT_UR), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.placementMode = "UpperRight";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_PLACEMENT_LL), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.placementMode = "LowerLeft";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_PLACEMENT_LR), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.placementMode = "LowerRight";
    else if (SendMessageW(GetDlgItem(mHWND, IDM_PLACEMENT_LAST), BM_GETCHECK, 0, 0) == BST_CHECKED) settings.placementMode = "LastLocation";

    // Options
    settings.enableDirectoryMoves = (SendMessageW(GetDlgItem(mHWND, IDM_OPT_DIRECTORY_MOVES), BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.preserveDirectoryStructure = (SendMessageW(mPreserveStructureHwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.createEmptyDirectories = (SendMessageW(mCreateEmptyDirsHwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.enableSidecarFiles = (SendMessageW(GetDlgItem(mHWND, IDM_OPT_SIDECAR_FILES), BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.hideQueuedSourceFiles = (SendMessageW(GetDlgItem(mHWND, IDM_OPT_HIDDEN_SOURCE), BM_GETCHECK, 0, 0) == BST_CHECKED);
}
