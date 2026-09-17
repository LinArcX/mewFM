#include "FileManager.hpp"

#include <filesystem>
#include <system_error>

FileManager::FileManager()
{
  setPath(std::filesystem::current_path().string());
}

bool FileManager::setPath(const std::string& path)
{
  std::error_code ec;
  if (!std::filesystem::is_directory(path, ec))
  {
    return false;
  }
  m_currentPath = path;
  m_entries.clear();
  for (const auto& entry : std::filesystem::directory_iterator(path, ec))
  {
    m_entries.push_back(entry.path().filename().string());
  }
  return true;
}

const std::string& FileManager::currentPath() const
{
  return m_currentPath;
}

const std::vector<std::string>& FileManager::entries() const
{
  return m_entries;
}
