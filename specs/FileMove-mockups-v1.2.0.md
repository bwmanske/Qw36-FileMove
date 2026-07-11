# Window Mockups

## Build System

**Platform**: Windows (primary), Linux (future consideration)
**Language**: C++17 with GDI+
**Build Tool**: CMake 3.5+

### Project Structure

```
FileMove/
├── CMakeLists.txt                    # cmake_minimum_required(VERSION 3.5)
├── src/
│   ├── main.cpp                      # Entry point
│   ├── window/                       # Main and child windows
│   │   ├── main_window.h/cpp        # Primary group list window
│   │   ├── dialogs/                 # About, Status, Settings, etc.
│   │   └── controls/                # Custom controls (group list, etc.)
│   ├── data/                         # JSON storage and file I/O
│   │   ├── json_parser.h/cpp        # Uses nlohmann/json or similar
│   │   └── file_io.h/cpp            # Log file, JSON persistence
│   ├── queue/                        # Queue management
│   │   ├── queue_manager.h/cpp
│   │   └── worker_thread.h/cpp      # Background move thread
│   ├── drag_drop/                    # Explorer integration
│   │   └── drop_target.h/cpp        # IDropTarget implementation
│   └── utils/
│       ├── logging.h/cpp             # Debug console and log file
│       └── cmdline_parser.h/cpp      # Command-line argument parsing
├── assets/
│   ├── icons/FileMove-icon.png      # Application icon
│   ├── images/about-image.png       # About dialog image (128x128)
│   ├── images/green-check.png       # Directory exists status
│   ├── images/red-x.png             # Directory missing status
│   └── images/orange-question.png   # Directory undetermined status
└── specs/
    ├── FileMove-spec-v1.2.0.md      # Primary spec
    └── FileMove-mockups-v1.2.0.md   # UI mockups and implementation details
```

## Group Tooltip

```text
+-----------------------------------------------------------+
| Invoices 2026                                             |
|                                                           |
| [✓] Z:\Accounting\Invoices                               |
| [X] X:\Archive\Invoices                                  |
| [?] Y:\ColdStorage\Invoices                              |
+-----------------------------------------------------------+
```

### Notes

- The first line shows the group name.
- Each destination appears on its own line under the group name.
- Each destination line begins with a per-directory status icon.
- Use the same 3 status states used elsewhere in the app:
  - `✓` directory exists
  - `X` directory missing
  - `?` directory undetermined
- The tooltip should use this layout for group rows in the main window.

## Settings Window

```text
+-----------------------------------------------------------+
| Settings                                            [X]   |
|-----------------------------------------------------------|
| Sort Order                                                |
|   ( ) Most Recently Used      ( ) Least Recently Used     |
|   ( ) A thru Z                ( ) Z thru A                |
|   ( ) Added First             ( ) Added Last              |
|                                                           |
| Placement                                                 |
|   ( ) Upper Left                         Upper Right ( )  |
|                    ( ) Last Location                      |
|   ( ) Lower Left                         Lower Right ( )  |
|                                                           |
| Options                                                   |
|   [ ] Move directories with subdirectories and files      |
|   [ ] Create .filemove-queued sidecar files               |
|   [ ] Experimental: mark queued source files as hidden    |
|                                                           |
| Startup Preview                                           |
|   Size: [ 320 ] W   [ 500 ] H                             |
|   Saved Position: Left [    0 ]  Top [    0 ]             |
|   Note: Restored within visible screen bounds.            |
|                                                           |
|                                      [OK] [Cancel]        |
+-----------------------------------------------------------+
```

### Notes

- Keep this window small and modal.
- Use radio buttons for sort and placement because only one choice is valid in each section.
- Use independent checkboxes in the `Options` section because each option can be enabled or disabled separately.
- Show saved width, height, left, and top as read-only fields or small labels.
- `OK` saves settings to JSON.
- `Cancel` closes without saving.

## About Window

```text
+-----------------------------------------------------------+
| About                                               [X]   |
|-----------------------------------------------------------|
|                         [128 x 128 image]                 |
|                                                           |
| Build Information                                         |
|   Version: 1.1.0          Built On: 2026-05-09 21:45:00   |
|   Command Line: -D MV -I WorkGroups                       |
|                                                           |
| FileMove is a compact Windows utility for routing files   |
| from Explorer into named destination groups.              |
|                                                           |
|                                                  [Close]  |
+-----------------------------------------------------------+
```

### Notes
 
- `About` is opened from the `New / Tools` header right-click menu.
- The top image is centered and uses `FileGroupMover/Assets/Images/about-image.png`.
- The top section shows build information for Version `1.2.0`.
- `Version` is left justified and `Built On` is right justified on the same line.
- The second line shows `Command Line:` followed by any command-line arguments that were specified.
- The About window uses the application icon.


## Status Window

```text
+-----------------------------------------------------------+
| Status                                              [X]   |
|-----------------------------------------------------------|
| Active Files                                              |
|   JSON: C:\Users\Brad\AppData\Roaming\FileMove\FileMove...|
|   LOG:  C:\Users\Brad\AppData\Roaming\FileMove\FileMove...|
|                                                     [Open]|
|                                                           |
| Queue Status                                              |
|   Queued / Processed: 12 / 48    Worker State: Moving     |
|   Current File:  C:\Users\Brad\Downloads\Report.pdf       |
|   Current Destination: Z:\Accounting\Invoices             |
|   Last Queue Error: None                                  |
|                                             [Pause]       |
|                                                           |
| JSON Files In Default Data Directory                      |
|   FileMove.json                                           |
|   WorkGroups.json                                         |
|   ArchiveGroups.json                                      |
|   QuarterlyWork.json                                      |
|   Testing.json                                            |
|                                                           |
|                          [New] [Open Selected]            |
+-----------------------------------------------------------+
```

### Notes
 
- `Status` is opened from the `New / Tools` header right-click menu.
- The Active Files section shows the full JSON path and the full `.log` path.
- The `LOG` line includes a right-justified `Open` button.
- `Open` launches the active `.log` file in the default editor.
- The queue-status section is the second section in this window.
- The queue-status section includes a right-justified `Pause` or `Resume` button below `Last Queue Error`.
- `Queued / Processed` shows the current unfinished destination-file count followed by the count of destination files completed by the worker thread.
- `Worker State` should show `Manual Pause` when the user pauses transfers.
- `Worker State` should show an error-pause state when the worker pauses because of a transfer error.
- The JSON file list shows files found in the default data directory.
- The window height should allow at least 5 visible JSON file rows in this list.
- The bottom row includes `New` and `Open Selected`.
- `New` creates or opens a JSON file by base name only in the default data directory.
- Selecting a JSON file and choosing `Open Selected` immediately opens that JSON file.
- After `Open Selected` succeeds, the Status window closes automatically.
- Opening a JSON file from this list also switches the active `.log` file to the matching base-name `.log` file in the default data directory.
- If `New` succeeds, the Status window also closes automatically.
- When possible, the main window title bar should display the active JSON base name in parentheses after the app name.
- Example: `FileMove (FileMove)`
 

## New JSON File Window

```text
+-----------------------------------------------------------+
| New File                                            [X]   |
|-----------------------------------------------------------|
| New JSON Base Name                                        |
|   [QuarterlyWork                                    ]     |
|                                                           |
| Enter a base name only. The file will be created in the   |
| default data directory.                                   |
|                                                           |
|                                      [OK] [Cancel]        |
+-----------------------------------------------------------+
```

### Notes

- This window is opened from the `New` button in the Status window.
- Only a base name is allowed.
- Paths are not allowed.
- Extensions are not allowed.
- The app creates or opens `<BaseName>.json` and `<BaseName>.log` in the default data directory.

## Add or Modify Group Window

```text
+-----------------------------------------------------------+
| Group                                               [X]   |
|-----------------------------------------------------------|
| Group Name                                                |
|   [Invoices 2026                                    ]     |
|                                                           |
| Destinations                                              |
|   [✓] Z:\Accounting\Invoices                             |
|   [?] X:\Archive\Invoices                                |
|                                                           |
|   [Add Destination] [Delete Destination]                  |
|                                                           |
| Enter a unique group name and add one or more folders.    |
|                                                           |
|                                      [OK] [Cancel]        |
+-----------------------------------------------------------+
```

## Add Group State

```text
+-----------------------------------------------------------+
| New Group                                           [X]   |
|-----------------------------------------------------------|
| Group Name                                                |
|   [                                                  ]    |
|                                                           |
| Destinations                                              |
|   [ ]                                                  |
|                                                           |
|   [Add Destination] [Delete Destination]                  |
|                                                           |
|   Enter a unique group name and choose one or more        |
|   folders.                                                |
|                                                           |
|                                      [OK] [Cancel]        |
+-----------------------------------------------------------+
```

## Edit Group State

```text
+-----------------------------------------------------------+
| Edit Group                                          [X]   |
|-----------------------------------------------------------|
| Group Name                                                |
|   [Invoices 2026                                    ]     |
|                                                           |
| Destinations                                              |
|   [✓] Z:\Accounting\Invoices                             |
|   [X] X:\Archive\Invoices                                |
|                                                           |
|   [Add Destination] [Delete Destination]                  |
|                                                           |
|   Enter a unique group name and add one or more folders.  |
|                                                           |
|                                      [OK] [Cancel]        |
+-----------------------------------------------------------+
```

### Notes

- `New / Settings -> New` opens the add-group version.
- Right-click `Edit` on a group opens the edit version with the fields pre-populated.
- Each destination row begins with a 3-state icon:
  - `✓` directory exists
  - `X` directory missing
  - `?` directory undetermined
- Each group can contain multiple destinations.
- `Add Destination` inserts another destination row into the list.
- `Delete Destination` removes the selected destination row from the list.
- The helper line below the destination list is for validation and guidance, not for per-destination existence state.
- `OK` should validate:
  - group name is not empty
  - group name is unique unless editing the same group
  - at least one destination is present
  - each destination is valid or can be created with confirmation
  - undetermined destinations may remain saved and should be checked again when used
- Keep labels left-aligned and controls compact to match the app's dense style.

## Right-Click Menu Mockups

## `New / Tools` Header Menu
 
```text
+------------------+
| New / Tools      |
| Queue Window (12)|
| Status           |
| Settings         |
| Search           |
| About            |
+------------------+
```
 
### Notes
 
- Right click on the pinned `New / Tools` header opens this menu.
- `Queue Window` opens the Queue window.
- `Status` opens the Status window.
- `Settings` opens the Settings window.
- `Search` opens the Search dialog.
- `About` opens the About window.

 
## Group Row Menu
 
```text
+------------------+
| Use Clipboard    |
| Edit             |
| Delete...        |
+------------------+
```
 
### Notes
 
- Right click on any actual group row opens this menu.
- `Use Clipboard` validates clipboard files and all destination folders before queueing the batch.
- `Edit` opens the Edit Group window populated with the selected group's values.
- `Delete...` should include confirmation before removing the group entry.
 
## Queue Window
 
```text
+-----------------------------------------------------------+
| Queue                                               [X]   |
|-----------------------------------------------------------|
| Queued Destinations (12)                                  |
|                                                           |
| Z:\Accounting\Invoices\Report.pdf                         |
| Z:\Accounting\Archive\Report.pdf                          |
| D:\Backups\Invoices\2026\Report.pdf                       |
| Z:\Accounting\Invoices\Budget.xlsx                        |
| Z:\Accounting\Archive\Budget.xlsx                         |
| ...                                                       |
+-----------------------------------------------------------+
```
 
### Notes
 
- `Queue Window` is opened from the `New / Tools` header right-click menu.
- The menu label shows the current queued destination count in parentheses.
- The window is non-modal and can remain open while the main window continues accepting drops and moves.
- The list updates automatically when destinations are added to the queue or completed by the worker thread.
 
## Shutdown Window
 
```text
+-----------------------------------------------------------+
| Shutdown                                            [X]   |
|-----------------------------------------------------------|
| File transfers are still active.                          |
|                                                           |
| Finish the current file across all destinations, or stop  |
| immediately and leave the source file in place.           |
|                                                           |
|   [Cancel Shutdown]                                       |
|   [Finish Current File And Exit]                          |
|   [Cancel Active File And Exit Immediately]               |
+-----------------------------------------------------------+
```
  
### Notes
  
- This window appears when the app is asked to close while a transfer is active or files remain queued.
- `Finish Current File And Exit` is the recommended default action.
- `Finish Current File And Exit` completes the current source file across all destinations, logs the remaining queued files, and then exits.
- `Cancel Active File And Exit Immediately` cancels the active source file, removes any incomplete destination file, logs the canceled file and remaining queued files, and then exits.

## File Conflict Dialog

```text
+-----------------------------------------------------------+
| File Conflict                                         [X] |
|-----------------------------------------------------------|
| The following file already exists in the destination:      |
|                                                           |
|   Report.pdf                                               |
|                                                           |
|   Destination: Z:\Accounting\Invoices\Report.pdf           |
|                                                           |
| How would you like to handle this conflict?               |
|                                                           |
|   [Replace]                                                |
|   [Keep Both]                                              |
|   [Skip]                                                   |
+-----------------------------------------------------------+
```

### Notes

- This is a modal dialog shown when a source file already exists at the destination path.
- The file name is shown on the first line.
- The full destination path is shown on the second line.
- Three buttons are stacked vertically:
  - `Replace` — Overwrites the existing destination file with the source file.
  - `Keep Both` — Renames the new file by appending `(1)`, `(2)`, etc. before the extension. Example: `Report (1).pdf`. If that name also exists, increments until a unique name is found.
  - `Skip` — Skips this file. The source file remains in place. The destination file is untouched.
- The dialog is compact and modal.
- The default button is `Replace`.

## Menu in Context
 
### Right click on a group row
 
```text
+--------------------------------------------------+
| Groups                                           |
| ------------------------------------------------ |
| Invoices 2026       +------------------+         |
| Tax Docs            | Use Clipboard    |         |
| Vendor PDFs         | Edit             |         |
| Archive             | Delete...        |         |
| Movies              +------------------+         |
| TV Shows                                         |
| ------------------------------------------------ |
| Queued: 12                    Status: Moving     |
+--------------------------------------------------+
```

 
### Right click on a group row
 
```text
+--------------------------------------------------+
| Groups                                           |
| ------------------------------------------------ |
| Invoices 2026       +------------------+         |
| Tax Docs            | Use Clipboard    |         |
| Vendor PDFs         | Edit             |         |
| Archive             | Delete...        |         |
| Movies              +------------------+         |
| TV Shows                                         |
| ------------------------------------------------ |
| Queued: 12                    Status: Moving     |
+--------------------------------------------------+
```

