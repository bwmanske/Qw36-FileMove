#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "queue/queue_manager.h"

enum class WorkerState {
    Idle,
    Moving,
    ManualPause,
    PausedError
};

enum class ShutdownAction {
    None,
    FinishCurrentAndExit,
    CancelAndExit
};

enum class ConflictResolution {
    Replace,
    ReplaceAll,
    KeepBoth,
    Skip
};

// Callback for conflict resolution dialog
using ConflictCallback = std::function<ConflictResolution(const std::string& sourceFile,
                                                           const std::string& destFile)>;

// Callback for error pause: returns true to retry, false to cancel
using ErrorPauseCallback = std::function<bool(const std::string& error)>;

// Callback for worker state changes
using WorkerStateChangedCallback = std::function<void(WorkerState state,
                                                       const std::string& currentFile,
                                                       const std::string& currentDest,
                                                       int queued,
                                                       int processed)>;

class WorkerThread {
public:
    WorkerThread();
    ~WorkerThread();

    // Start the worker thread
    void Start();

    // Stop the worker thread (graceful shutdown)
    void Stop();

    // Wait for worker thread to finish
    void WaitForCompletion();

    // Pause/resume control
    void Pause();
    void Resume();

    // Notify worker that new entries are available
    void Notify();

    // Shutdown control
    void RequestShutdown(ShutdownAction action);

    // Set callbacks
    void SetConflictCallback(ConflictCallback callback);
    void SetErrorPauseCallback(ErrorPauseCallback callback);
    void SetStateChangedCallback(WorkerStateChangedCallback callback);

    // Get current state
    WorkerState GetState() const;
    ShutdownAction GetShutdownAction() const;

    // Get processed count
    int GetProcessedCount() const;

    // Get current file/dest being processed
    std::string GetCurrentFile() const;
    std::string GetCurrentDest() const;
    std::string GetLastError() const;

    // Check if worker is busy
    bool IsBusy() const;

private:
    // Main worker loop
    void WorkerLoop();

    // Process a single entry
    void ProcessEntry(const PendingMoveEntry& entry);

    // Copy file to a single destination
    bool CopyToDestination(const std::string& sourceFile,
                          const std::string& destPath,
                          std::string& error);

    // Remove source file
    bool RemoveSourceFile(const std::string& sourceFile);

    // Handle file conflict
    ConflictResolution HandleConflict(const std::string& sourceFile,
                                      const std::string& destFile);

    // Generate unique destination file name for "Keep Both"
    std::string GenerateUniqueDestName(const std::string& destPath);

    // Check if we should pause
    bool ShouldPause();

    // Check if we should stop
    bool ShouldStop();

    // Update state and notify callback
    void UpdateState(WorkerState state,
                    const std::string& currentFile = "",
                    const std::string& currentDest = "");

    // Log transfer result
    void LogTransferResult(const std::string& fileName,
                          const std::string& sourceDir,
                          const std::string& destDir,
                          const std::string& result);

    std::thread mWorkerThread;
    std::atomic<bool> mRunning;
    std::atomic<bool> mPaused;
    std::atomic<bool> mStopping;
    std::atomic<ShutdownAction> mShutdownAction;
    std::atomic<int> mProcessedCount;

    mutable std::mutex mMutex;
    std::condition_variable mCondition;

    WorkerState mCurrentState;
    std::string mCurrentFile;
    std::string mCurrentDest;
    std::string mLastError;

    ConflictCallback mConflictCallback;
    ErrorPauseCallback mErrorPauseCallback;
    WorkerStateChangedCallback mStateChangedCallback;

    // Replace-all state, scoped to current group
    bool mReplaceAllActive;
    std::string mLastGroupId;
};

// Global worker thread instance
extern WorkerThread gWorkerThread;
