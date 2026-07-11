# AGENTS.md

## Core Source of Truth
- Implementation must strictly follow `specs/FileMove-spec-v1.3.0.md`.
- User interface design must strictly follow `specs/FileMove-mockups-v1.3.0`

## Key Constraints
- **Tech Stack**: Win32, C++17, CMake (supports Windows and Linux).
- **Version Focus**: Strictly version 1.3.0.
- **Workflow**: Explorer-first; the app is a compact destination chooser for Windows Explorer users.
- **CLI Parsing**: Implement case-insensitive options (`/D`, `/I`, `/O`, `/S`, `/P`) exactly as specified. Options MUST be preceded by `/` or `-`.

## UI/UX Conventions
- **Style**: Extremely dense and compact.
- **Windows**: Most secondary windows (Settings, About, Status, Group Editor) are modal. The Queue window is modeless.
- **Iconography**: 
  - Use 3-state images for destination existence in the group editor: `✓` (exists), `X` (missing), `?` (undetermined).
  - Use "about-image.png" for the About window.
  - Use "FileMove-icon.ico" as the application icon.
- **Automatic Behavior**: 
  - The Status window closes automatically after a successful JSON file operation (open or new).
  - A shutdown prompt must appear if the app is closed while work is in progress/queued.

## Implementation Details
- **Safety**: Ensure destination operations succeed before removing any source file (unless in debug `CP` mode).
- **Sidecars**: Use `.filemove-queued` as a sidecar marker for files currently in the queue if the option is enabled.
- **Logging**: Follow specific CSV and `---->` record formats defined in the spec.
