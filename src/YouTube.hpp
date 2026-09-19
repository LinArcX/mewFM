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

  [[nodiscard]] bool loadChannel(int index, bool forceNetwork);

  static std::string cacheDir();
  static std::string cacheFileFor(const std::string& url);

private:
  std::vector<YouTubeChannel> m_channels;
  int m_activeChannel = -1;
  std::string m_activeChannelName;
  std::vector<YouTubeVideo> m_videos;
};
