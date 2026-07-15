#include "about.h"
#include "utils/logging.h"
#include "resources/resource_loader.h"
#include "resources/resource_ids.h"
#include "GeneratedBuildConfig.h"
#include <objbase.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

using namespace Gdiplus;

AboutDialog::AboutDialog()
    : mHWND(NULL)
{
}

AboutDialog::~AboutDialog() {
}

void AboutDialog::Show(HWND parent, const std::wstring& commandLine) {
    // Strip path, keeping only the exe name and arguments
    mCommandLine = commandLine;
    size_t lastSlash = mCommandLine.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        mCommandLine = mCommandLine.substr(lastSlash + 1);
    }
    // Remove leading quote if the exe path was quoted
    if (!mCommandLine.empty() && mCommandLine[0] == L'"') {
        mCommandLine = mCommandLine.substr(1);
    }
    // Remove trailing quote after exe name (e.g., "FileMove.exe" /D -> FileMove.exe /D)
    size_t closeQuote = mCommandLine.find(L"\" ");
    if (closeQuote != std::wstring::npos) {
        mCommandLine = mCommandLine.substr(0, closeQuote) + mCommandLine.substr(closeQuote + 1);
    } else {
        size_t closeTab = mCommandLine.find(L"\"\t");
        if (closeTab != std::wstring::npos) {
            mCommandLine = mCommandLine.substr(0, closeTab) + mCommandLine.substr(closeTab + 1);
        } else if (mCommandLine.size() > 1 && mCommandLine.back() == L'"') {
            mCommandLine.pop_back();
        }
    }

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(AboutDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveAboutClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    mHWND = CreateWindowExW(
        0,
        L"FileMoveAboutClass",
        L"About",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 280,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    }

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

    UnregisterClassW(L"FileMoveAboutClass", hInstance);
}

AboutDialog* AboutDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<AboutDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK AboutDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AboutDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<AboutDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            SetWindowTextW(hwnd, L"About");
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

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_KEYDOWN: {
            DestroyWindow(hwnd);
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

void AboutDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);
}

void AboutDialog::OnPaint(HDC hdc) {
    GetClientRect(mHWND, &mClientRect);
    RECT clientRect = mClientRect;

    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(RGB(245, 245, 250));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw about-image.png from embedded resource
    void* pImg = LoadEmbeddedPng(mHInstance, IDR_ABOUT_IMAGE);
    if (pImg) {
        Image* img = static_cast<Image*>(pImg);
        Graphics graphics(hdc);
        graphics.DrawImage(img, 136, 10, 128, 128);
        delete img;
    }

    // Draw "Build Information" header
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    RECT headerRect = { 10, 145, clientRect.right - 10, 165 };
    DrawTextW(hdc, L"Build Information", -1, &headerRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
    SelectObject(hdc, hFont);

    // Version line: "Version: 1.3.1" left, "Built On: ..." right
    RECT versionRect = { 15, 165, clientRect.right - 15, 185 };
    std::wstring versionText = L"Version: 1.3.1";
    DrawTextW(hdc, versionText.c_str(), -1, &versionRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Built On - from generated build config
    std::wstring builtOn = std::wstring(L"Built On: ") + FILEMOVE_BUILD_DATE_STR;
    RECT builtRect = { (clientRect.right - 15) / 2, 165, clientRect.right - 15, 185 };
    DrawTextW(hdc, builtOn.c_str(), -1, &builtRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Command Line
    RECT cmdRect = { 15, 185, clientRect.right - 15, 205 };
    std::wstring cmdText = L"Command Line: " + mCommandLine;
    DrawTextW(hdc, cmdText.c_str(), -1, &cmdRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Description
    RECT descRect = { 15, 210, clientRect.right - 15, 250 };
    DrawTextW(hdc, L"FileMove is a compact Windows utility for routing files\nfrom Explorer into named destination groups.",
        -1, &descRect, DT_LEFT | DT_TOP);

    DeleteObject(hOldFont);
    DeleteObject(hFont);
}
