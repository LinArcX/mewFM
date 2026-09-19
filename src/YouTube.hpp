#pragma once

#include <string>
#include <vector>

struct YouTubeChannel
{
  std::string name;
  std::string url;
};

struct YouTubeVideo
{
  std::string id;
  std::string title;
  std::string url;
  int duration = -1;
  std::string uploadDate;
};

enum class YouTubeLoadStatus
{
  Idle,
  Loading,
  Done,
  Failed,
};

class YouTubeManager
{
public:
  void setChannels(const std::vector<YouTubeChannel>& channels);
  const std::vector<YouTubeChannel>& channels() const;
  void addChannel(const YouTubeChannel& channel);
  void setChannelName(int index, const std::string& name);
  void clearChannels();

  int activeChannel() const;
  const std::string& activeChannelName() const;
  const std::vector<YouTubeVideo>& videos() const;

  [[nodiscard]] bool startLoadChannel(int index, bool forceNetwork);
  YouTubeLoadStatus pollLoad();
  void cancelLoad();
  YouTubeLoadStatus loadStatus() const;
  const std::string& loadingName() const;

  static std::string cacheDir();
  static std::string cacheFileFor(const std::string& url);

private:
  bool finishLoadFromData(const std::string& data, int index);

  std::vector<YouTubeChannel> m_channels;
  int m_activeChannel = -1;
  std::string m_activeChannelName;
  std::vector<YouTubeVideo> m_videos;

  YouTubeLoadStatus m_loadStatus = YouTubeLoadStatus::Idle;
  int m_loadPid = -1;
  int m_loadFd = -1;
  int m_loadChannelIndex = -1;
  std::string m_loadBuffer;
  std::string m_loadCachePath;
  std::string m_loadDisplayName;
};
