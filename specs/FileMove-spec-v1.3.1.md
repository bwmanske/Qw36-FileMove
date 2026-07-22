# FileMove

## Product idea

Build a small Windows desktop utility that stores named destination groups for files.

Each group stores:

- A group name
- One or more destination folder paths

The app itself does not need to be the main place where files are selected. The user continues working in Windows Explorer, selects files there, and then uses the app as a compact destination chooser.

The app should stay small and dense so many groups and controls can fit on screen without wasted space.

## Explorer-first workflow

### First run

1. App opens with no saved groups.
2. User clicks `New`.
3. User enters a group name.
4. User adds one or more destination folders on local drives or mapped network drives.
5. App saves the group.
6. User goes to Windows Explorer, selects files, and sends them to the group using one of the supported Explorer integration methods.

### Later runs

1. App opens and loads saved groups.
2. User sees a compact searchable list of groups.
3. User selects files in Windows Explorer.
4. User uses drag and drop or `Use Clipboard` with the selected group.
5. App moves the files to the group's saved destination list.

### Creating another group

1. User clicks `New`.
2. User enters a group name.
3. User adds one or more destinations.
4. The new group appears in the list and becomes available immediately for Explorer-based file actions.

## Core requirements

- Save group name and destination list between sessions.
- Support local drives and mapped network drives.
- Present a reusable interface element for selecting existing groups.
- Keep the interface compact and dense.
- Validate that each destination exists or allow it to be created.
- Prevent duplicate group names unless the user is editing an existing group.
- Support moving one or more files selected in Windows Explorer.
- Confirm success or show a clear error for any file that failed.

## File interaction methods

The app should support Explorer-based ways of handing files to a saved group.

- Drag and drop files from Explorer onto a group in the app
- Copy or cut in Explorer, then `Paste Into Group` in the app

## Recommended interface

Use a single compact window focused entirely on managing and selecting destination groups.

### Main layout
  
- A pinned header at the top of the window containing a `New Group` button and a gear icon button
- Dense list of saved groups below the header
- Right-click context menu for list-level actions (on group rows)
- A small status bar at the bottom of the main window
 
Main-window status bar:



- Show `Queued:` left justified
- Show the current unfinished destination-file count after `Queued:`
- Show `Status:` right justified
- Show the current worker state after `Status:`

This is a minimal list-focused window intended to stay visible without taking much screen space.

### Group list presentation
 
- The list contains all saved groups.
- Each group row should display only the group name.
- All group rows are drop targets for files dragged from Windows Explorer.
- The full destination path should not be shown inside the list row.
- Hovering over a group should show a tooltip with:
  - Group name on the first line
  - One line per destination underneath
  - A per-destination status icon on the left of each destination path
  - Full destination paths
- The tooltip destination status icons should use the same 3 states used elsewhere in the app:
  - `Directory Exists`
  - `Directory Missing`
  - `Directory Undetermined`
 
Why:
 
- Windows paths can be very long.
- Showing full paths in the list wastes horizontal space.
- Group names are the primary scanning target.
- Tooltips keep the interface dense while still making the full path available on demand.
- A pinned header with `New Group` and gear buttons keeps creation and settings access consistent regardless of scrolling or sorting.
 

### Header behavior
  
- The header contains two buttons:
  - `New Group` button on the left — creates a new group
  - Gear icon button on the right — opens the tools context menu
- Left click on the `New Group` button opens the window for creating a new group name and destination location.
- Left click on the gear icon button opens a context menu with:
  - `Queue Window`
  - `Status`
  - `Settings`
  - `Search`
  - `About`
- Right click on either button has no action.


- `Active JSON` opens the Active JSON window.
- `Queue Window` opens a non-modal Queue window.
- `Settings` opens the settings window, which includes the sort-order selection and placement options.
- `About` opens the About window.
- The `Queue Window` menu label should show the current queued destination count in parentheses.
 

### About window
 
Add a separate `About` window opened from the gear icon button context menu.
 
The top of the About window must show:


- A centered `128 x 128` image from `FileGroupMover/Assets/Images/about-image.png`
- Build information:
  - Version
  - Built On date and time
  - Command line arguments used for the current run

About window behavior:

- In the Build Information section, `Version` should be left justified and `Built On` should be right justified on the same line.
- The second line in the Build Information section should show `Command Line:` followed by any command-line arguments that were specified for the current run.
- The About window should use the application icon.
- The About window must not be resizable. It maintains the size it is created with.

### Active JSON window

Add a separate `Active JSON` window opened from the gear icon button context menu.

The top section of the Active JSON window must show:


- Currently open JSON file full path
- Currently open `.log` file full path
- An `Open` button on the `LOG` line to open the active `.log` file in the default editor

Active JSON window behavior:

- The user must be able to switch to another JSON file without restarting the app.
- The Active JSON window must show a list of JSON files found in the default data directory.
- The Active JSON window must include a `New` button beside `Open Selected`.
- The `New` button must allow creation of a new JSON file without leaving the app.
- Selecting one of those JSON files opens that JSON file immediately.
- Double-clicking a JSON file entry in the list has the same effect as selecting it and clicking `Open Selected`.
- If the selected JSON file exists but has length `0`, open it as an empty/default settings file instead of reporting an error.
- When a new or empty JSON file is opened, the `Options` settings must default to `false` for all 5 options.
- After a JSON file is opened successfully from that list, the Active JSON window should close automatically.
- When a JSON file is opened from that list, the active `.log` file must become a file with the same base name and the `.log` extension, located in the default data directory.
- The `New` button must accept only a base name, not a path.
- If the user attempts to include a path, reject it and keep the user in the new-file dialog.
- When `New` is used, create or open `<BaseName>.json` in the default data directory.
- When `New` is used, create or open `<BaseName>.log` in the default data directory.
- If the requested base name already exists in the default data directory, switch to that file exactly as though it had been selected and `Open Selected` had been used.
- After a new JSON file is created or an existing matching JSON file is opened successfully from the `New` action, the Active JSON window should close automatically.
- The Active Files section should show the full JSON path and the full `.log` path.
- The `LOG` line in the Active Files section should include a right-justified `Open` button.
- Pressing `Open` should launch the active `.log` file in the system's default editor.
- The Active JSON window should be tall enough to show the Active Files section and at least 5 visible JSON file rows in the default-directory file list.
- When possible, the currently open JSON file base name should also be shown in the main window title bar in parentheses after the app name.
- Example title bar format: `FileMove (FileMove)`

### Queue window
 
Add a separate non-modal `Queue` window opened from the gear icon button context menu.
 
Queue window behavior:

- The Queue window must show the destination file paths currently still queued or in progress.
- The Queue window must be allowed to remain open while the main window continues accepting drops and moves.
- The Queue window must update automatically when destination entries are added to the queue.
- The Queue window must update automatically when destination moves complete and leave the queue.
- The Queue window must include a Queue Status section at the top showing: Queued/Processed count, Worker State, Current File, Current Destination, Last Queue Error.
- The Queue window must include a `Pause / Resume` button positioned just above the listbox that toggles the worker thread pause state.
- The Queue window must include a `Delete` button below the listbox that removes one or more selected entries from the queue.
- The Queue window must include an `Empty` button below the listbox that removes all entries from the queue.
- The `Empty` button must show a confirmation dialog before clearing all entries.
- The `Delete` and `Empty` buttons are only enabled when the worker thread is in a manual pause state (`Manual Pause`).
- When the worker is active or idle, the `Delete` and `Empty` buttons must be disabled.
- The `Delete` button requires at least one entry to be selected in the listbox. Multi-select is supported.
- The `Empty` button requires at least one queued entry to be enabled.
- The Queue window must include a `Close` button that closes the Queue window.
- When queue entries are canceled via the `Delete` or `Empty` buttons, log each canceled entry to the active `.log` file.
- When the queue is emptied, log the number of entries that were removed.

### Search window

Add a separate modal `Search` window opened from the gear icon button context menu.

Search window behavior:

- The Search window contains a text input box at the top for typing a search string.
- Below the text input box is a listbox that displays all loaded group names.
- As the user types into the text input box, the listbox is filtered in real time to show only groups whose name contains the typed string.
- The search is case-insensitive.
- If no groups match the search string, the listbox is empty.
- The Search window includes a `Close` button at the bottom-right.
- The Search window closes when the `Close` button is clicked.
- The Search window also closes when the user double-clicks a group name in the listbox.
- When a group name is selected (double-clicked), the main window's group list scrolls to display that group.
- The group should be scrolled as close to the center of the main window as possible.
- The selected group receives a system highlight color (`COLOR_HIGHLIGHT`) in the main window's group list.
- The highlight is permanent until cleared by a subsequent group selection or another search.
- The Search window must not be resizable. It maintains the size it is created with.

### Group row right-click behavior

Right clicking on any group row opens a context menu with:
 
- `Use Clipboard`
- `Edit`
- `Delete`


`Use Clipboard` behavior:

- Verify that the clipboard contains only valid file entries
- Verify that every destination directory in the group is valid
- If both checks pass, try to move the clipboard files to the group's destination
- If both checks pass, try to move the clipboard files to all of the group's destinations
- If the clipboard contents are invalid or the destination is invalid, do not move anything and report the problem to the user

`Edit` behavior:

- Open the same window used by left-clicking the header
- Pre-populate that window with the selected group's name and destination list

`Delete` behavior:

- Delete the entire group entry
- Require confirmation from the user before deleting
- Do not delete the entry if the user does not confirm

### Group sorting

Move the sort-order selection into the settings window rather than showing it in the main window.

Required sort options:

- Most Recently Used
- Least Recently Used
- Added First
- Added Last
- A thru Z
- Z thru A

Definitions:

- Most Recently Used sorts by `LastUsedAt` descending
- Least Recently Used sorts by `LastUsedAt` ascending
- Added First sorts by `CreatedAt` ascending
- Added Last sorts by `CreatedAt` descending
- A thru Z sorts by group name ascending
- Z thru A sorts by group name descending

- Default sort: `Most Recently Used`
- Save the selected sort mode in the JSON settings so the app restores it on startup
- Sorting applies only to actual group entries

### Multiple destinations per group

- Each group can contain one or more destination folders.
- The Add or Modify Group window must display a destination list rather than a single destination field.
- Each destination row in the list must begin with a small status icon.
- The status icon must support 3 states:
  - `Directory Exists`
  - `Directory Missing`
  - `Directory Undetermined`
- `Directory Undetermined` is used when the app cannot reliably confirm the directory state, such as when a drive is busy or has not spun up yet.
- Destinations can be added to the list.
- Destinations can be deleted from the list.
- At least one destination is required before the group can be saved.
- Each destination in the list must be validated independently.
- Duplicate destination entries within the same group should not be allowed.

### Window placement

Add a placement setting with these options:

- `Upper Left`
- `Upper Right`
- `Lower Left`
- `Lower Right`
- `Last Location`

Behavior:

- Save the selected placement option in JSON settings.
- When closing the app, save the current window height, width, and screen position.
- If placement is `Last Location`, restore the saved position on startup.
- If placement is one of the corner options, place the window in that corner on startup.
- Always restore the saved height and width on startup.
- When restoring size and position, keep the window within the bounds of the current screen.

Startup precedence:

- First use the command-line placement option if provided
- Otherwise use the placement setting from JSON
- Otherwise default to `Upper Left`

### Settings window scope

The Settings window should contain:

- Sort-order options
- Placement options
- An `Options` section
- A checkbox option to enable moving directories together with the subdirectories and files inside them
- A checkbox option to preserve directory structure at the destination (dependent on directory moves being enabled, indented to show dependency)
- A checkbox option to create empty directories at the destination (dependent on preserve directory structure being enabled, further indented to show dependency)
- A checkbox option to enable or disable creation of `.filemove-queued` sidecar files
- A checkbox option for an experimental mode that marks queued source files as hidden
- The Settings window must not be resizable. It maintains the size it is created with.

The Queue Status section should not remain in the Settings window. Queue status is shown in the Queue window.

## Compact UI layout sketch

```text
+--------------------------------------------------+
| Groups                                           |
| ------------------------------------------------ |
| ------------------------------------------------ |
| Queued: 12                    Status: Moving    |
+--------------------------------------------------+
```

Notes:

- Keep controls short and aligned.
- Use small standard Windows control sizes.
- Prefer list and text controls over large panels or cards.
- Keep padding tight so more groups remain visible.
- Assume `Move` for version 1.
- Show the full group name and destination paths in a tooltip when hovering over a row.
- In that tooltip, show the group name first and then list each destination on its own line with a per-destination status icon.
- Put search and sort in secondary windows or menus instead of the main surface.

## Best interface element for reusing groups

The best primary control is a persistent list view of saved groups.

Why:

- It is denser than large buttons or cards.
- It keeps many groups visible at once.
- It makes drag and drop practical because each group row can serve as a drop target.
- It lets the user reselect destinations quickly for repeated tasks.
- It avoids path-length layout problems when each row shows only the group name.
- It works well when a pinned header is always available at the top of the list.
- It supports separate right-click behavior for the fixed top row and the actual group rows.

## Data model

### Group

- `Id`
- `Name`
- `DestinationPaths`
- `CreatedAt`
- `UpdatedAt`
- `LastUsedAt`

### Pending move entry

- `Id`
- `GroupId`
- `SourceFilePath`
- `DestinationDirectories`
- `ActiveLogFilePath`
- `DebugTransferMode`
- `QueuedAt`
- `Status`

## Command-line options

The app should support case-insensitive command-line options.

### Parsing rule

When parsing the command line, recognize an option only when the command letter is immediately preceded by `/` or `-`.

Examples of valid option prefixes:

- `/D`
- `-D`
- `/I`
- `-I`

This rule should be used to improve command-line error detection and to avoid treating ordinary file names or free text as options.

### Supported options

- `D` enables debug mode and requires a mode value
- `I` specifies an input JSON file
- `O` specifies an output log file
- `S` specifies the displayed sort order
- `P` specifies the window placement

Accepted forms should include:

- `/D <value>`
- `/d <value>`
- `-D <value>`
- `-d <value>`
- `/I <path>`
- `/i <path>`
- `-I <path>`
- `-i <path>`
- `/O <path>`
- `/o <path>`
- `-O <path>`
- `-o <path>`
- `/S <value>`
- `/s <value>`
- `-S <value>`
- `-s <value>`
- `/P <value>`
- `/p <value>`
- `-P <value>`
- `-p <value>`

### Command-line parse errors

If any problem is found while parsing the command line:

- Open the debug console window even if debug mode was not explicitly requested
- Display the parsing error clearly
- List all valid command-line options and accepted formats
- Display a message instructing the user to press `Enter`
- Wait for `Enter` to be pressed
- Exit the application after `Enter` is pressed

Problems that should trigger this behavior include:

- Unknown options
- Missing values for `D`, `I`, `O`, or `S`
- Missing values for `P`
- Invalid `D` values
- Invalid `I` file names or extensions
- Invalid `O` file names or extensions
- Invalid `S` values
- Invalid `P` values
- Malformed option syntax
- Malformed startup JSON in the resolved input file

Recommended help text should list:

- `/D <MV|CP>` or `-D <MV|CP>` for debug mode
- `/I <path>` or `-I <path>` for the input JSON file
- `/O <path>` or `-O <path>` for the output log file
- `/S <value>` or `-S <value>` for the sort mode
- `/P <value>` or `-P <value>` for the placement mode

### Debug option

The `D` option enables the debug console and selects the debug transfer mode.

Recommended accepted values:

- `MV` for normal move behavior
- `CP` for debug copy behavior

Behavior:

- The `D` option is case-insensitive.
- `-D MV` or `/D MV` opens the debug console and uses normal move behavior.
- `MV` stands for the Linux `mv` command.
- In `MV` mode, the worker thread removes the source file only after all required destination operations succeed.
- `-D CP` or `/D CP` opens the debug console and changes the worker thread to debug copy behavior.
- `CP` stands for the Linux `cp` command.
- In `CP` mode, the worker thread must not remove the source file after destination operations succeed.
- In `CP` mode, the app must log and display in the debug console that the source file was not removed.
- `CP` mode exists to help test data integrity while keeping the source file in place.
- If the `D` value is invalid, treat it as a command-line parse error.

Examples:

- `FileMove.exe -D MV`
- `FileMove.exe /D CP`
- `FileMove.exe -D MV -I C:\Data\FileMove.json`

### Input file option

The `I` option specifies an alternate JSON file to use at startup.

Behavior:

- The input file must use the `.json` extension or no extension.
- If the `I` value has no extension, append `.json`.
- If the `I` value has an extension other than `.json`, treat it as a startup error.
- If the `I` value does not include a directory, use the default data directory and combine it with the provided file name.
- Example: `-I NewGroup` resolves to `%AppData%\Roaming\FileMove\NewGroup.json`.
- If the `I` value includes a valid directory, use that directory and file name as provided.
- If the JSON file exists, load groups and settings from that file.
- If the JSON file exists but has length `0`, treat it as an empty/default settings file instead of reporting an error.
- When a new or empty JSON file is loaded, default `EnableDirectoryMoves`, `PreserveDirectoryStructure`, `CreateEmptyDirectories`, `EnableSidecarFiles`, and `HideQueuedSourceFiles` to `false`.
- If the JSON file exists but is malformed, treat it as a startup error.
- If the JSON file does not exist, create a new JSON file at the resolved path.
- If `I` is not provided, use the default JSON file at the default location.

Examples:

- `MyApp.exe -I NewGroup`
- `MyApp.exe /I C:\Data\Groups.json`
- `MyApp.exe -i D:\Portable\Config\WorkGroups.json`

### Output file option

The `O` option specifies an alternate log file.

Behavior:

- The output log file must use the `.log` extension or no extension.
- If the `O` value has no extension, append `.log`.
- If the `O` value has an extension other than `.log`, treat it as a startup error.
- If `O` is not provided, derive the log file name from the resolved JSON file name by using the same base name, the `.log` extension, and the same directory as the resolved JSON file.
- If `O` is provided and does not include a directory, use the resolved JSON file directory and the provided base name.
- If `O` is provided and includes a directory, use that full resolved path.
- If `O` is provided, write log output to the resolved log path.
- Log output should append new results to the end of the log file.
- This option allows the log file to be written in a different directory from the JSON file when a directory is included in `O`.
- When opening a `.log` file, if the file size is above 60,000 bytes, delete lines from the top of the file until the file size is below 50,000 bytes.
- Log-file trimming should remove whole lines only and preserve the newest records.

Default naming examples:

- `FileMove.json` -> `FileMove.log`
- `WorkGroups.json` -> `WorkGroups.log`
- `%AppData%\Roaming\FileMove\NewGroup.json` -> `%AppData%\Roaming\FileMove\NewGroup.log`

Examples:

- `MyApp.exe -I NewGroup`
- `MyApp.exe -I NewGroup -O TestRun`
- `MyApp.exe /O C:\Logs\FileMover.log`
- `MyApp.exe /I D:\Portable\Config\WorkGroups.json /O D:\Portable\Logs\WorkGroups.log`

### Sort option

The `S` option specifies the displayed sort order at startup.

Recommended accepted values:

- `MRU` for Most Recently Used
- `LRU` for Least Recently Used
- `AF` for Added First
- `AL` for Added Last
- `AZ` for A thru Z
- `ZA` for Z thru A

The app may also accept the full internal setting names for clarity:

- `MostRecentlyUsed`
- `LeastRecentlyUsed`
- `AddedFirst`
- `AddedLast`
- `AtoZ`
- `ZtoA`

Behavior:

- The `S` option is case-insensitive.
- If `S` is provided, it overrides the saved sort mode for that launch.
- The selected sort should be shown immediately in the sort control when the app opens.
- If the `S` value is invalid, treat it as a command-line parse error.

Examples:

- `MyApp.exe /S MRU`
- `MyApp.exe -s AddedLast`
- `MyApp.exe /I C:\Data\Groups.json /S LRU`
- `MyApp.exe /S AZ`

### Placement option

The `P` option specifies the window placement at startup.

Recommended accepted values:

- `UL` for Upper Left
- `UR` for Upper Right
- `LL` for Lower Left
- `LR` for Lower Right
- `LAST` for Last Location

The app may also accept the full internal setting names for clarity:

- `UpperLeft`
- `UpperRight`
- `LowerLeft`
- `LowerRight`
- `LastLocation`

Behavior:

- The `P` option is case-insensitive.
- If `P` is provided, it overrides the saved placement setting for that launch.
- If the `P` value is invalid, treat it as a command-line parse error.
- The restored window height and width should still come from the saved values in JSON.
- Restored size and position must be kept within the bounds of the current screen.

Examples:

- `MyApp.exe /P UL`
- `MyApp.exe -p LastLocation`
- `MyApp.exe /I C:\Data\Groups.json /P LR`

### Log file format

The log file should contain two kinds of records.

Transfer records:

- One completed file operation per line
- Written in CSV format
- Fields:
  - File Name
  - Source Directory
  - Destination Directory
  - Date and time of the completed operation
  - `Success` or the reason for failure

Canceled transfers should also be logged in the same CSV format:

- Use the file name, source directory, destination directory, and cancellation time
- Put a cancellation result such as `Canceled during shutdown`
- If the worker removed an incomplete destination file during cleanup, that detail may be included in the result text

Error-pause and queue-cancel records should also be written clearly:

- If a transfer error causes the worker to pause, log the error before prompting the user to retry or cancel
- If queued files are cleared because the user cancels during an error pause, log the queued files that were not processed
- If individual queue entries are canceled via the Queue window `Delete` button, log each canceled entry with its source file name and destination path
- If the queue is emptied via the Queue window `Empty` button, log the total number of entries that were removed

Other records:

- Start the line with `---->`
- Follow with descriptive text
- Used for app-level and session-level information

Required `---->` records should include:

- App start time
- JSON file open time
- JSON file close time
- Log file open time
- Log file close time
- Command-line options included at startup
- Active JSON file name
- Active log file name
- Active sort mode
- Active placement mode
- Window size (width x height) at startup
- Window position (Left, Top) at startup
- New JSON file creation from the command line, when applicable
- New JSON file creation from the Active JSON window, when applicable
- Command-line parsing errors, if any

Example transfer record:

```text
"Report.pdf","C:\Users\Brad\Downloads","Z:\Accounting\Invoices","2026-04-25 10:42:18","Success"
```

Example non-transfer records:

```text
----> App started: 2026-04-25 10:41:03
----> LOG file opened: 2026-04-25 10:41:03 (C:\Logs\FileMover.log)
----> JSON file opened: 2026-04-25 10:41:03 (C:\Data\Groups.json)
----> Command line options: /D /I C:\Data\Groups.json /O C:\Logs\FileMover.log
----> JSON file: C:\Data\Groups.json
----> LOG file: C:\Logs\FileMover.log
----> Sort mode: MostRecentlyUsed
----> Placement mode: UpperLeft
----> Window size: 320 x 500
----> Window position: Left 0, Top 0
----> New JSON file created: C:\Data\Groups.json
```

### Debug mode behavior

When debug mode is enabled:

- Open a console window alongside the main app window
- Write transfer status messages to the console
- Write startup and argument parsing information to the console
- Write warnings and error details to the console
- Write the active debug transfer mode, `MV` or `CP`, to the console
- Write JSON file open and close events to the console
- Write log file open and close events to the console
- Write new JSON file creation events to the console
- Write move-thread source and destination file names to the console

The console output should be useful to a programmer during testing and troubleshooting.

Additional debug-mode behavior:

- `MV` mode:
  - Use the normal move behavior
  - Remove the source file only after all required destination operations succeed
- `CP` mode:
  - Copy to all required destinations
  - Do not remove the source file
  - Log that the source file was intentionally not removed because debug copy mode is active
  - Show that same information in the debug console

### Developer-expandable debug output

The debug logging design should make it easy for a programmer to recompile the app and add more diagnostic output.

Recommendation:

- Route debug messages through a central logging helper
- Keep normal user status messages separate from debug-only messages
- Allow additional debug categories to be added in code without changing the main UI

Normal app launches should not open the console window.

## Local storage recommendation

For this version of the app, use a JSON file for local storage.

### Default storage location

Default JSON file under:

`%AppData%\Roaming\FileMove\FileMove.json`

Default log file:

`%AppData%\Roaming\FileMove\FileMove.log`

Default-resolution rules:

- If neither `I` nor `O` is provided, use `FileMove.json` and `FileMove.log` in `%AppData%\Roaming\FileMove\`.
- If `I` is provided without a directory, resolve it inside `%AppData%\Roaming\FileMove\`.
- If `O` is not provided, resolve the log file beside the active JSON file by reusing the JSON base name and replacing the extension with `.log`.

### Why JSON

- Easy to build
- Easy to back up
- Easy to inspect during development
- Good fit for a small utility with a modest number of saved groups
- No database dependency is required
- Works well with user-selected alternate file locations through the command line

### JSON structure recommendation

All JSON keys use **PascalCase**. The app writes PascalCase on save and accepts both PascalCase and camelCase on load for backward compatibility.

**Top-level keys:**

| Key | Type | Description |
|---|---|---|
| `Version` | integer | App data schema version |
| `LastSelectedGroupId` | string | UUID of the last-used group |
| `SortMode` | integer | 0=MostRecentlyUsed, 1=LeastRecentlyUsed, 2=AddedFirst, 3=AddedLast, 4=AtoZ, 5=ZtoA |
| `PlacementMode` | integer | 0=UpperLeft, 1=UpperRight, 2=LowerLeft, 3=LowerRight, 4=LastLocation |
| `WindowWidth` | integer | Saved window width in pixels |
| `WindowHeight` | integer | Saved window height in pixels |
| `WindowLeft` | integer | Saved window X position |
| `WindowTop` | integer | Saved window Y position |
| `EnableDirectoryMoves` | boolean | Allow recursive directory moves (optional, defaults false) |
| `PreserveDirectoryStructure` | boolean | Preserve directory structure at destination (optional, defaults false) |
| `CreateEmptyDirectories` | boolean | Create empty directories at destination (optional, defaults false) |
| `EnableSidecarFiles` | boolean | Create .filemove-queued markers (optional, defaults false) |
| `HideQueuedSourceFiles` | boolean | Mark queued source files as hidden (optional, defaults false) |
| `Groups` | array | Array of group objects |

**Group object keys:**

| Key | Type | Description |
|---|---|---|
| `Id` | string | Unique group identifier |
| `Name` | string | Display name for the group |
| `DestinationPaths` | array of strings | One or more destination folder paths |
| `CreatedAt` | string | ISO 8601 timestamp of group creation |
| `UpdatedAt` | string | ISO 8601 timestamp of last edit |
| `LastUsedAt` | string | ISO 8601 timestamp of last file operation |

Example:

```json
{
  "Version": 1,
  "LastSelectedGroupId": "435af99ae947449f86f2b62a346af51b",
  "SortMode": 4,
  "PlacementMode": 0,
  "EnableDirectoryMoves": true,
  "PreserveDirectoryStructure": true,
  "CreateEmptyDirectories": true,
  "EnableSidecarFiles": false,
  "HideQueuedSourceFiles": false,
  "WindowWidth": 320,
  "WindowHeight": 500,
  "WindowLeft": 0,
  "WindowTop": 0,
  "Groups": [
    {
      "Id": "435af99ae947449f86f2b62a346af51b",
      "Name": "Invoices 2026",
      "DestinationPaths": [
        "Z:\\Accounting\\Invoices",
        "X:\\Archive\\Invoices"
      ],
      "CreatedAt": "2026-04-25T09:00:00",
      "UpdatedAt": "2026-04-25T09:00:00",
      "LastUsedAt": "2026-04-25T09:15:00"
    }
  ]
}
```

Compatibility rule:

- The current JSON format stores only `DestinationPaths`.
- Older JSON files that contain a single `DestinationPath` value should still be accepted during load.
- When an older file is opened, the single legacy `DestinationPath` should be treated as a one-item `DestinationPaths` list.
- When the file is next saved, only `DestinationPaths` should be written.
- The parser accepts both PascalCase (e.g., `Groups`, `Id`, `Name`) and camelCase (e.g., `groups`, `id`, `name`) keys on load.
- The parser accepts both integer and string values for `SortMode` and `PlacementMode` on load.
- The writer always outputs PascalCase keys and integer values for `SortMode`/`PlacementMode`.

### Version 1 storage scope

- Save groups in JSON
- Save last selected group in JSON
- Save the selected sort mode in JSON
- Save the selected placement mode in JSON
- Save window height and width in JSON when closing
- Save window position in JSON when closing
- Write transfer and app-session records to a `.log` file
- Append new records to the end of the log file
- Write transfer records as one-line CSV entries
- Write non-transfer records on lines that begin with `---->`
- Transfer history is not stored in JSON

## Behavior details

### Drag and drop

- User drags one or more files from Explorer onto a group row.
- For each dropped file, verify that the source file is valid.
- Verify that every destination directory in the group is valid.
- Add each validated file as a pending move entry in a queue.
- Continue processing all dropped files until all files are either queued or an error is found.
- If any error is detected while preparing the batch, remove all queued entries created for that batch and display the error.
- If no errors are detected, release the queued entries to be moved by the move worker thread.
- Group rows should highlight on hover to make drop targets obvious.
- Hovering over a group row should also show a tooltip with the group name and full destination paths.
- In that tooltip, each destination path should have its own availability icon so unavailable destinations are visible before the user drops files.
- The header is not a drop target.

### Clipboard paste

- User copies or cuts files in Explorer.
- User right clicks a group in the app.
- User chooses `Use Clipboard`.
- App verifies that the clipboard contains only valid files.
- App checks which destination directories in the group currently exist.
- If one or more destinations are unavailable, warn the user and list the unavailable destination directories.
- If no destinations are available, allow only cancellation of the operation.
- If at least one destination is available, allow the user to continue with only the available destinations.
- For each valid clipboard file, add a pending move entry to the queue.
- Continue processing all clipboard files until all files are either queued or an error is found.
- If any error is detected while preparing the batch, remove all queued entries created for that batch and display the error.
- If no errors are detected, release the queued entries to be moved by the move worker thread.
- After successfully releasing the clipboard batch, clear the clipboard to prevent stale file references on subsequent use.

### Queue and release behavior

- File moves should use a two-stage process:
  - Prepare and queue
  - Release and move
- The prepare stage must validate every source file and every destination directory before any file in the batch is moved.
- Before queueing a drag-and-drop or clipboard batch, test every destination directory for the selected group to see whether it exists.
- If one or more destination directories do not exist, warn the user and list the unavailable destination directories.
- If no destination directories are available, allow only cancellation and do not queue the batch.
- If at least one destination directory is available, allow the user to continue with only the available destinations.
- If the user continues with only the available destinations, log each unavailable destination directory and keep the source files in place after the available destination operations succeed.
- A batch is considered valid only if every file in that batch passes validation.
- If any file in the batch fails validation, the entire batch is canceled before release.
- The app should display the detected error and leave no entries from the failed batch in the queue.
- If directory moves are disabled, dropped or pasted directory paths must be treated as invalid input and the batch must fail before release.
- If directory moves are enabled, directory inputs must be expanded recursively into file moves before queue release.
- When the "Preserve directory structure" option is enabled, preserve the directory name and its relative subdirectory structure under each destination directory.
- When the "Create empty directories" option is enabled, empty subdirectories within the moved directory tree must also be created at each destination. Subdirectories that only contain other empty directories should be skipped — only the leaf-most empty directories need to be created.
- During queue preparation, compare the full source file path to every derived destination file path for that source file.
- If a source file and its only destination file path are the same, show a clear error message and cancel the batch without queueing it.
- If a source file matches one or more destination file paths but other destinations remain, offer `Cancel` or `Continue`.
- If the user chooses `Continue`, ignore only the matching destination file paths, continue queueing the remaining destinations for that source file, and keep the source file in place after the remaining destination operations succeed.
- If the user chooses `Cancel` for that source-versus-destination-path conflict, cancel the batch without queueing it.
- Queue deduplication must be based on the full source file path.
- If an input file's full source path is already queued or currently being processed, skip that source file and continue processing the rest of the input files.
- When a duplicate source file is skipped, show a clear `Already queued` message that includes the source file name.
- The queued count shown in the main status bar and Queue window must be derived from the unfinished destination files represented by all queue entries, not from the number of source-file queue entries.
- The processed count shown in the Queue window must be derived from completed destination-file operations, not from the number of completed source-file queue entries.
- When queue entries are created, record the currently active `.log` file path on each queue entry.
- When queue entries are created, record the active debug transfer mode on each queue entry.
- Queue processing must write results to the `.log` file captured on the queue entry, not to a later log file that may become active after startup changes.
- If the sidecar-file option is enabled, when a source file is added to the queue, create a hidden sidecar marker file by appending `.filemove-queued` to the source file path.
- If the sidecar-file option is disabled, do not create the `.filemove-queued` marker.
- If the experimental hidden-source option is enabled, mark queued source files as hidden when they are added to the queue.
- If a queued source file is removed from the queue because the batch is canceled, the app must remove that source file's `.filemove-queued` marker when that option is enabled.
- If the app hid a source file because of the experimental hidden-source option and the source file remains in place after cancellation or completion, restore the file's visible state.

### Move worker thread

- A separate worker thread should process queued and released move entries.
- The main application thread should remain free to accept the next drag-and-drop or clipboard batch even if previous moves are still running.
- The worker thread should move released files in queue order.
- Transfer results from the worker thread should be written to the queue entry's recorded `.log` file and to the debug console when appropriate.
- Before each destination operation, the worker thread should write a `---->` record that includes the source file name and the destination file name.
- Before each destination operation, the worker thread should also write the source file name and destination file name to the debug console when it is available.
- The worker thread should copy or move the source data safely to all destinations before removing the source file.
- The source file must not be removed until every destination copy for that file has completed successfully.
- If any destination copy fails, the source file must remain in place.
- If one or more destination directories were skipped because they were unavailable at queue time, the source file must remain in place after the available destination operations succeed.
- If one or more destination file paths were ignored because they matched the source file path, the source file must remain in place after the remaining destination operations succeed.
- If the queue entry was created in debug `CP` mode, the source file must remain in place even when all destination copies succeed.
- In debug `CP` mode, the worker thread must log and show in the debug console that the source file was not removed.
- After a queued source file finishes successfully, remove its `.filemove-queued` sidecar marker when that option is enabled.
- Partial results should be logged clearly so failures at individual destinations can be diagnosed.
- The worker thread should report completion or failure for each moved file and each destination.
- For shutdown behavior, the unit of active work should be the current source file across all of its required destinations.
- The worker thread must support a manual pause and resume action from the Queue window.
- During a manual pause, no new destination copy or source-file removal step should begin until the user selects `Resume`.
- While manually paused, the Queue window `Worker State` should show `Manual Pause`.
- If the user chooses to cancel a move in progress during shutdown, the worker thread must stop as soon as it can do so safely.
- If cancellation happens after a destination file has been created but before that destination file is complete, the worker thread must remove the incomplete destination file during cleanup.
- If a canceled transfer had already completed for one destination but not others, the incomplete destination files should be cleaned up and the cancellation should be logged clearly.
- When a move is canceled during shutdown, log the canceled file to the active `.log` file and show the cancellation in the debug console.
- If a transfer error occurs while the worker thread is moving a file, the worker thread must pause automatically.
- Before offering the user a choice to retry or cancel after a transfer error, the app must log the error and remove any partial destination file created by the failed attempt.
- During this error pause, the user may correct the underlying problem and then choose `Retry`.
- If the user chooses `Retry`, the worker thread should retry the file normally.
- If the user chooses `Cancel` during an error pause, clear the remaining queue and log the queued files that were not processed.
- If the queue is cleared because of cancellation, remove the `.filemove-queued` sidecar markers for those source files.
- During an error pause, the Queue window `Worker State` should show a paused-for-error state such as `Paused - Error`.

### Group creation and editing

- Group names must be unique.
- Group names must be no longer than 200 characters.
- Group names must contain at least 3 alphabetic characters.
- The editor should not use a separate destination-exists status section below the destination list.
- Destination existence should be shown directly on each destination row by the row's status icon.
- Each destination path in the list should be validated when saved.
- If a destination does not exist, the app should offer to create it.
- If a destination state is undetermined, allow the group to be saved and check that destination again when the group is used.
- If a network drive is unavailable, keep the group but show that it is offline.

### File conflicts

- If a file already exists in the destination, show the File Conflict dialog with four options:
  - `Replace` — Overwrites the existing destination file with the source file.
  - `Replace All` — Overwrites the current file and all remaining conflicting files for the rest of the current batch (drop or clipboard operation). The effect is scoped to the batch and resets when a new batch starts.
  - `Keep Both` — Renames the new file by appending `(1)`, `(2)`, etc. before the extension. Example: `Report (1).pdf`. If that name also exists, increments until a unique name is found.
  - `Skip` — Skips this file. The source file remains in place. The destination file is untouched.
- Show per-file success and failure after completion.

### Startup state

- If groups exist, auto-select the last used group.
- Restore the user's last selected sort mode.
- Resolve startup placement in this order:
  - Command-line `P` option
  - Placement setting from JSON
  - Default `Upper Left`
- If no groups exist, focus the UI on creating the first group.
- If the app is launched with the `D` option, also open the debug console window.
- If the app is launched with `-D MV` or `/D MV`, use normal move behavior.
- If the app is launched with `-D CP` or `/D CP`, use debug copy behavior and keep the source file.
- If the app is launched with the `I` option, resolve the JSON path using the command-line path rules before opening it.
- If the resolved JSON file does not exist, create it.
- If the app is launched with the `O` option, resolve the log path using the command-line path rules before opening it.
- If `O` is not provided, derive the log file name from the resolved JSON file name by changing the extension to `.log` and keeping the same directory.
- If the app is launched with the `S` option, use that sort mode for the displayed group list.
- If `S` is not provided, use the saved sort mode from JSON.
- If the app is launched with the `P` option, use that placement mode for the window.
- If `P` is not provided, use the saved placement mode from JSON.
- Restore the saved window height and width on startup.
- Keep the restored window fully within the bounds of the current screen.
- If the active JSON file is changed from the Active JSON window, immediately reload groups and settings from that file.
- If the active JSON file is changed from the Active JSON window, immediately switch the active `.log` file to the matching base-name `.log` file in the default data directory.
- If the active JSON file is changed from the Active JSON window successfully, close the Active JSON window automatically.
- If the selected JSON file from the Active JSON window cannot be loaded because it is malformed or otherwise invalid, show an error message and keep the current JSON file and current `.log` file active.
- If command-line parsing fails, or if startup file resolution fails, open the debug console, show the error and valid options when applicable, wait for `Enter`, and then exit.
- If startup JSON loading fails because the resolved JSON file is malformed or cannot be loaded, open the debug console, show the error and valid options, wait for `Enter`, and then exit.

### Shutdown state

- If the user requests shutdown while the move worker thread is idle, close normally.
- If the user requests shutdown while the move worker thread is active or files remain in queue, show a shutdown prompt with these choices:
  - `Cancel Shutdown`
  - `Finish Current File And Exit`
  - `Cancel Active File And Exit Immediately`
- If the user chooses `Cancel shutdown`, return to the app and continue processing normally.
- `Finish Current File And Exit` should be the recommended default choice.
- If the user chooses `Finish Current File And Exit`, allow the current source file to finish across all of its required destinations and then exit without starting the next queued file.
- If the worker is not actively transferring a source file but files remain queued, `Finish Current File And Exit` should log the queued files and exit immediately.
- If the user chooses `Cancel Active File And Exit Immediately`, request cancellation of the active source file and perform cleanup before exit.
- If the active move is canceled during shutdown, remove any incomplete destination file created by that move.
- If the active move is canceled during shutdown, leave the source file in place.
- If the active move is canceled during shutdown, leave already completed destination files in place.
- Log the canceled file to the active `.log` file and show the cancellation in the debug console.
- If the user chooses `Finish Current File And Exit` or `Cancel Active File And Exit Immediately`, also log any files still remaining in the queue before the app exits.
- Queue entries that were not started before shutdown should be logged as remaining in queue, not as successful transfers.
- If shutdown is requested while the worker thread is paused because of a transfer error, log the files remaining in the queue and allow shutdown to continue.
- When the app closes, save the current window height and width to JSON.
- When the app closes, save the current window position to JSON.
- These saved values should be used at the next startup as allowed by the selected placement mode.

## Suggested Windows technology

The selected technology for version 1 is `C++17 with GDI+` and `CMake` for the build system.

Why this is the chosen platform:

- Direct Windows API access with minimal overhead
- Native Windows feel without runtime dependencies
- Full control over window messages, drag-and-drop, and file operations
- Excellent performance for file transfer operations
- CMake provides cross-platform build flexibility (Windows focus for v1)
- Mature ecosystem with available JSON libraries

### Build requirements

- **CMake**: Minimum version 3.5 or greater in `CMakeLists.txt`
- **Compiler**: Visual Studio 2017 or later (MSVC) with C++17 support, or GCC 7+ / Clang 5+
- **Windows SDK**: For GDI+ and Windows API functions
- **JSON library**: Use a lightweight JSON library (e.g., nlohmann/json, jsoncpp) instead of creating a custom module

### Project structure recommendation

```
FileMove/
├── CMakeLists.txt                    # Build configuration (cmake_minimum_required 3.5+)
├── build.ps1                         # Build script with -RC support
├── src/
│   ├── main.cpp                      # Application entry point
│   ├── window/                       # Window classes and message handling
│   │   ├── main_window.h/cpp
│   │   ├── group_list.h/cpp
│   │   └── dialogs/                  # About, Status, Settings, etc.
│   ├── data/                         # JSON storage and file I/O
│   │   ├── json_parser.h/cpp
│   │   └── file_io.h/cpp
│   ├── queue/                        # Queue management and worker thread
│   │   ├── queue_manager.h/cpp
│   │   └── worker_thread.h/cpp
│   ├── drag_drop/                    # Explorer drag-and-drop support
│   │   └── drop_target.h/cpp
│   ├── resources/                    # Embedded resource helpers
│   │   ├── resource_ids.h
│   │   ├── resource_loader.h/cpp
│   └── utils/                        # Helpers for logging, parsing, etc.
├── resources/
│   ├── FileMove.rc.in                # Resource script template
│   └── GenerateRc.cmake              # CMake script for .rc regeneration
├── assets/                           # Source assets (embedded at build time)
│   ├── icons/
│   │   ├── FileMove-icon.ico
│   │   └── FileMove-icon.png
│   └── images/
│       ├── about-image.png
│       ├── green-check.png
│       ├── red-x.png
│       └── orange-question.png
└── specs/
    ├── FileMove-spec-v1.3.1.md
    └── FileMove-mockups-v1.3.1.md
```

### Embedded resources requirement

All image assets must be embedded directly into the executable at build time. The executable must be fully self-contained — no external asset files or folders are required at runtime.

**Requirements:**

- The application icon (`.ico`) must be embedded as a standard Windows `ICON` resource so it displays correctly in Windows Explorer, the taskbar, the title bar, and the Alt-Tab switcher.
- All PNG images (about image, 3-state status icons) must be embedded as `RT_RCDATA` resources and loaded at runtime via `IStream` + `Gdiplus::Image::FromStream()`.
- A CMake `add_custom_command` must generate the `.rc` file from `resources/FileMove.rc.in` with correct paths to the `assets/` directory.
- The `.rc` generation command must depend on all asset files so that changes to any icon or image trigger automatic regeneration.
- A `PRE_BUILD` command on the `FileMove` target must regenerate the `.rc` before every build to ensure asset file changes are always picked up by the resource compiler.
- The build must not rely on a `POST_BUILD` step to copy assets to the output directory.
- The executable must function correctly when placed in any directory without accompanying asset files.
- `build.ps1 -RC` must regenerate the `.rc` file without a full rebuild.

**Embedded resources:**

| Resource | Type | Purpose |
|---|---|---|
| `FileMove-icon.ico` | `ICON` | Application icon (Explorer, taskbar, title bar) |
| `about-image.png` | `RCDATA` | About window image (128x128) |
| `green-check.png` | `RCDATA` | Directory exists status icon |
| `red-x.png` | `RCDATA` | Directory missing status icon |
| `orange-question.png` | `RCDATA` | Directory undetermined status icon |

## Version 1.3.1 feature set

- Create, rename, delete groups
- Save group name + destination list
- Pinned header with `New Group` button and gear icon button at the top of the window
- Left click on header opens new group dialog
- Right click on header opens context menu: Queue Window, Status, Settings, About
- Open an About window from the header right-click menu
- Show a centered 128 x 128 image at the top of the About window
- Show a Build Information section in the About window with Version and Built On date/time
- Show the current run's command-line arguments in the Build Information section of the About window
- Open an Active JSON window from the header right-click menu
- Open a non-modal Queue window from the header right-click menu
- Show the active JSON file name and active `.log` file name at the top of the Active JSON window
- Show JSON files from the default data directory in the Active JSON window and allow switching without restarting
- Add a `Pause / Resume` control in the Queue window
- Show the currently queued destination file paths in the Queue window and update the list as queue contents change
- Make every actual group row a drop target for files from Windows Explorer
- Open a context menu with `Use Clipboard`, `Edit`, and `Delete` by right-clicking a group row
- Validate clipboard contents and destination path before `Use Clipboard` moves files
- Validate source files and all destination directories before any dropped or clipboard batch is released to move
- Optionally allow directory drops and clipboard directory entries to expand into recursive file moves
- Show a File Conflict dialog when a destination file already exists, with `Replace`, `Replace All`, `Keep Both`, and `Skip` options
- Preserve directory structure at destination when the "Preserve directory structure" option is enabled
- Create empty directories at destination when both "Preserve directory structure" and "Create empty directories" options are enabled
- Queue all files in a batch before releasing any file to the move worker thread
- Cancel the entire batch and clear its queued entries if any validation error is found
- Deduplicate queued work by full source path and skip duplicate source files while continuing the rest of the batch
- Show an `Already queued` message with the duplicate source file name when a duplicate source file is skipped
- Use a separate move worker thread so the main app remains free for the next batch
- Allow the move worker thread to be paused and resumed manually
- Allow one or more destinations per group
- Allow destinations to be added to and deleted from the destination list in the group editor
- Show a 3-state destination-status icon on each destination row in the group editor
- Use the provided green check, red X, and orange question icons for the 3 destination-status states
- Optionally create a hidden `.filemove-queued` sidecar marker when a source file is added to the queue
- If enabled, remove the `.filemove-queued` sidecar marker when a queued source file completes or is canceled
- Allow the sidecar marker feature to be turned on or off from Settings
- Allow an experimental hidden-source-file queue marker to be turned on or off from Settings
- Record the active `.log` file on each queue entry when that entry is added to the queue
- Record the active debug transfer mode on each queue entry when that entry is added to the queue
- Copy or move data to all destinations successfully before removing the source file
- If shutdown is requested while a move is active or files remain queued, offer `Cancel Shutdown`, `Finish Current File And Exit`, or `Cancel Active File And Exit Immediately`
- If a transfer error occurs while moving a file, pause the worker, log the error, clean up any partial destination file, and offer Retry or Cancel
- If Cancel is chosen during an error pause, clear the queue and log the queued files
- If shutdown is requested during an error pause, log the remaining queued files and allow shutdown
- On shutdown cancellation of an active move, remove any incomplete destination file
- If shutdown continues while items remain queued, log the remaining queued files before exit
- Open the group editor populated with the selected group's values when `Edit` is chosen
- Confirm with the user before `Delete` removes a group entry
- Provide sort options for Most Recently Used, Least Recently Used, Added First, Added Last, A thru Z, and Z thru A
- Provide placement options for Upper Left, Upper Right, Lower Left, Lower Right, and Last Location
- Place search in a secondary window or dialog opened from the right-click menu
- Move Queue Status from Settings into the Queue window
- Place sort settings in the settings window
- Place placement settings in the settings window
- Show only group names in the list for compact display
- Show full group name and all destination paths in a hover tooltip
- Show a per-destination availability icon inside the group tooltip for each destination path
- Drag and drop files from Explorer onto a group
- Move files into selected group destination
- Assume `Move` only for version 1
- Support case-insensitive `D` command-line option for debug mode with `MV` and `CP` values
- Support case-insensitive `I` command-line option to load or create an alternate JSON file
- Support case-insensitive `O` command-line option to select an alternate log file
- Support case-insensitive `S` command-line option to select the displayed sort order
- Support case-insensitive `P` command-line option to select window placement
- Parse options only when the command letter is prefixed with `/` or `-`
- On command-line parse errors, open the console, show the error and valid options, wait for `Enter`, and then exit
- On startup JSON load errors, open the console, show the malformed-JSON or load error together with the valid options, wait for `Enter`, and then exit
- Show a console window in debug mode with transfer and diagnostic status output
- In debug `CP` mode, log and display that the source file was not removed
- Append log records to the log file
- Write CSV transfer records with file name, source directory, destination directory, completion time, and result
- Write `---->` session records for startup time, command-line options, JSON file name, log file name, active sort mode, active placement mode, and parse errors
- Save and restore window size and position through JSON settings
- Keep restored window placement within the visible screen bounds
- Compact status and error display
- Remember last selected group
- Use `FileGroupMover/Assets/Icons/FileMove-icon.png` as the application icon
- Embed all image assets (icon, about image, 3-state status icons) into the executable at build time
- The executable must be fully self-contained with no external asset files required at runtime
- The application icon must display correctly in Windows Explorer, the taskbar, the title bar, and the Alt-Tab switcher
 
## Design principles
 
- Explorer-first workflow
- Dense layout first
- Minimal clicks for repeated filing tasks
- No wasted space

- Clear visibility of destination paths
- Safe handling of conflicts and unavailable locations
- Keyboard-friendly where possible

## One-sentence product definition

A compact Windows utility called FileMove that stores named destination groups and lets users send files to them directly from Windows Explorer.
