#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <gdiplus.h>
#include <string>

#include "utils/cmdline_parser.h"
#include "utils/logging.h"
#include "data/file_io.h"
#include "data/json_parser.h"
#include "queue/queue_manager.h"
#include "queue/worker_thread.h"
#include "window/main_window.h"
#include "window/dialogs/queue_window.h"
#include "window/dialogs/conflict_dialog.h"
#include "GeneratedBuildConfig.h"

using namespace Gdiplus;

ULONG_PTR gdiplusToken;
HINSTANCE gInstance;
std::wstring gCommandLine;
ParsedCommandLine gParsedArgs;
AppData gAppData;
std::wstring gJsonPath;
std::wstring gLogPath;

static void HandleParseError(const std::wstring& errorMessage) {
    OpenDebugConsole();
    DebugConsoleWriteLine(L"");
    DebugConsoleWriteLine(L"Command-line parse error:");
    DebugConsoleWriteLine(errorMessage);
    DebugConsoleWriteLine(L"");
    DebugConsoleWriteLine(GetCommandLineHelp());
    DebugConsoleWriteLine(L"");
    DebugConsoleWriteLine(L"Press Enter to exit...");
    DebugConsoleWriteLine(L"");

    // Wait for Enter key using ReadConsoleW (stdin redirection is unreliable in GUI apps)
    {
        HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
        wchar_t buf[256];
        DWORD read;
        while (true) {
            if (ReadConsoleW(hInput, buf, _countof(buf) - 1, &read, NULL)) {
                if (read > 0) {
                    buf[read] = L'\0';
                    break;
                }
            }
            Sleep(50);
        }
    }

    CloseDebugConsole();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    gInstance = hInstance;

    // Initialize common controls (required for ListView)
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    gCommandLine = GetCommandLineW();

    // Parse command line
    std::wstring parseError;
    if (!ParseCommandLine(gCommandLine, gParsedArgs, parseError)) {
        HandleParseError(parseError);
        return 1;
    }

    // Open debug console if D was specified
    if (gParsedArgs.debugModeSpecified) {
        OpenDebugConsole();
        DebugConsoleWriteLine(L"FileMove v1.3.1 - Debug Mode");
        DebugConsoleWriteLine(std::wstring(L"Built: ") + FILEMOVE_BUILD_DATE_STR);
        DebugConsoleWriteLine(L"Debug transfer mode: " +
            std::wstring(gParsedArgs.debugMode == DebugMode::MV ? L"MV" : L"CP"));
    }

    // Resolve JSON and log paths
    if (gParsedArgs.inputFileSpecified) {
        gJsonPath = ResolveJsonPath(gParsedArgs.inputFile);
    } else {
        gJsonPath = GetDefaultJsonPath();
    }

    gLogPath = ResolveLogPath(gJsonPath, gParsedArgs.outputFile);

    // Open log file
    OpenLogFile(gLogPath);
    LogInfo(L"App started: " + GetTimestamp());
    LogInfo(L"FileMove v1.3.1");
    LogInfo(std::wstring(L"Built: ") + FILEMOVE_BUILD_DATE_STR);
    LogInfo(L"LOG file opened: " + GetTimestamp() + L" (" + gLogPath + L")");
    LogInfo(L"Command line options: " + gCommandLine);

    if (gParsedArgs.debugModeSpecified) {
        DebugConsoleWriteLine(L"JSON file: " + gJsonPath);
        DebugConsoleWriteLine(L"LOG file: " + gLogPath);
    }

    // Load or create JSON file
    if (!FileExists(gJsonPath)) {
        if (!CreateDefaultJson(gJsonPath)) {
            std::wstring err = L"Failed to create JSON file: " + gJsonPath;
            if (gParsedArgs.debugModeSpecified) {
                HandleParseError(err);
            } else {
                OpenDebugConsole();
                HandleParseError(err);
            }
            return 1;
        }
        LogInfo(L"New JSON file created: " + gJsonPath);
        if (gParsedArgs.debugModeSpecified) {
            DebugConsoleWriteLine(L"New JSON file created: " + gJsonPath);
        }
    }

    DebugConsoleWriteLine(L"About to call LoadAppData...");
    if (!LoadAppData(gJsonPath, gAppData)) {
        std::wstring err = L"Failed to load JSON file (malformed): " + gJsonPath;
        if (gParsedArgs.debugModeSpecified) {
            HandleParseError(err);
        } else {
            OpenDebugConsole();
            HandleParseError(err);
        }
        CloseLogFile();
        return 1;
    }

    DebugConsoleWriteLine(L"LoadAppData succeeded, groups loaded: " + std::to_wstring(gAppData.groups.size()));
    LogInfo(L"JSON file opened: " + GetTimestamp() + L" (" + gJsonPath + L")");

    // Apply command-line overrides
    if (gParsedArgs.sortModeSpecified) {
        gAppData.settings.sortMode = SortModeToString(gParsedArgs.sortMode);
    }
    if (gParsedArgs.placementModeSpecified) {
        gAppData.settings.placementMode = PlacementModeToString(gParsedArgs.placementMode);
    }

    LogInfo(L"Sort mode: " + std::wstring(gAppData.settings.sortMode.begin(), gAppData.settings.sortMode.end()));
    LogInfo(L"Placement mode: " + std::wstring(gAppData.settings.placementMode.begin(), gAppData.settings.placementMode.end()));
    LogInfo(L"Window size: " + std::to_wstring(gAppData.settings.windowWidth) + L" x " + std::to_wstring(gAppData.settings.windowHeight));
    LogInfo(L"Window position: Left " + std::to_wstring(gAppData.settings.windowLeft) + L", Top " + std::to_wstring(gAppData.settings.windowTop));

    // Initialize queue manager with settings
    InitQueueManager();
    gQueueManager.SetEnableSidecarFiles(gAppData.settings.enableSidecarFiles);
    gQueueManager.SetHideQueuedSourceFiles(gAppData.settings.hideQueuedSourceFiles);
    gQueueManager.SetEnableDirectoryMoves(gAppData.settings.enableDirectoryMoves);
    gQueueManager.SetPreserveDirectoryStructure(gAppData.settings.preserveDirectoryStructure);
    gQueueManager.SetCreateEmptyDirectories(gAppData.settings.createEmptyDirectories);

    if (gParsedArgs.debugModeSpecified) {
        gQueueManager.SetDebugMode(gParsedArgs.debugMode == DebugMode::CP
            ? DebugTransferMode::CP : DebugTransferMode::MV);
    }

    // Wire queue change callback to refresh queue window (post to UI thread)
    gQueueManager.SetChangedCallback([]() {
        HWND hwnd = gQueueWindow.GetHWND();
        if (hwnd) {
            PostMessageW(hwnd, WM_QUEUE_REFRESH, 0, 0);
        }
    });

    // Wire worker state callback to update main window status bar
    gWorkerThread.SetStateChangedCallback([](WorkerState state, const std::string& file,
                                               const std::string& dest, int queued, int processed) {
        (void)file;
        (void)dest;
        (void)processed;
        HWND hwnd = gMainWindow.GetHWND();
        if (hwnd) {
            PostMessageW(hwnd, WM_UPDATE_STATUSBAR, (WPARAM)queued, (LPARAM)state);
        }
    });

    // Wire conflict resolution callback
    gWorkerThread.SetConflictCallback([](const std::string& sourceFile, const std::string& destFile) -> ConflictResolution {
        std::wstring fileName;
        size_t lastSlash = sourceFile.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            fileName = std::wstring(sourceFile.begin() + lastSlash + 1, sourceFile.end());
        } else {
            fileName = std::wstring(sourceFile.begin(), sourceFile.end());
        }
        std::wstring destPath = std::wstring(destFile.begin(), destFile.end());

        HWND hwnd = gMainWindow.GetHWND();
        ConflictDialog dlg;
        return dlg.Show(hwnd, fileName, destPath);
    });

    // Start worker thread
    gWorkerThread.Start();

    // Initialize COM (required for drag-and-drop)
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
        MessageBox(NULL, L"GDI+ initialization failed.", L"FileMove", MB_ICONERROR);
        CloseLogFile();
        return 1;
    }

    // Create main window
    DebugConsoleWriteLine(L"About to create main window...");
    LogInfo(L"About to create main window");
    HWND hWnd = gMainWindow.Create(hInstance, nCmdShow);
    if (hWnd) {
        DebugConsoleWriteLine(L"Main window created successfully, HWND=" + std::to_wstring(reinterpret_cast<ULONG_PTR>(hWnd)));
        LogInfo(L"Main window created successfully");
    } else {
        DebugConsoleWriteLine(L"ERROR: Main window creation returned NULL!");
        LogInfo(L"ERROR: Main window creation returned NULL!");
    }
    gMainWindow.SetJsonBaseName(GetBaseName(gJsonPath));

    // Resolve sort mode: CLI override > JSON settings > default MRU
    SortMode activeSortMode = SortModeFromString(gAppData.settings.sortMode);
    if (gParsedArgs.sortModeSpecified) {
        activeSortMode = gParsedArgs.sortMode;
    }
    gMainWindow.UpdateGroups(gAppData.groups, activeSortMode);
    DebugConsoleWriteLine(L"UpdateGroups called with " + std::to_wstring(gAppData.groups.size()) + L" groups");

    // Apply window placement
    PlacementMode placement = PlacementModeFromString(gAppData.settings.placementMode);
    if (gParsedArgs.placementModeSpecified) {
        placement = gParsedArgs.placementMode;
    }

    gMainWindow.ApplyPlacement(placement,
        gAppData.settings.windowWidth,
        gAppData.settings.windowHeight,
        gAppData.settings.windowLeft,
        gAppData.settings.windowTop);
    DebugConsoleWriteLine(L"ApplyPlacement done, about to enter message loop");
    DebugConsoleWriteLine(L"Is window visible? " + std::wstring(IsWindowVisible(hWnd) ? L"YES" : L"NO"));

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Stop worker thread before cleanup
    gWorkerThread.Stop();
    gWorkerThread.WaitForCompletion();

    // Save window state on exit
    RECT rect;
    if (GetWindowRect(gMainWindow.GetHWND(), &rect)) {
        gAppData.settings.windowWidth = rect.right - rect.left;
        gAppData.settings.windowHeight = rect.bottom - rect.top;
        gAppData.settings.windowLeft = rect.left;
        gAppData.settings.windowTop = rect.top;
        SaveAppData(gJsonPath, gAppData);
    }

    LogInfo(L"JSON file closed: " + GetTimestamp() + L" (" + gJsonPath + L")");
    CloseLogFile();
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();

    return static_cast<int>(msg.wParam);
}
