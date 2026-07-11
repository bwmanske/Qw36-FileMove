#include "group_list.h"

#ifndef LVM_SETTOPINDEX
#define LVM_SETTOPINDEX (LVM_FIRST + 117)
#endif
#include "data/file_io.h"
#include <windows.h>
#include <algorithm>
#include <commctrl.h>

GroupList::GroupList()
    : mListHWND(NULL)
    , mSortMode(SortMode::MostRecentlyUsed)
    , mHighlightedRow(-1)
{
}

GroupList::~GroupList() {
}

HWND GroupList::Create(HWND parent, RECT rect) {
    mListHWND = CreateWindowExW(
        0,
        WC_LISTVIEWW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
        rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
        parent,
        NULL,
        NULL,
        NULL
    );

    if (!mListHWND) return NULL;

    // Set extended style for full row select and double buffering
    DWORD exStyle = ListView_GetExtendedListViewStyle(mListHWND);
    ListView_SetExtendedListViewStyle(mListHWND,
        exStyle | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    // Add a single column spanning the full width (no header visible due to LVS_NOCOLUMNHEADER)
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.pszText = L"";
    col.cx = rect.right - rect.left;
    col.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(mListHWND, 0, &col);

    return mListHWND;
}

void GroupList::UpdateFromData(const std::vector<Group>& groups, SortMode sortMode) {
    mGroups = groups;
    mSortMode = sortMode;

    // Sort groups
    SortGroups(mGroups, sortMode);

    // Clear existing items
    ListView_DeleteAllItems(mListHWND);

    // Add items
    for (size_t i = 0; i < mGroups.size(); i++) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, mGroups[i].name.c_str(), -1, NULL, 0);
        if (wLen <= 0) continue;
        std::wstring nameW(wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, mGroups[i].name.c_str(), -1, &nameW[0], wLen);

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<wchar_t*>(nameW.c_str());
        ListView_InsertItem(mListHWND, &item);
    }
}

void GroupList::SortGroups(std::vector<Group>& groups, SortMode sortMode) {
    std::sort(groups.begin(), groups.end(),
        [sortMode](const Group& a, const Group& b) {
            return CompareGroups(a, b, sortMode) < 0;
        });
}

int GroupList::CompareGroups(const Group& a, const Group& b, SortMode sortMode) {
    switch (sortMode) {
        case SortMode::MostRecentlyUsed:
            return b.lastUsedAt.compare(a.lastUsedAt);
        case SortMode::LeastRecentlyUsed:
            return a.lastUsedAt.compare(b.lastUsedAt);
        case SortMode::AddedFirst:
            return a.createdAt.compare(b.createdAt);
        case SortMode::AddedLast:
            return b.createdAt.compare(a.createdAt);
        case SortMode::AtoZ: {
            std::string lowerA = a.name;
            std::string lowerB = b.name;
            std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);
            std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);
            return lowerA.compare(lowerB);
        }
        case SortMode::ZtoA: {
            std::string lowerA = a.name;
            std::string lowerB = b.name;
            std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);
            std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);
            return lowerB.compare(lowerA);
        }
    }
    return 0;
}

std::wstring GroupList::BuildTooltip(const Group& group) const {
    std::wstring tooltip;
    {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, group.name.c_str(), -1, NULL, 0);
        if (wLen > 1) {
            std::wstring nameW(wLen - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, group.name.c_str(), -1, &nameW[0], wLen);
            tooltip += nameW;
        }
    }
    tooltip += L"\r\n";

    for (const auto& path : group.destinationPaths) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
        if (wLen <= 1) continue;
        std::wstring widePath(wLen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &widePath[0], wLen);
        std::wstring status = GetDirectoryStatus(path);
        tooltip += status;
        tooltip += L" ";
        tooltip += widePath;
        tooltip += L"\r\n";
    }

    return tooltip;
}

std::wstring GroupList::GetDirectoryStatus(const std::string& path) const {
    int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    if (wLen <= 1) return L"[?]";
    std::wstring widePath(wLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &widePath[0], wLen);

    struct _stat64 st;
    if (_wstat64(widePath.c_str(), &st) != 0) {
        // Check if it's a drive that exists but path doesn't
        if (widePath.size() >= 2 && widePath[1] == L':') {
            std::wstring driveStr = widePath.substr(0, 2) + L"\\";
            if (GetDriveTypeW(driveStr.c_str()) != DRIVE_NO_ROOT_DIR) {
                return L"?"; // Drive exists but path doesn't - undetermined
            }
        }
        return L"\u2717"; // Missing
    }

    if ((st.st_mode & _S_IFDIR) != 0) {
        return L"\u2713"; // Exists
    }

    return L"?"; // Undetermined
}

void GroupList::SetRightClickCallback(GroupRightClickCallback callback) {
    mRightClickCallback = callback;
}

void GroupList::SetSelectCallback(GroupSelectCallback callback) {
    mSelectCallback = callback;
}

void GroupList::SetDropCallback(GroupDropCallback callback) {
    mDropCallback = callback;
}

std::string GroupList::GetGroupIdAtRow(int row) const {
    if (row < 0 || row >= static_cast<int>(mGroups.size())) return "";
    return mGroups[row].id;
}

std::string GroupList::GetGroupNameAtRow(int row) const {
    if (row < 0 || row >= static_cast<int>(mGroups.size())) return "";
    return mGroups[row].name;
}

std::string GroupList::GetSelectedGroupId() const {
    int sel = ListView_GetNextItem(mListHWND, -1, LVNI_SELECTED);
    if (sel < 0) return "";
    return GetGroupIdAtRow(sel);
}

int GroupList::GetRowCount() const {
    return static_cast<int>(mGroups.size());
}

HWND GroupList::GetHWND() const {
    return mListHWND;
}

void GroupList::Invalidate() {
    if (mListHWND) {
        InvalidateRect(mListHWND, NULL, TRUE);
    }
}

void GroupList::SetIconDirectory(const std::wstring& iconDir) {
    mIconDir = iconDir;
}

std::wstring GroupList::GetTooltipForRow(int row) const {
    if (row < 0 || row >= static_cast<int>(mGroups.size())) return L"";
    return BuildTooltip(mGroups[row]);
}

void GroupList::ScrollToGroup(const std::string& groupName) {
    if (!mListHWND) return;

    // Find the row index for the group
    int targetRow = -1;
    for (int i = 0; i < static_cast<int>(mGroups.size()); i++) {
        if (mGroups[i].name == groupName) {
            targetRow = i;
            break;
        }
    }
    if (targetRow < 0) return;

    // Ensure the item is visible
    ListView_EnsureVisible(mListHWND, targetRow, FALSE);

    // Try to center: get visible range and scroll to center the target
    RECT listRect;
    GetClientRect(mListHWND, &listRect);
    int visibleHeight = listRect.bottom - listRect.top;

    // Calculate ideal top index to center the target
    int itemsVisible = visibleHeight / 20; // approximate item height of 20px
    int halfVisible = itemsVisible / 2;
    int idealTop = targetRow - halfVisible;
    if (idealTop < 0) idealTop = 0;
    int maxTop = static_cast<int>(mGroups.size()) - itemsVisible;
    if (maxTop < 0) maxTop = 0;
    if (idealTop > maxTop) idealTop = maxTop;

    // Scroll to the ideal position
    SendMessageW(mListHWND, LVM_SETTOPINDEX, idealTop, 0);

    // Clear previous selection
    if (mHighlightedRow >= 0) {
        ListView_SetItemState(mListHWND, mHighlightedRow, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }

    // Select the item
    ListView_SetItemState(mListHWND, targetRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(mListHWND, targetRow, FALSE);
    mHighlightedRow = targetRow;
}

void GroupList::HighlightGroup(const std::string& groupName) {
    mHighlightedGroup = groupName;
    mHighlightedRow = -1;
    for (int i = 0; i < static_cast<int>(mGroups.size()); i++) {
        if (mGroups[i].name == groupName) {
            mHighlightedRow = i;
            break;
        }
    }
    if (mListHWND) {
        InvalidateRect(mListHWND, NULL, TRUE);
    }
}

void GroupList::ClearHighlight() {
    if (mHighlightedRow >= 0) {
        ListView_SetItemState(mListHWND, mHighlightedRow, 0, LVIS_SELECTED | LVIS_FOCUSED);
        mHighlightedRow = -1;
    }
    if (!mHighlightedGroup.empty()) {
        mHighlightedGroup.clear();
        if (mListHWND) {
            RedrawWindow(mListHWND, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        }
    }
}

const std::string& GroupList::GetHighlightedGroup() const {
    return mHighlightedGroup;
}
