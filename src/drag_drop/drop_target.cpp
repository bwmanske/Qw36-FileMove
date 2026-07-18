#include "drop_target.h"
#include "utils/logging.h"
#include <commctrl.h>
#include <shellapi.h>

DropTarget::DropTarget()
    : mMainHWND(NULL), mListViewHWND(NULL), mRef(1), mDropEffect(DROPEFFECT_NONE), mHighlightedRow(-1)
{
}

DropTarget::~DropTarget() {
}

void DropTarget::Init(HWND mainHwnd, HWND listViewHwnd, DropRowHandler handler) {
    mMainHWND = mainHwnd;
    mListViewHWND = listViewHwnd;
    mHandler = handler;
}

STDMETHODIMP DropTarget::QueryInterface(REFIID riid, void** ppvObject) {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
        *ppvObject = static_cast<IDropTarget*>(this);
        AddRef();
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DropTarget::AddRef() {
    return InterlockedIncrement(reinterpret_cast<LONG*>(&mRef));
}

STDMETHODIMP_(ULONG) DropTarget::Release() {
    ULONG prev = InterlockedDecrement(reinterpret_cast<LONG*>(&mRef));
    if (prev == 0) {
        delete this;
    }
    return prev;
}

STDMETHODIMP DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                                      POINTL pt, DWORD* pdwEffect) {
    mDropEffect = DROPEFFECT_MOVE;
    ClearHighlight();

    // pt is in screen coords; map to ListView client coords
    POINT ptClient = { pt.x, pt.y };
    ScreenToClient(mMainHWND, &ptClient);
    MapWindowPoints(mMainHWND, mListViewHWND, &ptClient, 1);

    LVHITTESTINFO hti = {};
    hti.pt = ptClient;
    int row = ListView_HitTest(mListViewHWND, &hti);
    if (row >= 0) {
        HighlightRow(row);
    }

    *pdwEffect = mDropEffect;
    return S_OK;
}

STDMETHODIMP DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    mDropEffect = DROPEFFECT_MOVE;

    // pt is in screen coords; map to ListView client coords
    POINT ptClient = { pt.x, pt.y };
    ScreenToClient(mMainHWND, &ptClient);
    MapWindowPoints(mMainHWND, mListViewHWND, &ptClient, 1);

    LVHITTESTINFO hti = {};
    hti.pt = ptClient;
    int row = ListView_HitTest(mListViewHWND, &hti);
    if (row >= 0 && row != mHighlightedRow) {
        ClearHighlight();
        HighlightRow(row);
    } else if (row < 0) {
        ClearHighlight();
    }
    *pdwEffect = mDropEffect;
    return S_OK;
}

STDMETHODIMP DropTarget::DragLeave() {
    mDropEffect = DROPEFFECT_NONE;
    ClearHighlight();
    return S_OK;
}

STDMETHODIMP DropTarget::Drop(IDataObject* pDataObj, DWORD grfKeyState,
                                  POINTL pt, DWORD* pdwEffect) {
    *pdwEffect = DROPEFFECT_NONE;

    std::vector<std::wstring> droppedFiles = GetDroppedFiles(pDataObj);
    if (droppedFiles.empty()) {
        ClearHighlight();
        return S_OK;
    }

    // pt is in screen coords; map to ListView client coords
    POINT ptClient = { pt.x, pt.y };
    ScreenToClient(mMainHWND, &ptClient);
    MapWindowPoints(mMainHWND, mListViewHWND, &ptClient, 1);

    LVHITTESTINFO hti = {};
    hti.pt = ptClient;
    int row = ListView_HitTest(mListViewHWND, &hti);

    ClearHighlight();

    if (row < 0) {
        return S_OK;
    }

    std::vector<std::string> files;
    for (const auto& f : droppedFiles) {
        int len = WideCharToMultiByte(CP_UTF8, 0, f.c_str(), (int)f.size(), NULL, 0, NULL, NULL);
        if (len <= 0) continue;
        std::string narrow;
        narrow.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, f.c_str(), (int)f.size(), &narrow[0], len, NULL, NULL);
        files.push_back(narrow);
    }

    if (mHandler) {
        mHandler(row, files);
        *pdwEffect = DROPEFFECT_MOVE;
    }

    return S_OK;
}

void DropTarget::HighlightRow(int row) {
    if (row < 0 || !mListViewHWND) return;
    LVITEMW lvi = {};
    lvi.iItem = row;
    lvi.iSubItem = 0;
    lvi.mask = LVIF_STATE;
    lvi.state = LVIS_SELECTED | LVIS_FOCUSED;
    lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_SetItem(mListViewHWND, &lvi);
    ListView_EnsureVisible(mListViewHWND, row, FALSE);
    mHighlightedRow = row;
}

void DropTarget::ClearHighlight() {
    if (mHighlightedRow >= 0 && mListViewHWND) {
        LVITEMW lvi = {};
        lvi.iItem = mHighlightedRow;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_STATE;
        lvi.state = 0;
        lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItem(mListViewHWND, &lvi);
    }
    mHighlightedRow = -1;
}

std::vector<std::wstring> DropTarget::GetDroppedFiles(IDataObject* pDataObj) {
    std::vector<std::wstring> files;

    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};

    if (pDataObj->GetData(&fmt, &stg) != S_OK) {
        return files;
    }

    HDROP hDrop = reinterpret_cast<HDROP>(stg.hGlobal);
    if (!hDrop) {
        ReleaseStgMedium(&stg);
        return files;
    }

    UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < fileCount; i++) {
        wchar_t path[MAX_PATH];
        UINT len = DragQueryFileW(hDrop, i, path, _countof(path));
        if (len > 0 && len < _countof(path)) {
            files.push_back(std::wstring(path));
        }
    }

    DragFinish(hDrop);
    ReleaseStgMedium(&stg);

    return files;
}