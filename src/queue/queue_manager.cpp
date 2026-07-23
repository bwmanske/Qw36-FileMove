#include "queue_manager.h"
#include "data/file_io.h"
#include "utils/logging.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <fstream>
#include <map>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

QueueManager gQueueManager;

// Recursively find empty subdirectories under a directory
// Returns relative paths (relative to dirPath) of empty directories
static void FindEmptyDirectories(const std::wstring& dirPath, std::vector<std::wstring>& emptyDirs) {
    std::wstring searchPattern = dirPath;
    if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/') {
        searchPattern += L'\\';
    }
    searchPattern += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    std::vector<std::wstring> subDirs;
    bool hasFiles = false;

    do {
        std::wstring entryName = findData.cFileName;
        if (entryName == L"." || entryName == L"..") continue;

        std::wstring entryPath = dirPath;
        if (!entryPath.empty() && entryPath.back() != L'\\' && entryPath.back() != L'/') {
            entryPath += L'\\';
        }
        entryPath += entryName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            subDirs.push_back(entryPath);
        } else {
            hasFiles = true;
        }
    } while (FindNextFileW(hFind, &findData) != 0);

    FindClose(hFind);

    // Always recurse into subdirectories to find empty ones
    for (const auto& subDir : subDirs) {
        FindEmptyDirectories(subDir, emptyDirs);
    }

    // Only mark this directory as empty if it has no files AND no subdirectories
    // (i.e., it's a truly empty leaf directory)
    if (hasFiles) return;
    if (!subDirs.empty()) return;

    // Compute relative path from dirPath
    std::wstring relPath = dirPath;
    if (!relPath.empty() && (relPath.back() == L'\\' || relPath.back() == L'/')) {
        relPath.pop_back();
    }
    emptyDirs.push_back(relPath);
}

void InitQueueManager() {
    // Queue manager is ready to use
}

QueueManager::QueueManager()
    : mEnableSidecarFiles(false)
    , mHideQueuedSourceFiles(false)
    , mEnableDirectoryMoves(false)
    , mPreserveDirectoryStructure(false)
    , mCreateEmptyDirectories(false)
    , mCurrentDebugMode(DebugTransferMode::MV)
{
}

void QueueManager::SetChangedCallback(QueueChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mChangedCallback = callback;
}

void QueueManager::SetEnableSidecarFiles(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mEnableSidecarFiles = enabled;
}

void QueueManager::SetHideQueuedSourceFiles(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mHideQueuedSourceFiles = enabled;
}

void QueueManager::SetEnableDirectoryMoves(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mEnableDirectoryMoves = enabled;
}

void QueueManager::SetPreserveDirectoryStructure(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPreserveDirectoryStructure = enabled;
}

void QueueManager::SetCreateEmptyDirectories(bool enabled) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCreateEmptyDirectories = enabled;
}

void QueueManager::SetDebugMode(DebugTransferMode mode) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCurrentDebugMode = mode;
}

static std::wstring StringToWString(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    if (len <= 0) return std::wstring(s.begin(), s.end());
    std::wstring result;
    result.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], len);
    return result;
}

static std::string WStringToString(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string result;
    result.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], len, NULL, NULL);
    return result;
}

bool QueueManager::ValidateSourceFile(const std::string& filePath, std::wstring& error) {
    std::wstring widePath = StringToWString(filePath);

    // Check if file exists
    if (!FileExists(widePath)) {
        error = L"Source file does not exist: " + widePath;
        return false;
    }

    return true;
}

std::vector<std::string> QueueManager::ValidateDestinations(
    const std::vector<std::string>& dirs,
    std::vector<std::string>& unavailable)
{
    std::vector<std::string> available;
    unavailable.clear();

    for (const auto& dir : dirs) {
        std::wstring wideDir = StringToWString(dir);
        if (DirectoryExists(wideDir)) {
            available.push_back(dir);
        } else {
            unavailable.push_back(dir);
        }
    }

    return available;
}

bool QueueManager::CreateSidecar(const std::string& sourceFilePath) {
    if (!mEnableSidecarFiles) return true;

    std::string sidecarPath = sourceFilePath + ".filemove-queued";
    std::wstring wideSidecar = StringToWString(sidecarPath);

    // Create hidden sidecar file
    HANDLE hFile = CreateFileW(
        wideSidecar.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_HIDDEN,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    CloseHandle(hFile);
    return true;
}

bool QueueManager::HideSourceFile(const std::string& sourceFilePath) {
    if (!mHideQueuedSourceFiles) return true;

    std::wstring widePath = StringToWString(sourceFilePath);
    HANDLE hFile = CreateFileW(
        widePath.c_str(),
        FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD attrs = GetFileAttributesW(widePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        CloseHandle(hFile);
        return false;
    }

    attrs |= FILE_ATTRIBUTE_HIDDEN;
    bool success = SetFileAttributesW(widePath.c_str(), attrs) != 0;

    CloseHandle(hFile);
    return success;
}

bool QueueManager::IsAlreadyQueued(const std::string& sourceFilePath) const {
    for (const auto& entry : mEntries) {
        if (entry.sourceFilePath == sourceFilePath &&
            (entry.status == QueueEntryStatus::Queued ||
             entry.status == QueueEntryStatus::Released ||
             entry.status == QueueEntryStatus::Processing)) {
            return true;
        }
    }
    return false;
}

std::string QueueManager::GenerateEntryId() {
    auto now = std::chrono::system_clock::now();
    auto count = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::mt19937 gen(static_cast<unsigned>(count));
    std::uniform_int_distribution<> dis(0, 0xFFFFFF);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "move-%06x", dis(gen));
    return std::string(buf);
}

bool QueueManager::PrepareBatch(const std::string& groupId,
                                 const std::vector<std::string>& sourceFiles,
                                 const std::vector<std::string>& destinationDirs,
                                 std::wstring& errorMessage)
{
    mMutex.lock();
    errorMessage.clear();

    if (sourceFiles.empty()) {
        errorMessage = L"No source files provided";
        mMutex.unlock();
        return false;
    }

    if (destinationDirs.empty()) {
        errorMessage = L"No destination directories provided";
        mMutex.unlock();
        return false;
    }

    // Expand directory sources into individual files, tracking source directory root for structure preservation
    struct SourceFileWithRoot {
        std::string filePath;
        std::string sourceDirRoot; // empty for non-directory sources
    };
    std::vector<SourceFileWithRoot> expandedSources;
    // Map from sourceDirRoot to its empty subdirectories (relative paths)
    std::map<std::string, std::vector<std::string>> emptyDirMap;
    for (const auto& src : sourceFiles) {
        std::wstring wideSrc = StringToWString(src);
        if (DirectoryExists(wideSrc)) {
            if (!mEnableDirectoryMoves) {
                errorMessage = L"Directory moves are disabled: " + wideSrc;
                mMutex.unlock();
                return false;
            }
            // Expand directory recursively
            auto files = EnumerateDirectoryFiles(wideSrc);
            for (const auto& f : files) {
                SourceFileWithRoot sf;
                sf.filePath = WStringToString(f);
                sf.sourceDirRoot = src;
                expandedSources.push_back(sf);
            }
            // Find empty subdirectories (only when Preserve directory structure is also enabled)
            if (mCreateEmptyDirectories && mPreserveDirectoryStructure) {
                std::vector<std::wstring> emptyDirs;
                FindEmptyDirectories(wideSrc, emptyDirs);
                for (const auto& ed : emptyDirs) {
                    std::string relPath = WStringToString(ed);
                    // Compute relative path from sourceDirRoot
                    if (src.size() <= relPath.size()) {
                        relPath = relPath.substr(src.size());
                        if (!relPath.empty() && (relPath[0] == '\\' || relPath[0] == '/')) {
                            relPath = relPath.substr(1);
                        }
                    }
                    if (!relPath.empty()) {
                        emptyDirMap[src].push_back(relPath);
                    }
                }
            }
        } else {
            SourceFileWithRoot sf;
            sf.filePath = src;
            expandedSources.push_back(sf);
        }
    }

    if (expandedSources.empty()) {
        errorMessage = L"No files found in source directories";
        mMutex.unlock();
        return false;
    }

    // Validate all source files first
    std::vector<SourceFileWithRoot> validSources;
    std::vector<std::string> skippedDuplicates;

    for (const auto& sf : expandedSources) {
        if (IsAlreadyQueued(sf.filePath)) {
            skippedDuplicates.push_back(sf.filePath);
            // Log as Rejected CSV entry
            std::string srcDir = sf.filePath;
            size_t sl = srcDir.find_last_of("/\\");
            if (sl != std::string::npos) srcDir = srcDir.substr(0, sl);
            else srcDir.clear();
            std::string srcFile = sf.filePath;
            size_t sf2 = srcFile.find_last_of("/\\");
            if (sf2 != std::string::npos) srcFile = srcFile.substr(sf2 + 1);
            else srcFile = sf.filePath;
            LogTransfer(StringToWString(srcFile), StringToWString(srcDir), L"", GetTimestamp(), L"Rejected - Already queued");
            continue;
        }

        std::wstring validationError;
        if (ValidateSourceFile(sf.filePath, validationError)) {
            validSources.push_back(sf);
        } else {
            // Log as Rejected CSV entry
            std::string srcDir = sf.filePath;
            size_t sl = srcDir.find_last_of("/\\");
            if (sl != std::string::npos) srcDir = srcDir.substr(0, sl);
            else srcDir.clear();
            std::string srcFile = sf.filePath;
            size_t sf2 = srcFile.find_last_of("/\\");
            if (sf2 != std::string::npos) srcFile = srcFile.substr(sf2 + 1);
            else srcFile = sf.filePath;
            LogTransfer(StringToWString(srcFile), StringToWString(srcDir), L"", GetTimestamp(), L"Rejected - " + validationError);
            errorMessage = validationError;
            mMutex.unlock();
            return false;
        }
    }

    if (validSources.empty()) {
        if (!skippedDuplicates.empty()) {
            errorMessage = L"All files are already queued";
        } else {
            errorMessage = L"No valid source files";
        }
        mMutex.unlock();
        return false;
    }

    // Validate destinations
    std::vector<std::string> unavailableDirs;
    std::vector<std::string> availableDirs = ValidateDestinations(destinationDirs, unavailableDirs);

    if (availableDirs.empty()) {
        errorMessage = L"No destination directories are available:\n";
        for (const auto& dir : unavailableDirs) {
            errorMessage += StringToWString(dir) + L"\n";
        }
        mMutex.unlock();
        return false;
    }

    // Check for source-vs-destination path conflicts
    // For each source, determine which destinations it already matches
    struct SourceConflictInfo {
        std::string sourcePath;
        std::vector<std::string> matchingDests;  // destinations where source already exists
        std::vector<std::string> nonMatchingDests; // destinations where source doesn't exist
    };
    std::vector<SourceConflictInfo> conflictInfos;

    for (const auto& sf : validSources) {
        SourceConflictInfo info;
        info.sourcePath = sf.filePath;

        std::string srcFileName;
        size_t lastSlash = sf.filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            srcFileName = sf.filePath.substr(lastSlash + 1);
        } else {
            srcFileName = sf.filePath;
        }

        for (const auto& dest : availableDirs) {
            std::string destFilePath = dest;
            if (!destFilePath.empty() && destFilePath.back() != '/' && destFilePath.back() != '\\') {
                destFilePath += '\\';
            }
            if (mPreserveDirectoryStructure && !sf.sourceDirRoot.empty()) {
                // Compute relative path from source dir root
                std::string relPath = sf.filePath;
                if (sf.sourceDirRoot.size() <= relPath.size()) {
                    relPath = relPath.substr(sf.sourceDirRoot.size());
                    if (!relPath.empty() && (relPath[0] == '\\' || relPath[0] == '/')) {
                        relPath = relPath.substr(1);
                    }
                }
                // Get source directory basename
                std::string srcDirBase = sf.sourceDirRoot;
                size_t ds = srcDirBase.find_last_of("/\\");
                if (ds != std::string::npos) {
                    srcDirBase = srcDirBase.substr(ds + 1);
                }
                destFilePath += srcDirBase + "\\" + relPath;
            } else {
                destFilePath += srcFileName;
            }

            // Normalize path separators for comparison
            std::string normalizedSrc = sf.filePath;
            for (auto& c : normalizedSrc) { if (c == '\\') c = '/'; }
            std::string normalizedDest = destFilePath;
            for (auto& c : normalizedDest) { if (c == '\\') c = '/'; }

            if (normalizedDest == normalizedSrc) {
                info.matchingDests.push_back(dest);
            } else {
                info.nonMatchingDests.push_back(dest);
            }
        }
        conflictInfos.push_back(info);
    }

    // Process conflicts
    for (const auto& info : conflictInfos) {
        if (info.matchingDests.empty()) {
            // No conflicts, proceed normally
            continue;
        }

        if (info.nonMatchingDests.empty()) {
            // Source matches ALL destinations - already in place everywhere, skip
            std::wstring msg = L"Source already in all destinations, skipping: " +
                              StringToWString(info.sourcePath);
            LogInfo(msg);
            DebugConsoleWriteLine(msg);
            // Log as Rejected CSV entry
            std::string srcDir = info.sourcePath;
            size_t sl = srcDir.find_last_of("/\\");
            if (sl != std::string::npos) srcDir = srcDir.substr(0, sl);
            else srcDir.clear();
            std::string srcFile = info.sourcePath;
            size_t sf2 = srcFile.find_last_of("/\\");
            if (sf2 != std::string::npos) srcFile = srcFile.substr(sf2 + 1);
            else srcFile = info.sourcePath;
            LogTransfer(StringToWString(srcFile), StringToWString(srcDir), L"", GetTimestamp(), L"Rejected - Source already in all destinations");
            // Mark as duplicate to skip
            skippedDuplicates.push_back(info.sourcePath);
        } else {
            // Source matches SOME destinations - offer Continue/Cancel
            std::wstring msg = L"Source file already exists in some destinations.\n\n";
            msg += L"Matching destinations:\n";
            for (const auto& d : info.matchingDests) {
                msg += L"  " + StringToWString(d) + L"\n";
            }
            msg += L"\nContinue with remaining destinations only?";

            int result = MessageBoxW(NULL, msg.c_str(),
                L"Source-Destination Conflict",
                MB_YESNO | MB_ICONWARNING | MB_TOPMOST);

            if (result == IDNO) {
                errorMessage = L"User canceled due to source-destination conflict: " +
                              StringToWString(info.sourcePath);
                mMutex.unlock();
                return false;
            }
            // User chose Yes - matching destinations will be filtered out when creating entries
        }
    }

    // Log skipped duplicates
    for (const auto& dup : skippedDuplicates) {
        std::wstring msg = L"Already queued: " + StringToWString(dup);
        LogInfo(msg);
        DebugConsoleWriteLine(msg);
    }

    // Create entries for valid sources
    std::string logPath = WStringToString(GetLogFilePath());
    std::string timestamp = GetIsoTimestamp();

    for (size_t i = 0; i < validSources.size(); i++) {
        const auto& sf = validSources[i];

        // Skip sources that were already queued or match all destinations
        bool shouldSkip = false;
        for (const auto& dup : skippedDuplicates) {
            if (dup == sf.filePath) {
                shouldSkip = true;
                break;
            }
        }
        if (shouldSkip) continue;

        // Build destination list, filtering out matching destinations
        std::vector<std::string> entryDests;
        const auto& conflictInfo = conflictInfos[i];
        if (!conflictInfo.matchingDests.empty()) {
            // Use only non-matching destinations
            entryDests = conflictInfo.nonMatchingDests;
        } else {
            entryDests = availableDirs;
        }

        // Skip if no destinations remain after filtering
        if (entryDests.empty()) {
            std::wstring msg = L"No remaining destinations for source, skipping: " +
                              StringToWString(sf.filePath);
            LogInfo(msg);
            // Log as Rejected CSV entry
            std::string srcDir = sf.filePath;
            size_t sl = srcDir.find_last_of("/\\");
            if (sl != std::string::npos) srcDir = srcDir.substr(0, sl);
            else srcDir.clear();
            std::string srcFile = sf.filePath;
            size_t sf2 = srcFile.find_last_of("/\\");
            if (sf2 != std::string::npos) srcFile = srcFile.substr(sf2 + 1);
            else srcFile = sf.filePath;
            LogTransfer(StringToWString(srcFile), StringToWString(srcDir), L"", GetTimestamp(), L"Rejected - No remaining destinations");
            continue;
        }

        // If preserving directory structure, compute destination directory paths
        if (mPreserveDirectoryStructure && !sf.sourceDirRoot.empty()) {
            std::string srcDirBase = sf.sourceDirRoot;
            size_t ds = srcDirBase.find_last_of("/\\");
            if (ds != std::string::npos) {
                srcDirBase = srcDirBase.substr(ds + 1);
            }
            std::string relPath = sf.filePath.substr(sf.sourceDirRoot.size());
            if (!relPath.empty() && (relPath[0] == '\\' || relPath[0] == '/')) {
                relPath = relPath.substr(1);
            }
            // Extract parent directory from relative path
            std::string relDir;
            size_t lastSep = relPath.find_last_of("/\\");
            if (lastSep != std::string::npos) {
                relDir = relPath.substr(0, lastSep);
            }
            std::vector<std::string> fullDests;
            for (const auto& dest : entryDests) {
                std::string fullDest = dest;
                if (!fullDest.empty() && fullDest.back() != '/' && fullDest.back() != '\\') {
                    fullDest += '\\';
                }
                fullDest += srcDirBase;
                if (!relDir.empty()) {
                    fullDest += "\\" + relDir;
                }
                fullDests.push_back(fullDest);
            }
            entryDests = fullDests;
        }

        PendingMoveEntry entry;
        entry.id = GenerateEntryId();
        entry.groupId = groupId;
        entry.sourceFilePath = sf.filePath;
        entry.destinationDirectories = entryDests;
        entry.activeLogFilePath = logPath;
        entry.debugTransferMode = mCurrentDebugMode;
        // Populate empty directories for this source
        if (mCreateEmptyDirectories && !sf.sourceDirRoot.empty()) {
            auto it = emptyDirMap.find(sf.sourceDirRoot);
            if (it != emptyDirMap.end()) {
                entry.emptySourceDirectories = it->second;
            }
        }
        entry.queuedAt = timestamp;
        entry.status = QueueEntryStatus::Queued;

        mEntries.push_back(entry);

        // Create sidecar file
        CreateSidecar(sf.filePath);

        // Hide source file if option enabled
        HideSourceFile(sf.filePath);
    }

    // Notify UI of queue change (release lock first to avoid deadlock)
    mMutex.unlock();
    if (mChangedCallback) {
        mChangedCallback();
    }

    return true;
}

void QueueManager::ReleasePreparedEntries() {
    mMutex.lock();

    for (auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Queued) {
            entry.status = QueueEntryStatus::Released;
        }
    }
    mMutex.unlock();

    if (mChangedCallback) {
        mChangedCallback();
    }
}

void QueueManager::CancelPreparedEntries() {
    mMutex.lock();

    for (auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Queued) {
            // Remove sidecar and restore visibility
            RemoveSidecar(entry.sourceFilePath);
            RestoreSourceVisibility(entry.sourceFilePath);
            entry.status = QueueEntryStatus::Canceled;
        }
    }
    mMutex.unlock();

    if (mChangedCallback) {
        mChangedCallback();
    }
}

void QueueManager::CancelAllEntries() {
    bool callbackNeeded = false;
    {
        mMutex.lock();

        auto it = mEntries.begin();
        while (it != mEntries.end()) {
            if (it->status == QueueEntryStatus::Queued ||
                it->status == QueueEntryStatus::Released) {
                std::wstring msg = L"Queue canceled: " + StringToWString(it->sourceFilePath);
                LogInfo(msg);
                RemoveSidecar(it->sourceFilePath);
                RestoreSourceVisibility(it->sourceFilePath);
                it = mEntries.erase(it);
            } else {
                ++it;
            }
        }
        callbackNeeded = (mChangedCallback != nullptr);
        mMutex.unlock();
    }

    if (callbackNeeded) {
        mChangedCallback();
    }
}

std::vector<PendingMoveEntry> QueueManager::GetQueuedEntries() const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<PendingMoveEntry> result;

    for (const auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Queued ||
            entry.status == QueueEntryStatus::Released ||
            entry.status == QueueEntryStatus::Processing) {
            result.push_back(entry);
        }
    }

    return result;
}

std::vector<PendingMoveEntry> QueueManager::GetReleasedEntries() const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<PendingMoveEntry> result;

    for (const auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Released) {
            result.push_back(entry);
        }
    }

    return result;
}

std::vector<PendingMoveEntry> QueueManager::GetAllEntries() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mEntries;
}

bool QueueManager::GetNextEntry(PendingMoveEntry& entry) {
    mMutex.lock();

    for (auto& e : mEntries) {
        if (e.status == QueueEntryStatus::Released) {
            e.status = QueueEntryStatus::Processing;
            entry = e;
            mMutex.unlock();
            if (mChangedCallback) {
                mChangedCallback();
            }
            return true;
        }
    }

    mMutex.unlock();
    return false;
}

void QueueManager::MarkCompleted(const std::string& entryId) {
    bool callbackNeeded = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mEntries.begin(), mEntries.end(),
            [&entryId](const PendingMoveEntry& entry) { return entry.id == entryId; });
        if (it != mEntries.end()) {
            RemoveSidecar(it->sourceFilePath);
            mEntries.erase(it);
        }
        callbackNeeded = (mChangedCallback != nullptr);
    }

    if (callbackNeeded) {
        mChangedCallback();
    }
}

void QueueManager::MarkFailed(const std::string& entryId, const std::string& error) {
    bool callbackNeeded = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mEntries.begin(), mEntries.end(),
            [&entryId](const PendingMoveEntry& entry) { return entry.id == entryId; });
        if (it != mEntries.end()) {
            RemoveSidecar(it->sourceFilePath);
            RestoreSourceVisibility(it->sourceFilePath);
            mEntries.erase(it);
        }
        callbackNeeded = (mChangedCallback != nullptr);
    }

    if (callbackNeeded) {
        mChangedCallback();
    }
}

void QueueManager::MarkCanceled(const std::string& entryId) {
    bool callbackNeeded = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mEntries.begin(), mEntries.end(),
            [&entryId](const PendingMoveEntry& entry) { return entry.id == entryId; });
        if (it != mEntries.end()) {
            RemoveSidecar(it->sourceFilePath);
            RestoreSourceVisibility(it->sourceFilePath);
            mEntries.erase(it);
        }
        callbackNeeded = (mChangedCallback != nullptr);
    }

    if (callbackNeeded) {
        mChangedCallback();
    }
}

int QueueManager::GetQueuedDestinationCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    int count = 0;

    for (const auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Queued ||
            entry.status == QueueEntryStatus::Released ||
            entry.status == QueueEntryStatus::Processing) {
            count += static_cast<int>(entry.destinationDirectories.size());
        }
    }

    return count;
}

bool QueueManager::IsEmpty() const {
    std::lock_guard<std::mutex> lock(mMutex);

    for (const auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Queued ||
            entry.status == QueueEntryStatus::Released ||
            entry.status == QueueEntryStatus::Processing) {
            return false;
        }
    }

    return true;
}

bool QueueManager::IsProcessing() const {
    std::lock_guard<std::mutex> lock(mMutex);

    for (const auto& entry : mEntries) {
        if (entry.status == QueueEntryStatus::Processing) {
            return true;
        }
    }

    return false;
}

void QueueManager::RemoveSidecar(const std::string& sourceFilePath) {
    if (!mEnableSidecarFiles) return;

    std::string sidecarPath = sourceFilePath + ".filemove-queued";
    std::wstring wideSidecar = StringToWString(sidecarPath);

    // Delete the sidecar file if it exists
    if (FileExists(wideSidecar)) {
        DeleteFileW(wideSidecar.c_str());
    }
}

void QueueManager::RestoreSourceVisibility(const std::string& sourceFilePath) {
    if (!mHideQueuedSourceFiles) return;

    std::wstring widePath = StringToWString(sourceFilePath);
    DWORD attrs = GetFileAttributesW(widePath.c_str());

    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
        attrs &= ~FILE_ATTRIBUTE_HIDDEN;
        SetFileAttributesW(widePath.c_str(), attrs);
    }
}
