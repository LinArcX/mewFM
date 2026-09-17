#include "FileManager.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <system_error>

namespace fs = std::filesystem;

FileManager::FileManager()
{
  loadPath(fs::current_path().string());
}

bool FileManager::loadPath(const std::string& path)
{
  std::error_code ec;
  if (!fs::is_directory(path, ec))
  {
    return false;
  }
  m_currentPath = path;
  m_entries.clear();
  fs::directory_iterator it(path, ec);
  if (ec)
  {
    return false;
  }
  for (const auto& dirEntry : it)
  {
    Entry entry;
    const fs::path& p = dirEntry.path();
    if (buildEntry(p.string(), p.filename().string(), entry))
    {
      m_entries.push_back(entry);
    }
  }
  sortEntries();
  return true;
}


bool FileManager::setPath(const std::string& path)
{
  if (path == m_currentPath)
  {
    return true;
  }
  std::string old = m_currentPath;
  if (!loadPath(path))
  {
    return false;
  }
  m_backStack.push_back(old);
  m_forwardStack.clear();
  return true;
}

bool FileManager::goBack()
{
  if (m_backStack.empty())
  {
    return false;
  }
  std::string target = m_backStack.back();
  m_backStack.pop_back();
  std::string old = m_currentPath;
  if (!loadPath(target))
  {
    return false;
  }
  m_forwardStack.push_back(old);
  return true;
}

bool FileManager::goForward()
{
  if (m_forwardStack.empty())
  {
    return false;
  }
  std::string target = m_forwardStack.back();
  m_forwardStack.pop_back();
  std::string old = m_currentPath;
  if (!loadPath(target))
  {
    return false;
  }
  m_backStack.push_back(old);
  return true;
}

bool FileManager::goUp()
{
  std::string parent = parentOf(m_currentPath);
  if (parent.empty() || parent == m_currentPath)
  {
    return false;
  }
  return setPath(parent);
}

bool FileManager::canGoBack() const
{
  return !m_backStack.empty();
}

bool FileManager::canGoForward() const
{
  return !m_forwardStack.empty();
}

std::string FileManager::parentOf(const std::string& path)
{
  if (path.empty() || path == "/")
  {
    return "";
  }
  std::string p = path;
  while (p.size() > 1 && p.back() == '/')
  {
    p.pop_back();
  }
  size_t slash = p.find_last_of('/');
  if (slash == std::string::npos)
  {
    return "";
  }
  if (slash == 0)
  {
    return "/";
  }
  return p.substr(0, slash);
}

const std::string& FileManager::currentPath() const
{
  return m_currentPath;
}

const std::vector<Entry>& FileManager::entries() const
{
  return m_entries;
}

bool FileManager::buildEntry(const std::string& fullPath, const std::string& name, Entry& out)
{
  struct stat st;
  if (::stat(fullPath.c_str(), &st) != 0)
  {
    return false;
  }
  out.name = name;
  out.isDirectory = S_ISDIR(st.st_mode) != 0;
  out.isExecutable = !out.isDirectory && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
  out.ownerText = ownerName(st.st_uid);
  out.permText = permissionsToString(static_cast<unsigned int>(st.st_mode));
  if (out.isDirectory)
  {
    out.sizeText = countChildren(fullPath);
    out.typeText = "Folder";
  }
  else
  {
    out.sizeText = formatSize(static_cast<unsigned long long>(st.st_size));
    out.typeText = typeTextForName(name);
  }
  return true;
}

void FileManager::sortEntries()
{
  std::sort(m_entries.begin(), m_entries.end(),
    [](const Entry& a, const Entry& b)
    {
      if (a.isDirectory != b.isDirectory)
      {
        return a.isDirectory;
      }
      return a.name < b.name;
    });
}

std::string FileManager::formatSize(unsigned long long bytes) const
{
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4)
  {
    value /= 1024.0;
    unit++;
  }
  char buf[32];
  if (unit == 0)
  {
    std::snprintf(buf, sizeof(buf), "%llu %s", bytes, units[unit]);
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
  }
  return std::string(buf);
}

std::string FileManager::countChildren(const std::string& path) const
{
  std::error_code ec;
  size_t count = 0;
  for (const auto& dirEntry : fs::directory_iterator(path, ec))
  {
    (void)dirEntry;
    count++;
  }
  if (ec)
  {
    return "-";
  }
  if (count == 1)
  {
    return "1 item";
  }
  return std::to_string(count) + " items";
}

std::string FileManager::permissionsToString(unsigned int mode) const
{
  char buf[11];
  buf[0] = S_ISDIR(mode) ? 'd' : '-';
  buf[1] = (mode & S_IRUSR) ? 'r' : '-';
  buf[2] = (mode & S_IWUSR) ? 'w' : '-';
  buf[3] = (mode & S_IXUSR) ? 'x' : '-';
  buf[4] = (mode & S_IRGRP) ? 'r' : '-';
  buf[5] = (mode & S_IWGRP) ? 'w' : '-';
  buf[6] = (mode & S_IXGRP) ? 'x' : '-';
  buf[7] = (mode & S_IROTH) ? 'r' : '-';
  buf[8] = (mode & S_IWOTH) ? 'w' : '-';
  buf[9] = (mode & S_IXOTH) ? 'x' : '-';
  buf[10] = '\0';
  return std::string(buf);
}

std::string FileManager::ownerName(unsigned int uid) const
{
  struct passwd* pw = getpwuid(uid);
  if (pw == nullptr || pw->pw_name == nullptr)
  {
    return std::to_string(uid);
  }
  return std::string(pw->pw_name);
}

std::string FileManager::typeTextForName(const std::string& name) const
{
  size_t dot = name.find_last_of('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size())
  {
    return "File";
  }
  return name.substr(dot + 1) + " file";
}
