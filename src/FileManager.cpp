#include "FileManager.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <pwd.h>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

std::string FileManager::trashRootDir()
{
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg != nullptr && xdg[0] != '\0')
  {
    return std::string(xdg) + "/Trash";
  }
  const char* home = std::getenv("HOME");
  if (home == nullptr)
  {
    return std::string();
  }
  return std::string(home) + "/.local/share/Trash";
}


static std::string currentIsoDateTime()
{
  std::time_t now = std::time(nullptr);
  struct tm tmBuf;
  if (localtime_r(&now, &tmBuf) == nullptr)
  {
    return std::string();
  }
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
  return std::string(buf);
}


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

static std::string lowercaseCopy(const std::string& s)
{
  std::string out = s;
  for (char& c : out)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

static bool endsWith(const std::string& s, const char* suffix)
{
  size_t sl = std::strlen(suffix);
  if (s.size() < sl)
  {
    return false;
  }
  return s.compare(s.size() - sl, sl, suffix) == 0;
}

static bool runCommand(const std::vector<std::string>& args)
{
  if (args.empty())
  {
    return false;
  }
  struct sigaction oldAction;
  struct sigaction newAction;
  std::memset(&newAction, 0, sizeof(newAction));
  newAction.sa_handler = SIG_DFL;
  sigemptyset(&newAction.sa_mask);
  if (sigaction(SIGCHLD, &newAction, &oldAction) != 0)
  {
    return false;
  }
  pid_t pid = fork();
  if (pid < 0)
  {
    sigaction(SIGCHLD, &oldAction, nullptr);
    return false;
  }
  if (pid == 0)
  {
    std::vector<char*> argv;
    for (size_t i = 0; i < args.size(); i++)
    {
      argv.push_back(const_cast<char*>(args[i].c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }
  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  sigaction(SIGCHLD, &oldAction, nullptr);
  if (waited != pid)
  {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool FileManager::isArchive(const std::string& name)
{
  std::string lower = lowercaseCopy(name);
  return endsWith(lower, ".tar")     ||
         endsWith(lower, ".tar.gz")  ||
         endsWith(lower, ".tgz")     ||
         endsWith(lower, ".tar.bz2") ||
         endsWith(lower, ".tbz2")    ||
         endsWith(lower, ".tar.xz")  ||
         endsWith(lower, ".txz")     ||
         endsWith(lower, ".zip");
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
  m_allEntries.clear();
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
      m_allEntries.push_back(entry);
    }
  }

  applyFilter();
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

bool FileManager::createFile(const std::string& name)
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
  if (fs::exists(full, ec))
  {
    return false;
  }
  std::ofstream out(full.string());
  if (!out)
  {
    return false;
  }
  out.close();
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

bool FileManager::copyEntries(const std::vector<std::string>& names)
{
  if (names.empty())
  {
    return false;
  }
  std::error_code ec;
  for (size_t i = 0; i < names.size(); i++)
  {
    const std::string& name = names[i];
    if (name.empty() || name == "." || name == "..")
    {
      return false;
    }
    if (name.find('/') != std::string::npos)
    {
      return false;
    }
    fs::path full = fs::path(m_currentPath) / name;
    if (!fs::exists(full, ec))
    {
      return false;
    }
  }
  m_clipboardNames = names;
  m_clipboardSource = m_currentPath;
  m_clipboardMode = ClipboardMode::Copy;
  return true;
}

bool FileManager::trashEntry(const std::string& name)
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
  fs::path src = fs::path(m_currentPath) / name;
  if (!fs::exists(src, ec))
  {
    return false;
  }
  std::string root = trashRootDir();
  if (root.empty())
  {
    return false;
  }
  fs::path filesDir = fs::path(root) / "files";
  fs::path infoDir = fs::path(root) / "info";
  fs::create_directories(filesDir, ec);
  if (ec)
  {
    return false;
  }
  fs::create_directories(infoDir, ec);
  if (ec)
  {
    return false;
  }
  std::string uniqueName = uniqueNameIn(filesDir.string(), name);
  if (uniqueName.empty())
  {
    return false;
  }
  fs::path dst = filesDir / uniqueName;
  fs::rename(src, dst, ec);
  if (ec)
  {
    ec.clear();
    fs::copy(src, dst, fs::copy_options::recursive, ec);
    if (ec)
    {
      return false;
    }
    fs::remove_all(src, ec);
    if (ec)
    {
      return false;
    }
  }
  fs::path infoDst = infoDir / (uniqueName + ".trashinfo");
  std::ofstream out(infoDst.string());
  if (out)
  {
    out << "[Trash Info]\n";
    out << "Path=" << fs::absolute(src, ec).string() << "\n";
    out << "DeletionDate=" << currentIsoDateTime() << "\n";
  }
  return loadPath(m_currentPath);
}

bool FileManager::extractArchive(const std::string& name)
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
  if (!fs::is_regular_file(full, ec))
  {
    return false;
  }
  std::string lower = lowercaseCopy(name);
  std::vector<std::string> args;
  if (endsWith(lower, ".zip"))
  {
    args.push_back("unzip");
    args.push_back("-o");
    args.push_back(full.string());
    args.push_back("-d");
    args.push_back(m_currentPath);
  }
  else if (endsWith(lower, ".tar")     ||
           endsWith(lower, ".tar.gz")  ||
           endsWith(lower, ".tgz")     ||
           endsWith(lower, ".tar.bz2") ||
           endsWith(lower, ".tbz2")    ||
           endsWith(lower, ".tar.xz")  ||
           endsWith(lower, ".txz"))
  {
    args.push_back("tar");
    args.push_back("-xf");
    args.push_back(full.string());
    args.push_back("-C");
    args.push_back(m_currentPath);
  }
  else
  {
    return false;
  }
  if (!runCommand(args))
  {
    return false;
  }
  return loadPath(m_currentPath);
}

static bool readTrashOriginalPath(const std::string& trashName, std::string& outPath)
{
  std::string root = FileManager::trashRootDir();
  if (root.empty())
  {
    return false;
  }
  fs::path infoPath = fs::path(root) / "info" / (trashName + ".trashinfo");
  std::ifstream in(infoPath.string());
  if (!in)
  {
    return false;
  }
  std::string line;
  while (std::getline(in, line))
  {
    if (line.size() >= 5 && line.compare(0, 5, "Path=") == 0)
    {
      outPath = line.substr(5);
      return !outPath.empty();
    }
  }
  return false;
}

bool FileManager::isRestoreConflict(const std::string& trashName) const
{
  if (trashName.empty() || trashName.find('/') != std::string::npos)
  {
    return false;
  }
  std::string originalPath;
  if (!readTrashOriginalPath(trashName, originalPath))
  {
    return false;
  }
  std::error_code ec;
  return fs::exists(originalPath, ec);
}

bool FileManager::restoreEntry(const std::string& trashName, bool overwrite)
{
  if (trashName.empty() || trashName.find('/') != std::string::npos)
  {
    return false;
  }
  std::string originalPath;
  if (!readTrashOriginalPath(trashName, originalPath))
  {
    return false;
  }
  std::string root = trashRootDir();
  if (root.empty())
  {
    return false;
  }
  std::error_code ec;
  fs::path src = fs::path(root) / "files" / trashName;
  if (!fs::exists(src, ec))
  {
    return false;
  }
  fs::path dst(originalPath);
  if (fs::exists(dst, ec))
  {
    if (!overwrite)
    {
      return false;
    }
    fs::remove_all(dst, ec);
    if (ec)
    {
      return false;
    }
  }
  fs::path parent = dst.parent_path();
  if (!parent.empty())
  {
    fs::create_directories(parent, ec);
    if (ec)
    {
      return false;
    }
  }
  fs::rename(src, dst, ec);
  if (ec)
  {
    return false;
  }
  fs::path infoPath = fs::path(root) / "info" / (trashName + ".trashinfo");
  fs::remove(infoPath, ec);
  return loadPath(m_currentPath);
}

bool FileManager::emptyTrash()
{
  std::string root = trashRootDir();
  if (root.empty())
  {
    return false;
  }
  std::error_code ec;
  fs::path filesDir = fs::path(root) / "files";
  fs::path infoDir = fs::path(root) / "info";
  fs::remove_all(filesDir, ec);
  ec.clear();
  fs::remove_all(infoDir, ec);
  ec.clear();
  fs::create_directories(filesDir, ec);
  if (ec)
  {
    return false;
  }
  fs::create_directories(infoDir, ec);
  if (ec)
  {
    return false;
  }
  return loadPath(m_currentPath);
}

bool FileManager::cutEntries(const std::vector<std::string>& names)
{
  if (names.empty())
  {
    return false;
  }
  std::error_code ec;
  for (size_t i = 0; i < names.size(); i++)
  {
    const std::string& name = names[i];
    if (name.empty() || name == "." || name == "..")
    {
      return false;
    }
    if (name.find('/') != std::string::npos)
    {
      return false;
    }
    fs::path full = fs::path(m_currentPath) / name;
    if (!fs::exists(full, ec))
    {
      return false;
    }
  }
  m_clipboardNames = names;
  m_clipboardSource = m_currentPath;
  m_clipboardMode = ClipboardMode::Cut;
  return true;
}

static double monotonicSeconds()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
  {
    return 0.0;
  }
  return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

static bool sameDevice(const std::string& a, const std::string& b)
{
  struct stat sa;
  struct stat sb;
  if (::stat(a.c_str(), &sa) != 0)
  {
    return false;
  }
  if (::stat(b.c_str(), &sb) != 0)
  {
    return false;
  }
  return sa.st_dev == sb.st_dev;
}

void FileManager::clearFileOp()
{
  m_opKind = FileOpKind::None;
  m_opStage = FileOpStage::None;
  m_opLabel.clear();
  m_opSrcDir.clear();
  m_opDstDir.clear();
  m_opTopNames.clear();
  m_opFiles.clear();
  m_opDirs.clear();
  m_opBuffer.clear();
  m_opDirIndex = 0;
  m_opFileIndex = 0;
  m_opDeleteIndex = 0;
  m_opRenameIndex = 0;
  m_opTotalBytes = 0;
  m_opDoneBytes = 0;
  m_opSrcFd = -1;
  m_opDstFd = -1;
  m_opFailed = false;
  m_opPaused = false;
  m_opProgress = FileOpProgress();
}

void FileManager::closeFileOpFds()
{
  if (m_opSrcFd >= 0)
  {
    ::close(m_opSrcFd);
    m_opSrcFd = -1;
  }
  if (m_opDstFd >= 0)
  {
    ::close(m_opDstFd);
    m_opDstFd = -1;
  }
}

bool FileManager::collectDirPlan(const std::string& srcRoot, const std::string& dstRoot)
{
  struct DirPair
  {
    fs::path src;
    fs::path dst;
  };
  std::vector<DirPair> stack;
  DirPair root;
  root.src = fs::path(srcRoot);
  root.dst = fs::path(dstRoot);
  stack.push_back(root);
  m_opDirs.push_back(dstRoot);

  std::error_code ec;
  while (!stack.empty())
  {
    DirPair cur = stack.back();
    stack.pop_back();
    fs::directory_iterator it(cur.src, ec);
    if (ec)
    {
      return false;
    }
    for (const auto& entry : it)
    {
      const fs::path& p = entry.path();
      std::string name = p.filename().string();
      fs::path d = cur.dst / name;
      if (fs::is_directory(p, ec))
      {
        m_opDirs.push_back(d.string());
        DirPair sub;
        sub.src = p;
        sub.dst = d;
        stack.push_back(sub);
      }
      else if (fs::is_regular_file(p, ec))
      {
        std::uintmax_t sz = fs::file_size(p, ec);
        if (ec)
        {
          ec.clear();
          sz = 0;
        }
        FileOpFile f;
        f.src = p.string();
        f.dst = d.string();
        f.size = static_cast<unsigned long long>(sz);
        m_opFiles.push_back(f);
        m_opTotalBytes += f.size;
      }
      ec.clear();
    }
  }
  return true;
}

bool FileManager::buildFileOpPlan(const std::string& srcDir,
                                  const std::string& dstDir,
                                  const std::vector<std::string>& names,
                                  bool moveMode)
{
  std::error_code ec;
  for (size_t i = 0; i < names.size(); i++)
  {
    const std::string& name = names[i];
    if (name.empty() || name == "." || name == ".." ||
        name.find('/') != std::string::npos)
    {
      return false;
    }
    fs::path src = fs::path(srcDir) / name;
    if (!fs::exists(src, ec))
    {
      return false;
    }

    std::string dstName = name;
    if (!moveMode)
    {
      dstName = uniqueNameIn(dstDir, name);
      if (dstName.empty())
      {
        return false;
      }
      std::string srcStr = src.string();
      if (dstDir.size() > srcStr.size() &&
          dstDir.compare(0, srcStr.size(), srcStr) == 0 &&
          dstDir[srcStr.size()] == '/')
      {
        return false;
      }
    }
    fs::path dst = fs::path(dstDir) / dstName;

    if (fs::is_directory(src, ec))
    {
      if (!collectDirPlan(src.string(), dst.string()))
      {
        return false;
      }
    }
    else if (fs::is_regular_file(src, ec))
    {
      std::uintmax_t sz = fs::file_size(src, ec);
      if (ec)
      {
        ec.clear();
        sz = 0;
      }
      FileOpFile f;
      f.src = src.string();
      f.dst = dst.string();
      f.size = static_cast<unsigned long long>(sz);
      m_opFiles.push_back(f);
      m_opTotalBytes += f.size;
    }
    ec.clear();
  }
  return true;
}

bool FileManager::startPaste()
{
   fprintf(stderr, "[startPaste] mode=%d clipSrc='%s' cur='%s' names=%zu active=%d\n",
          static_cast<int>(m_clipboardMode), m_clipboardSource.c_str(),
          m_currentPath.c_str(), m_clipboardNames.size(),
          m_opProgress.active ? 1 : 0);

  if (m_clipboardMode == ClipboardMode::None || m_clipboardNames.empty())
  {
    return false;
  }
  if (m_opProgress.active)
  {
    return false;
  }

  const bool isMove = (m_clipboardMode == ClipboardMode::Cut);
  if (isMove && m_clipboardSource == m_currentPath)
  {
    return false;
  }

  clearFileOp();

  const std::string srcDir = m_clipboardSource;
  const std::string dstDir = m_currentPath;

  m_opSrcDir = srcDir;
  m_opDstDir = dstDir;
  m_opTopNames = m_clipboardNames;
  m_opBuffer.resize(64 * 1024);

  if (isMove && sameDevice(srcDir, dstDir))
  {
    m_opKind = FileOpKind::Move;
    m_opStage = FileOpStage::RenameTopLevel;
    m_opTotalBytes = 1;
  }
  else
  {
    m_opKind = isMove ? FileOpKind::Move : FileOpKind::Copy;
    m_opStage = FileOpStage::MakeDirs;
    if (!buildFileOpPlan(srcDir, dstDir, m_clipboardNames, isMove))
    {
      clearFileOp();
      return false;
    }
  }

  m_opLabel = std::string(isMove ? "Moving " : "Copying ") +
              std::to_string(m_clipboardNames.size()) +
              (m_clipboardNames.size() == 1 ? " item" : " items");

  m_opProgress.active = true;
  m_opProgress.failed = false;
  m_opProgress.label = m_opLabel;
  m_opProgress.totalBytes = m_opTotalBytes;
  m_opProgress.doneBytes = 0;
  return true;
}

bool FileManager::stepCopyChunk()
{
  if (m_opSrcFd < 0)
  {
    if (m_opFileIndex >= m_opFiles.size())
    {
      return true;
    }
    const FileOpFile& f = m_opFiles[m_opFileIndex];
    int sfd = ::open(f.src.c_str(), O_RDONLY);
    if (sfd < 0)
    {
      return false;
    }
    int dfd = ::open(f.dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dfd < 0)
    {
      ::close(sfd);
      return false;
    }
    m_opSrcFd = sfd;
    m_opDstFd = dfd;
  }

  ssize_t got = ::read(m_opSrcFd, m_opBuffer.data(), m_opBuffer.size());
  if (got < 0)
  {
    closeFileOpFds();
    return false;
  }
  if (got == 0)
  {
    closeFileOpFds();
    m_opFileIndex++;
    return true;
  }

  ssize_t sent = 0;
  while (sent < got)
  {
    ssize_t w = ::write(m_opDstFd,
                        m_opBuffer.data() + sent,
                        static_cast<size_t>(got - sent));
    if (w < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }
      closeFileOpFds();
      return false;
    }
    sent += w;
  }
  m_opDoneBytes += static_cast<unsigned long long>(got);
  return true;
}

bool FileManager::stepFileOp(double budgetSeconds)
{
  const double startTime = monotonicSeconds();
  while (true)
  {
    if (monotonicSeconds() - startTime >= budgetSeconds)
    {
      return false;
    }

    switch (m_opStage)
    {
      case FileOpStage::None:
        return true;

      case FileOpStage::MakeDirs:
      {
        if (m_opDirIndex >= m_opDirs.size())
        {
          m_opStage = FileOpStage::CopyFiles;
          break;
        }
        std::error_code ec;
        fs::create_directories(m_opDirs[m_opDirIndex], ec);
        if (ec)
        {
          m_opFailed = true;
          return true;
        }
        m_opDirIndex++;
        break;
      }

      case FileOpStage::CopyFiles:
      {
        if (m_opSrcFd < 0 && m_opFileIndex >= m_opFiles.size())
        {
          m_opDoneBytes = m_opTotalBytes;
          m_opStage = (m_opKind == FileOpKind::Move)
            ? FileOpStage::DeleteSources
            : FileOpStage::None;
          break;
        }
        if (!stepCopyChunk())
        {
          m_opFailed = true;
          return true;
        }
        break;
      }

      case FileOpStage::DeleteSources:
      {
        if (m_opDeleteIndex >= m_opTopNames.size())
        {
          m_opStage = FileOpStage::None;
          break;
        }
        std::error_code ec;
        fs::path src = fs::path(m_opSrcDir) / m_opTopNames[m_opDeleteIndex];
        fs::remove_all(src, ec);
        m_opDeleteIndex++;
        break;
      }

      case FileOpStage::RenameTopLevel:
      {
        if (m_opRenameIndex >= m_opTopNames.size())
        {
          m_opDoneBytes = m_opTotalBytes;
          m_opStage = FileOpStage::None;
          break;
        }
        std::error_code ec;
        fs::path src = fs::path(m_opSrcDir) / m_opTopNames[m_opRenameIndex];
        fs::path dst = fs::path(m_opDstDir) / m_opTopNames[m_opRenameIndex];
        m_opRenameIndex++;
        if (src == dst)
        {
          break;
        }
        if (fs::exists(dst, ec))
        {
          m_opFailed = true;
          return true;
        }
        fs::rename(src, dst, ec);
        if (ec)
        {
          m_opFailed = true;
          return true;
        }
        break;
      }
    }
  }
}

void FileManager::finalizeFileOp(bool success)
{
  closeFileOpFds();
  m_opProgress.active = false;
  m_opProgress.failed = !success;
  m_opProgress.paused = false;
  m_opProgress.doneBytes = m_opDoneBytes;
  m_opProgress.totalBytes = m_opTotalBytes;
  m_opStage = FileOpStage::None;
  m_opKind = FileOpKind::None;
  m_opFailed = !success;
  m_opPaused = false;

  if (success && m_clipboardMode == ClipboardMode::Cut)
  {
    m_clipboardMode = ClipboardMode::None;
    m_clipboardNames.clear();
    m_clipboardSource.clear();
  }

  loadPath(m_currentPath);
}

FileOpStatus FileManager::pollFileOp()
{
      FILE* f = std::fopen("/tmp/mewFM.log", "a");
    if (f != nullptr)
    {
      std::fprintf(f, "poll active=%d stage=%d kind=%d fileIdx=%zu done=%llu total=%llu\n",
                   m_opProgress.active ? 1 : 0,
                   static_cast<int>(m_opStage),
                   static_cast<int>(m_opKind),
                   m_opFileIndex,
                   m_opDoneBytes,
                   m_opTotalBytes);
      std::fclose(f);
    }

  if (!m_opProgress.active)
  {
    return FileOpStatus::Idle;
  }

  if (m_opPaused)
  {
    m_opProgress.paused = true;
    return FileOpStatus::Running;
  }

  const double kBudget = 0.008;
  const bool done = stepFileOp(kBudget);

  m_opProgress.doneBytes = m_opDoneBytes;
  m_opProgress.totalBytes = m_opTotalBytes;

  if (!done)
  {
    return FileOpStatus::Running;
  }

  const bool success = !m_opFailed;
  const FileOpKind finishedKind = m_opKind;
  finalizeFileOp(success);

  if (!success)
  {
    return FileOpStatus::Failed;
  }
  return (finishedKind == FileOpKind::Move)
    ? FileOpStatus::FinishedMove
    : FileOpStatus::FinishedCopy;
}

const FileOpProgress& FileManager::fileOpProgress() const
{
  return m_opProgress;
}

void FileManager::setFileOpPaused(bool paused)
{
  if (!m_opProgress.active)
  {
    return;
  }
  if (m_opPaused == paused)
  {
    return;
  }
  m_opPaused = paused;
  m_opProgress.paused = paused;
}

bool FileManager::fileOpPaused() const
{
  return m_opPaused;
}

ClipboardMode FileManager::clipboardMode() const
{
  return m_clipboardMode;
}

bool FileManager::refresh()
{
  return loadPath(m_currentPath);
}

void FileManager::setFilter(const std::string& filter)
{
  if (m_filter == filter)
  {
    return;
  }
  m_filter = filter;
  applyFilter();
}


const std::string& FileManager::filter() const
{
  return m_filter;
}


void FileManager::applyFilter()
{
  m_entries.clear();
  for (size_t i = 0; i < m_allEntries.size(); i++)
  {
    if (m_filter.empty() || m_allEntries[i].name.find(m_filter) != std::string::npos)
    {
      m_entries.push_back(m_allEntries[i]);
    }
  }
  sortEntries();
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
      int cmp = 0;
      switch (field)
      {
        case SortField::Name:
        {
          if (a.name < b.name) cmp = -1;
          else if (b.name < a.name) cmp = 1;
          break;
        }
        case SortField::Size:
        {
          if (a.sizeBytes < b.sizeBytes) cmp = -1;
          else if (b.sizeBytes < a.sizeBytes) cmp = 1;
          break;
        }
        case SortField::Type:
        {
          if (a.typeText < b.typeText) cmp = -1;
          else if (b.typeText < a.typeText) cmp = 1;
          break;
        }
        case SortField::Owner:
        {
          if (a.ownerText < b.ownerText) cmp = -1;
          else if (b.ownerText < a.ownerText) cmp = 1;
          break;
        }
        case SortField::Permissions:
        {
          if (a.permText < b.permText) cmp = -1;
          else if (b.permText < a.permText) cmp = 1;
          break;
        }
      }
      if (cmp == 0 && field != SortField::Name)
      {
        if (a.name < b.name) cmp = -1;
        else if (b.name < a.name) cmp = 1;
      }
      return ascending ? (cmp < 0) : (cmp > 0);
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
