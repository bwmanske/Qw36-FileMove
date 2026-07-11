#include "search.h"
#include "resources/resource_loader.h"
#include <dwmapi.h>
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

#ifndef LB_SETCURSEL
#define LB_SETCURSEL 387
#endif

#ifndef LB_GETCURSEL
#define LB_GETCURSEL 386
#endif

#ifndef LB_GETTEXT
#define LB_GETTEXT (WM_USER + 189)
#endif

#ifndef LBN_DBLCLK
#define LBN_DBLCLK (-452)
#endif

#ifndef LBN_SELCHANGE
#define LBN_SELCHANGE (-1)
#endif

#ifndef EN_CHANGE
#define EN_CHANGE (701)
#endif

#ifndef BN_CLICKED
#define BN_CLICKED (0)
#endif

#ifndef NM_CLICK
#define NM_CLICK 0
#endif

#ifndef NM_DBLCLK
#define NM_DBLCLK (-2)
#endif

SearchDialog::SearchDialog()
    : mHWND(NULL), mEditHWND(NULL), mListHWND(NULL), mFiltering(false)
{
}

SearchDialog::~SearchDialog() {
}

std::wstring SearchDialog::Show(HWND parent, const std::vector<std::string>& groupNames) {
    mAllGroups = groupNames;
    mResult = L"";

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(SearchDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveSearchClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveSearchClass",
        L"Search",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 355,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    } else {
        UnregisterClassW(L"FileMoveSearchClass", hInstance);
        return mResult;
    }

    GetClientRect(mHWND, &mClientRect);

    // Create font for controls
    HFONT hCtrlFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Edit box (ID 7002)
    mEditHWND = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_EDITW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 30, mClientRect.right - 20, 22,
        mHWND, reinterpret_cast<HMENU>(7002), hInstance, NULL
    );
    SendMessageW(mEditHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

    // Listbox
    mListHWND = CreateWindowExW(
        0,
        WC_LISTBOXW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_NOTIFY,
        10, 60, mClientRect.right - 20, mClientRect.bottom - 95,
        mHWND, reinterpret_cast<HMENU>(7003), hInstance, NULL
    );
    SendMessageW(mListHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

    // Close button
    int btnY = mClientRect.bottom - 30;
    HWND hCloseBtn = CreateWindowExW(0, WC_BUTTONW, L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 70, btnY, 60, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_SEARCH_CLOSE), hInstance, NULL);
    SendMessageW(hCloseBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hCtrlFont), MAKELPARAM(TRUE, 0));

    // Populate listbox with all groups
    for (const auto& name : mAllGroups) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, NULL, 0);
        if (wLen > 1) {
            std::wstring nameW(wLen - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &nameW[0], wLen);
            SendMessageW(mListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nameW.c_str()));
        }
    }

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

    UnregisterClassW(L"FileMoveSearchClass", hInstance);
    return mResult;
}

SearchDialog* SearchDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<SearchDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK SearchDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SearchDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<SearchDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"Search");
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
            HWND hwndCtrl = reinterpret_cast<HWND>(lParam);
            int id = LOWORD(wParam);
            short code = static_cast<short>(HIWORD(wParam));

            // Listbox double-click — select and close
            if (hwndCtrl == instance->mListHWND && code == LBN_DBLCLK && !instance->mFiltering) {
                int sel = static_cast<int>(SendMessageW(instance->mListHWND, LB_GETCURSEL, 0, 0));
                if (sel >= 0) {
                    wchar_t buffer[512];
                    SendMessageW(instance->mListHWND, LB_GETTEXT, sel, reinterpret_cast<LPARAM>(buffer));
                    instance->mResult = std::wstring(buffer);
                    DestroyWindow(instance->mHWND);
                }
                break;
            }

            // Edit box EN_CHANGE — live filter
            if (id == 7002 && code == EN_CHANGE) {
                instance->FilterList();
                break;
            }

            // Close button
            if (id == IDM_SEARCH_CLOSE && code == BN_CLICKED) {
                instance->mResult = L"";
                DestroyWindow(instance->mHWND);
                break;
            }
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

void SearchDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);

    if (mEditHWND) {
        MoveWindow(mEditHWND, 15, 30, mClientRect.right - 30, 22, TRUE);
    }
    if (mListHWND) {
        MoveWindow(mListHWND, 15, 60, mClientRect.right - 30, mClientRect.bottom - 90, TRUE);
    }
    MoveWindow(GetDlgItem(mHWND, IDM_SEARCH_CLOSE), mClientRect.right - 75, mClientRect.bottom - 35, 60, 24, TRUE);
}

void SearchDialog::OnPaint(HDC hdc) {
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

    RECT labelRect = { 15, 8, clientRect.right - 15, 28 };
    DrawTextW(hdc, L"Search for Group", -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void SearchDialog::OnCommand(int id) {
    (void)id;
}

void SearchDialog::FilterList() {
    mFiltering = true;

    wchar_t buffer[512];
    GetWindowTextW(mEditHWND, buffer, _countof(buffer));
    std::wstring searchStr(buffer);

    // Convert to lowercase for case-insensitive comparison
    std::wstring searchLower;
    searchLower.reserve(searchStr.size());
    for (wchar_t c : searchStr) {
        searchLower += static_cast<wchar_t>(std::tolower(c));
    }

    SendMessageW(mListHWND, LB_RESETCONTENT, 0, 0);

    for (const auto& name : mAllGroups) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, NULL, 0);
        if (wLen <= 1) continue;
        std::wstring nameW(wLen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &nameW[0], wLen);

        if (searchLower.empty()) {
            SendMessageW(mListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nameW.c_str()));
        } else {
            std::wstring nameLower;
            nameLower.reserve(nameW.size());
            for (wchar_t c : nameW) {
                nameLower += static_cast<wchar_t>(std::tolower(c));
            }
            if (nameLower.find(searchLower) != std::wstring::npos) {
                SendMessageW(mListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(nameW.c_str()));
            }
        }
    }

    mFiltering = false;
}
