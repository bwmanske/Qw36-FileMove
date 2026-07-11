#pragma once

#include <windows.h>
#include <string>
#include <vector>

enum SearchButtons {
    IDM_SEARCH_CLOSE = 7001
};

class SearchDialog {
public:
    SearchDialog();
    ~SearchDialog();

    // Show the Search dialog (modal). Accepts group names, returns selected name or empty string.
    std::wstring Show(HWND parent, const std::vector<std::string>& groupNames);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static SearchDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);
    void FilterList();

    HWND mHWND;
    HINSTANCE mHInstance;
    HWND mEditHWND;
    HWND mListHWND;
    RECT mClientRect;
    std::vector<std::string> mAllGroups;
    std::wstring mResult;
    bool mFiltering;
};
