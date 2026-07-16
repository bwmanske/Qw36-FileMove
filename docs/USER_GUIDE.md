# FileMove v1.3.1 — User Guide

## Overview

FileMove is a compact Windows utility that stores named destination groups for files. You select files in Windows Explorer, then use FileMove as a destination chooser to move them to one or more saved folders.

**Data directory**: `%AppData%\Roaming\FileMove\`

## Getting Started

### First Launch

1. Run `FileMove.exe`
2. The main window opens with a compact group list
3. Left-click the **New Group** button to create your first group
4. Enter a group name (at least 3 letters, max 200 characters)
5. Add one or more destination folder paths
6. Click **OK**

### Creating a Group

1. Left-click **New Group** button
2. Enter a unique group name
3. Click **Add Destination** to add destination folders
4. Each destination shows a status icon:
   - **Green check** — Directory exists
   - **Red X** — Directory missing (app can create it)
   - **Orange question** — State undetermined (e.g., busy network drive)
5. At least one destination is required
6. Click **OK**

## Moving Files

### Drag and Drop

1. Select files in Windows Explorer
2. Drag them onto a group row in FileMove
3. Files are validated and queued
4. The worker thread moves files to all destinations
5. Source files are removed after all destinations succeed

### Clipboard Paste

1. In Explorer, copy or cut files (`Ctrl+C` or `Ctrl+X`)
2. Right-click a group row in FileMove
3. Select **Use Clipboard**
4. Files are validated and moved

## Main Window

### Layout

```
+--------------------------------------------------+
| New Group                              [gear]    |
+--------------------------------------------------+
| Group Name 1                                     |
| Group Name 2                                     |
| Group Name 3                                     |
+--------------------------------------------------+
| Queued: 5                      Status: Moving    |
+--------------------------------------------------+
```

### Header

| Button | Action | Result |
|---|---|---|
| **New Group** | Left-click | Open group editor (create new group) |
| **Gear icon** | Left-click | Context menu: Queue Window, Status, Settings, Search, About |
| Either | Right-click | No action |

The **Queue Window** menu label shows the current queued destination count in parentheses.

### Title Bar

The title bar shows the app name followed by the active JSON file base name in parentheses (e.g., `FileMove (FileMove)`).

### Group List

- Each row displays only the group name
- Hover to see full destination paths with status icons
- Right-click a row for: **Use Clipboard**, **Edit**, **Delete**

### Status Bar

- **Queued:** — Number of destination files waiting or in progress
- **Status:** — Current worker state (Idle, Moving, Paused, etc.)

## Dialogs

### Group Editor

Used for creating and editing groups. Contains:
- **Name** field (3-200 characters, must contain letters)
- **Destination list** with per-row status icons
- **Add Destination** / **Delete Destination** buttons
- **OK** / **Cancel** buttons

### Status Window

Accessed via gear button > `Status`. Shows:
- **Active Files**: Current JSON and `.log` file paths
- **Open** button on the LOG line to launch the log file in the default editor
- **JSON file list**: All `.json` files in the data directory
- **Open Selected**: Switch to the selected JSON file
- **New**: Create a new JSON file (base name only)
- **Queue Status**: `Queued / Processed:` counts, Worker State, Pause/Resume button
- **Close** button

The Status window closes automatically after successfully opening or creating a JSON file.

### Queue Window

Accessed via gear button > `Queue Window`. Non-modal window showing:
- All destination file paths currently queued or in progress
- Live-updating list as files are added and processed
- Current queued count in the heading
- **Pause / Resume** button in the heading to toggle the worker thread pause state
- **Delete** button — removes selected entries from the queue (enabled only during Manual Pause, requires at least one selection)
- **Empty** button — removes all entries from the queue with a confirmation dialog (enabled only during Manual Pause)
- **Close** button

### Settings Window

Accessed via gear button > `Settings`. Contains:

**Sort Order** (radio buttons):
- Most Recently Used (default)
- Least Recently Used
- Added First
- Added Last
- A thru Z
- Z thru A

**Placement** (radio buttons):
- Upper Left
- Upper Right
- Lower Left
- Lower Right
- Last Location

**Options** (checkboxes):
- Move directories with subdirectories and files — Recursively move directories and their contents
- Preserve directory structure at destination — Preserve the source directory name and subdirectory structure under each destination (dependent on directory moves being enabled)
- Create empty directories — Also create empty subdirectories at the destination (dependent on preserve directory structure being enabled)
- Create .filemove-queued sidecar files — Create hidden `.filemove-queued` marker files for queued sources
- Experimental: mark queued source files as hidden — Mark source files as hidden while queued

### Search Window

Accessed via gear button > `Search`. Modal dialog containing:
- Text input box at the top for typing a search string
- Listbox below that filters group names in real time as you type (case-insensitive)
- Double-click a group name to select it — the main window scrolls to that group and highlights it
- **Close** button at the bottom-right

### About Window

Accessed via gear button > `About`. Shows:
- Application icon and image
- Version and build date
- Command-line arguments for the current session

## Command-Line Options

All options are case-insensitive. Use `/` or `-` prefix.

| Option | Values | Description |
|---|---|---|
| `/D` | `MV`, `CP` | Debug mode: `MV` = normal move, `CP` = copy only |
| `/I` | `<path>` | Input JSON file (`.json` extension auto-added if missing) |
| `/O` | `<path>` | Output log file (`.log` extension auto-added if missing) |
| `/S` | `MRU`, `LRU`, `AF`, `AL`, `AZ`, `ZA` | Sort mode |
| `/P` | `UL`, `UR`, `LL`, `LR`, `LAST` | Window placement |

### Examples

```powershell
# Normal launch
FileMove.exe

# Debug mode with copy behavior
FileMove.exe /D CP

# Custom JSON file, sorted A-Z, upper-right corner
FileMove.exe /I WorkGroups /S AZ /P UR

# Full debug with custom log location
FileMove.exe -D MV -I C:\Data\Groups.json -O C:\Logs\Groups.log
```

### Sort Mode Values

| Code | Full Name | Meaning |
|---|---|---|
| `MRU` | `MostRecentlyUsed` | Most recently used first |
| `LRU` | `LeastRecentlyUsed` | Least recently used first |
| `AF` | `AddedFirst` | Oldest groups first |
| `AL` | `AddedLast` | Newest groups first |
| `AZ` | `AtoZ` | Alphabetical |
| `ZA` | `ZtoA` | Reverse alphabetical |

### Placement Mode Values

| Code | Full Name | Meaning |
|---|---|---|
| `UL` | `UpperLeft` | Upper-left corner of screen |
| `UR` | `UpperRight` | Upper-right corner of screen |
| `LL` | `LowerLeft` | Lower-left corner of screen |
| `LR` | `LowerRight` | Lower-right corner of screen |
| `LAST` | `LastLocation` | Restore last saved position |

## File Conflicts

When a destination file already exists, you will be prompted:

| Option | Behavior |
|---|---|
| **Replace** | Overwrites the existing file |
| **Keep Both** | Renames new file with `(1)`, `(2)`, etc. |
| **Skip** | Leaves both files untouched |

## Shutdown Behavior

If you close FileMove while files are being processed:

| Option | Behavior |
|---|---|
| **Cancel Shutdown** | Returns to app, continues processing |
| **Finish Current File And Exit** | Completes current file, then exits |
| **Cancel Active File And Exit Immediately** | Stops immediately, cleans up partial files |

## Log File

Location: `%AppData%\Roaming\FileMove\<BaseName>.log`

The log file contains two types of records:

**Transfer records** (CSV format):
```
"Report.pdf","C:\Users\Brad\Downloads","Z:\Accounting","2026-06-09 10:42:18","Success"
```

**Info records** (prefixed with `---->`):
```
----> App started: 2026-06-09 10:41:03
----> JSON file opened: 2026-06-09 10:41:03 (C:\Data\Groups.json)
```

Log files are automatically trimmed when they exceed 60 KB (reduced to 50 KB by removing oldest entries).

## Troubleshooting

### Files Not Moving

- Check that destination folders exist (green check icon in group editor)
- Verify you have write permissions to destination folders
- Check the `.log` file for error details

### Network Drive Unavailable

- Groups with offline network drives show an orange question icon
- Files will queue but the unavailable destination will be skipped
- Source files remain in place if any destination was unavailable

### Queue Stuck

- Open Status window and click **Pause / Resume** to reset the worker
- Check for file conflicts or permission issues in the log file

### Debug Mode

Launch with `/D MV` to open a debug console with detailed output:
```powershell
FileMove.exe /D MV
```
