# FileMove v1.3.1 — Building Guide

## Prerequisites

| Requirement | Minimum Version | Notes |
|---|---|---|
| CMake | 3.10 | Cross-platform build system |
| C++ Compiler | MSVC 2017+ (VS 15.9+) | C++17 support required |
| Windows SDK | 10.0.17763.0+ | Win32 API, GDI+ headers |
| Git | Any | For cloning the repository |

### Alternative Compilers

GCC 7+ and Clang 5+ are supported in theory, but the application is primarily developed and tested on Windows with MSVC.

## Quick Build (Windows)

### 1. Clone the Repository

```powershell
git clone <repository-url>
cd FileMove
```

### 2. Configure

```powershell
cmake -B build -A x64
```

This creates a `build/` directory with the CMake configuration. The `-A x64` flag targets 64-bit Windows. For 32-bit, use `-A Win32`.

### 3. Build

```powershell
cmake --build build --config Release
```

The Release executable will be at `build/Release/FileMove.exe`.

For a Debug build:

```powershell
cmake --build build --config Debug
```

### 4. Embedded Resources

All image assets (icons, PNG images) are embedded directly into the executable at build time via a Windows resource script (`.rc`). No external asset files are needed at runtime — the executable is fully self-contained.

**Embedded resources:**
- `FileMove-icon.ico` — Application icon (shows in Explorer, taskbar, title bar)
- `about-image.png` — About window image
- `green-check.png`, `red-x.png`, `orange-question.png` — 3-state directory status icons

The `.rc` file is generated from `resources/FileMove.rc.in` by a CMake custom command that depends on all asset files. A `PRE_BUILD` step regenerates the `.rc` before every build, ensuring asset file changes are always picked up by the resource compiler.

### 5. Build Script

`build.ps1` provides a convenient wrapper around CMake:

```powershell
.\build.ps1                  # Build Release (default)
.\build.ps1 -Config Debug    # Build Debug
.\build.ps1 -Clean           # Remove build/ and rebuild Release
.\build.ps1 -Both            # Build both Release and Debug
.\build.ps1 -Test            # Build and run unit tests
.\build.ps1 -RC              # Regenerate .rc resource script from asset files
```

The `-RC` flag regenerates `build/FileMove.rc` from `resources/FileMove.rc.in` and the current asset files. Use this after modifying any icon or image in `assets/` without doing a full rebuild. The `-RC` step is also run automatically during normal builds when asset files have changed.

## Building Tests

The test harness is built alongside the main executable:

```powershell
cmake --build build --config Release
```

The test executable will be at `build/Release/test_harness.exe`.

### Running Tests

```powershell
cd build/Release
.\test_harness.exe
```

Or via CTest:

```powershell
cd build
ctest -C Release
```

### Test Coverage

| Module | Tests |
|---|---|
| cmdline_parser | 45 |
| file_io | 25 |
| json_parser | 30 |
| queue_manager | 89 |
| settings_persistence | 20 |
| copy_vs_move_mode | 15 |
| preserve_directory_structure | 40 |
| create_empty_directories | 35 |
| find_empty_directories | 15 |
| source_dest_conflict_structure | 8 |
| **Total** | **308** |

## Linked Libraries

| Library | Purpose |
|---|---|
| `gdiplus` | GDI+ image rendering (icons, about image) |
| `uuid` | COM GUID utilities |
| `ole32` | COM initialization, IDataObject for drag-and-drop |
| `oleaut32` | OLE automation support |
| `shell32` | Shell file operations |
| `shlwapi` | Lightweight shell utilities |
| `comctl32` | Common controls |
| `dwmapi` | Desktop Window Manager (DWM composition for window theming) |

## Third-Party Dependencies

| Dependency | Version | Integration |
|---|---|---|
| nlohmann/json | 3.11.3 | Header-only, located in `third-party/nlohmann/json.hpp` |

No external package managers or network downloads are required at build time.

## Project Structure

```
FileMove/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── window/
│   │   ├── main_window.h/cpp
│   │   ├── group_list.h/cpp
│   │   └── dialogs/
│   │       ├── about.h/cpp
│   │       ├── status.h/cpp
│   │       ├── settings.h/cpp
│   │       ├── group_editor.h/cpp
│   │       ├── queue_window.h/cpp
│   │       ├── search.h/cpp
│   │       ├── conflict_dialog.h/cpp
│   │       └── new_file.h/cpp
│   ├── data/
│   │   ├── json_parser.h/cpp
│   │   └── file_io.h/cpp
│   ├── queue/
│   │   ├── queue_manager.h/cpp
│   │   └── worker_thread.h/cpp
│   ├── drag_drop/
│   │   └── drop_target.h/cpp
│   ├── resources/
│   │   ├── resource_ids.h
│   │   └── resource_loader.h/cpp
│   └── utils/
│       ├── cmdline_parser.h/cpp
│       └── logging.h/cpp
├── tests/
│   └── test_harness.cpp
├── resources/
│   ├── FileMove.rc.in                # Resource script template
│   └── GenerateRc.cmake              # CMake script for .rc regeneration
├── third-party/
│   └── nlohmann/
│       └── json.hpp
├── assets/
│   ├── icons/
│   └── images/
└── specs/
```

## Troubleshooting

### Build Fails with Missing Headers

Ensure the Windows SDK is installed. In Visual Studio Installer, verify that the "Windows 10 SDK" workload component is selected.

### Assets Not Found at Runtime

All assets are now embedded in the executable. If images fail to load, verify the `.rc` file was generated correctly during CMake configuration and that the `assets/` directory exists at build time.

### Updated Asset Not Appearing in Built Executable

The `.rc` file only contains paths to asset files, not their content. When an asset file changes, the `.rc` content remains the same, so MSBuild's resource compiler may not detect the change. The `PRE_BUILD` command in `CMakeLists.txt` regenerates the `.rc` before every build to ensure asset changes are always picked up. If you modified an asset and the change doesn't appear:

1. Run `.\build.ps1` (a full build will regenerate the `.rc` and recompile resources)
2. If still not appearing, run `.\build.ps1 -Clean` for a clean rebuild

### Linker Errors for GDI+

Verify `gdiplus` is in `target_link_libraries`. The Windows SDK must include the GDI+ libraries.
