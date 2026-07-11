#pragma once

#include <windows.h>
#include <string>

class AboutDialog {
public:
    AboutDialog();
    ~AboutDialog();

    // Show the About dialog (modal)
    void Show(HWND parent, const std::wstring& commandLine);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static AboutDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnSize(int width, int height);

    HWND mHWND;
    HINSTANCE mHInstance;
    std::wstring mCommandLine;
    RECT mClientRect;
};
