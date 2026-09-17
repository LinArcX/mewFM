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
  bool isDirectory = false;
  bool isExecutable = false;
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

  void setShowHidden(bool show);
  bool showHidden() const;

private:
  bool loadPath(const std::string& path);
  static std::string parentOf(const std::string& path);

  bool buildEntry(const std::string& fullPath, const std::string& name, Entry& out);
  void sortEntries();
  std::string formatSize(unsigned long long bytes) const;
  std::string countChildren(const std::string& path) const;
  std::string permissionsToString(unsigned int mode) const;
  std::string ownerName(unsigned int uid) const;
  std::string typeTextForName(const std::string& name) const;

  std::string m_currentPath;
  std::vector<Entry> m_entries;
  std::vector<std::string> m_backStack;
  std::vector<std::string> m_forwardStack;

  bool m_showHidden = false;

};
