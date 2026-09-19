#pragma once

#include <mpv/client.h>
#include <string>
#include <vector>

enum class MusicPlayerState
{
  Idle,
  Playing,
  Paused,
};

class MusicPlayer
{
public:
  MusicPlayer();
  ~MusicPlayer();

  MusicPlayer(const MusicPlayer&) = delete;
  MusicPlayer& operator=(const MusicPlayer&) = delete;

  [[nodiscard]] bool init();
  void shutdown();
  void update();

  [[nodiscard]] bool playPlaylist(const std::vector<std::string>& paths, int index);
  [[nodiscard]] bool next();
  [[nodiscard]] bool previous();
  void togglePause();
  void stop();

  void setVolume(int percent);
  int volume() const;

  void setPosition(double seconds);
  double position() const;
  double duration() const;

  bool isActive() const;
  bool isPlaying() const;
  MusicPlayerState state() const;
  const std::string& currentFile() const;

private:
  [[nodiscard]] bool playIndex(int index);

  struct mpv_handle* m_pMpv = nullptr;
  MusicPlayerState m_state = MusicPlayerState::Idle;
  std::string m_currentFile;
  std::vector<std::string> m_playlist;
  int m_playlistIndex = -1;
  int m_volume = 100;
};
