#include "group_editor.h"
#include "data/file_io.h"
#include "utils/logging.h"
#include "resources/resource_loader.h"
#include "resources/resource_ids.h"
#include <objbase.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <cwchar>

using namespace Gdiplus;

#ifndef LB_RESETCONTENT
#define LB_RESETCONTENT 379
#endif

#ifndef LB_ADDSTRING
#define LB_ADDSTRING (WM_USER + 180)
#endif

#ifndef LB_GETCURSEL
#define LB_GETCURSEL 386
#endif

#ifndef LB_GETTEXT
#define LB_GETTEXT (WM_USER + 190)
#endif

#ifndef LB_GETTEXTLEN
#define LB_GETTEXTLEN (WM_USER + 189)
#endif

#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif

#ifndef LBS_OWNERDRAWFIXED
#define LBS_OWNERDRAWFIXED 0x0020L
#endif

#ifndef LBS_NOINTEGRALHEIGHT
#define LBS_NOINTEGRALHEIGHT 0x0008L
#endif

#ifndef LB_GETCOUNT
#define LB_GETCOUNT 395
#endif

#ifndef ODT_LISTBOX
#define ODT_LISTBOX 2
#endif

#ifndef ODA_DRAWENTIRE
#define ODA_DRAWENTIRE 1
#endif

#ifndef ODA_SELECT
#define ODA_SELECT 2
#endif

#ifndef ODA_FOCUS
#define ODA_FOCUS 4
#endif

#ifndef ODS_SELECTED
#define ODS_SELECTED 0x0001
#endif

GroupEditorDialog::GroupEditorDialog()
    : mHWND(NULL), mNameEditHWND(NULL), mDestListHWND(NULL),
      mEditing(false), mAccepted(false),
      mIconExists(NULL), mIconMissing(NULL), mIconUndetermined(NULL)
{
}

GroupEditorDialog::~GroupEditorDialog() {
    UnloadStatusIcons();
}

bool GroupEditorDialog::Show(HWND parent, const Group* group, const std::vector<Group>& existingGroups,
                               GroupSavedCallback onGroupSaved) {
    mExistingGroups = existingGroups;
    mOnGroupSaved = onGroupSaved;
    mAccepted = false;

    if (group) {
        mGroup = *group;
        mEditing = true;
    } else {
        mGroup = {};
        mGroup.id = GenerateGroupId();
        mGroup.createdAt = GetIsoTimestamp();
        mGroup.updatedAt = mGroup.createdAt;
        mGroup.lastUsedAt = "";
        mEditing = false;
    }

    HINSTANCE hInstance = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(GroupEditorDialog::WndProc), &hInstance);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileMoveGroupEditorClass";
    wc.hIcon = LoadEmbeddedIcon(hInstance, 32);
    wc.hIconSm = LoadEmbeddedIcon(hInstance, 16);

    RegisterClassExW(&wc);

    std::wstring title = mEditing ? L"Edit Group" : L"New Group";
    mHWND = CreateWindowExW(
        0,
        L"FileMoveGroupEditorClass",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 360,
        parent, NULL, hInstance, this
    );

    if (mHWND) {
        SetWindowLongPtr(mHWND, GWL_USERDATA, reinterpret_cast<LONG_PTR>(this));
        DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(mHWND, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        ShowWindow(mHWND, SW_SHOW);
        UpdateWindow(mHWND);
    } else {
        return false;
    }

    GetClientRect(mHWND, &mClientRect);

    // Group Name edit
    mNameEditHWND = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_EDITW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        15, 35, mClientRect.right - 30, 22,
        mHWND, NULL, hInstance, NULL
    );
    {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, mGroup.name.c_str(), -1, NULL, 0);
        if (wLen > 0) {
            std::wstring nameW(wLen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, mGroup.name.c_str(), -1, &nameW[0], wLen);
            SetWindowTextW(mNameEditHWND, nameW.c_str());
        }
    }
    // Set non-bold font for compactness
    {
        NONCLIENTMETRICSW ncm = {};
        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
        ncm.lfMessageFont.lfWeight = FW_NORMAL;
        HFONT hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        SendMessageW(mNameEditHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(TRUE, 0));
    }

    // Destinations list (owner-draw for status icons)
    mDestListHWND = CreateWindowExW(
        0,
        WC_LISTBOXW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | LBS_OWNERDRAWFIXED,
        15, 80, mClientRect.right - 30, 120,
        mHWND, reinterpret_cast<HMENU>(100), hInstance, NULL
    );
    // Set font on listbox so LB_GETTEXT works correctly
    {
        NONCLIENTMETRICSW ncm2 = {};
        ncm2.cbSize = sizeof(NONCLIENTMETRICSW);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm2, 0);
        ncm2.lfMessageFont.lfWeight = FW_NORMAL;
        HFONT hListFont = CreateFontIndirectW(&ncm2.lfMessageFont);
        SendMessageW(mDestListHWND, WM_SETFONT, reinterpret_cast<WPARAM>(hListFont), MAKELPARAM(TRUE, 0));
        DeleteObject(hListFont);
    }

    // Load status icons
    LoadStatusIcons();

    RefreshDestinations();

    // Create non-bold font for buttons (same as Settings)
    HFONT hBtnFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");

    // Add/Delete buttons
    HWND hAddBtn = CreateWindowExW(0, WC_BUTTONW, L"Add Destination",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        15, 205, 120, 22,
        mHWND, reinterpret_cast<HMENU>(IDM_EDITOR_ADD_DEST), hInstance, NULL);
    SendMessageW(hAddBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    HWND hDelBtn = CreateWindowExW(0, WC_BUTTONW, L"Delete Destination",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        145, 205, 130, 22,
        mHWND, reinterpret_cast<HMENU>(IDM_EDITOR_DELETE_DEST), hInstance, NULL);
    SendMessageW(hDelBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    // OK/Cancel buttons (positioned at bottom of taller window)
    int btnY = mClientRect.bottom - 35;
    HWND hOkBtn = CreateWindowExW(0, WC_BUTTONW, L"OK",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 150, btnY, 65, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_EDITOR_OK), hInstance, NULL);
    SendMessageW(hOkBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    HWND hCancelBtn = CreateWindowExW(0, WC_BUTTONW, L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        mClientRect.right - 75, btnY, 70, 24,
        mHWND, reinterpret_cast<HMENU>(IDM_EDITOR_CANCEL), hInstance, NULL);
    SendMessageW(hCancelBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), MAKELPARAM(TRUE, 0));

    SetFocus(mNameEditHWND);
    InvalidateRect(mHWND, NULL, TRUE);

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

    DeleteObject(hBtnFont);
    UnregisterClassW(L"FileMoveGroupEditorClass", hInstance);
    return mAccepted;
}

const Group& GroupEditorDialog::GetGroup() const {
    return mGroup;
}

GroupEditorDialog* GroupEditorDialog::GetInstance(HWND hwnd) {
    return reinterpret_cast<GroupEditorDialog*>(GetWindowLongPtr(hwnd, GWL_USERDATA));
}

LRESULT CALLBACK GroupEditorDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GroupEditorDialog* instance = GetInstance(hwnd);

    if (!instance && msg != WM_NCCREATE) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            instance = static_cast<GroupEditorDialog*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWL_USERDATA, reinterpret_cast<LONG_PTR>(instance));
            instance->mHInstance = cs->hInstance;
            return TRUE;
        }

        case WM_CREATE: {
            GroupEditorDialog* ge = GetInstance(hwnd);
            SetWindowTextW(hwnd, ge && ge->mEditing ? L"Edit Group" : L"New Group");
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

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            if (mis && mis->CtlType == ODT_LISTBOX) {
                instance->OnMeasureItem(mis);
                return TRUE;
            }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (dis && dis->CtlType == ODT_LISTBOX) {
                instance->OnDrawItem(dis);
                return TRUE;
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

void GroupEditorDialog::OnSize(int width, int height) {
    GetClientRect(mHWND, &mClientRect);
}

void GroupEditorDialog::OnPaint(HDC hdc) {
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

    RECT labelRect = { 15, 12, clientRect.right - 15, 32 };
    DrawTextW(hdc, L"Group Name", -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    labelRect = { 15, 60, clientRect.right - 15, 80 };
    DrawTextW(hdc, L"Destinations", -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT helpRect = { 15, 235, clientRect.right - 15, 270 };
    DrawTextW(hdc, L"Enter a unique group name and add one or more folders.",
        -1, &helpRect, DT_LEFT | DT_TOP);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void GroupEditorDialog::OnCommand(int id) {
    switch (id) {
        case IDM_EDITOR_ADD_DEST: {
            // Open folder browser
            BROWSEINFOW bi = {};
            bi.lpszTitle = L"Select a destination folder";
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                wchar_t path[MAX_PATH];
                if (SHGetPathFromIDListW(pidl, path)) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
                    std::string pathStr(len - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, path, -1, &pathStr[0], len, NULL, NULL);

                    // Check for duplicates
                    bool duplicate = false;
                    for (const auto& existing : mGroup.destinationPaths) {
                        if (existing == pathStr) {
                            duplicate = true;
                            break;
                        }
                    }

                    if (!duplicate) {
                        mGroup.destinationPaths.push_back(pathStr);
                        RefreshDestinations();
                    } else {
                        MessageBoxW(mHWND, L"This destination is already in the list.",
                            L"Duplicate Destination", MB_ICONWARNING);
                    }
                }
                CoTaskMemFree(pidl);
            }
            break;
        }
        case IDM_EDITOR_DELETE_DEST: {
            int sel = static_cast<int>(SendMessageW(mDestListHWND, LB_GETCURSEL, 0, 0));
            if (sel >= 0) {
                mGroup.destinationPaths.erase(mGroup.destinationPaths.begin() + sel);
                RefreshDestinations();
            }
            break;
        }
        case IDM_EDITOR_OK: {
            // Read group name
            wchar_t nameBuf[201];
            GetWindowTextW(mNameEditHWND, nameBuf, _countof(nameBuf));
            std::wstring nameW(nameBuf);

            if (nameW.empty()) {
                MessageBoxW(mHWND, L"Group name cannot be empty.", L"Validation Error", MB_ICONWARNING);
                return;
            }

            // Check for at least 3 alphabetic characters
            int alphaCount = 0;
            for (wchar_t c : nameW) {
                if (iswalpha(c)) alphaCount++;
            }
            if (alphaCount < 3) {
                MessageBoxW(mHWND, L"Group name must contain at least 3 alphabetic characters.",
                    L"Validation Error", MB_ICONWARNING);
                return;
            }

            if (nameW.size() > 200) {
                MessageBoxW(mHWND, L"Group name must be no longer than 200 characters.",
                    L"Validation Error", MB_ICONWARNING);
                return;
            }

            if (mGroup.destinationPaths.empty()) {
                MessageBoxW(mHWND, L"At least one destination is required.",
                    L"Validation Error", MB_ICONWARNING);
                return;
            }

            // Check uniqueness
            int nameLen = WideCharToMultiByte(CP_UTF8, 0, nameW.c_str(), -1, NULL, 0, NULL, NULL);
            std::string nameStr(nameLen - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, nameW.c_str(), -1, &nameStr[0], nameLen, NULL, NULL);
            for (const auto& existing : mExistingGroups) {
                if (existing.name == nameStr && existing.id != mGroup.id) {
                    MessageBoxW(mHWND, L"A group with this name already exists.",
                        L"Validation Error", MB_ICONWARNING);
                    return;
                }
            }

            // Check for missing destinations
            std::wstring missingPaths;
            for (const auto& path : mGroup.destinationPaths) {
                int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
                if (wLen <= 0) continue;
                std::wstring widePath(wLen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &widePath[0], wLen);
                if (!DirectoryExists(widePath)) {
                    std::wstring msgText = L"Destination does not exist. Create it?\n\n" + widePath;
                    int result = MessageBoxW(mHWND, msgText.c_str(),
                        L"Missing Destination", MB_YESNOCANCEL | MB_ICONWARNING);
                    if (result == IDCANCEL) return;
                    if (result == IDYES) {
                        EnsureDirectoryExists(widePath);
                    }
                }
            }

            mGroup.name = nameStr;
            mGroup.updatedAt = GetIsoTimestamp();
            mAccepted = true;

            if (mOnGroupSaved) {
                mOnGroupSaved(mGroup);
            }

            DestroyWindow(mHWND);
            break;
        }
        case IDM_EDITOR_CANCEL: {
            mAccepted = false;
            DestroyWindow(mHWND);
            break;
        }
    }
}

bool GroupEditorDialog::ValidateGroup() {
    return !mGroup.name.empty() && !mGroup.destinationPaths.empty();
}

std::wstring GroupEditorDialog::GetDirectoryStatus(const std::string& path) const {
    int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    if (wLen <= 1) return L"[\u2717]";
    std::wstring widePath(wLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &widePath[0], wLen);

    struct _stat64 st;
    if (_wstat64(widePath.c_str(), &st) != 0) {
        if (widePath.size() >= 2 && widePath[1] == L':') {
            std::wstring driveStr = widePath.substr(0, 2) + L"\\";
            if (GetDriveTypeW(driveStr.c_str()) != DRIVE_NO_ROOT_DIR) {
                return L"[?]";
            }
        }
        return L"[\u2717]";
    }

    if ((st.st_mode & _S_IFDIR) != 0) {
        return L"[\u2713]";
    }

    return L"[?]";
}

void GroupEditorDialog::RefreshDestinations() {
    if (!mDestListHWND) return;

    // Convert all paths to wide-char and store in our vector
    mDestPathsWide.clear();
    for (const auto& path : mGroup.destinationPaths) {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
        if (wLen <= 0) continue;
        std::wstring widePath(wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &widePath[0], wLen);
        mDestPathsWide.push_back(widePath);
    }

    // Reset listbox and add items (listbox stores the wide strings for count/selection)
    SendMessageW(mDestListHWND, LB_RESETCONTENT, 0, 0);

    for (const auto& widePath : mDestPathsWide) {
        SendMessageW(mDestListHWND, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widePath.c_str()));
    }
}

void GroupEditorDialog::LoadStatusIcons() {
    mIconExists = LoadEmbeddedPng(mHInstance, IDR_GREEN_CHECK);
    mIconMissing = LoadEmbeddedPng(mHInstance, IDR_RED_X);
    mIconUndetermined = LoadEmbeddedPng(mHInstance, IDR_ORANGE_QUESTION);
}

void GroupEditorDialog::UnloadStatusIcons() {
    if (mIconExists) { delete static_cast<Image*>(mIconExists); mIconExists = NULL; }
    if (mIconMissing) { delete static_cast<Image*>(mIconMissing); mIconMissing = NULL; }
    if (mIconUndetermined) { delete static_cast<Image*>(mIconUndetermined); mIconUndetermined = NULL; }
}

void GroupEditorDialog::OnMeasureItem(MEASUREITEMSTRUCT* mis) {
    // Set item height to 22 pixels (enough for 16x16 icon + text)
    mis->itemHeight = 22;
    mis->itemWidth = 0;
}

void GroupEditorDialog::OnDrawItem(DRAWITEMSTRUCT* dis) {
    if (dis->itemAction & (ODA_DRAWENTIRE | ODA_SELECT | ODA_FOCUS)) {
        // Fill background
        if (dis->itemState & ODS_SELECTED) {
            FillRect(dis->hDC, &dis->rcItem, (HBRUSH)COLOR_HIGHLIGHT);
            SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
        } else {
            FillRect(dis->hDC, &dis->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
            SetTextColor(dis->hDC, GetSysColor(COLOR_WINDOWTEXT));
        }
        SetBkMode(dis->hDC, TRANSPARENT);

        // Get path from our stored vector (bypass LB_GETTEXT which is unreliable)
        if (dis->itemID >= mDestPathsWide.size()) return;
        const std::wstring& widePath = mDestPathsWide[dis->itemID];

        // Determine status icon for this path
        int len = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 1) return;
        std::string path(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), -1, &path[0], len, NULL, NULL);

        std::wstring status = GetDirectoryStatus(path);

        Image* pIcon = NULL;
        if (status == L"[\u2713]") pIcon = static_cast<Image*>(mIconExists);
        else if (status == L"[\u2717]") pIcon = static_cast<Image*>(mIconMissing);
        else pIcon = static_cast<Image*>(mIconUndetermined);

        // Draw icon using GDI+
        int iconSize = 16;
        int iconX = dis->rcItem.left + 4;
        int iconY = dis->rcItem.top + (dis->rcItem.bottom - dis->rcItem.top - iconSize) / 2;
        if (pIcon) {
            Graphics graphics(dis->hDC);
            graphics.DrawImage(pIcon, iconX, iconY, iconSize, iconSize);
        }

        // Draw text
        RECT textRect = dis->rcItem;
        textRect.left = dis->rcItem.left + iconSize + 8;
        HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hFont);
        DrawTextW(dis->hDC, widePath.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(dis->hDC, hOldFont);
        DeleteObject(hFont);
    }
}
