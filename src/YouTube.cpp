#include "YouTube.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;

static constexpr int kCacheMaxAgeSeconds = 15 * 60;

void YouTubeManager::setChannels(const std::vector<YouTubeChannel>& channels)
{
  m_channels = channels;
}

const std::vector<YouTubeChannel>& YouTubeManager::channels() const
{
  return m_channels;
}

void YouTubeManager::addChannel(const YouTubeChannel& channel)
{
  if (channel.url.empty())
  {
    return;
  }
  for (size_t i = 0; i < m_channels.size(); i++)
  {
    if (m_channels[i].url == channel.url)
    {
      return;
    }
  }
  m_channels.push_back(channel);
}

void YouTubeManager::setChannelName(int index, const std::string& name)
{
  if (index < 0 || index >= static_cast<int>(m_channels.size()) || name.empty())
  {
    return;
  }
  m_channels[static_cast<size_t>(index)].name = name;
}

void YouTubeManager::clearChannels()
{
  m_channels.clear();
  m_activeChannel = -1;
  m_activeChannelName.clear();
  m_videos.clear();
}

int YouTubeManager::activeChannel() const
{
  return m_activeChannel;
}

const std::string& YouTubeManager::activeChannelName() const
{
  return m_activeChannelName;
}

const std::vector<YouTubeVideo>& YouTubeManager::videos() const
{
  return m_videos;
}

std::string YouTubeManager::cacheDir()
{
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  std::string base;
  if (xdg != nullptr && xdg[0] != '\0')
  {
    base = xdg;
  }
  else
  {
    const char* home = std::getenv("HOME");
    base = (home != nullptr) ? std::string(home) + "/.cache" : std::string("/tmp");
  }
  return base + "/rah/youtube";
}

static unsigned long long fnv1a(const std::string& s)
{
  unsigned long long h = 1469598103934665603ULL;
  for (size_t i = 0; i < s.size(); i++)
  {
    h ^= static_cast<unsigned char>(s[i]);
    h *= 1099511628211ULL;
  }
  return h;
}

std::string YouTubeManager::cacheFileFor(const std::string& url)
{
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%016llx", fnv1a(url));
  return cacheDir() + "/" + std::string(buf) + ".tsv";
}

static bool loadCacheFile(const std::string& path, std::string& out)
{
  struct stat st;
  if (::stat(path.c_str(), &st) != 0)
  {
    return false;
  }
  double age = difftime(std::time(nullptr), st.st_mtime);
  if (age < 0.0 || age >= static_cast<double>(kCacheMaxAgeSeconds))
  {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return !out.empty();
}

static void saveCacheFile(const std::string& path, const std::string& data)
{
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
  {
    return;
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

static void parseVideoLines(const std::string& data,
                            std::vector<YouTubeVideo>& videos,
                            std::string& channelName)
{
  videos.clear();
  channelName.clear();
  size_t i = 0;
  while (i < data.size())
  {
    size_t nl = data.find('\n', i);
    if (nl == std::string::npos)
    {
      nl = data.size();
    }
    std::string line = data.substr(i, nl - i);
    i = nl + 1;
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    if (line.empty())
    {
      continue;
    }
    std::vector<std::string> parts;
    size_t start = 0;
    while (true)
    {
      size_t tab = line.find('\t', start);
      if (tab == std::string::npos)
      {
        parts.push_back(line.substr(start));
        break;
      }
      parts.push_back(line.substr(start, tab - start));
      start = tab + 1;
    }
    if (parts.size() < 3)
    {
      continue;
    }
    YouTubeVideo v;
    v.id = parts[0];
    v.title = parts[1];
    v.url = parts[2];
    if (!v.id.empty())
    {
      v.url = "https://www.youtube.com/watch?v=" + v.id;
    }
    v.duration = -1;
    if (parts.size() >= 4 && parts[3] != "NA" && !parts[3].empty())
    {
      v.duration = std::atoi(parts[3].c_str());
    }
    if (parts.size() >= 5 && parts[4] != "NA")
    {
      v.uploadDate = parts[4];
    }
    if (parts.size() >= 6 && parts[5] != "NA" && !parts[5].empty() && channelName.empty())
    {
      channelName = parts[5];
    }
    if (!v.url.empty() && !v.title.empty())
    {
      videos.push_back(v);
    }
  }
}

static const char* printFormat()
{
  return "%(id)s\t%(title)s\t%(url)s\t%(duration)s\t%(upload_date)s\t%(channel)s";
}

bool YouTubeManager::finishLoadFromData(const std::string& data, int index)
{
  std::vector<YouTubeVideo> parsed;
  std::string channelName;
  parseVideoLines(data, parsed, channelName);
  if (parsed.empty())
  {
    if (m_loadIsAppend)
    {
      m_hasMore = false;
      return true;
    }
    return false;
  }
  m_activeChannel = index;
  if (m_loadIsAppend)
  {
    m_videos.insert(m_videos.end(), parsed.begin(), parsed.end());
  }
  else
  {
    m_videos = parsed;
    if (!channelName.empty())
    {
      m_activeChannelName = channelName;
      if (index >= 0 && index < static_cast<int>(m_channels.size()))
      {
        m_channels[static_cast<size_t>(index)].name = channelName;
      }
    }
    else if (index >= 0 && index < static_cast<int>(m_channels.size()))
    {
      m_activeChannelName = m_channels[static_cast<size_t>(index)].name;
    }
  }
  m_loadedCount = static_cast<int>(m_videos.size());
  m_hasMore = (static_cast<int>(parsed.size()) >= 10);
  return true;
}

void YouTubeManager::cancelLoad()
{
  if (m_loadFd >= 0)
  {
    close(m_loadFd);
    m_loadFd = -1;
  }
  m_loadPid = -1;
  m_loadChannelIndex = -1;
  m_loadBuffer.clear();
  m_loadCachePath.clear();
  m_loadStatus = YouTubeLoadStatus::Idle;
  m_loadIsAppend = false;
}

bool YouTubeManager::loadMore()
{
  if (m_loadStatus == YouTubeLoadStatus::Loading)
  {
    return false;
  }
  if (!m_hasMore)
  {
    return false;
  }
  if (m_activeChannel < 0 ||
      m_activeChannel >= static_cast<int>(m_channels.size()))
  {
    return false;
  }
  if (m_loadFd >= 0)
  {
    return false;
  }
  const std::string& url = m_channels[static_cast<size_t>(m_activeChannel)].url;
  m_loadChannelIndex = m_activeChannel;
  m_loadCachePath.clear();
  m_loadDisplayName = m_channels[static_cast<size_t>(m_activeChannel)].name;

  int pipefd[2];
  if (pipe(pipefd) != 0)
  {
    m_loadStatus = YouTubeLoadStatus::Failed;
    return false;
  }

  int flags = fcntl(pipefd[0], F_GETFL, 0);
  if (flags >= 0)
  {
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
  }

  char startBuf[16];
  char endBuf[16];
  std::snprintf(startBuf, sizeof(startBuf), "%d", m_loadedCount + 1);
  std::snprintf(endBuf, sizeof(endBuf), "%d", m_loadedCount + 10);

  pid_t pid = fork();
  if (pid < 0)
  {
    close(pipefd[0]);
    close(pipefd[1]);
    m_loadStatus = YouTubeLoadStatus::Failed;
    return false;
  }

  if (pid == 0)
  {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0)
    {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    const char* argv[] = {
      "yt-dlp",
      "--flat-playlist",
      "--no-warnings",
      "--ignore-errors",
      "--playlist-start",
      startBuf,
      "--playlist-end",
      endBuf,
      "--print",
      printFormat(),
      url.c_str(),
      nullptr,
    };
    execvp("yt-dlp", const_cast<char* const*>(argv));
    _exit(127);
  }

  close(pipefd[1]);
  m_loadFd = pipefd[0];
  m_loadPid = pid;
  m_loadBuffer.clear();
  m_loadStatus = YouTubeLoadStatus::Loading;
  m_loadIsAppend = true;
  return true;
}

bool YouTubeManager::startLoadChannel(int index, bool forceNetwork)
{
  if (index < 0 || index >= static_cast<int>(m_channels.size()))
  {
    return false;
  }
  if (m_loadStatus == YouTubeLoadStatus::Loading)
  {
    cancelLoad();
  }

  const std::string& url = m_channels[static_cast<size_t>(index)].url;
  const std::string cachePath = cacheFileFor(url);
  m_loadChannelIndex = index;
  m_loadCachePath = cachePath;
  m_loadDisplayName = m_channels[static_cast<size_t>(index)].name;
  m_loadedCount = 0;
  m_hasMore = true;
  m_loadIsAppend = false;

  if (!forceNetwork)
  {
    std::string data;
    if (loadCacheFile(cachePath, data))
    {
      if (finishLoadFromData(data, index))
      {
        m_loadStatus = YouTubeLoadStatus::Done;
      }
      else
      {
        m_loadStatus = YouTubeLoadStatus::Failed;
      }
      return m_loadStatus == YouTubeLoadStatus::Done;
    }
  }

  int pipefd[2];
  if (pipe(pipefd) != 0)
  {
    m_loadStatus = YouTubeLoadStatus::Failed;
    return false;
  }

  int flags = fcntl(pipefd[0], F_GETFL, 0);
  if (flags >= 0)
  {
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    close(pipefd[0]);
    close(pipefd[1]);
    m_loadStatus = YouTubeLoadStatus::Failed;
    return false;
  }

  if (pid == 0)
  {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0)
    {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    const char* argv[] = {
      "yt-dlp",
      "--flat-playlist",
      "--no-warnings",
      "--ignore-errors",
      "--playlist-start",
      "1",
      "--playlist-end",
      "10",
      "--print",
      printFormat(),
      url.c_str(),
      nullptr,
    };
    execvp("yt-dlp", const_cast<char* const*>(argv));
    _exit(127);
  }

  close(pipefd[1]);
  m_loadFd = pipefd[0];
  m_loadPid = pid;
  m_loadBuffer.clear();
  m_loadStatus = YouTubeLoadStatus::Loading;
  return true;
}

YouTubeLoadStatus YouTubeManager::pollLoad()
{
  if (m_loadStatus != YouTubeLoadStatus::Loading)
  {
    return m_loadStatus;
  }
  if (m_loadFd < 0)
  {
    m_loadStatus = YouTubeLoadStatus::Failed;
    return m_loadStatus;
  }

  bool eof = false;
  char buf[16384];
  while (true)
  {
    ssize_t n = read(m_loadFd, buf, sizeof(buf));
    if (n > 0)
    {
      m_loadBuffer.append(buf, static_cast<size_t>(n));
    }
    else if (n == 0)
    {
      eof = true;
      break;
    }
    else if (errno == EINTR)
    {
      continue;
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      break;
    }
    else
    {
      eof = true;
      break;
    }
  }

  if (!eof)
  {
    return YouTubeLoadStatus::Loading;
  }

  close(m_loadFd);
  m_loadFd = -1;
  m_loadPid = -1;

  if (m_loadBuffer.empty())
  {
    m_loadBuffer.clear();
    m_loadStatus = YouTubeLoadStatus::Failed;
    return m_loadStatus;
  }

  saveCacheFile(m_loadCachePath, m_loadBuffer);

  const int idx = m_loadChannelIndex;
  const bool ok = (idx >= 0 && idx < static_cast<int>(m_channels.size()))
    ? finishLoadFromData(m_loadBuffer, idx)
    : false;
  m_loadBuffer.clear();
  m_loadStatus = ok ? YouTubeLoadStatus::Done : YouTubeLoadStatus::Failed;
  return m_loadStatus;
}

YouTubeLoadStatus YouTubeManager::loadStatus() const
{
  return m_loadStatus;
}

const std::string& YouTubeManager::loadingName() const
{
  return m_loadDisplayName;
}
