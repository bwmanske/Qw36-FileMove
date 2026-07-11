#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "data/json_parser.h"
#include "utils/cmdline_parser.h"

// Callback when a group row is right-clicked
using GroupRightClickCallback = std::function<void(int groupId, POINT pt)>;

// Callback when a group row is left-clicked (for selection)
using GroupSelectCallback = std::function<void(int groupId)>;

// Callback for drag-and-drop on a group row
using GroupDropCallback = std::function<void(int groupId, const std::vector<std::string>& files)>;

class GroupList {
public:
    GroupList();
    ~GroupList();

    // Create the list control within a parent window
    HWND Create(HWND parent, RECT rect);

    // Update the list from app data (applies current sort mode)
    void UpdateFromData(const std::vector<Group>& groups, SortMode sortMode);

    // Set callbacks
    void SetRightClickCallback(GroupRightClickCallback callback);
    void SetSelectCallback(GroupSelectCallback callback);
    void SetDropCallback(GroupDropCallback callback);

    // Get the group ID at a given row index
    std::string GetGroupIdAtRow(int row) const;

    // Get the group name at a given row index
    std::string GetGroupNameAtRow(int row) const;

    // Get the currently selected group ID
    std::string GetSelectedGroupId() const;

    // Get the row count
    int GetRowCount() const;

    // Get the HWND of the list control
    HWND GetHWND() const;

    // Force a redraw
    void Invalidate();

    // Set the icon directory for status icons
    void SetIconDirectory(const std::wstring& iconDir);

    // Get tooltip text for a row index
    std::wstring GetTooltipForRow(int row) const;

    // Scroll to a group by name, centering it if possible
    void ScrollToGroup(const std::string& groupName);

    // Highlight a group by name with system highlight color
    void HighlightGroup(const std::string& groupName);

    // Clear any active highlight
    void ClearHighlight();

    // Get the currently highlighted group name
    const std::string& GetHighlightedGroup() const;

private:
    // Sort groups by the current sort mode
    void SortGroups(std::vector<Group>& groups, SortMode sortMode);

    // Compare two groups by sort mode
    static int CompareGroups(const Group& a, const Group& b, SortMode sortMode);

    // Build tooltip text for a group
    std::wstring BuildTooltip(const Group& group) const;

    // Get directory status string for tooltip
    std::wstring GetDirectoryStatus(const std::string& path) const;

    HWND mListHWND;
    std::vector<Group> mGroups;
    SortMode mSortMode;
    std::wstring mIconDir;

    GroupRightClickCallback mRightClickCallback;
    GroupSelectCallback mSelectCallback;
    GroupDropCallback mDropCallback;

    std::string mHighlightedGroup;
    int mHighlightedRow;
};
