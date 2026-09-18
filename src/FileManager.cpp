#include "FileManager.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <system_error>

namespace fs = std::filesystem;

static std::string uniqueNameIn(const std::string& dir, const std::string& name)
{
  std::error_code ec;
  if (!fs::exists(fs::path(dir) / name, ec))
  {
    return name;
  }
  size_t dot = name.find_last_of('.');
  std::string stem = name;
  std::string ext;
  if (dot != std::string::npos && dot != 0)
  {
    stem = name.substr(0, dot);
    ext = name.substr(dot);
  }
  for (int i = 2; i < 1000; i++)
  {
    std::string candidate = stem + " (" + std::to_string(i) + ")" + ext;
    if (!fs::exists(fs::path(dir) / candidate, ec))
    {
      return candidate;
    }
  }
  return std::string();
}

FileManager::FileManager()
{
  loadPath(fs::current_path().string());
}

void FileManager::resetTo(const std::string& path)
{
  m_backStack.clear();
  m_forwardStack.clear();
  loadPath(path);
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
    const fs::path& p = dirEntry.path();
    std::string name = p.filename().string();
    if (!m_showHidden && !name.empty() && name[0] == '.')
    {
      continue;
    }
    Entry entry;
    if (buildEntry(p.string(), name, entry))
    {
      m_entries.push_back(entry);
    }
  }

  sortEntries();
  return true;
}

void FileManager::setShowHidden(bool show)
{
  if (m_showHidden == show)
  {
    return;
  }
  m_showHidden = show;
  loadPath(m_currentPath);
}

bool FileManager::showHidden() const
{
  return m_showHidden;
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

bool FileManager::createDirectory(const std::string& name)
{
  if (name.empty() || name == "." || name == "..")
  {
    return false;
  }
  if (name.find('/') != std::string::npos)
  {
    return false;
  }
  std::error_code ec;
  fs::path full = fs::path(m_currentPath) / name;
  if (!fs::create_directory(full, ec))
  {
    return false;
  }
  return loadPath(m_currentPath);
}

bool FileManager::renameEntry(const std::string& oldName, const std::string& newName)
{
  if (oldName.empty() || newName.empty())
  {
    return false;
  }
  if (oldName == "." || oldName == ".." || newName == "." || newName == "..")
  {
    return false;
  }
  if (oldName.find('/') != std::string::npos || newName.find('/') != std::string::npos)
  {
    return false;
  }
  if (oldName == newName)
  {
    return true;
  }
  std::error_code ec;
  fs::path oldFull = fs::path(m_currentPath) / oldName;
  fs::path newFull = fs::path(m_currentPath) / newName;
  if (!fs::exists(oldFull, ec))
  {
    return false;
  }
  if (fs::exists(newFull, ec))
  {
    return false;
  }
  fs::rename(oldFull, newFull, ec);
  if (ec)
  {
    return false;
  }
  return loadPath(m_currentPath);
}

bool FileManager::deleteEntry(const std::string& name)
{
  if (name.empty())
  {
    return false;
  }
  if (name == "." || name == "..")
  {
    return false;
  }
  if (name.find('/') != std::string::npos)
  {
    return false;
  }
  std::error_code ec;
  fs::path full = fs::path(m_currentPath) / name;
  if (!fs::exists(full, ec))
  {
    return false;
  }
  fs::remove_all(full, ec);
  if (ec)
  {
    return false;
  }
  return loadPath(m_currentPath);
}

bool FileManager::copyEntry(const std::string& name)
{
  if (name.empty() || name == "." || name == "..")
  {
    return false;
  }
  if (name.find('/') != std::string::npos)
  {
    return false;
  }
  std::error_code ec;
  fs::path full = fs::path(m_currentPath) / name;
  if (!fs::exists(full, ec))
  {
    return false;
  }
  m_clipboardName = name;
  m_clipboardSource = m_currentPath;
  m_clipboardMode = ClipboardMode::Copy;
  return true;
}

bool FileManager::cutEntry(const std::string& name)
{
  if (name.empty() || name == "." || name == "..")
  {
    return false;
  }
  if (name.find('/') != std::string::npos)
  {
    return false;
  }
  std::error_code ec;
  fs::path full = fs::path(m_currentPath) / name;
  if (!fs::exists(full, ec))
  {
    return false;
  }
  m_clipboardName = name;
  m_clipboardSource = m_currentPath;
  m_clipboardMode = ClipboardMode::Cut;
  return true;
}

bool FileManager::paste()
{
  if (m_clipboardMode == ClipboardMode::None || m_clipboardName.empty())
  {
    return false;
  }
  std::error_code ec;
  fs::path src = fs::path(m_clipboardSource) / m_clipboardName;
  if (!fs::exists(src, ec))
  {
    m_clipboardMode = ClipboardMode::None;
    m_clipboardName.clear();
    m_clipboardSource.clear();
    return false;
  }
  fs::path dst = fs::path(m_currentPath) / m_clipboardName;
  if (m_clipboardMode == ClipboardMode::Cut)
  {
    if (src == dst)
    {
      m_clipboardMode = ClipboardMode::None;
      m_clipboardName.clear();
      m_clipboardSource.clear();
      return true;
    }
    if (fs::exists(dst, ec))
    {
      return false;
    }
  }
  else
  {
    std::string newName = uniqueNameIn(m_currentPath, m_clipboardName);
    if (newName.empty())
    {
      return false;
    }
    dst = fs::path(m_currentPath) / newName;
  }
  if (m_clipboardMode == ClipboardMode::Copy)
  {
    std::string srcStr = src.string();
    std::string curStr = m_currentPath;
    if (curStr.size() > srcStr.size() &&
        curStr.compare(0, srcStr.size(), srcStr) == 0 &&
        curStr[srcStr.size()] == '/')
    {
      return false;
    }
  }
  if (m_clipboardMode == ClipboardMode::Cut)
  {
    fs::rename(src, dst, ec);
    if (ec)
    {
      return false;
    }
    m_clipboardMode = ClipboardMode::None;
    m_clipboardName.clear();
    m_clipboardSource.clear();
  }
  else
  {
    fs::copy(src, dst, fs::copy_options::recursive, ec);
    if (ec)
    {
      return false;
    }
  }
  return loadPath(m_currentPath);
}

ClipboardMode FileManager::clipboardMode() const
{
  return m_clipboardMode;
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
    out.sizeBytes = static_cast<unsigned long long>(st.st_size);
    out.sizeText = formatSize(out.sizeBytes);
    out.typeText = typeTextForName(name);
  }
  return true;
}

void FileManager::sortEntries()
{
  const SortField field = m_sortField;
  const bool ascending = m_sortAscending;
  std::sort(m_entries.begin(), m_entries.end(),
    [field, ascending](const Entry& a, const Entry& b)
    {
      if (a.isDirectory != b.isDirectory)
      {
        return a.isDirectory;
      }
      const Entry& x = ascending ? a : b;
      const Entry& y = ascending ? b : a;
      bool less = false;
      switch (field)
      {
        case SortField::Name:        less = x.name < y.name; break;
        case SortField::Size:        less = x.sizeBytes < y.sizeBytes; break;
        case SortField::Type:        less = x.typeText < y.typeText; break;
        case SortField::Owner:       less = x.ownerText < y.ownerText; break;
        case SortField::Permissions: less = x.permText < y.permText; break;
      }
      if (less)
      {
        return true;
      }
      if (field == SortField::Name)
      {
        return false;
      }
      return x.name < y.name;
    });
}

void FileManager::setSort(SortField field, bool ascending)
{
  if (m_sortField == field && m_sortAscending == ascending)
  {
    return;
  }
  m_sortField = field;
  m_sortAscending = ascending;
  sortEntries();
}

SortField FileManager::sortField() const
{
  return m_sortField;
}

bool FileManager::sortAscending() const
{
  return m_sortAscending;
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
