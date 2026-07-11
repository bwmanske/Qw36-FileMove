# FileMove Implementation Guide

## Build System

### CMake Configuration

`CMakeLists.txt` defines the project with the following settings:

- **Minimum CMake version**: 3.10
- **Project version**: 1.2.0
- **C++ standard**: C++17 (required)
- **Executable type**: WIN32 subsystem (no console on normal launch)

```cmake
cmake_minimum_required(VERSION 3.10)
project(FileMove VERSION 1.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Source Files

```cmake
set(SOURCES
    src/main.cpp
    src/utils/cmdline_parser.cpp
    src/utils/logging.cpp
    src/data/file_io.cpp
    src/data/json_parser.cpp
    src/queue/queue_manager.cpp
    src/queue/worker_thread.cpp
    src/window/main_window.cpp
    src/window/group_list.cpp
    src/window/dialogs/about.cpp
    src/window/dialogs/status.cpp
    src/window/dialogs/settings.cpp
    src/window/dialogs/group_editor.cpp
    src/window/dialogs/queue_window.cpp
    src/window/dialogs/search.cpp
    src/window/dialogs/new_file.cpp
    src/drag_drop/drop_target.cpp
    src/resources/resource_loader.cpp
    ${CMAKE_BINARY_DIR}/FileMove.rc
)

# Test harness (separate target)
add_executable(test_harness
    tests/test_harness.cpp
    src/utils/cmdline_parser.cpp
    src/utils/logging.cpp
    src/data/file_io.cpp
    src/data/json_parser.cpp
    src/queue/queue_manager.cpp
)
```

### Linked Libraries

| Library | Purpose |
|---|---|
| `gdiplus` | GDI+ image rendering (icons, about image) |
| `uuid` | COM GUID utilities |
| `ole32` | COM initialization, IDataObject for drag-and-drop |
| `oleaut32` | OLE automation support |
| `shell32` | Shell utilities, file operations, drag-and-drop (`HDROP`, `DragQueryFileW`) |
| `comctl32` | Common controls (buttons, listboxes, edit controls) |

### Compile Definitions

| Definition | Purpose |
|---|---|
| `UNICODE` / `_UNICODE` | Wide-character API throughout |
| `WIN32_LEAN_AND_MEAN` | Exclude rarely-used Windows headers |
| `NOMINMAX` | Prevent min/max macro conflicts |
| `FILEMOVE_VERSION` | Injected project version string (`"1.2.0"`) |
| `FILEMOVE_BUILD_DATE` | Compiler `__DATE__ __TIME__` macro |

### Embedded Resources

All image assets are embedded directly into the executable via a Windows resource script (`.rc`). No external asset files are needed at runtime.

**Resource script generation:**

```cmake
set(FILEMOVE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
configure_file(
    resources/FileMove.rc.in
    ${CMAKE_BINARY_DIR}/FileMove.rc
    @ONLY
)
```

The `.rc` file is generated from `resources/FileMove.rc.in` with `@FILEMOVE_ASSETS_DIR@` replaced by the actual source path. The Windows resource compiler embeds the assets into the final `.exe`.

**Embedded resources:**

| Resource ID | Type | File | Purpose |
|---|---|---|---|
| `IDR_MAINICON` (101) | `ICON` | `FileMove-icon.ico` | Application icon (Explorer, taskbar, title bar) |
| `IDR_ABOUT_IMAGE` (201) | `RCDATA` | `about-image.png` | About window image |
| `IDR_GREEN_CHECK` (202) | `RCDATA` | `green-check.png` | Directory exists status icon |
| `IDR_RED_X` (203) | `RCDATA` | `red-x.png` | Directory missing status icon |
| `IDR_ORANGE_QUESTION` (204) | `RCDATA` | `orange-question.png` | Directory undetermined status icon |

**Resource loader** (`src/resources/resource_loader.h/cpp`):
- `LoadEmbeddedIcon(hInstance, size)` — Loads the `.ico` via `LoadImageW(MAKEINTRESOURCEW(...))`
- `LoadEmbeddedPng(hInstance, resourceId)` — Loads `RCDATA` via `FindResourceW` → `CreateStreamOnHGlobal` → `Gdiplus::Image::FromStream`

### Project Structure

```
FileMove/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                      # WinMain, GDI+ init, message loop
│   ├── window/
│   │   ├── main_window.h/cpp         # (Phase 4) Main window
│   │   ├── group_list.h/cpp          # (Phase 4) Group list control
│   │   └── dialogs/
│   │       ├── group_editor.h/cpp    # (Phase 5) Add/modify group
│   │       ├── about.h/cpp           # (Phase 5) About window
│   │       ├── status.h/cpp          # (Phase 5) Status window
│   │       ├── settings.h/cpp        # (Phase 5) Settings window
│   │       ├── queue_window.h/cpp    # (Phase 5) Queue window
│   │       ├── search.h/cpp          # (Phase 5) Search dialog
│   │       ├── shutdown.h/cpp        # (Phase 5) Shutdown prompt
│   │       └── new_file.h/cpp        # (Phase 5) New JSON file dialog
│   ├── data/
│   │   ├── json_parser.h/cpp         # (Phase 2) JSON read/write
│   │   └── file_io.h/cpp             # (Phase 2) File path utilities
│   ├── queue/
│   │   ├── queue_manager.h/cpp       # (Phase 3) Queue management
│   │   └── worker_thread.h/cpp       # (Phase 3) Background move thread
│   ├── drag_drop/
│   │   └── drop_target.h/cpp         # (Phase 6) IDropTarget impl
│   ├── resources/
│   │   ├── resource_ids.h            # (Phase 8) Embedded resource ID definitions
│   │   ├── resource_loader.h/cpp     # (Phase 8) Load embedded resources
│   └── utils/
│       ├── cmdline_parser.h/cpp      # (Phase 2) CLI argument parsing
│       └── logging.h/cpp             # (Phase 2) Debug console + log file
├── resources/
│   └── FileMove.rc.in                # (Phase 8) Resource script template
├── third-party/
│   └── nlohmann/
│       └── json.hpp                  # nlohmann/json v3.11.3 (header-only)
├── tests/
│   └── test_harness.cpp              # (Phase 7) Unit tests (189 tests)
├── assets/
│   ├── icons/
│   │   ├── FileMove-icon.ico         # Application icon (embedded at build)
│   │   └── FileMove-icon.png         # PNG variant
│   └── images/
│       ├── about-image.png           # About window image (embedded at build)
│       ├── green-check.png           # Directory exists status (embedded at build)
│       ├── red-x.png                 # Directory missing status (embedded at build)
│       └── orange-question.png       # Directory undetermined status (embedded at build)
└── specs/
    ├── FileMove-spec-v1.2.0.md
    └── FileMove-mockups-v1.2.0.md
```

## Phase 1: Entry Point & Window Scaffold

### `src/main.cpp`

The application entry point orchestrates the full startup and shutdown sequence:

1. **Command-line parsing** — `ParseCommandLine()` validates all options. On error, opens debug console, displays help, waits for Enter, exits.

2. **Debug console** — Opens if `-D MV` or `-D CP` specified.

3. **Path resolution** — Resolves JSON and log file paths from CLI or defaults.

4. **Log file** — Opens for appending, trims if over 60KB, writes startup records.

5. **JSON file** — Creates default if missing, loads existing (handles empty/malformed).

6. **Settings override** — CLI sort/placement override saved JSON settings.

7. **Queue manager init** — `InitQueueManager()` with settings from JSON.

8. **Worker thread start** — `gWorkerThread.Start()`.

9. **GDI+ init** — `GdiplusStartup` before window creation.

10. **Main window creation** — `gMainWindow.Create()`, sets JSON base name in title, updates group list, applies window placement.

11. **Message loop** — Standard `GetMessage`/`TranslateMessage`/`DispatchMessage`.

12. **Shutdown** — Worker thread stop, saves window state to JSON, writes close records to log, `GdiplusShutdown`.

**Global state:** All globals declared in `main.cpp` and accessed via `extern` in other modules.

### GDI+ Include Order

`objbase.h` must be included before `gdiplus.h` when `WIN32_LEAN_AND_MEAN` is defined, because GDI+ references `IStream` which is excluded by the lean-and-mean flag.

## Phase 2: Core Infrastructure

### Startup Flow

The `WinMain` entry point orchestrates the full startup sequence (see Phase 1 `src/main.cpp` section for details).

### Global State

| Variable | Type | Purpose |
|---|---|---|
| `gdiplusToken` | `ULONG_PTR` | GDI+ shutdown token |
| `gInstance` | `HINSTANCE` | Current module instance |
| `gCommandLine` | `std::wstring` | Full command line string |
| `gParsedArgs` | `ParsedCommandLine` | Parsed CLI options |
| `gAppData` | `AppData` | Loaded groups and settings |
| `gJsonPath` | `std::wstring` | Active JSON file path |
| `gLogPath` | `std::wstring` | Active log file path |
| `gQueueManager` | `QueueManager` | Global queue manager |
| `gWorkerThread` | `WorkerThread` | Global worker thread |
| `gMainWindow` | `MainWindow` | Global main window |
| `gQueueWindow` | `QueueWindow` | Global modeless queue window |

### `src/utils/cmdline_parser.h/cpp`

Parses command-line arguments with strict validation.

**Parsing rules:**
- Options must be prefixed with `/` or `-`
- Case-insensitive option letters and values
- Tokenizer respects quoted strings
- Each option validates its value immediately

**Supported options:**

| Option | Values | Purpose |
|---|---|---|
| `D` | `MV`, `CP` | Debug mode (move or copy) |
| `I` | `<path>` | Input JSON file (`.json` or no extension) |
| `O` | `<path>` | Output log file (`.log` or no extension) |
| `S` | `MRU`, `LRU`, `AF`, `AL`, `AZ`, `ZA` (or full names) | Sort mode |
| `P` | `UL`, `UR`, `LL`, `LR`, `LAST` (or full names) | Placement mode |

**Error handling:** On any parse error, `ParseCommandLine` returns `false` and sets `errorMessage`. The caller (`WinMain`) opens the debug console, displays the error with `GetCommandLineHelp()`, waits for Enter, and exits.

**Helper functions:**
- `SortModeToString()` / `PlacementModeToString()` — Convert to JSON string representation
- `SortModeFromString()` / `PlacementModeFromString()` — Convert JSON string back to enum
- `SortModeDisplayName()` / `PlacementModeDisplayName()` — Convert to UI display name
- `GetCommandLineHelp()` — Returns formatted help text

### `src/utils/logging.h/cpp`

Provides debug console output and log file writing.

**Debug console:**
- `OpenDebugConsole()` — Calls `AllocConsole()`, redirects stdout
- `CloseDebugConsole()` — Calls `FreeConsole()`
- `DebugConsoleWrite()` / `DebugConsoleWriteLine()` — Writes to console via `WriteConsoleW` (no-op if console not open)

**Log file:**
- `OpenLogFile(path)` — Sets active log path, triggers trim check
- `CloseLogFile()` — Clears active log path
- `TrimLogFileIfNeeded()` — If file exceeds 60KB, removes oldest lines until under 50KB
- `LogInfo(message)` — Writes `----> message` to log file and debug console
- `LogTransfer(fileName, sourceDir, destDir, dateTime, result)` — Writes CSV-formatted transfer record
- `GetTimestamp()` — Returns current time as `YYYY-MM-DD HH:MM:SS`

**Log format:**
- Transfer records: `"file","source","dest","timestamp","Success|Error"`
- Info records: `----> descriptive text`

### `src/data/file_io.h/cpp`

File and path utilities for the application.

**Default paths:**
- `GetDefaultDataDirectory()` — Returns `%AppData%\Roaming\FileMove\`, creates if missing
- `GetDefaultJsonPath()` — Returns `.../FileMove.json`
- `GetDefaultLogPath()` — Returns `.../FileMove.log`

**Path resolution:**
- `ResolveJsonPath(inputPath)` — If no directory in input, places in default data directory
- `ResolveLogPath(jsonPath, outputOption)` — Derives from JSON if `O` not specified; respects directory in `O` value

**Path utilities:**
- `GetBaseName(path)` — File name without extension
- `GetDirectory(path)` — Directory portion of path
- `GetFileName(path)` — File name from path
- `ReplaceExtension(path, newExt)` — Replaces file extension

**File operations:**
- `EnsureDirectoryExists(path)` — Recursively creates directory
- `FileExists(path)` / `DirectoryExists(path)` — Existence checks
- `GetFileSize(path)` — Returns size in bytes (-1 on error)
- `ListJsonFiles(directory)` — Returns all `.json` files in directory

### `src/data/json_parser.h/cpp`

JSON persistence for groups and settings using nlohmann/json.

**Data model:**

```cpp
struct Group {
    std::string id;
    std::string name;
    std::vector<std::string> destinationPaths;
    std::string createdAt;
    std::string updatedAt;
    std::string lastUsedAt;
};

struct AppSettings {
    int version = 1;
    std::string lastSelectedGroupId;
    std::string sortMode = "MostRecentlyUsed";
    std::string placementMode = "UpperLeft";
    int windowWidth = 320;
    int windowHeight = 500;
    int windowLeft = 0;
    int windowTop = 0;
    bool enableDirectoryMoves = false;
    bool enableSidecarFiles = false;
    bool hideQueuedSourceFiles = false;
};

struct AppData {
    AppSettings settings;
    std::vector<Group> groups;
};
```

**Key functions:**
- `LoadAppData(path, data)` — Parses JSON file into `AppData`. Returns `true` on success. Empty files (0 bytes) return default `AppData`. Malformed JSON returns `false`.
- `SaveAppData(path, data)` — Serializes `AppData` to pretty-printed JSON (4-space indent).
- `CreateDefaultJson(path)` — Creates a new JSON file with default settings.
- `GenerateGroupId()` — Generates unique `grp-XXXXXX` IDs.
- `GetIsoTimestamp()` — Returns current UTC time in ISO 8601 format.

**Legacy migration:** Groups with a single `DestinationPath` field are automatically migrated to a one-item `destinationPaths` array on load. Saved files always use `destinationPaths`.

## Phase 3a: Data Model & Queue Manager

### `src/queue/queue_manager.h/cpp`

Manages the file move queue with two-stage prepare/release semantics.

**Data model:**

```cpp
enum class QueueEntryStatus {
    Queued,       // Prepared, waiting for release
    Released,     // Released to worker thread
    Processing,   // Currently being processed by worker
    Completed,    // Successfully completed
    Failed,       // Failed during processing
    Canceled      // Canceled by user or shutdown
};

enum class DebugTransferMode {
    MV,  // Normal move (remove source after all destinations succeed)
    CP   // Debug copy (do not remove source)
};

struct PendingMoveEntry {
    std::string id;
    std::string groupId;
    std::string sourceFilePath;
    std::vector<std::string> destinationDirectories;
    std::string activeLogFilePath;
    DebugTransferMode debugTransferMode;
    std::string queuedAt;
    QueueEntryStatus status;
};
```

**Two-stage queue operations:**

1. **Prepare** (`PrepareBatch`) — Validates all source files and destination directories before any entry is queued. If any validation fails, the entire batch is rejected. On success, entries are added with `Queued` status.

2. **Release** (`ReleasePreparedEntries`) — Moves all `Queued` entries to `Released` status, making them available for the worker thread.

**Validation in `PrepareBatch`:**
- Validates each source file exists
- Checks directory moves option for directory inputs
- Deduplicates by source file path (skips already-queued files with "Already queued" message)
- Validates destination directories, separates available from unavailable
- Rejects batch if no destinations are available
- Checks for source-vs-destination path conflicts

**Sidecar files:**
- When `enableSidecarFiles` is true, creates `.filemove-queued` hidden marker file for each queued source
- Removes sidecar on completion, cancellation, or failure
- `CreateSidecar()` / `RemoveSidecar()` handle the marker lifecycle

**Hidden source files:**
- When `hideQueuedSourceFiles` is true, marks source files as hidden when queued
- `RestoreSourceVisibility()` restores visible state on completion/cancellation

**Thread safety:**
- All public methods acquire `mMutex` before accessing `mEntries`
- Worker thread will access queue through thread-safe methods

**Queue state callback:**
- `SetChangedCallback()` registers a callback invoked whenever queue state changes
- Main window uses this to update status bar queued count

**Global instance:**
- `gQueueManager` — Global queue manager instance
- `InitQueueManager()` — Initializes with settings from loaded JSON

## Phase 3b: Worker Thread

### `src/queue/worker_thread.h/cpp`

Background thread that processes queued file moves with pause/resume, error handling, and shutdown support.

**Worker states:**
- `Idle` — No work in progress
- `Moving` — Actively transferring files
- `ManualPause` — Paused by user from Status window
- `PausedError` — Paused due to transfer error, awaiting retry/cancel

**Shutdown actions:**
- `None` — Normal operation
- `FinishCurrentAndExit` — Complete current source file across all destinations, then exit
- `CancelAndExit` — Cancel immediately, clean up partial files, exit

**Main worker loop:**
1. Checks for pause state, waits if paused
2. Gets next entry from queue manager
3. Processes entry across all destinations
4. Checks shutdown action after each entry
5. Loops until stopping flag is set

**File transfer process:**
1. For each destination directory:
   - Checks pause/shutdown state
   - Logs operation to log file and debug console
   - Checks for file conflicts (calls conflict callback)
   - Copies file to destination using Windows `CopyFile`
   - On success: increments processed count, logs success
   - On failure: removes partial file, pauses for error handling
2. After all destinations:
   - In MV mode: removes source file only if all destinations succeeded
   - In CP mode: keeps source file, logs that it was preserved
   - If any destination failed/skipped: keeps source file in place

**Conflict resolution:**
- `Replace` — Overwrites existing destination file
- `KeepBoth` — Renames new file with `(1)`, `(2)`, etc. before extension
- `Skip` — Skips this destination, source file remains in place

**Error handling:**
- On transfer error: pauses worker, removes partial destination file
- Calls error pause callback with error message
- User can choose Retry (continues) or Cancel (clears queue)
- During error pause, worker state shows `Paused - Error`

**Thread synchronization:**
- `std::atomic<bool>` for running, paused, stopping flags
- `std::atomic<ShutdownAction>` for shutdown control
- `std::mutex` + `std::condition_variable` for pause/resume signaling
- Worker polls queue manager every 500ms when idle

**Global instance:**
- `gWorkerThread` — Global worker thread instance
- Started during app initialization after queue manager setup
- Stopped during app shutdown before cleanup

## Phase 4: Main Window

### `src/window/main_window.h/cpp`

The main application window with pinned header, group list, and status bar.

**Window layout:**
- Pinned header (28px): Contains "New / Tools" button
- Group list: Fills space between header and status bar
- Status bar (22px): Shows "Queued: X" (left) and "Status: X" (right)

**Header behavior:**
- Left click: Opens `GroupEditorDialog` in new mode (empty fields, "New Group" title)
- Right click: Shows context menu with Queue Window, Status, Settings, Search, About

**Group list integration:**
- Embedded `GroupList` control
- Receives `WM_NOTIFY` for right-click events
- Updates from `gAppData.groups` on startup and after group changes

**Status bar:**
- `UpdateStatusBar(queuedCount, workerState)`: Refreshes display
- Queued count from `gQueueManager.GetQueuedDestinationCount()`
- Worker state from `gWorkerThread.GetState()`

**Window placement:**
- `ApplyPlacement()`: Positions window based on placement mode
- Respects screen bounds, clamps to work area
- Supports UpperLeft, UpperRight, LowerLeft, LowerRight, LastLocation

**Shutdown prompt:**
- `ShowShutdownPrompt()`: Returns user choice
- Uses `MessageBoxW` with Yes/No/Cancel
- Yes: `ShutdownFinishCurrent` — calls `gWorkerThread.RequestShutdown(FinishCurrentAndExit)`
- No: `ShutdownCancelImmediate` — calls `gWorkerThread.RequestShutdown(CancelAndExit)`
- Cancel: Returns to app, no action

**WM_CLOSE handler:**
- Checks `gWorkerThread.IsBusy()` and `gQueueManager.GetQueuedDestinationCount()`
- If idle and empty: exits immediately
- If active or queued: shows shutdown prompt, routes to appropriate shutdown action

**Tooltip support:**
- `mTooltipHWND` — Tooltip control created in `Create()`
- `ShowTooltip(row)` — Builds tooltip from `GroupList::GetTooltipForRow()`, adds to tooltip control
- `NM_HOVER` notification from group list triggers tooltip display
- Hover delay set to 500ms via `TTM_SETDELAYTIME`

**Window procedure:**
- `MainWindowWndProc`: Global procedure, accesses instance via `GWL_USERDATA`
- Handles `WM_NCCREATE`, `WM_SIZE`, `WM_PAINT`, `WM_COMMAND`, `WM_NOTIFY`, `WM_CONTEXTMENU`, `WM_LBUTTONDOWN`, `WM_CLOSE`, `WM_DESTROY`
- `WM_NOTIFY` handles both `NM_RCLICK` (group right-click) and `NM_HOVER` (tooltip)
- Friend declaration allows access to private members

**OnCommand handlers:**
- `IDM_QUEUE_WINDOW` — Creates or foregrounds modeless `QueueWindow`
- `IDM_STATUS` — Opens modal `StatusDialog` with JSON file switching callback
- `IDM_SETTINGS` — Opens modal `SettingsDialog`, saves to JSON on OK, refreshes group list
- `IDM_SEARCH` — Opens modal `SearchDialog`, filters group list by substring match
- `IDM_ABOUT` — Opens modal `AboutDialog`
- `IDM_EDIT` — Opens `GroupEditorDialog` pre-populated with selected group
- `IDM_DELETE` — Confirms with `MessageBoxW`, removes group, saves JSON, refreshes list
- `IDM_USE_CLIPBOARD` — Placeholder for Phase 6

### `src/window/group_list.h/cpp`

Custom list control for displaying saved groups.

**Features:**
- Report-style ListView with single "Name" column
- Full row selection, double-buffered rendering
- Sorts groups by current sort mode (MRU, LRU, AF, AL, AZ, ZA)
- Right-click context menu via `WM_NOTIFY` / `NM_RCLICK`

**Sorting:**
- `SortGroups()`: Sorts groups in-place by sort mode
- `CompareGroups()`: Static comparison function
- Handles timestamp-based sorting (MRU/LRU/AF/AL) and alphabetical (AZ/ZA)

**Tooltip building:**
- `BuildTooltip()`: Creates multi-line tooltip with group name and destinations
- `GetDirectoryStatus()`: Returns V/X/? for each destination path
- Checks drive availability for network paths

**Callbacks:**
- `SetRightClickCallback()`: For group context menu
- `SetSelectCallback()`: For group selection
- `SetDropCallback()`: For drag-and-drop (Phase 6)

**Integration:**
- Created as child of main window
- Resized on `WM_SIZE` to fill available space
- Updated from `gAppData.groups` on startup

## Phase 5: Dialogs

All dialogs are modal (except Queue Window which is modeless). Each dialog registers its own window class, creates controls, runs a local message loop, and unregisters on exit.

### Dialog Pattern

Each dialog follows this structure:

```cpp
class SomeDialog {
public:
    SomeDialog();
    ~SomeDialog();

    // Show dialog (modal). Returns result or modifies output parameter.
    ResultType Show(HWND parent, ...);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static SomeDialog* GetInstance(HWND hwnd);

    void OnPaint(HDC hdc);
    void OnCommand(int id);
    void OnSize(int width, int height);

    HWND mHWND;
    HINSTANCE mHInstance;
    RECT mClientRect;
    // Dialog-specific state...
};
```

**Message loop:** Each dialog runs its own message loop processing only messages for its window, allowing the parent to remain responsive to system events.

**WIN32_LEAN_AND_MEAN compatibility:** Macros like `Button_SetCheck`, `ListBox_AddString`, etc. are excluded by `WIN32_LEAN_AND_MEAN`. All dialogs use `SendMessageW` with explicit message constants (`BM_SETCHECK`, `LB_ADDSTRING`, etc.) defined locally.

### `src/window/dialogs/about.h/cpp`

Modal About window showing build information.

**Layout:**
- Centered 128x128 image from `assets/images/about-image.png`
- "Build Information" header
- "Version: 1.2.0" (left) and "Built On: DATE TIME" (right) on same line
- "Command Line:" followed by current run's arguments
- Description text

**Behavior:**
- Dismisses on any click or key press
- Uses GDI+ `Image::FromFile` to render the about image
- `__DATE__` and `__TIME__` macros provide build timestamp

### `src/window/dialogs/status.h/cpp`

Modal Status window showing active files, queue status, and JSON file list.

**Layout:**
- **Active Files section:** JSON path, LOG path with right-justified "Open" button
- **Queue Status section:** Queued/Processed count, Worker State, Current File, Current Destination, Last Queue Error, Pause/Resume button
- **JSON Files section:** Listbox of `.json` files in default data directory
- **Bottom buttons:** "New" and "Open Selected"

**Behavior:**
- "Open" button launches log file in default editor via `ShellExecuteW`
- Pause/Resume toggles worker thread state
- "New" opens New File dialog, creates/opens JSON file, auto-closes Status on success
- "Open Selected" loads the selected JSON file, switches active log file, auto-closes Status on success
- JSON file list auto-selects the currently active file
- `RequestClose()` allows external code to close the dialog (used for auto-close)

**JSON file switching callback:**
- `JsonFileOpenCallback` is invoked when user selects a JSON file
- Callback loads new `AppData`, updates `gJsonPath`/`gLogPath`, refreshes group list
- On failure, shows error and keeps current file active

### `src/window/dialogs/settings.h/cpp`

Modal Settings window for sort order, placement, and options.

**Layout:**
- **Sort Order:** 6 radio buttons in 2 columns (MRU, LRU, AZ, ZA, AF, AL)
- **Placement:** 5 radio buttons (UL, UR, LL, LR, Last Location)
- **Startup Preview:** Read-only display of saved size and position
- **Options:** 3 checkboxes (Directory Moves, Sidecar Files, Hidden Source)
- **Bottom buttons:** "OK" and "Cancel"

**Behavior:**
- Pre-populates controls from current `AppSettings`
- "OK" calls `ApplySettings()` to read control states and update settings, then saves to JSON
- "Cancel" discards changes
- After OK, main window refreshes group list with new sort mode and updates queue manager with new options

**Button IDs:**
- Sort: `IDM_SORT_MRU` (3001) through `IDM_SORT_AL` (3006)
- Placement: `IDM_PLACEMENT_UL` (3010) through `IDM_PLACEMENT_LAST` (3014)
- Options: `IDM_OPT_DIRECTORY_MOVES` (3020) through `IDM_OPT_HIDDEN_SOURCE` (3022)

### `src/window/dialogs/group_editor.h/cpp`

Modal dialog for adding or modifying a group.

**Layout:**
- **Group Name:** Edit control (200 char max)
- **Destinations:** Listbox showing each destination with 3-state status icon (`[V]`, `[X]`, `[?]`)
- **Destination buttons:** "Add Destination" (opens folder browser), "Delete Destination"
- **Help text:** Context-appropriate instruction
- **Bottom buttons:** "OK" and "Cancel"

**Behavior:**
- **New mode:** Empty fields, title "New Group"
- **Edit mode:** Pre-populated from existing group, title "Edit Group"
- **Add Destination:** Opens `SHBrowseForFolderW` dialog, validates for duplicates
- **Delete Destination:** Removes selected destination from list
- **OK validation:**
  - Name not empty, at least 3 alphabetic characters, max 200 characters
  - Name unique (unless editing same group)
  - At least one destination required
  - Each missing destination prompts to create with Yes/No/Cancel
- **On save:** Invokes `GroupSavedCallback` to update `gAppData.groups` and save to JSON

**Destination status icons:**
- `[V]` — Directory exists (confirmed via `_wstat64`)
- `[X]` — Directory missing (drive doesn't exist)
- `[?]` — Directory undetermined (drive exists but path doesn't)

### `src/window/dialogs/queue_window.h/cpp`

Modeless Queue window showing currently queued destination file paths.

**Layout:**
- Title bar shows "Queue (N)" with current count
- Heading: "Queued Destinations (N)"
- Listbox of destination file paths

**Behavior:**
- Remains open while main window continues accepting drops
- `UpdateList(destinations)` refreshes the list and title
- Global instance `gQueueWindow` allows access from anywhere
- Created once, subsequent menu clicks bring it to foreground

### `src/window/dialogs/search.h/cpp`

Modal Search dialog for filtering groups by name.

**Layout:**
- "Search Groups" label
- Edit control for search text
- "OK" and "Cancel" buttons

**Behavior:**
- Returns search text on OK, empty string on Cancel
- Main window filters `gAppData.groups` by substring match and refreshes group list
- Initial text can be provided (e.g., previous search)

### `src/window/dialogs/new_file.h/cpp`

Modal dialog for creating a new JSON file by base name only.

**Layout:**
- "New JSON Base Name" label
- Edit control
- Help text: "Enter a base name only. The file will be created in the default data directory."
- "OK" and "Cancel" buttons

**Behavior:**
- Returns `NewFileResult{accepted, baseName}`
- **Validation on OK:**
  - Name not empty
  - No path separators (`\`, `/`)
  - No extensions (`.`)
  - Shows warning and keeps dialog open on validation failure
- Status window uses this to create/open new JSON files

### Main Window Integration

All dialogs are wired into `MainWindow::OnCommand()`:

| Menu Item | Dialog | Trigger |
|---|---|---|
| Queue Window | `QueueWindow` | Header right-click |
| Status | `StatusDialog` | Header right-click |
| Settings | `SettingsDialog` | Header right-click |
| Search | `SearchDialog` | Header right-click |
| About | `AboutDialog` | Header right-click |
| New Group | `GroupEditorDialog` | Header left-click |
| Edit Group | `GroupEditorDialog` | Group right-click |
| Delete Group | `MessageBoxW` confirmation | Group right-click |

**Group operations:**
- **New:** Creates new group with generated ID, adds to `gAppData.groups`, saves JSON, refreshes list
- **Edit:** Finds group by selected ID, pre-populates editor, saves changes back
- **Delete:** Confirms with user, removes from `gAppData.groups`, saves JSON, refreshes list

## Phase 6: Explorer Integration

### `src/drag_drop/drop_target.h/cpp`

COM `IDropTarget` implementation for accepting file drops from Windows Explorer onto group list rows.

**Architecture:**
- `DropTarget` class inherits from `IDropTarget` and implements all four required methods
- Registered on the group list's HWND via `RegisterDragDrop()`
- Hit-tests the drop point against ListView rows to determine target group
- Extracts file paths from `IDataObject` using `CF_HDROP` format
- Invokes `DropRowHandler` callback with (row, files) for the main window to process

**COM lifecycle:**
- Reference counting via `AddRef()`/`Release()` with `InterlockedIncrement`/`InterlockedDecrement`
- Self-deletes when reference count reaches zero
- `QueryInterface` supports `IID_IUnknown` and `IID_IDropTarget`

**Drop flow:**
1. `DragEnter` — Checks if data object contains `CF_HDROP`, sets `DROPEFFECT_MOVE`
2. `DragOver` — No-op (effect already set)
3. `DragLeave` — No-op
4. `Drop` — Extracts files, hit-tests row, invokes handler

**File extraction:**
- `GetDroppedFiles()` queries `IDataObject` for `CF_HDROP` via `FORMATETC`/`STGMEDIUM`
- Iterates `DragQueryFileW` for each file in the drop
- Properly calls `DragFinish()` and `ReleaseStgMedium()` to clean up

**Hit-testing:**
- Converts screen coordinates to client coordinates via `ScreenToClient()`
- Uses `ListView_HitTest()` with `LVHITTESTINFO` to find target row
- Returns `S_OK` with `DROPEFFECT_NONE` if drop is outside a valid row

### Clipboard Integration (`main_window.cpp::OnUseClipboard`)

Clipboard paste handler for the "Use Clipboard" context menu item.

**Flow:**
1. Opens clipboard via `OpenClipboard(mHWND)`
2. Checks `IsClipboardFormatAvailable(CF_HDROP)` for file list
3. Gets `HGLOBAL` via `GetClipboardData(CF_HDROP)`, casts to `HDROP`
4. Closes clipboard immediately after getting handle
5. Extracts file paths via `DragQueryFileW`
6. Cleans up with `DragFinish(hDrop)`
7. Gets selected group from `mGroupList.GetSelectedGroupId()`
8. Calls `gQueueManager.PrepareBatch()` with files and group destinations
9. On success, calls `gQueueManager.ReleasePreparedEntries()`
10. Updates group `lastUsedAt` and saves JSON

**Error handling:**
- Clipboard not available: shows error message
- No file data in clipboard: shows error message
- No group selected: prompts user to select a group first
- Validation failure: shows `PrepareBatch` error message

### Main Window Integration (`main_window.cpp::OnDrop`)

Drop handler that processes files dropped on a group row.

**Flow:**
1. Validates row index against group list count
2. Gets group ID from `mGroupList.GetGroupIdAtRow(row)`
3. Finds group in `gAppData.groups` to get destination paths
4. Calls `gQueueManager.PrepareBatch(groupId, files, destinations, errorMsg)`
5. On failure: shows error message box
6. On success: calls `gQueueManager.ReleasePreparedEntries()`
7. Updates group `lastUsedAt` timestamp
8. Saves JSON

### Queue Integration

Both drop and clipboard use the same two-stage queue flow:

1. **Prepare** — `gQueueManager.PrepareBatch()` validates all source files and destinations:
   - Checks each source file exists
   - Validates destination directories (separates available from unavailable)
   - Deduplicates by source path (skips already-queued files)
   - Checks for source-vs-destination path conflicts
   - Creates sidecar files if enabled
   - Hides source files if enabled
   - Returns `false` with error message on any validation failure

2. **Release** — `gQueueManager.ReleasePreparedEntries()` moves all `Queued` entries to `Released` status, making them available for the worker thread

### Drop Target Registration

In `MainWindow::Create()`:

```cpp
mDropTarget = new DropTarget();
mDropTarget->Init(mGroupList.GetHWND(),
    [this](int row, const std::vector<std::string>& files) {
        this->OnDrop(row, files);
    });
RegisterDragDrop(mGroupList.GetHWND(), mDropTarget);
```

The drop target is registered on the group list's HWND (not the main window), so hit-testing is performed against the ListView control directly.

## Implementation Notes

### Window Classes (C++/GDI+)

All windows use the Win32 API directly (no MFC/ATL). Each window type follows this pattern:

```cpp
LRESULT CALLBACK SomeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
```

Custom drawing (group list rows, status icons, tooltips) uses GDI+ `Graphics` objects created from the window DC in `WM_PAINT` handlers.

### WIN32_LEAN_AND_MEAN Macro Definitions

`WIN32_LEAN_AND_MEAN` excludes many commonly-used macros. These are defined locally in files that need them:

| Macro | Value | Used By |
|---|---|---|
| `GWL_USERDATA` | `-21` | All windows (instance pointer storage) |
| `GET_X_LPARAM` | `((int)(short)LOWORD(lp))` | `main_window.cpp` |
| `GET_Y_LPARAM` | `((int)(short)HIWORD(lp))` | `main_window.cpp` |
| `TD_INITIAL` | `0` | Tooltip hover time |
| `BST_CHECKED` | `1` | Settings, Group Editor |
| `BST_UNCHECKED` | `0` | Settings |
| `BM_SETCHECK` | `241` | Settings (radio/checkbox state) |
| `BM_GETCHECK` | `240` | Settings (reading state) |
| `LB_RESETCONTENT` | `379` | Status, Queue, Group Editor |
| `LB_ADDSTRING` | `380` | Status, Queue, Group Editor |
| `LB_GETCURSEL` | `386` | Status, Group Editor |
| `LB_SETCURSEL` | `387` | Status |

All dialogs use `SendMessageW` with these constants instead of the convenience macros (`Button_SetCheck`, `ListBox_AddString`, etc.) that are excluded by `WIN32_LEAN_AND_MEAN`.

### Dialog Button ID Ranges

Each dialog uses its own ID range to avoid conflicts:

| Dialog | Range | IDs |
|---|---|---|
| Header Menu | 1001-1005 | Queue, Status, Settings, Search, About |
| Group Menu | 2001-2003 | Use Clipboard, Edit, Delete |
| Settings | 3001-3030 | Sort radios, Placement radios, Options, OK/Cancel |
| Status | 4001-4004 | Open Log, Pause/Resume, New, Open Selected |
| Queue | 5001 | (placeholder) |
| Group Editor | 6001-6004 | Add/Delete Dest, OK/Cancel |
| Search | 7001-7002 | OK/Cancel |
| New File | 8001-8002 | OK/Cancel |

### JSON Library Integration

nlohmann/json (v3.11.3, single-header) is used for all JSON operations. The header is stored locally at `third-party/nlohmann/json.hpp` and included via the `third-party` include path.

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Parse JSON from string
json j = json::parse(json_string);

// Access values
std::string name = j["name"];
int version = j["version"];

// Serialize to pretty-printed string
std::string output = j.dump(4);
```

### Drag and Drop Implementation

Per-row `IDropTarget` implementation for Explorer integration:

```cpp
class DropTarget : public IDropTarget {
    // Implement QueryInterface, AddRef, Release
    STDMETHOD(DragEnter)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
    STDMETHOD(DragLeave)();
    STDMETHOD(Drop)(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
};
```

Each group list row will have its own registered `IDropTarget` instance with hit-testing to determine which row received the drop.

### GDI+ Drawing

Initialize GDI+ in `WinMain`:

```cpp
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;
GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
```

Use in `WM_PAINT` or custom control drawing:

```cpp
Graphics graphics(hdc);
Image* image = Image::FromFile(L"assets\\images\\green-check.png");
graphics.DrawImage(image, x, y, width, height);
delete image;
```

### Status Icons

Three-state icons loaded from `assets/images/` at runtime via GDI+ `Image::FromFile`:

| File | State | Tooltip Symbol |
|---|---|---|
| `green-check.png` | Directory Exists | `✓` |
| `red-x.png` | Directory Missing | `X` |
| `orange-question.png` | Directory Undetermined | `?` |

Icons are used in:
- Group editor destination rows
- Group list hover tooltips
- Any UI surface showing destination path availability

### Threading Model

- **Main thread**: UI message loop, drag-and-drop acceptance, clipboard operations
- **Worker thread** (`std::thread`): Processes queued file moves, reports results back via thread-safe queue
- **Synchronization**: Win32 events and `std::mutex` for pause/resume, shutdown signaling, and queue access

### Build Commands

```powershell
# Configure
cmake -B build -S .

# Build (Release)
cmake --build build --config Release

# Output: build/Release/FileMove.exe
```

## Phase 7: Polish & Integration

### Application Icon Integration

The application icon (`FileMove-icon.ico`) is loaded at runtime from the assets directory relative to the executable:

1. `MainWindow::Create()` resolves the executable directory via `GetModuleFileNameW()`
2. Icon loaded via `LoadImageW()` with `LR_LOADFROMFILE` at 32x32 and 16x16
3. Assigned to `WNDCLASSEXW.hIcon` and `hIconSm` before `RegisterClassExW()`
4. Also set explicitly via `WM_SETICON` after window creation for taskbar/alt-tab

The executable directory (`mExeDir`) is stored as a member variable and used by all dialogs to locate assets (icons, images).

### 3-State Icon Rendering

**Group List Tooltips:**
- `GetDirectoryStatus()` returns Unicode symbols: `✓` (exists), `✗` (missing), `?` (undetermined)
- `BuildTooltip()` prepends the status symbol to each destination path

**Group Editor Destination List:**
- Owner-draw ListBox (`LBS_OWNERDRAWFIXED`) with custom `WM_MEASUREITEM` and `WM_DRAWITEM` handlers
- Status icons loaded as GDI+ `Image*` objects from PNG files (`green-check.png`, `red-x.png`, `orange-question.png`)
- `OnDrawItem()` renders icons via `Graphics::DrawImage()` at 16x16, followed by text via `DrawTextW()`
- Icons stored as `void*` in header (GDI+ not available in header scope), cast to `Image*` in `.cpp`
- Proper cleanup in `UnloadStatusIcons()` via destructor

### Directory Move Expansion

When `enableDirectoryMoves` is enabled, `PrepareBatch()` expands directory sources into individual file entries:

1. Each source path is checked with `DirectoryExists()`
2. If a directory, `EnumerateDirectoryFiles()` recursively walks the tree using `FindFirstFileW`/`FindNextFileW`
3. All discovered files are added to the expanded source list
4. Each file is validated and queued individually
5. If directory moves are disabled and a directory is encountered, the batch is rejected with an error

`EnumerateDirectoryFiles()` in `file_io.cpp`:
- Recursive helper `EnumerateDirectoryFilesRecursive()` skips `.` and `..` entries
- Collects full file paths for all files in the directory tree

### Source-vs-Destination Path Conflict Detection

Enhanced conflict detection in `PrepareBatch()` with three-way categorization:

1. **No match** (source not in any destination): Proceed normally
2. **All match** (source already in all destinations): Skip silently, log as duplicate
3. **Partial match** (source in some destinations): Show `MessageBoxW` with Yes/No:
   - Yes: Continue with only non-matching destinations
   - No: Cancel entire batch with error message

Implementation:
- `SourceConflictInfo` struct tracks matching and non-matching destinations per source
- Path normalization (forward slashes) for accurate comparison
- Entry creation filters out matching destinations per source
- Source files with partial matches keep the source in place after remaining destinations succeed

### Worker Thread File Copy

`CopyToDestination()` implements actual file transfer:
- Uses `CopyFileW()` with `FAIL_IF_EXISTS` for atomicity
- Error handling for `ERROR_ACCESS_DENIED`, `ERROR_DISK_FULL`, `ERROR_SHARING_VIOLATION`
- Retry loop in `ProcessEntry()`: on failure, pauses worker, offers Retry/Cancel via `ErrorPauseCallback`
- On Retry: loops back to attempt the same destination again
- On Cancel: clears remaining queue, marks entry as failed

### Asset Path Resolution

All asset paths are resolved relative to the executable directory:
- `mExeDir` computed once in `MainWindow::Create()` via `GetModuleFileNameW()`
- Icons: `mExeDir + "\\assets\\icons\\FileMove-icon.ico"`
- Images: `mExeDir + "\\assets\\images\\"` (about-image, status icons)
- CMake `POST_BUILD` command copies `assets/` to build output directory

## Unit Tests

### Test Harness

`tests/test_harness.cpp` — Console application with simple inline test framework (`ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_STR_EQ`, `ASSERT_WSTR_EQ`).

**Build:** `cmake --build build --target test_harness --config Release`
**Run:** `build/Release/test_harness.exe`
**CMake:** `ctest --config Release`

### Test Coverage (189 tests)

**cmdline_parser (45 tests):**
- Empty command line, valid/invalid `/D` values (MV, CP, case insensitive)
- `/I` with no extension (adds .json), .json extension, invalid extension
- `/O` with no extension (adds .log), .log extension, invalid extension
- `/S` all sort modes (MRU, LRU, AF, AL, AZ, ZA, full names, invalid)
- `/P` all placement modes (UL, UR, LL, LR, LAST, full names, invalid)
- Multiple options, unknown options, non-prefixed tokens (skipped)
- `SortModeToString`/`FromString` round-trips
- `PlacementModeToString`/`FromString` round-trips
- `GetCommandLineHelp` returns non-empty

**file_io (25 tests):**
- `GetBaseName`, `GetDirectory`, `GetFileName`, `ReplaceExtension`
- `ResolveJsonPath` with/without directory, with/without extension
- `ResolveLogPath` derived from JSON, explicit path
- `FileExists`, `DirectoryExists`, `GetFileSize` with temp files
- `EnsureDirectoryExists` recursive creation
- `EnumerateDirectoryFiles` recursive enumeration
- `ListJsonFiles` filtering

**json_parser (30 tests):**
- `CreateDefaultJson` creates valid default file
- Save/load round-trip with all settings and groups
- Empty file (0 bytes) loads as default
- Malformed JSON returns false
- Legacy `DestinationPath` migration to `destinationPaths`
- `GenerateGroupId` produces unique IDs
- `GetIsoTimestamp` produces valid ISO 8601 format
- Multiple groups save/load

**queue_manager (89 tests):**
- Empty queue state
- `PrepareBatch` with no files, no destinations, non-existent source/dest
- Valid batch preparation, release, entry retrieval
- Deduplication by source path
- `CancelPreparedEntries`, `CancelAllEntries`
- Sidecar file creation and cleanup
- Multiple files in batch, multiple destinations per file
- Directory move disabled (rejects directories)
- Directory move enabled (expands directories recursively)

### Bugs Fixed During Testing

1. **`EnsureDirectoryExists` logic error** — `_waccess` return value was negated incorrectly (`!_waccess()` instead of `_waccess() != 0`), preventing directory creation
2. **`cmdline_parser` non-prefixed tokens** — Tokens without `/` or `-` prefix were treated as errors instead of being skipped per spec

## Phase 8: Documentation & Embedded Resources

### Documentation

- `docs/BUILDING.md` — CMake build instructions, prerequisites, linked libraries, troubleshooting
- `docs/TESTING.md` — Unit test breakdown, 8 integration test scenarios, debug mode testing, log verification
- `docs/USER_GUIDE.md` — Getting started, all dialogs, CLI options, file conflicts, shutdown behavior, troubleshooting

### Embedded Resources

All image assets are embedded into the executable at build time via a Windows resource script. The executable is fully self-contained — no external `assets/` folder is needed at runtime.

**New files:**
- `resources/FileMove.rc.in` — Resource script template with `@FILEMOVE_ASSETS_DIR@` macro
- `src/resources/resource_ids.h` — Shared resource ID definitions (101, 201-204)
- `src/resources/resource_loader.h/cpp` — `LoadEmbeddedIcon()` and `LoadEmbeddedPng()` helpers

**Modified files:**
- `CMakeLists.txt` — Added `configure_file` for .rc generation, added resource_loader.cpp and generated .rc to sources, removed `POST_BUILD` asset copy
- `main_window.cpp` — Icon loaded via `LoadEmbeddedIcon()` instead of `LoadImageW(LR_LOADFROMFILE)`
- `about.cpp` — About image loaded via `LoadEmbeddedPng(IDR_ABOUT_IMAGE)` instead of `Image::FromFile()`
- `group_editor.cpp` — 3-state icons loaded via `LoadEmbeddedPng()` instead of `Image::FromFile()`

**Key implementation details:**
- `.ico` is embedded as a standard `ICON` resource, so Windows Explorer displays it automatically
- PNG images are embedded as `RT_RCDATA` resources, loaded at runtime via `IStream` + `Gdiplus::Image::FromStream()`
- `WIN32_LEAN_AND_MEAN` excludes `<objbase.h>`, so `resource_loader.cpp` must `#include <objbase.h>` before `<gdiplus.h>` to make `IStream` available
- CMake `configure_file` generates the `.rc` with correct absolute paths to the `assets/` directory
