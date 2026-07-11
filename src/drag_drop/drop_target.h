#pragma once

#include <windows.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <functional>

// Callback when files are dropped: (row, files)
using DropRowHandler = std::function<void(int row, const std::vector<std::string>& files)>;

class DropTarget : public IDropTarget {
public:
    DropTarget();
    ~DropTarget();

    // Initialize with main window HWND, ListView HWND, and handler
    void Init(HWND mainHwnd, HWND listViewHwnd, DropRowHandler handler);

    // COM interface methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                            POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
    STDMETHODIMP DragLeave() override;
    STDMETHODIMP Drop(IDataObject* pDataObj, DWORD grfKeyState,
                      POINTL pt, DWORD* pdwEffect) override;

    // Clear drag highlight (called from MainWindow after drop)
    void ClearHighlight();

private:
    // Extract file paths from data object
    std::vector<std::wstring> GetDroppedFiles(IDataObject* pDataObj);

    // Highlight a ListView row
    void HighlightRow(int row);

    HWND mMainHWND;
    HWND mListViewHWND;
    DropRowHandler mHandler;
    ULONG mRef;
    DWORD mDropEffect;
    int mHighlightedRow;
};
