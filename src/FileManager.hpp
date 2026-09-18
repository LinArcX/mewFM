#pragma once

#include <string>
#include <vector>

struct Entry
{
  std::string name;
  std::string sizeText;
  std::string typeText;
  std::string ownerText;
  std::string permText;
  unsigned long long sizeBytes = 0;
  bool isDirectory = false;
  bool isExecutable = false;
};

enum class ClipboardMode
{
  None,
  Copy,
  Cut,
};

enum class SortField
{
  Name,
  Size,
  Type,
  Owner,
  Permissions,
};

enum class FileOpStatus
{
  Idle,
  Running,
  FinishedCopy,
  FinishedMove,
  Failed,
};

struct FileOpProgress
{
  bool active = false;
  bool failed = false;
  bool paused = false;
  std::string label;
  unsigned long long totalBytes = 0;
  unsigned long long doneBytes = 0;
};

class FileManager
{
public:
  FileManager();

  bool setPath(const std::string& path);
  bool goBack();
  bool goForward();
  bool goUp();
  bool canGoBack() const;
  bool canGoForward() const;

  const std::string& currentPath() const;
  const std::vector<Entry>& entries() const;
  static std::string trashRootDir();
  static bool isArchive(const std::string& name);

  void setShowHidden(bool show);
  bool showHidden() const;
  void setFilter(const std::string& filter);
  const std::string& filter() const;
  void resetTo(const std::string& path);
  [[nodiscard]] bool createDirectory(const std::string& name);
  [[nodiscard]] bool createFile(const std::string& name);
  [[nodiscard]] bool renameEntry(const std::string& oldName, const std::string& newName);
  [[nodiscard]] bool deleteEntry(const std::string& name);
  [[nodiscard]] bool trashEntry(const std::string& name);
  [[nodiscard]] bool extractArchive(const std::string& name);
  [[nodiscard]] bool restoreEntry(const std::string& trashName, bool overwrite);
  [[nodiscard]] bool isRestoreConflict(const std::string& trashName) const;
  [[nodiscard]] bool emptyTrash();
  [[nodiscard]] bool copyEntries(const std::vector<std::string>& names);
  [[nodiscard]] bool cutEntries(const std::vector<std::string>& names);
  [[nodiscard]] bool startPaste();
  [[nodiscard]] FileOpStatus pollFileOp();
  const FileOpProgress& fileOpProgress() const;
  void setFileOpPaused(bool paused);
  bool fileOpPaused() const;
  ClipboardMode clipboardMode() const;
  void setSort(SortField field, bool ascending);
  SortField sortField() const;
  bool sortAscending() const;
  [[nodiscard]] bool refresh();

private:
  enum class FileOpKind
  {
    None,
    Copy,
    Move,
  };

  enum class FileOpStage
  {
    None,
    MakeDirs,
    CopyFiles,
    DeleteSources,
    RenameTopLevel,
  };

  struct FileOpFile
  {
    std::string src;
    std::string dst;
    unsigned long long size = 0;
  };

  bool loadPath(const std::string& path);
  void applyFilter();
  static std::string parentOf(const std::string& path);

  bool buildFileOpPlan(const std::string& srcDir,
                       const std::string& dstDir,
                       const std::vector<std::string>& names,
                       bool moveMode);
  bool collectDirPlan(const std::string& srcRoot, const std::string& dstRoot);
  bool stepFileOp(double budgetSeconds);
  bool stepCopyChunk();
  void closeFileOpFds();
  void finalizeFileOp(bool success);
  void clearFileOp();

  bool buildEntry(const std::string& fullPath, const std::string& name, Entry& out);
  void sortEntries();
  std::string formatSize(unsigned long long bytes) const;
  std::string countChildren(const std::string& path) const;
  std::string permissionsToString(unsigned int mode) const;
  std::string ownerName(unsigned int uid) const;
  std::string typeTextForName(const std::string& name) const;

  std::string m_currentPath;
  std::vector<Entry> m_allEntries;
  std::vector<Entry> m_entries;
  std::string m_filter;
  std::vector<std::string> m_backStack;
  std::vector<std::string> m_forwardStack;

  ClipboardMode m_clipboardMode = ClipboardMode::None;
  std::vector<std::string> m_clipboardNames;
  std::string m_clipboardSource;

  SortField m_sortField = SortField::Name;
  bool m_sortAscending = true;

  bool m_showHidden = false;

  FileOpKind m_opKind = FileOpKind::None;
  FileOpStage m_opStage = FileOpStage::None;
  std::string m_opLabel;
  std::string m_opSrcDir;
  std::string m_opDstDir;
  std::vector<std::string> m_opTopNames;
  std::vector<FileOpFile> m_opFiles;
  std::vector<std::string> m_opDirs;
  std::vector<char> m_opBuffer;
  size_t m_opDirIndex = 0;
  size_t m_opFileIndex = 0;
  size_t m_opDeleteIndex = 0;
  size_t m_opRenameIndex = 0;
  unsigned long long m_opTotalBytes = 0;
  unsigned long long m_opDoneBytes = 0;
  int m_opSrcFd = -1;
  int m_opDstFd = -1;
  bool m_opFailed = false;
  bool m_opPaused = false;
  FileOpProgress m_opProgress;
};
