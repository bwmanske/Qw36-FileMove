# FileMove v1.3.7 — Testing Guide

## Unit Tests

The test harness (`tests/test_harness.cpp`) provides 470 passing assertions across fifteen modules. Tests run as a console application with no external dependencies.

### Running Tests

```powershell
# Build
cmake --build build --config Release

# Run directly
.\build\Release\test_harness.exe

# Or via CTest
cd build
ctest -C Release
```

### Test Framework

A simple inline test framework is used. Each test:
- Sets up test fixtures (temp files, directories, data structures)
- Executes the function under test
- Validates results with `ASSERT_*` macros
- Cleans up fixtures in a teardown block

Test output format:
```
FileMove v1.3.7 - Unit Tests
==============================
Testing cmdline_parser...
  cmdline_parser tests done.
...
==============================
Results: 470 passed, 0 failed
```

### Module Breakdown

Counts are runtime assertion totals (some assertions run inside loops). Modules are listed in `main()` call order.

| Module | Assertions | Covers |
|---|---|---|
| `cmdline_parser` | 89 | `/D`, `/I`, `/O`, `/S`, `/P` options; sort/placement round-trips; help text |
| `file_io` | 26 | Path utilities, JSON/log path resolution, file/dir existence, enumeration |
| `json_parser` | 112 | Default JSON, save/load round-trip, empty/malformed/legacy, GUID + timestamp |
| `logging_buffer` | 34 | Buffered entries, flush on open/switch/close, discard on switch failure |
| `queue_manager` | 27 | Prepare/release, dedup, cancel, sidecar, directory move on/off |
| `preserve_directory_structure` | 37 | Dest path prefixing, nested structure, multiple sources/dests |
| `create_empty_directories` | 34 | Leaf/nested empty dir detection, mixed content, disabled mode |
| `copy_vs_move_mode` | 11 | CP vs MV entry creation, mid-session mode change |
| `settings_json_roundtrip` | 19 | preserve/createEmpty round-trip, defaults, legacy JSON |
| `find_empty_directories` | 13 | Deep nesting, empty parent/child, multiple empty dirs |
| `source_dest_conflict_structure` | 4 | File at structured dest (skip), no-conflict cases |
| `replace_all_conflict` | 12 | ReplaceAll sticky flag scoped to group, reset on new batch |
| `nested_dest_dir_creation` | 3 | Nested destination directory creation |
| `delete_empty_directory` | 26 | `IsDirectoryEmpty`, JSON round-trip, worker single-level delete (file drops) |
| `delete_empty_directory_structure` | 23 | JSON round-trip + legacy key, worker multi-level structure delete (directory drops) |
| **Total** | **470** | |

## Integration Test Scenarios

These scenarios require manual execution of the built application.

### Scenario 1: Drag-and-Drop Flow

1. Launch `FileMove.exe`
2. Create a group with a known destination folder
3. In Explorer, select 2-3 files
4. Drag files onto the group row
5. Verify files appear in the queue
6. Verify files are moved to the destination
7. Verify source files are removed
8. Check `.log` file for CSV transfer records

### Scenario 2: Clipboard Flow

1. In Explorer, copy or cut 2-3 files
2. Right-click a group row, select `Use Clipboard`
3. Verify files are moved to the destination
4. For `Cut` operations, verify source files are removed
5. For `Copy` operations, verify source files remain
6. Verify clipboard is cleared after successful use
7. Verify multiple consecutive "Use Clipboard" operations work without crash

### Scenario 3: Queue Processing

1. Queue multiple batches to the same group
2. Open Queue window from gear button > `Queue Window`
3. Verify queue updates as files process
4. Pause processing via Queue window
5. Resume processing
6. Verify all files complete

### Scenario 4: Shutdown Behavior

1. Queue a batch of files
2. While processing, close the application
3. Verify shutdown prompt appears with three options
4. Test each option:
   - **Cancel Shutdown**: App continues processing
   - **Finish Current File And Exit**: Current file completes, remaining logged
   - **Cancel Active File And Exit Immediately**: Active file cancelled, cleanup performed

### Scenario 5: CLI Options

| Command | Expected Behavior |
|---|---|
| `FileMove.exe /D MV` | Debug console opens, normal move behavior |
| `FileMove.exe /D CP` | Debug console opens, copy-only behavior |
| `FileMove.exe /I TestGroup` | Loads `TestGroup.json` from data directory |
| `FileMove.exe /S AZ` | Groups sorted A-Z |
| `FileMove.exe /P UR` | Window placed in upper-right corner |
| `FileMove.exe /D XX` | Parse error, help text displayed, exits on Enter |

### Scenario 6: JSON File Switching

1. Open Active JSON window
2. Verify JSON file list is populated
3. Select a different JSON file
4. Verify groups reload from selected file
5. Verify Active JSON window closes automatically
6. Verify active `.log` file switches to matching base name
7. Verify old log file ends with `LoadAppData: attempting to load <path>` followed by `----> LOG file closed: <timestamp>`
8. Verify new log file starts with `----> LOG file opened`, `----> JSON file switched`, then buffered LoadAppData entries
9. Verify a blank line separates old content from new entries when switching to an existing non-empty log file
10. On switch failure: verify old log stays open, failure is logged, and buffer is discarded

### Scenario 7: Settings Persistence

1. Open Settings window
2. Change sort mode to `Added Last`
3. Change placement to `Lower Right`
4. Enable directory moves, delete empty dirs, preserve structure, create empty dirs, sidecar files, and hidden source options
5. Close and restart the application
6. Verify all settings are restored

### Scenario 8: Error Handling

| Condition | Expected Behavior |
|---|---|
| Drop files on group with missing destination | Warning dialog, option to continue with available destinations |
| Drop files where source = destination | Conflict detection, Cancel/Continue dialog |
| Drop duplicate files already in queue | "Already queued" message, duplicate skipped |
| Transfer error during move | Worker pauses, Retry/Cancel dialog |
| Malformed JSON file | Startup error, debug console, exits on Enter |

### Scenario 9: Directory Structure Preservation

1. Enable "Preserve directory structure" in Settings
2. Create a directory with subdirectories and files
3. Drag the directory onto a group
4. Verify destination paths include source directory basename + relative subdirectory structure
5. Verify files land in correct nested destination folders

### Scenario 10: Empty Directory Creation

1. Enable both "Preserve directory structure" and "Create empty directories" in Settings
2. Create a directory tree with some empty subdirectories
3. Drag the directory onto a group
4. Verify empty leaf subdirectories are recreated at each destination
5. Verify non-leaf empty directories (those containing other empty dirs) are not separately created

### Scenario 11: JSON File Switching Log Verification

1. Launch FileMove with an existing JSON file that has prior log entries
2. Note the current log file path and content
3. Open Active JSON window, select a different JSON file
4. Verify old log file's last entries: `LoadAppData: attempting to load <new-path>` then `----> LOG file closed: <timestamp>`
5. Verify new log file's first entries: `----> LOG file opened`, `----> JSON file switched`, then the buffered `LoadAppData` entries
6. Verify blank line separates old content from new entries in the new log file
7. Trigger a switch failure (e.g., select a malformed JSON file)
8. Verify old log remains open, failure is logged, and no buffer entries appear in any log

### Scenario 12: Delete Empty Directory (single level, file drops)

1. Enable "Delete Empty Directory" in Settings (independent; "Move directories" not required)
2. Create a directory containing a single file, and drag that file onto a group
3. Verify the file is moved to the destination and the (now empty) source directory is removed
4. Repeat with a directory that still contains other files — verify the source directory is NOT removed
5. Disable "Delete Empty Directory" and repeat step 2 — verify the emptied source directory is left in place
6. Verify a drive root is never deleted (option is a no-op for files directly under a root)

### Scenario 13: Delete Empty Directory Structure (multi level, directory drops)

1. Enable both "Move directories" and "Delete empty directory Structure" in Settings
2. Create `TV Sources\Show 1\Season 1\file1.txt` and `TV Sources\Show 1\Season 2\file2.txt`
3. Drag the directory `Show 1` onto a group
4. Verify both files are moved to the destination
5. Verify `Season 1`, `Season 2`, and `Show 1` are all removed (empty structure cleaned up)
6. Verify `TV Sources` is NOT removed (never deletes above the dropped directory)
7. Disable "Delete empty directory Structure" and repeat — verify the full source structure is left in place

## Debug Mode Testing

### MV Mode (Normal Move)

```powershell
FileMove.exe /D MV
```

- Console window opens
- Source files removed after all destinations succeed
- Console shows transfer details

### CP Mode (Debug Copy)

```powershell
FileMove.exe /D CP
```

- Console window opens
- Source files **not** removed after destinations succeed
- Console logs "Source file not removed (CP mode)"

## Log File Verification

After any file operation, verify the `.log` file contains:

1. `---->` records for app start, JSON/log file open, command-line options
2. `----> LOG file closed: <timestamp>` record when closing a log file
3. CSV transfer records (status first, all fields quoted, directories end with `\`):
   `"Success","file.mp4","C:\Source\","D:\Dest\","2026-07-23 12:00:00"`
4. Rejected CSV entries for queue rejections (destination empty):
   `"Rejected - Already queued","file.mp4","C:\Source","","2026-07-23 12:00:00"`
5. Cancellation records with `Canceled during shutdown` result
6. Error records with descriptive failure reasons

Default log location: `%AppData%\Roaming\FileMove\FileMove.log`
