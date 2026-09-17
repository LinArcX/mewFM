#pragma once

#include <string>
#include <vector>

class FileManager
{
public:
  FileManager();
  bool setPath(const std::string& path);
  const std::string& currentPath() const;
  const std::vector<std::string>& entries() const;

private:
  std::string m_currentPath;
  std::vector<std::string> m_entries;
};
