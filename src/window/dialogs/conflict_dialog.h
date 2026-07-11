#pragma once

#include <windows.h>
#include <string>
#include "queue/worker_thread.h"

class ConflictDialog {
public:
    ConflictDialog();
    ~ConflictDialog();

    ConflictResolution Show(HWND parent, const std::wstring& fileName, const std::wstring& destPath);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static ConflictDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);

    HWND mHWND;
    HINSTANCE mHInstance;
    RECT mClientRect;
    std::wstring mFileName;
    std::wstring mDestPath;
    ConflictResolution mResult;
};
