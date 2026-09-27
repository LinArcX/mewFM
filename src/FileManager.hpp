#pragma once

#include <string>
#include <vector>

/**
 * @brief Describes a single directory entry (file or folder).
 *
 * Stores both raw and formatted metadata so the UI can render each row of
 * the file listing without performing additional syscalls per frame.
 */
struct Entry
{
  std::string name;                  ///< File or directory name (no path component).
  std::string sizeText;              ///< Human-readable size, e.g. "1.2 MB".
  std::string typeText;              ///< Type description, e.g. "cpp file" or "Folder".
  std::string ownerText;             ///< Owner user name resolved from the UID.
  std::string permText;              ///< Permissions string, e.g. "drwxr-xr-x".
  unsigned long long sizeBytes = 0;  ///< Raw size in bytes (0 for directories).
  bool isDirectory = false;          ///< True when the entry is a directory.
  bool isExecutable = false;         ///< True when any execute bit is set on a file.
};

/**
 * @brief Identifies the current mode of the internal clipboard.
 */
enum class ClipboardMode
{
  None, ///< Clipboard is empty.
  Copy, ///< Entries were copied; source files remain in place.
  Cut,  ///< Entries were cut; sources are removed after a successful paste.
};

/**
 * @brief Selects which column the file list is sorted by.
 */
enum class SortField
{
  Name,        ///< Sort alphabetically by name.
  Size,        ///< Sort by file size in bytes.
  Type,        ///< Sort by type description.
  Owner,       ///< Sort by owner name.
  Permissions, ///< Sort by permission string.
};

/**
 * @brief Reports the current status of a long-running file operation
 *        such as copy, move, or paste.
 */
enum class FileOpStatus
{
  Idle,         ///< No operation is in progress.
  Running,      ///< An operation is in progress.
  FinishedCopy, ///< A copy operation completed successfully.
  FinishedMove, ///< A move operation completed successfully.
  Failed,       ///< The operation failed.
};

/**
 * @brief Progress information for a running file operation.
 *
 * Used by the UI to render the progress bar, label, and pause/resume button
 * in the status bar while a copy, move, or paste is in progress.
 */
struct FileOpProgress
{
  bool active = false;               ///< True while a file operation is running.
  bool failed = false;               ///< True when the last operation failed.
  bool paused = false;               ///< True when the user paused the operation.
  std::string label;                 ///< Human-readable description of the operation.
  unsigned long long totalBytes = 0; ///< Total number of bytes to process.
  unsigned long long doneBytes = 0;  ///< Number of bytes processed so far.
};

/**
 * @brief Owns the current directory, its entries, and all filesystem
 *        operations performed by the application.
 *
 * A FileManager instance represents a single browsing session (one tab).
 * It loads a directory, filters and sorts the entries, exposes the
 * navigation stacks (back/forward), and drives long-running copy, move,
 * trash, restore, and archive-extraction operations.
 */
class FileManager
{
public:
  /**
   * @brief Constructs a FileManager and loads the current working directory.
   */
  FileManager();

  /**
   * @brief Navigates to @p path and pushes the previous directory onto the back stack.
   * @param path Absolute path of the directory to open.
   * @return True on success, false if the path is not a directory.
   */
  bool setPath(const std::string& path);

  /** @brief Navigates to the previous directory in the back stack. */
  bool goBack();

  /** @brief Navigates to the next directory in the forward stack. */
  bool goForward();

  /** @brief Navigates to the parent directory of the current path. */
  bool goUp();

  /** @return True if the back stack is non-empty. */
  bool canGoBack() const;

  /** @return True if the forward stack is non-empty. */
  bool canGoForward() const;

  /** @return The path of the directory currently being browsed. */
  const std::string& currentPath() const;

  /** @return The filtered and sorted entries of the current directory. */
  const std::vector<Entry>& entries() const;

  /**
   * @brief Resolves the freedesktop.org trash root directory.
   * @return Path to the Trash directory, or an empty string if HOME is unset.
   */
  static std::string trashRootDir();

  /**
   * @brief Checks whether a filename looks like a supported archive.
   * @param name File name (not a full path).
   * @return True for tar, tar.gz, tgz, tar.bz2, tbz2, tar.xz, txz and zip files.
   */
  static bool isArchive(const std::string& name);

  /**
   * @brief Toggles the visibility of hidden (dot) files.
   * @param show True to show hidden files.
   */
  void setShowHidden(bool show);

  /** @return True if hidden files are currently visible. */
  bool showHidden() const;

  /**
   * @brief Sets the live filter applied to entry names.
   * @param filter Substring matched against entry names (case-sensitive).
   */
  void setFilter(const std::string& filter);

  /** @return The current filter string. */
  const std::string& filter() const;

  /**
   * @brief Resets navigation stacks and loads @p path as the initial directory.
   * @param path Absolute path of the directory to open.
   */
  void resetTo(const std::string& path);

  /**
   * @brief Creates a new directory inside the current directory.
   * @param name Name of the new directory (no path separators).
   * @return True on success.
   */
  [[nodiscard]] bool createDirectory(const std::string& name);

  /**
   * @brief Creates a new empty file inside the current directory.
   * @param name Name of the new file (no path separators).
   * @return True on success.
   */
  [[nodiscard]] bool createFile(const std::string& name);

  /**
   * @brief Renames an entry inside the current directory.
   * @param oldName Current name.
   * @param newName New name (must not already exist).
   * @return True on success.
   */
  [[nodiscard]] bool renameEntry(const std::string& oldName, const std::string& newName);

  /**
   * @brief Permanently deletes an entry (file or directory tree).
   * @param name Name of the entry to delete.
   * @return True on success.
   */
  [[nodiscard]] bool deleteEntry(const std::string& name);

  /**
   * @brief Moves an entry to the freedesktop.org trash directory.
   * @param name Name of the entry to move to trash.
   * @return True on success.
   */
  [[nodiscard]] bool trashEntry(const std::string& name);

  /**
   * @brief Extracts a supported archive into the current directory.
   * @param name Archive file name (must be inside the current directory).
   * @return True on success.
   */
  [[nodiscard]] bool extractArchive(const std::string& name);

  /**
   * @brief Restores a trashed entry to its original location.
   * @param trashName Name of the file inside the trash's files/ directory.
   * @param overwrite When true, overwrite an existing file at the destination.
   * @return True on success.
   */
  [[nodiscard]] bool restoreEntry(const std::string& trashName, bool overwrite);

  /**
   * @brief Checks whether restoring @p trashName would overwrite an existing file.
   * @param trashName Name of the file inside the trash's files/ directory.
   * @return True if the original destination already exists.
   */
  [[nodiscard]] bool isRestoreConflict(const std::string& trashName) const;

  /**
   * @brief Empties the freedesktop.org trash directory.
   * @return True on success.
   */
  [[nodiscard]] bool emptyTrash();

  /**
   * @brief Stores the given entries in the clipboard in Copy mode.
   * @param names Names of entries to copy.
   * @return True if all entries exist.
   */
  [[nodiscard]] bool copyEntries(const std::vector<std::string>& names);

  /**
   * @brief Stores the given entries in the clipboard in Cut mode.
   * @param names Names of entries to cut.
   * @return True if all entries exist.
   */
  [[nodiscard]] bool cutEntries(const std::vector<std::string>& names);

  /**
   * @brief Starts a paste operation from the internal clipboard.
   * @return True if the operation was accepted and is now running.
   */
  [[nodiscard]] bool startPaste();

  /**
   * @brief Advances the currently running file operation by a small time slice.
   * @return Current FileOpStatus after the step.
   */
  [[nodiscard]] FileOpStatus pollFileOp();

  /** @return Read-only progress information of the current file operation. */
  const FileOpProgress& fileOpProgress() const;

  /**
   * @brief Pauses or resumes the currently running file operation.
   * @param paused True to pause, false to resume.
   */
  void setFileOpPaused(bool paused);

  /** @return True if the current file operation is paused. */
  bool fileOpPaused() const;

  /** @return The current clipboard mode (None, Copy, or Cut). */
  ClipboardMode clipboardMode() const;

  /**
   * @brief Sets the sort field and direction.
   * @param field     Column to sort by.
   * @param ascending True for ascending order, false for descending.
   */
  void setSort(SortField field, bool ascending);

  /** @return The currently active sort field. */
  SortField sortField() const;

  /** @return True if sorting is currently ascending. */
  bool sortAscending() const;

  /**
   * @brief Reloads the current directory from disk.
   * @return True on success.
   */
  [[nodiscard]] bool refresh();

private:
  /**
   * @brief High-level kind of file operation currently running.
   */
  enum class FileOpKind
  {
    None, ///< No operation.
    Copy, ///< Copy from source to destination.
    Move, ///< Move from source to destination.
  };

  /**
   * @brief Stage of a multi-step file operation.
   */
  enum class FileOpStage
  {
    None,           ///< No operation.
    MakeDirs,       ///< Create the destination directory tree.
    CopyFiles,      ///< Copy file contents chunk by chunk.
    DeleteSources,  ///< Remove source files after a cross-device move.
    RenameTopLevel, ///< Fast-path rename for same-device moves.
  };

  /**
   * @brief Describes a single file involved in a copy or move operation.
   */
  struct FileOpFile
  {
    std::string src;             ///< Absolute source path.
    std::string dst;             ///< Absolute destination path.
    unsigned long long size = 0; ///< File size in bytes.
  };

  /**
   * @brief Loads the directory at @p path into m_allEntries and reapplies filter/sort.
   * @param path Absolute path of the directory to load.
   * @return True on success.
   */
  bool loadPath(const std::string& path);

  /**
   * @brief Rebuilds m_entries from m_allEntries using the current filter and sort.
   */
  void applyFilter();

  /**
   * @brief Computes the parent directory of @p path.
   * @param path Absolute path.
   * @return Parent path, or an empty string when @p path is the root.
   */
  static std::string parentOf(const std::string& path);

  /**
   * @brief Builds the directory / file plan for a copy or move operation.
   * @param srcDir   Source directory.
   * @param dstDir   Destination directory.
   * @param names    Names of the entries to process.
   * @param moveMode True for move, false for copy.
   * @return True on success.
   */
  bool buildFileOpPlan(const std::string& srcDir,
                       const std::string& dstDir,
                       const std::vector<std::string>& names,
                       bool moveMode);

  /**
   * @brief Recursively walks @p srcRoot, populating m_opDirs and m_opFiles.
   * @param srcRoot Source directory to walk.
   * @param dstRoot Destination directory that mirrors the source tree.
   * @return True on success.
   */
  bool collectDirPlan(const std::string& srcRoot, const std::string& dstRoot);

  /**
   * @brief Advances the current file operation, spending at most @p budgetSeconds seconds.
   * @param budgetSeconds Time slice in seconds.
   * @return True when the operation has finished (successfully or not).
   */
  bool stepFileOp(double budgetSeconds);

  /**
   * @brief Copies one chunk of the current file during the CopyFiles stage.
   * @return True on success, false on I/O error.
   */
  bool stepCopyChunk();

  /**
   * @brief Closes the source and destination file descriptors used during copy.
   */
  void closeFileOpFds();

  /**
   * @brief Finalizes a file operation, updates progress, and reloads the directory.
   * @param success True if the operation completed successfully.
   */
  void finalizeFileOp(bool success);

  /**
   * @brief Resets all fields related to a running file operation.
   */
  void clearFileOp();

  /**
   * @brief Populates @p out with metadata for a single entry.
   * @param fullPath Absolute path of the entry.
   * @param name     File or directory name (no path).
   * @param out      Output Entry structure to fill.
   * @return True on success.
   */
  bool buildEntry(const std::string& fullPath, const std::string& name, Entry& out);

  /**
   * @brief Sorts m_entries using the current sort field and direction.
   */
  void sortEntries();

  /**
   * @brief Formats a byte count as a human-readable string (e.g. "1.2 MB").
   * @param bytes Number of bytes.
   * @return Formatted size string.
   */
  std::string formatSize(unsigned long long bytes) const;

  /**
   * @brief Counts direct children of a directory for the size column.
   * @param path Absolute path of the directory.
   * @return Formatted count string, e.g. "12 items".
   */
  std::string countChildren(const std::string& path) const;

  /**
   * @brief Converts a POSIX mode into an "ls -l" style permission string.
   * @param mode Raw mode_t value.
   * @return 10-character permission string.
   */
  std::string permissionsToString(unsigned int mode) const;

  /**
   * @brief Resolves a numeric UID into a user name.
   * @param uid Numeric user id.
   * @return User name, or the numeric id as a string if lookup fails.
   */
  std::string ownerName(unsigned int uid) const;

  /**
   * @brief Derives the type description for a file name from its extension.
   * @param name File name.
   * @return Type description string, e.g. "cpp file" or "File".
   */
  std::string typeTextForName(const std::string& name) const;

  std::string m_currentPath;                  ///< Absolute path of the browsed directory.
  std::vector<Entry> m_allEntries;            ///< Unfiltered entries of the current directory.
  std::vector<Entry> m_entries;               ///< Filtered and sorted entries exposed to the UI.
  std::string m_filter;                       ///< Active substring filter.
  std::vector<std::string> m_backStack;       ///< History for the Back button.
  std::vector<std::string> m_forwardStack;    ///< History for the Forward button.

  ClipboardMode m_clipboardMode = ClipboardMode::None; ///< Current clipboard mode.
  std::vector<std::string> m_clipboardNames;  ///< Names stored in the clipboard.
  std::string m_clipboardSource;              ///< Directory the clipboard entries came from.

  SortField m_sortField = SortField::Name;    ///< Active sort column.
  bool m_sortAscending = true;                ///< True for ascending, false for descending.

  bool m_showHidden = false;                  ///< True if hidden files are shown.

  FileOpKind m_opKind = FileOpKind::None;     ///< Current high-level operation kind.
  FileOpStage m_opStage = FileOpStage::None;  ///< Current stage of the operation.
  std::string m_opLabel;                      ///< Label shown in the status bar.
  std::string m_opSrcDir;                     ///< Source directory of the operation.
  std::string m_opDstDir;                     ///< Destination directory of the operation.
  std::vector<std::string> m_opTopNames;      ///< Top-level names being operated on.
  std::vector<FileOpFile> m_opFiles;          ///< Flat list of files to copy.
  std::vector<std::string> m_opDirs;          ///< Directories that must exist before copying.
  std::vector<char> m_opBuffer;               ///< Reusable I/O buffer.
  size_t m_opDirIndex = 0;                    ///< Index into m_opDirs.
  size_t m_opFileIndex = 0;                   ///< Index into m_opFiles.
  size_t m_opDeleteIndex = 0;                 ///< Index into m_opTopNames for deletion.
  size_t m_opRenameIndex = 0;                 ///< Index into m_opTopNames for rename.
  unsigned long long m_opTotalBytes = 0;      ///< Total bytes to transfer.
  unsigned long long m_opDoneBytes = 0;       ///< Bytes transferred so far.
  int m_opSrcFd = -1;                         ///< Currently open source file descriptor.
  int m_opDstFd = -1;                         ///< Currently open destination file descriptor.
  bool m_opFailed = false;                    ///< Set to true if the operation failed.
  bool m_opPaused = false;                    ///< True when the user paused the operation.
  FileOpProgress m_opProgress;                ///< Snapshot exposed to the UI.
};
