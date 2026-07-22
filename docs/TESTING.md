# FileMove v1.3.1 — Testing Guide

## Unit Tests

The test harness (`tests/test_harness.cpp`) provides 320 unit tests across ten modules. Tests run as a console application with no external dependencies.

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
FileMove v1.3.1 - Unit Tests
==============================
Testing cmdline_parser...
  cmdline_parser tests done.
...
==============================
Results: 320 passed, 0 failed
```

### Module: cmdline_parser (45 tests)

| Category | Tests |
|---|---|
| Empty command line | 1 |
| `/D` option (MV, CP, case insensitive, invalid) | 8 |
| `/I` option (no extension, .json, invalid extension) | 6 |
| `/O` option (no extension, .log, invalid extension) | 6 |
| `/S` option (all sort modes, invalid) | 8 |
| `/P` option (all placement modes, invalid) | 8 |
| Multiple options, unknown options, non-prefixed tokens | 4 |
| `SortModeToString`/`FromString` round-trips | 6 |
| `PlacementModeToString`/`FromString` round-trips | 6 |
| `GetCommandLineHelp` non-empty | 1 |

### Module: file_io (25 tests)

| Category | Tests |
|---|---|
| `GetBaseName`, `GetDirectory`, `GetFileName`, `ReplaceExtension` | 8 |
| `ResolveJsonPath` with/without directory and extension | 6 |
| `ResolveLogPath` derived from JSON, explicit path | 4 |
| `FileExists`, `DirectoryExists`, `GetFileSize` | 4 |
| `EnsureDirectoryExists` recursive creation | 2 |
| `EnumerateDirectoryFiles` recursive enumeration | 1 |

### Module: json_parser (29 tests)

| Category | Tests |
|---|---|
| `CreateDefaultJson` creates valid default file | 3 |
| Save/load round-trip with all settings and groups | 8 |
| Empty file (0 bytes) loads as default | 3 |
| Malformed JSON returns false | 3 |
| Legacy `DestinationPath` migration | 4 |
| `GenerateGroupId` uniqueness | 4 |
| `GetIsoTimestamp` ISO 8601 format validation | 3 |
| Multiple groups save/load | 1 |

### Module: queue_manager (89 tests)

| Category | Tests |
|---|---|
| Empty queue state | 3 |
| `PrepareBatch` validation (no files, no destinations, non-existent paths) | 12 |
| Valid batch preparation, release, entry retrieval | 15 |
| Deduplication by source path | 10 |
| `CancelPreparedEntries`, `CancelAllEntries` | 10 |
| Sidecar file creation and cleanup | 12 |
| Multiple files in batch, multiple destinations | 10 |
| Directory move disabled (rejects directories) | 8 |
| Directory move enabled (expands recursively) | 9 |

### Module: preserve_directory_structure (40 tests)

| Category | Tests |
|---|---|
| Single file, structure disabled — dest path unchanged | 3 |
| Single file, structure enabled — no dir root, dest unchanged | 3 |
| Directory source, structure enabled — dest gets srcDirBase prefix | 5 |
| Nested subdirectory, structure enabled — full path preserved | 5 |
| Multiple files from same dir root — all get same prefix | 5 |
| Structure enabled + multiple destinations — each gets prefix | 5 |
| Multiple directory sources — each gets own basename prefix | 5 |
| `FindEmptyDirsDirect` standalone verification | 9 |

### Module: create_empty_directories (35 tests)

| Category | Tests |
|---|---|
| Leaf empty directory detected | 5 |
| Leaf empty directory, structure disabled — not collected | 3 |
| Nested empty directories — only leaf-most recorded | 5 |
| Empty dir alongside files — recorded | 5 |
| Non-empty dir not recorded | 5 |
| CreateEmptyDirectories disabled — no empty dirs collected | 5 |
| Full directory tree with mixed content | 5 |

### Module: copy_vs_move_mode (15 tests)

| Category | Tests |
|---|---|
| CP mode entry creation | 3 |
| MV mode entry creation | 3 |
| Mode change mid-session — entries retain set mode | 9 |

### Module: settings_json_roundtrip (20 tests)

| Category | Tests |
|---|---|
| `preserveDirectoryStructure` true/false round-trip | 4 |
| `createEmptyDirectories` true/false round-trip | 4 |
| Both true round-trip | 4 |
| Both false by default on new JSON | 4 |
| Legacy JSON without fields — defaults to false | 4 |

### Module: find_empty_directories (15 tests)

| Category | Tests |
|---|---|
| Deeply nested empty chain — only leaf-most recorded | 5 |
| Empty dir with empty subdir — only leaf recorded | 5 |
| Multiple separate empty directories | 5 |

### Module: source_dest_conflict_structure (8 tests)

| Category | Tests |
|---|---|
| File already at structured dest path — skipped | 3 |
| File not at structured dest — no conflict, entry created | 5 |

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

### Scenario 7: Settings Persistence

1. Open Settings window
2. Change sort mode to `Added Last`
3. Change placement to `Lower Right`
4. Enable directory moves, preserve structure, create empty dirs, sidecar files, and hidden source options
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
2. CSV transfer records: `"FileName","SourceDir","DestDir","Timestamp","Success"`
3. Cancellation records with `Canceled during shutdown` result
4. Error records with descriptive failure reasons

Default log location: `%AppData%\Roaming\FileMove\FileMove.log`
