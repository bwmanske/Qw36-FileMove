#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include "data/json_parser.h"

enum class QueueEntryStatus {
    Queued,       // Prepared, waiting for release
    Released,     // Released to worker thread
    Processing,   // Currently being processed by worker
    Completed,    // Successfully completed
    Failed,       // Failed during processing
    Canceled      // Canceled by user or shutdown
};

enum class DebugTransferMode {
    MV,  // Normal move (remove source after all destinations succeed)
    CP   // Debug copy (do not remove source)
};

struct PendingMoveEntry {
    std::string id;
    std::string groupId;
    std::string sourceFilePath;
    std::vector<std::string> destinationDirectories;
    std::vector<std::string> emptySourceDirectories;
    std::string activeLogFilePath;
    DebugTransferMode debugTransferMode;
    std::string queuedAt;
    QueueEntryStatus status;
};

// Callback type for queue state changes
using QueueChangedCallback = std::function<void()>;

class QueueManager {
public:
    QueueManager();

    // Set callback for queue state changes
    void SetChangedCallback(QueueChangedCallback callback);

    // Set options from app settings
    void SetEnableSidecarFiles(bool enabled);
    void SetHideQueuedSourceFiles(bool enabled);
    void SetEnableDirectoryMoves(bool enabled);
    void SetPreserveDirectoryStructure(bool enabled);
    void SetCreateEmptyDirectories(bool enabled);

    // Set debug transfer mode from CLI
    void SetDebugMode(DebugTransferMode mode);

    // Two-stage queue operations
    // Prepare stage: validate and add entries to pending queue
    // Returns true if all entries were successfully prepared
    bool PrepareBatch(const std::string& groupId,
                      const std::vector<std::string>& sourceFiles,
                      const std::vector<std::string>& destinationDirs,
                      std::wstring& errorMessage);

    // Release stage: move prepared entries to released state for worker
    void ReleasePreparedEntries();

    // Cancel all prepared entries (before release)
    void CancelPreparedEntries();

    // Cancel all entries (including released/processing)
    void CancelAllEntries();

    // Get entries by status
    std::vector<PendingMoveEntry> GetQueuedEntries() const;
    std::vector<PendingMoveEntry> GetReleasedEntries() const;
    std::vector<PendingMoveEntry> GetAllEntries() const;

    // Get the next entry for the worker to process
    bool GetNextEntry(PendingMoveEntry& entry);

    // Mark an entry as completed/failed/canceled (entry is removed from queue)
    void MarkCompleted(const std::string& entryId);
    void MarkFailed(const std::string& entryId, const std::string& error);
    void MarkCanceled(const std::string& entryId);

    // Get total queued destination count (unfinished destination files)
    int GetQueuedDestinationCount() const;

    // Check if queue is empty (no queued or released entries)
    bool IsEmpty() const;

    // Check if any entry is currently processing
    bool IsProcessing() const;

    // Remove sidecar file for a source file
    void RemoveSidecar(const std::string& sourceFilePath);

    // Restore visible state for a source file (if hidden)
    void RestoreSourceVisibility(const std::string& sourceFilePath);

private:
    // Validate a single source file
    bool ValidateSourceFile(const std::string& filePath, std::wstring& error);

    // Validate destination directories, returns only available ones
    std::vector<std::string> ValidateDestinations(const std::vector<std::string>& dirs,
                                                   std::vector<std::string>& unavailable);

    // Create sidecar file
    bool CreateSidecar(const std::string& sourceFilePath);

    // Hide source file
    bool HideSourceFile(const std::string& sourceFilePath);

    // Check if a source file is already queued
    bool IsAlreadyQueued(const std::string& sourceFilePath) const;

    // Generate unique entry ID
    std::string GenerateEntryId();

    mutable std::mutex mMutex;
    std::vector<PendingMoveEntry> mEntries;
    QueueChangedCallback mChangedCallback;

    bool mEnableSidecarFiles;
    bool mHideQueuedSourceFiles;
    bool mEnableDirectoryMoves;
    bool mPreserveDirectoryStructure;
    bool mCreateEmptyDirectories;

    // Current log file path (set from main)
    std::string mCurrentLogPath;
    DebugTransferMode mCurrentDebugMode;
};

// Global queue manager instance
extern QueueManager gQueueManager;

// Initialize the global queue manager
void InitQueueManager();
