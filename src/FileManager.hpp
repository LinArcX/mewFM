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

  void setShowHidden(bool show);
  bool showHidden() const;
  void setFilter(const std::string& filter);
  const std::string& filter() const;
  void resetTo(const std::string& path);
  [[nodiscard]] bool createDirectory(const std::string& name);
  [[nodiscard]] bool renameEntry(const std::string& oldName, const std::string& newName);
  [[nodiscard]] bool deleteEntry(const std::string& name);
  [[nodiscard]] bool trashEntry(const std::string& name);
  [[nodiscard]] bool restoreEntry(const std::string& trashName, bool overwrite);
  [[nodiscard]] bool isRestoreConflict(const std::string& trashName) const;
  [[nodiscard]] bool emptyTrash();
  [[nodiscard]] bool copyEntries(const std::vector<std::string>& names);
  [[nodiscard]] bool cutEntries(const std::vector<std::string>& names);
  [[nodiscard]] bool paste();
  ClipboardMode clipboardMode() const;
  void setSort(SortField field, bool ascending);
  SortField sortField() const;
  bool sortAscending() const;
  [[nodiscard]] bool refresh();

private:
  bool loadPath(const std::string& path);
  void applyFilter();
  static std::string parentOf(const std::string& path);

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

};
