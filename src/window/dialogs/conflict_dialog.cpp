#include "conflict_dialog.h"
#include "resources/resource_loader.h"
#include <dwmapi.h>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

enum ConflictButtons {
    IDM_CONFLICT_REPLACE = 4001,
    IDM_CONFLICT_KEEP_BOTH,
    IDM_CONFLICT_SKIP
};

ConflictDialog::ConflictDialog()
    : mHWND(NULL), mResult(ConflictResolution::Replace)
{
}

ConflictDialog::~ConflictDialog() {
}

ConflictResolution ConflictDialog::Show(HWND parent, const std::wstring& fileName, const std::wstring& destPath) {
    mFileName = fileName;
    mDestPath = destPath;
    mResult = ConflictResolution::Replace;

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(ConflictDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveConflictClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveConflictClass",
        L"File Conflict",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 210,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    } else {
        return mResult;
    }

    GetClientRect(mHWND, &mClientRect);

    // Create buttons
    int btnY = mClientRect.bottom - 35;
    int btnW = 85;
    int btnGap = 8;
    int totalBtns = btnW * 3 + btnGap * 2;
    int startX = (mClientRect.right - totalBtns) / 2;

    HWND hReplace = CreateWindowExW(0, WC_BUTTONW, L"Replace",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        startX, btnY, btnW, 26,
        mHWND, reinterpret_cast<HMENU>(IDM_CONFLICT_REPLACE), hInstance, NULL);

    HWND hKeepBoth = CreateWindowExW(0, WC_BUTTONW, L"Keep Both",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        startX + btnW + btnGap, btnY, btnW, 26,
        mHWND, reinterpret_cast<HMENU>(IDM_CONFLICT_KEEP_BOTH), hInstance, NULL);

    HWND hSkip = CreateWindowExW(0, WC_BUTTONW, L"Skip",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        startX + (btnW + btnGap) * 2, btnY, btnW, 26,
        mHWND, reinterpret_cast<HMENU>(IDM_CONFLICT_SKIP), hInstance, NULL);

    SetFocus(hReplace);

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

    UnregisterClassW(L"FileMoveConflictClass", hInstance);
    return mResult;
}

ConflictDialog* ConflictDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<ConflictDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK ConflictDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ConflictDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<ConflictDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"File Conflict");
            return 0;
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

void ConflictDialog::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    RECT clientRect = mClientRect;

    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 250));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT labelRect = { 15, 15, clientRect.right - 15, 35 };
    DrawTextW(hdc, L"The following file already exists in the destination:",
        -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT fileRect = { 30, 40, clientRect.right - 15, 60 };
    DrawTextW(hdc, mFileName.c_str(), -1, &fileRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT destLabelRect = { 15, 70, clientRect.right - 15, 90 };
    DrawTextW(hdc, L"Destination:", -1, &destLabelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT destRect = { 30, 90, clientRect.right - 15, 110 };
    DrawTextW(hdc, mDestPath.c_str(), -1, &destRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT questionRect = { 15, 120, clientRect.right - 15, 140 };
    DrawTextW(hdc, L"How would you like to handle this conflict?",
        -1, &questionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void ConflictDialog::OnCommand(int id) {
    switch (id) {
        case IDM_CONFLICT_REPLACE: {
            mResult = ConflictResolution::Replace;
            DestroyWindow(mHWND);
            break;
        }
        case IDM_CONFLICT_KEEP_BOTH: {
            mResult = ConflictResolution::KeepBoth;
            DestroyWindow(mHWND);
            break;
        }
        case IDM_CONFLICT_SKIP: {
            mResult = ConflictResolution::Skip;
            DestroyWindow(mHWND);
            break;
        }
    }
}
