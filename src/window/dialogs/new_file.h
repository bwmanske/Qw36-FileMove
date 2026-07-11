#pragma once

#include <windows.h>
#include <string>

// Result from New File dialog
struct NewFileResult {
    bool accepted;
    std::wstring baseName;
};

enum NewFileButtons {
    IDM_NEWFILE_OK = 8001,
    IDM_NEWFILE_CANCEL
};

class NewFileDialog {
public:
    NewFileDialog();
    ~NewFileDialog();

    // Show the New File dialog (modal). Returns result with accepted flag.
    NewFileResult Show(HWND parent);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static NewFileDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);

    HWND mHWND;
    HINSTANCE mHInstance;
    HWND mEditHWND;
    RECT mClientRect;
    NewFileResult mResult;
};
