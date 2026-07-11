#include "worker_thread.h"
#include "utils/logging.h"
#include "data/file_io.h"
#include <thread>
#include <chrono>
#include <algorithm>
#include <windows.h>
#include <shlwapi.h>

WorkerThread gWorkerThread;

static std::wstring StringToWString(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

static std::string WStringToString(const std::wstring& s) {
    std::string result;
    result.reserve(s.size());
    for (wchar_t wc : s) {
        result += static_cast<char>(wc);
    }
    return result;
}

WorkerThread::WorkerThread()
    : mRunning(false)
    , mPaused(false)
    , mStopping(false)
    , mShutdownAction(ShutdownAction::None)
    , mProcessedCount(0)
    , mCurrentState(WorkerState::Idle)
{
}

WorkerThread::~WorkerThread() {
    Stop();
}

void WorkerThread::Start() {
    if (mRunning) return;

    mRunning = true;
    mPaused = false;
    mStopping = false;
    mShutdownAction = ShutdownAction::None;
    mProcessedCount = 0;
    mCurrentState = WorkerState::Idle;

    mWorkerThread = std::thread(&WorkerThread::WorkerLoop, this);
}

void WorkerThread::Stop() {
    mStopping = true;
    mCondition.notify_all();

    if (mWorkerThread.joinable()) {
        mWorkerThread.join();
    }

    mRunning = false;
    mCurrentState = WorkerState::Idle;
}

void WorkerThread::WaitForCompletion() {
    if (mWorkerThread.joinable()) {
        mWorkerThread.join();
    }
}

void WorkerThread::Pause() {
    mPaused = true;
    mCondition.notify_all();
    UpdateState(WorkerState::ManualPause);
}

void WorkerThread::Resume() {
    mPaused = false;
    mCondition.notify_all();
    UpdateState(WorkerState::Idle);
}

void WorkerThread::Notify() {
    mCondition.notify_all();
}

void WorkerThread::RequestShutdown(ShutdownAction action) {
    mShutdownAction = action;
    mStopping = true;
    mCondition.notify_all();
}

void WorkerThread::SetConflictCallback(ConflictCallback callback) {
    mConflictCallback = callback;
}

void WorkerThread::SetErrorPauseCallback(ErrorPauseCallback callback) {
    mErrorPauseCallback = callback;
}

void WorkerThread::SetStateChangedCallback(WorkerStateChangedCallback callback) {
    mStateChangedCallback = callback;
}

WorkerState WorkerThread::GetState() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentState;
}

ShutdownAction WorkerThread::GetShutdownAction() const {
    return mShutdownAction.load();
}

int WorkerThread::GetProcessedCount() const {
    return mProcessedCount.load();
}

bool WorkerThread::IsBusy() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentState == WorkerState::Moving;
}

std::string WorkerThread::GetCurrentFile() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentFile;
}

std::string WorkerThread::GetCurrentDest() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCurrentDest;
}

std::string WorkerThread::GetLastError() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mLastError;
}

void WorkerThread::WorkerLoop() {
    while (mRunning.load() && !mStopping.load()) {
        // Check for pause
        if (mPaused.load()) {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !mPaused.load() || mStopping.load();
            });
            continue;
        }

        // Get next entry from queue
        PendingMoveEntry entry;
        if (!gQueueManager.GetNextEntry(entry)) {
            // No more entries, go idle
            UpdateState(WorkerState::Idle);

            // Check if we should stop
            if (mStopping.load()) {
                break;
            }
            // Idle, wait for new entries
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait_for(lock, std::chrono::milliseconds(500), [this] {
                return mStopping.load() || mPaused.load();
            });
            continue;
        }

        // Process the entry
        ProcessEntry(entry);

        // Check shutdown action after completing entry
        if (mShutdownAction.load() == ShutdownAction::FinishCurrentAndExit) {
            // Log remaining queued files
            auto remaining = gQueueManager.GetQueuedEntries();
            for (const auto& r : remaining) {
                std::wstring msg = L"Remaining in queue at shutdown: " + StringToWString(r.sourceFilePath);
                LogInfo(msg);
                DebugConsoleWriteLine(msg);
            }
            break;
        }

        if (mShutdownAction.load() == ShutdownAction::CancelAndExit) {
            break;
        }
    }

    UpdateState(WorkerState::Idle);
}

static void CreateParentDirectories(const std::wstring& filePath) {
    size_t pos = filePath.find_last_of(L"\\/");
    while (pos != std::wstring::npos) {
        std::wstring dir = filePath.substr(0, pos);
        if (!DirectoryExists(dir)) {
            CreateDirectoryW(dir.c_str(), NULL);
        }
        pos = dir.find_last_of(L"\\/");
    }
}

void WorkerThread::ProcessEntry(const PendingMoveEntry& entry) {
    mCurrentFile = entry.sourceFilePath;
    UpdateState(WorkerState::Moving, entry.sourceFilePath);

    std::string sourceFileName;
    size_t lastSlash = entry.sourceFilePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        sourceFileName = entry.sourceFilePath.substr(lastSlash + 1);
    } else {
        sourceFileName = entry.sourceFilePath;
    }

    std::string sourceDir;
    if (lastSlash != std::string::npos) {
        sourceDir = entry.sourceFilePath.substr(0, lastSlash);
    }

    bool allDestinationsSucceeded = true;
    bool hadUnavailableDestinations = false;
    bool hadSkippedDestinations = false;

    for (const auto& destDir : entry.destinationDirectories) {
        // Check pause before each destination
        if (ShouldPause()) {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !mPaused.load() || mStopping.load();
            });
            if (mStopping.load()) return;
        }

        // Check shutdown
        if (mShutdownAction.load() == ShutdownAction::CancelAndExit) {
            // Cancel immediately
            std::wstring msg = L"Transfer canceled during shutdown: " + StringToWString(sourceFileName);
            LogInfo(msg);
            DebugConsoleWriteLine(msg);
            gQueueManager.MarkCanceled(entry.id);
            return;
        }

        mCurrentDest = destDir;
        UpdateState(WorkerState::Moving, entry.sourceFilePath, destDir);

        // Build destination file path
        std::string destFilePath = destDir;
        if (!destFilePath.empty() && destFilePath.back() != '/' && destFilePath.back() != '\\') {
            destFilePath += '\\';
        }
        destFilePath += sourceFileName;

        // Log before operation
        std::wstring logMsg = L"Processing: " + StringToWString(sourceFileName) +
                             L" -> " + StringToWString(destFilePath);
        LogInfo(logMsg);
        DebugConsoleWriteLine(logMsg);

        // Auto-create intermediate directories
        std::wstring wideDestPath = StringToWString(destFilePath);
        CreateParentDirectories(wideDestPath);

        // Check for file conflict
        if (FileExists(wideDestPath)) {
            ConflictResolution resolution = HandleConflict(entry.sourceFilePath, destFilePath);

            if (resolution == ConflictResolution::Skip) {
                // Skip this destination
                hadSkippedDestinations = true;
                mProcessedCount++;
                continue;
            } else if (resolution == ConflictResolution::KeepBoth) {
                // Generate unique name
                destFilePath = GenerateUniqueDestName(destFilePath);
                wideDestPath = StringToWString(destFilePath);
            }
            // Replace: proceed with original path
        }

        // Copy to destination with retry support
        bool destSuccess = false;
        while (!destSuccess) {
            std::string error;
            if (!CopyToDestination(entry.sourceFilePath, destFilePath, error)) {
                // Copy failed
                allDestinationsSucceeded = false;
                mLastError = error;

                // Log error
                std::wstring errMsg = L"Error copying " + StringToWString(sourceFileName) +
                                     L" to " + StringToWString(destFilePath) + L": " + StringToWString(error);
                LogInfo(errMsg);
                DebugConsoleWriteLine(errMsg);

                // Remove partial destination file
                if (FileExists(wideDestPath)) {
                    DeleteFileW(wideDestPath.c_str());
                }

                // Error pause
                UpdateState(WorkerState::PausedError, entry.sourceFilePath, destDir);

                if (mErrorPauseCallback) {
                    bool retry = mErrorPauseCallback(error);
                    if (retry) {
                        // User chose retry, loop will attempt again
                        UpdateState(WorkerState::Moving, entry.sourceFilePath, destDir);
                        continue;
                    } else {
                        // User chose cancel, clear remaining queue
                        gQueueManager.CancelAllEntries();
                        gQueueManager.MarkFailed(entry.id, error);
                        return;
                    }
                } else {
                    // No callback, mark as failed
                    gQueueManager.MarkFailed(entry.id, error);
                    LogTransferResult(sourceFileName, sourceDir, destDir, "Failed - " + error);
                    return;
                }
            } else {
                // Success
                destSuccess = true;
                mProcessedCount++;
                LogTransferResult(sourceFileName, sourceDir, destDir, "Success");
            }
        }
    }

    // Create empty directories for this source (if any)
    if (!entry.emptySourceDirectories.empty()) {
        for (const auto& emptyRelDir : entry.emptySourceDirectories) {
            for (const auto& destDir : entry.destinationDirectories) {
                std::string emptyDestPath = destDir;
                if (!emptyDestPath.empty() && emptyDestPath.back() != '/' && emptyDestPath.back() != '\\') {
                    emptyDestPath += '\\';
                }
                emptyDestPath += emptyRelDir;
                std::wstring wideEmptyPath = StringToWString(emptyDestPath);
                if (!DirectoryExists(wideEmptyPath)) {
                    CreateDirectoryW(wideEmptyPath.c_str(), NULL);
                    std::wstring msg = L"Created empty directory: " + wideEmptyPath;
                    LogInfo(msg);
                    DebugConsoleWriteLine(msg);
                }
            }
        }
    }

    // All destinations processed
    if (allDestinationsSucceeded && !hadSkippedDestinations && !hadUnavailableDestinations) {
        // Remove source file (unless in CP mode)
        if (entry.debugTransferMode == DebugTransferMode::CP) {
            // Debug copy mode - do not remove source
            std::wstring cpMsg = L"Debug CP mode: source file not removed: " +
                                StringToWString(entry.sourceFilePath);
            LogInfo(cpMsg);
            DebugConsoleWriteLine(cpMsg);
        } else {
            // Normal move mode - remove source
            if (!RemoveSourceFile(entry.sourceFilePath)) {
                std::wstring removeErr = L"Failed to remove source file: " +
                                        StringToWString(entry.sourceFilePath);
                LogInfo(removeErr);
                DebugConsoleWriteLine(removeErr);
            }
        }
    } else {
        // Some destinations failed or were skipped - keep source file
        std::wstring keepMsg = L"Source file kept in place (not all destinations succeeded): " +
                              StringToWString(entry.sourceFilePath);
        LogInfo(keepMsg);
        DebugConsoleWriteLine(keepMsg);
    }

    // Mark as completed
    gQueueManager.MarkCompleted(entry.id);
}

bool WorkerThread::CopyToDestination(const std::string& sourceFile,
                                    const std::string& destPath,
                                    std::string& error)
{
    std::wstring wideSource = StringToWString(sourceFile);
    std::wstring wideDest = StringToWString(destPath);

    if (!FileExists(wideSource)) {
        error = "Source file not found";
        return false;
    }

    // Use Windows CopyFileW for reliability
    if (CopyFileW(wideSource.c_str(), wideDest.c_str(), FALSE) == 0) {
        DWORD lastError = ::GetLastError();
        switch (lastError) {
            case ERROR_ACCESS_DENIED:
                error = "Access denied to destination";
                break;
            case ERROR_DISK_FULL:
                error = "Disk full";
                break;
            case ERROR_SHARING_VIOLATION:
                error = "File sharing violation";
                break;
            default:
                error = "CopyFile failed (error " + std::to_string(lastError) + ")";
                break;
        }
        return false;
    }

    return true;
}

bool WorkerThread::RemoveSourceFile(const std::string& sourceFile) {
    std::wstring widePath = StringToWString(sourceFile);
    return DeleteFileW(widePath.c_str()) != 0;
}

ConflictResolution WorkerThread::HandleConflict(const std::string& sourceFile,
                                                const std::string& destFile) {
    if (mConflictCallback) {
        return mConflictCallback(sourceFile, destFile);
    }
    // Default to Replace if no callback
    return ConflictResolution::Replace;
}

std::string WorkerThread::GenerateUniqueDestName(const std::string& destPath) {
    size_t dotPos = destPath.find_last_of('.');
    size_t slashPos = destPath.find_last_of("/\\");

    std::string dir, name, ext;
    if (slashPos != std::string::npos) {
        dir = destPath.substr(0, slashPos + 1);
        name = destPath.substr(slashPos + 1);
    } else {
        name = destPath;
    }

    if (dotPos != std::string::npos && dotPos > slashPos) {
        ext = name.substr(name.find_last_of('.'));
        name = name.substr(0, name.find_last_of('.'));
    }

    int counter = 1;
    std::string candidate;
    do {
        candidate = dir + name + " (" + std::to_string(counter) + ")" + ext;
        counter++;
    } while (FileExists(StringToWString(candidate)) && counter < 1000);

    return candidate;
}

bool WorkerThread::ShouldPause() {
    return mPaused.load();
}

bool WorkerThread::ShouldStop() {
    return mStopping.load();
}

void WorkerThread::UpdateState(WorkerState state,
                               const std::string& currentFile,
                               const std::string& currentDest) {
    std::string fileCopy;
    std::string destCopy;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCurrentState = state;
        if (state == WorkerState::Idle) {
            mCurrentFile.clear();
            mCurrentDest.clear();
        } else {
            if (!currentFile.empty()) mCurrentFile = currentFile;
            if (!currentDest.empty()) mCurrentDest = currentDest;
        }
        fileCopy = mCurrentFile;
        destCopy = mCurrentDest;
    }
    // Notify outside the lock to avoid deadlock with UI thread
    if (mStateChangedCallback) {
        int queued = gQueueManager.GetQueuedDestinationCount();
        int processed = mProcessedCount.load();
        mStateChangedCallback(state, fileCopy, destCopy, queued, processed);
    }
}

void WorkerThread::LogTransferResult(const std::string& fileName,
                                    const std::string& sourceDir,
                                    const std::string& destDir,
                                    const std::string& result) {
    std::wstring timestamp = GetTimestamp();
    LogTransfer(StringToWString(fileName),
                StringToWString(sourceDir),
                StringToWString(destDir),
                timestamp,
                StringToWString(result));
}
