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

static bool runCapture(const std::vector<std::string>& args, std::string& out)
{
  out.clear();
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

  int pipefd[2];
  if (pipe(pipefd) != 0)
  {
    sigaction(SIGCHLD, &oldAction, nullptr);
    return false;
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    close(pipefd[0]);
    close(pipefd[1]);
    sigaction(SIGCHLD, &oldAction, nullptr);
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
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (size_t i = 0; i < args.size(); i++)
    {
      argv.push_back(const_cast<char*>(args[i].c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipefd[1]);
  char buf[8192];
  while (true)
  {
    ssize_t n = read(pipefd[0], buf, sizeof(buf));
    if (n > 0)
    {
      out.append(buf, static_cast<size_t>(n));
    }
    else if (n < 0 && errno == EINTR)
    {
      continue;
    }
    else
    {
      break;
    }
  }
  close(pipefd[0]);

  int status = 0;
  pid_t waited = waitpid(pid, &status, 0);
  sigaction(SIGCHLD, &oldAction, nullptr);
  if (waited != pid)
  {
    return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
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

bool YouTubeManager::loadChannel(int index, bool forceNetwork)
{
  if (index < 0 || index >= static_cast<int>(m_channels.size()))
  {
    return false;
  }
  const std::string& url = m_channels[static_cast<size_t>(index)].url;
  const std::string cachePath = cacheFileFor(url);

  std::string data;
  bool fromCache = false;
  if (!forceNetwork && loadCacheFile(cachePath, data))
  {
    fromCache = true;
  }

  if (!fromCache)
  {
    std::vector<std::string> args;
    args.push_back("yt-dlp");
    args.push_back("--flat-playlist");
    args.push_back("--no-warnings");
    args.push_back("--ignore-errors");
    args.push_back("--print");
    args.push_back(printFormat());
    args.push_back(url);
    if (!runCapture(args, data))
    {
      return false;
    }
    saveCacheFile(cachePath, data);
  }

  std::vector<YouTubeVideo> parsed;
  std::string channelName;
  parseVideoLines(data, parsed, channelName);

  m_activeChannel = index;
  m_videos = parsed;
  if (!channelName.empty())
  {
    m_activeChannelName = channelName;
  }
  else
  {
    m_activeChannelName = m_channels[static_cast<size_t>(index)].name;
  }
  return true;
}
