# FileMove v1.2.0 — Testing Guide

## Unit Tests

The test harness (`tests/test_harness.cpp`) provides 189 unit tests across four modules. Tests run as a console application with no external dependencies.

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
- Validates results with `TEST_CHECK()` macros
- Cleans up fixtures in a teardown block

Test output format:
```
[PASS] TestName
[FAIL] TestName — Expected X, got Y
```

Summary:
```
Results: 189 passed, 0 failed, 189 total
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

### Module: json_parser (30 tests)

| Category | Tests |
|---|---|
| `CreateDefaultJson` creates valid default file | 3 |
| Save/load round-trip with all settings and groups | 8 |
| Empty file (0 bytes) loads as default | 3 |
| Malformed JSON returns false | 3 |
| Legacy `DestinationPath` migration | 4 |
| `GenerateGroupId` uniqueness | 4 |
| `GetIsoTimestamp` format validation | 3 |
| Multiple groups save/load | 2 |

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

### Scenario 3: Queue Processing

1. Queue multiple batches to the same group
2. Open Queue window from `New / Tools` > `Queue Window`
3. Verify queue updates as files process
4. Pause processing via Status window
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

1. Open Status window
2. Verify JSON file list is populated
3. Select a different JSON file
4. Verify groups reload from selected file
5. Verify Status window closes automatically
6. Verify active `.log` file switches to matching base name

### Scenario 7: Settings Persistence

1. Open Settings window
2. Change sort mode to `Added Last`
3. Change placement to `Lower Right`
4. Enable directory moves, sidecar files, and hidden source options
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
