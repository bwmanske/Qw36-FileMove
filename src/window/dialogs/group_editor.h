#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "data/json_parser.h"

// Button IDs
enum GroupEditorButtons {
    IDM_EDITOR_ADD_DEST = 6001,
    IDM_EDITOR_DELETE_DEST,
    IDM_EDITOR_OK,
    IDM_EDITOR_CANCEL
};

class GroupEditorDialog {
public:
    GroupEditorDialog();
    ~GroupEditorDialog();

    // Callback for when a group is saved
    using GroupSavedCallback = std::function<void(const Group&)>;

    // Show the Group Editor dialog (modal).
    // If group is null, creates a new group. If group is provided, edits it.
    // Returns true if OK was pressed and group is valid.
    bool Show(HWND parent, const Group* group, const std::vector<Group>& existingGroups,
               GroupSavedCallback onGroupSaved);

    // Get the edited group (only valid if Show returned true)
    const Group& GetGroup() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static GroupEditorDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);
    void OnMeasureItem(MEASUREITEMSTRUCT* mis);
    void OnDrawItem(DRAWITEMSTRUCT* dis);
    bool ValidateGroup();
    std::wstring GetDirectoryStatus(const std::string& path) const;
    void RefreshDestinations();
    void LoadStatusIcons();
    void UnloadStatusIcons();

    HWND mHWND;
    HINSTANCE mHInstance;
    RECT mClientRect;
    HWND mNameEditHWND;
    HWND mDestListHWND;
    Group mGroup;
    bool mEditing;
    bool mAccepted;
    GroupSavedCallback mOnGroupSaved;
    std::vector<Group> mExistingGroups;
    void* mIconExists;       // Gdiplus::Image*
    void* mIconMissing;      // Gdiplus::Image*
    void* mIconUndetermined; // Gdiplus::Image*
    std::vector<std::wstring> mDestPathsWide;  // Paths stored as wide-char for owner-draw
};
