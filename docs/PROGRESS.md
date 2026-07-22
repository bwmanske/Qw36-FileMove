# FileMove v1.2.0 Development Progress

## Implementation Plan

### Phase 1: Project Scaffolding & Build System
1. Create CMakeLists.txt (CMake 3.5+, C++17, link GDI+, nlohmann/json via FetchContent)
2. Create src/main.cpp — WinMain entry point, GDI+ init, message loop
3. Create directory structure: src/window/, src/window/dialogs/, src/data/, src/queue/, src/drag_drop/, src/utils/, third-party/nlohmann/
4. Copy json.hpp into third-party/nlohmann/
5. Copy resources from resources/ into assets/ (icons, images)
6. Verify clean build

### Phase 2: Core Infrastructure
7. src/utils/cmdline_parser.h/cpp — Parse /D, /I, /O, /S, /P options (case-insensitive, / or - prefix), validate values, error handling with console output
8. src/utils/logging.h/cpp — Debug console (AllocConsole), log file writer with CSV + ----> format, log file trimming at 60KB
9. src/data/json_parser.h/cpp — JSON read/write for data model (groups, settings, window state), legacy DestinationPath migration, nlohmann/json integration
10. src/data/file_io.h/cpp — Default data directory resolution (%AppData%\Roaming\FileMove\), file path utilities

### Phase 3a: Data Model & Queue Manager
11. Define data structures: Group, PendingMoveEntry, AppSettings in headers
12. src/queue/queue_manager.h/cpp — Queue with two-stage prepare/release, deduplication by source path, batch validation, sidecar file creation/removal, hidden source file toggle

### Phase 3b: Worker Thread
13. src/queue/worker_thread.h/cpp — Background thread, pause/resume, error pause with retry/cancel, shutdown handling (finish current / cancel immediately), safe copy-then-delete across multiple destinations, conflict resolution (replace/rename/skip)

### Phase 4: Main Window
14. src/window/main_window.h/cpp — Main window with pinned New/Tools header, group list, status bar, title with JSON base name, window placement restore, shutdown prompt
15. src/window/group_list.h/cpp — Custom list with per-row hover tooltip (group name + destinations with 3-state icons), row highlighting, right-click context menu, sort by mode

### Phase 5: Dialogs
16. src/window/dialogs/group_editor.h/cpp — Add/Modify group window with name field, destination list with 3-state icons, Add/Delete buttons, validation
17. src/window/dialogs/about.h/cpp — About window with 128x128 image, version, build date, command line
18. src/window/dialogs/status.h/cpp — Active JSON window with Active Files, JSON file list, New/Open Selected, auto-close on success
19. src/window/dialogs/settings.h/cpp — Settings window with sort radios, placement radios, startup preview, Options checkboxes
20. src/window/dialogs/queue_window.h/cpp — Modeless Queue window with Queue Status section, Pause/Resume button, and live-updating destination list
21. src/window/dialogs/search.h/cpp — Search dialog (from header menu)
22. src/window/dialogs/shutdown.h/cpp — Shutdown prompt with 3 choices
23. src/window/dialogs/new_file.h/cpp — New JSON file dialog (base name only)

### Phase 6: Explorer Integration
24. src/drag_drop/drop_target.h/cpp — IDropTarget implementation, per-row drop targeting, file validation, batch queuing
25. Clipboard integration in group list context menu — Use Clipboard with validation, destination check, batch queuing

### Phase 7: Polish & Integration
26. Application icon integration (.ico resource)
27. 3-state icon rendering (green-check, red-x, orange-question) via GDI+ in tooltips and destination lists
28. Directory move expansion (recursive file enumeration when option enabled)
29. Source-vs-destination path conflict detection
30. Integration testing: drag-drop flow, clipboard flow, queue processing, shutdown behavior, CLI options, JSON file switching
31. tests/test_harness.cpp — Unit tests for cmdline_parser, json_parser, file_io, queue_manager

### Phase 8: Documentation
31. Populate docs/BUILDING.md with CMake build instructions
32. Populate docs/TESTING.md with test scenarios
33. Populate docs/USER_GUIDE.md with usage instructions
34. Update docs/PROGRESS.md as implementation progresses

### Phase 9: Embedded Resources
35. Create resources/FileMove.rc.in — Resource script template with CMake macro for asset paths
36. Create src/resources/resource_ids.h — Shared resource ID definitions
37. Create src/resources/resource_loader.h/cpp — LoadEmbeddedIcon() and LoadEmbeddedPng() helpers
38. Update CMakeLists.txt — configure_file for .rc generation, remove POST_BUILD asset copy
39. Update main_window.cpp — Load icon from embedded ICON resource
40. Update about.cpp — Load about-image from embedded RCDATA resource
41. Update group_editor.cpp — Load 3-state icons from embedded RCDATA resources
42. Remove iconDir parameters from dialog APIs no longer needed
43. Update spec to include embedded resources requirement

---

## Key Technical Decisions

| Decision | Choice |
|---|---|
| JSON library | nlohmann/json (header-only, FetchContent) |
| UI framework | Win32 API + GDI+ (no MFC/ATL) |
| Threading | std::thread for worker, Win32 events/mutexes for sync |
| Icons | Embedded as Windows resources, loaded via IStream + GDI+ Image::FromStream |
| Drop target | Per-row IDropTarget with hit-testing |

## Status

- [x] Phase 1: Project Scaffolding & Build System
- [x] Phase 2: Core Infrastructure
- [x] Phase 3a: Data Model & Queue Manager
- [x] Phase 3b: Worker Thread
- [x] Phase 4: Main Window
- [x] Phase 5: Dialogs
- [x] Phase 6: Explorer Integration
- [x] Phase 7: Polish & Integration (including unit tests: 189 tests, 0 failures)
- [x] Phase 8: Documentation (BUILDING.md, TESTING.md, USER_GUIDE.md populated)
- [x] Phase 9: Embedded Resources (all assets embedded in .exe, icon visible in Explorer)
