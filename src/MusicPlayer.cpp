#include "MusicPlayer.hpp"

#include <cstdio>

MusicPlayer::MusicPlayer()
{
}

MusicPlayer::~MusicPlayer()
{
  shutdown();
}

bool MusicPlayer::init()
{
  if (m_pMpv != nullptr)
  {
    return true;
  }
  m_pMpv = mpv_create();
  if (m_pMpv == nullptr)
  {
    return false;
  }
  mpv_set_option_string(m_pMpv, "vid", "no");
  mpv_set_option_string(m_pMpv, "audio-display", "no");
  mpv_set_option_string(m_pMpv, "idle", "yes");
  mpv_set_option_string(m_pMpv, "input-default-bindings", "no");
  mpv_set_option_string(m_pMpv, "input-vo-keyboard", "no");
  mpv_set_option_string(m_pMpv, "osc", "no");
  mpv_set_option_string(m_pMpv, "terminal", "no");
  mpv_set_option_string(m_pMpv, "msg-level", "all=no");
  if (mpv_initialize(m_pMpv) < 0)
  {
    mpv_terminate_destroy(m_pMpv);
    m_pMpv = nullptr;
    return false;
  }
  setVolume(m_volume);
  return true;
}

void MusicPlayer::shutdown()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  mpv_terminate_destroy(m_pMpv);
  m_pMpv = nullptr;
  m_state = MusicPlayerState::Idle;
  m_currentFile.clear();
  m_playlist.clear();
  m_playlistIndex = -1;
}

void MusicPlayer::update()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  while (true)
  {
    mpv_event* ev = mpv_wait_event(m_pMpv, 0);
    if (ev == nullptr || ev->event_id == MPV_EVENT_NONE)
    {
      break;
    }
    if (ev->event_id == MPV_EVENT_END_FILE)
    {
      m_state = MusicPlayerState::Idle;
    }
    else if (ev->event_id == MPV_EVENT_SHUTDOWN)
    {
      m_state = MusicPlayerState::Idle;
    }
  }
}

bool MusicPlayer::playPlaylist(const std::vector<std::string>& paths, int index)
{
  if (m_pMpv == nullptr)
  {
    return false;
  }
  if (paths.empty() || index < 0 || index >= static_cast<int>(paths.size()))
  {
    return false;
  }
  m_playlist = paths;
  return playIndex(index);
}

bool MusicPlayer::playIndex(int index)
{
  if (m_pMpv == nullptr)
  {
    return false;
  }
  if (index < 0 || index >= static_cast<int>(m_playlist.size()))
  {
    return false;
  }
  const std::string& path = m_playlist[static_cast<size_t>(index)];
  const char* cmd[] = {"loadfile", path.c_str(), "replace", nullptr};
  if (mpv_command(m_pMpv, cmd) < 0)
  {
    return false;
  }
  m_playlistIndex = index;
  m_currentFile = path;
  m_state = MusicPlayerState::Playing;
  return true;
}

bool MusicPlayer::next()
{
  if (m_playlist.empty())
  {
    return false;
  }
  const int n = static_cast<int>(m_playlist.size());
  const int idx = (m_playlistIndex + 1 + n) % n;
  return playIndex(idx);
}

bool MusicPlayer::previous()
{
  if (m_playlist.empty())
  {
    return false;
  }
  const int n = static_cast<int>(m_playlist.size());
  const int idx = (m_playlistIndex - 1 + n) % n;
  return playIndex(idx);
}

void MusicPlayer::togglePause()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  if (m_state == MusicPlayerState::Idle)
  {
    return;
  }
  int paused = (m_state == MusicPlayerState::Paused) ? 0 : 1;
  mpv_set_property(m_pMpv, "pause", MPV_FORMAT_FLAG, &paused);
  m_state = (paused == 1) ? MusicPlayerState::Paused : MusicPlayerState::Playing;
}

void MusicPlayer::stop()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  const char* cmd[] = {"stop", nullptr};
  mpv_command(m_pMpv, cmd);
  m_state = MusicPlayerState::Idle;
  m_playlistIndex = -1;
}

void MusicPlayer::setVolume(int percent)
{
  if (percent < 0)
  {
    percent = 0;
  }
  if (percent > 100)
  {
    percent = 100;
  }
  m_volume = percent;
  if (m_pMpv == nullptr)
  {
    return;
  }
  double v = static_cast<double>(percent);
  mpv_set_property(m_pMpv, "volume", MPV_FORMAT_DOUBLE, &v);
}

int MusicPlayer::volume() const
{
  return m_volume;
}

void MusicPlayer::setPosition(double seconds)
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  if (seconds < 0.0)
  {
    seconds = 0.0;
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.3f", seconds);
  const char* cmd[] = {"seek", buf, "absolute", nullptr};
  mpv_command(m_pMpv, cmd);
}

double MusicPlayer::position() const
{
  if (m_pMpv == nullptr)
  {
    return 0.0;
  }
  double p = 0.0;
  if (mpv_get_property(m_pMpv, "time-pos", MPV_FORMAT_DOUBLE, &p) < 0)
  {
    return 0.0;
  }
  return p;
}

double MusicPlayer::duration() const
{
  if (m_pMpv == nullptr)
  {
    return 0.0;
  }
  double d = 0.0;
  if (mpv_get_property(m_pMpv, "duration", MPV_FORMAT_DOUBLE, &d) < 0)
  {
    return 0.0;
  }
  return d;
}

bool MusicPlayer::isActive() const
{
  return m_state != MusicPlayerState::Idle;
}

bool MusicPlayer::isPlaying() const
{
  return m_state == MusicPlayerState::Playing;
}

MusicPlayerState MusicPlayer::state() const
{
  return m_state;
}

const std::string& MusicPlayer::currentFile() const
{
  return m_currentFile;
}
