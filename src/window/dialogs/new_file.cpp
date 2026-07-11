#include "new_file.h"
#include "resources/resource_loader.h"
#include <dwmapi.h>
#include <commctrl.h>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

NewFileDialog::NewFileDialog()
    : mHWND(NULL), mEditHWND(NULL), mResult{false, L""}
{
}

NewFileDialog::~NewFileDialog() {
}

NewFileResult NewFileDialog::Show(HWND parent) {
    mResult = {false, L""};

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(NewFileDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveNewFileClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveNewFileClass",
        L"New File",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 180,
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

    // Create edit control
    GetClientRect(mHWND, &mClientRect);
    mEditHWND = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_EDITW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        15, 55, mClientRect.right - 30, 22,
        mHWND, NULL, hInstance, NULL
    );

    // Create OK button
    CreateWindowExW(0, WC_BUTTONW, L"OK",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 110, mClientRect.bottom - 40, 70, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_NEWFILE_OK), hInstance, NULL);

    // Create Cancel button
    CreateWindowExW(0, WC_BUTTONW, L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 35, mClientRect.bottom - 40, 70, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_NEWFILE_CANCEL), hInstance, NULL);

    SetFocus(mEditHWND);

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

    UnregisterClassW(L"FileMoveNewFileClass", hInstance);
    return mResult;
}

NewFileDialog* NewFileDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<NewFileDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK NewFileDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NewFileDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<NewFileDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"New File");
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

void NewFileDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);
}

void NewFileDialog::OnPaint(HDC hdc) {
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
    DrawTextW(hdc, L"New JSON Base Name", -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT helpRect = { 15, 85, clientRect.right - 15, 130 };
    DrawTextW(hdc, L"Enter a base name only. The file will be created in the\ndefault data directory.",
        -1, &helpRect, DT_LEFT | DT_TOP);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void NewFileDialog::OnCommand(int id) {
    switch (id) {
        case IDM_NEWFILE_OK: {
            wchar_t buffer[260];
            GetWindowTextW(mEditHWND, buffer, _countof(buffer));
            std::wstring baseName(buffer);

            // Validate: no path separators, no extensions
            if (baseName.empty()) {
                MessageBoxW(mHWND, L"Please enter a base name.", L"New File", MB_ICONWARNING);
                return;
            }

            if (baseName.find(L'\\') != std::wstring::npos ||
                baseName.find(L'/') != std::wstring::npos ||
                baseName.find(L'.') != std::wstring::npos) {
                MessageBoxW(mHWND, L"Enter a base name only. No paths or extensions allowed.",
                    L"New File", MB_ICONWARNING);
                return;
            }

            mResult = {true, baseName};
            DestroyWindow(mHWND);
            break;
        }
        case IDM_NEWFILE_CANCEL: {
            mResult = {false, L""};
            DestroyWindow(mHWND);
            break;
        }
    }
}
