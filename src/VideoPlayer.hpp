#pragma once

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <string>
#include <sys/types.h>

enum class VideoPlayerState
{
  Idle,
  Loading,
  Playing,
  Paused,
  Ended,
  Failed,
};

class VideoPlayer
{
public:
  VideoPlayer();
  ~VideoPlayer();

  VideoPlayer(const VideoPlayer&) = delete;
  VideoPlayer& operator=(const VideoPlayer&) = delete;

  [[nodiscard]] bool init();
  void shutdown();

  [[nodiscard]] bool open(const std::string& path);
  void close();
  void update();

  void togglePause();
  void stop();
  [[nodiscard]] bool play();
  void setVolume(int percent);
  int volume() const;
  void seekAbsolute(double seconds);
  void setSubtitleFont(const std::string& font);
  void setSubtitleFontSize(int size);
  const std::string& subtitleFont() const;
  int subtitleFontSize() const;

  double position() const;
  double duration() const;

  bool isActive() const;
  bool isPlaying() const;
  VideoPlayerState state() const;
  const std::string& currentFile() const;

  bool needsRender() const;
  void render(unsigned int fbo, int width, int height);
  void reportSwap();

private:
  static void onRenderUpdate(void* ctx);
  void cancelResolve();
  [[nodiscard]] bool startResolveYoutube(const std::string& ytUrl);
  void pollResolve();
  [[nodiscard]] bool loadResolved(const std::string& videoUrl, const std::string& audioUrl);

  struct mpv_handle* m_pMpv = nullptr;
  struct mpv_render_context* m_pRender = nullptr;
  VideoPlayerState m_state = VideoPlayerState::Idle;
  std::string m_currentFile;
  int m_volume = 100;
  bool m_renderUpdate = true;
  std::string m_subFont;
  int m_subFontSize = 0;

  pid_t m_resolvePid = -1;
  int m_resolveFd = -1;
  std::string m_resolveBuffer;
};
