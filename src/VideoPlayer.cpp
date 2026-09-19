#include "VideoPlayer.hpp"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static void* getGlProcAddress(void* ctx, const char* name)
{
  (void)ctx;
  return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

static bool isYoutubeUrl(const std::string& url)
{
  return url.find("youtube.com/") != std::string::npos ||
         url.find("youtu.be/") != std::string::npos;
}

static bool resolveYoutubeUrl(const std::string& ytUrl,
                              std::string& videoUrl,
                              std::string& audioUrl)
{
  videoUrl.clear();
  audioUrl.clear();

  int pipefd[2];
  if (pipe(pipefd) != 0)
  {
    return false;
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    close(pipefd[0]);
    close(pipefd[1]);
    return false;
  }

  if (pid == 0)
  {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    const char* argv[] = {
      "yt-dlp",
      "-f", "bv*+ba/b",
      "--no-warnings",
      "--get-url",
      ytUrl.c_str(),
      nullptr,
    };
    execvp("yt-dlp", const_cast<char* const*>(argv));
    _exit(127);
  }

  close(pipefd[1]);
  std::string output;
  char buf[4096];
  while (true)
  {
    ssize_t n = read(pipefd[0], buf, sizeof(buf));
    if (n > 0)
    {
      output.append(buf, static_cast<size_t>(n));
    }
    else if (n == 0)
    {
      break;
    }
    else if (errno == EINTR)
    {
      continue;
    }
    else
    {
      break;
    }
  }
  close(pipefd[0]);

  if (output.empty())
  {
    return false;
  }

  size_t nl1 = output.find('\n');
  std::string line1 = (nl1 == std::string::npos) ? output : output.substr(0, nl1);
  if (!line1.empty() && line1.back() == '\r')
  {
    line1.pop_back();
  }
  if (line1.empty())
  {
    return false;
  }
  videoUrl = line1;

  if (nl1 != std::string::npos)
  {
    size_t start2 = nl1 + 1;
    size_t nl2 = output.find('\n', start2);
    std::string line2 = (nl2 == std::string::npos)
      ? output.substr(start2)
      : output.substr(start2, nl2 - start2);
    if (!line2.empty() && line2.back() == '\r')
    {
      line2.pop_back();
    }
    if (!line2.empty())
    {
      audioUrl = line2;
    }
  }
  return true;
}

VideoPlayer::VideoPlayer()
{
}

VideoPlayer::~VideoPlayer()
{
  shutdown();
}

bool VideoPlayer::init()
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
  mpv_set_option_string(m_pMpv, "vo", "libmpv");
  mpv_set_option_string(m_pMpv, "gpu-api", "opengl");
  // These didn't improve the performance. Still when i hit View, it's laggish and slow to open player view.
  // mpv_set_option_string(m_pMpv, "hwdec", "auto-safe");
  // mpv_set_option_string(m_pMpv, "gpu-hwdec-interop", "auto");
  // mpv_set_option_string(m_pMpv, "scale", "bilinear");
  // mpv_set_option_string(m_pMpv, "dscale", "bilinear");
  // mpv_set_option_string(m_pMpv, "interpolation", "no");
  // mpv_set_option_string(m_pMpv, "vd-lavc-dr", "yes");
  mpv_set_option_string(m_pMpv, "force-window", "no");
  mpv_set_option_string(m_pMpv, "audio-display", "no");
  mpv_set_option_string(m_pMpv, "idle", "yes");
  mpv_set_option_string(m_pMpv, "input-default-bindings", "no");
  mpv_set_option_string(m_pMpv, "input-vo-keyboard", "no");
  mpv_set_option_string(m_pMpv, "osc", "no");
  mpv_set_option_string(m_pMpv, "terminal", "yes");
  mpv_set_option_string(m_pMpv, "msg-level", "all=warn");
  if (!m_subFont.empty())
  {
    mpv_set_option_string(m_pMpv, "sub-font", m_subFont.c_str());
  }
  if (m_subFontSize > 0)
  {
    std::string s = std::to_string(m_subFontSize);
    mpv_set_option_string(m_pMpv, "sub-font-size", s.c_str());
  }
  if (mpv_initialize(m_pMpv) < 0)
  {
    mpv_terminate_destroy(m_pMpv);
    m_pMpv = nullptr;
    return false;
  }

  mpv_opengl_init_params glParams{};
  glParams.get_proc_address = getGlProcAddress;
  glParams.get_proc_address_ctx = nullptr;

  mpv_render_param renderParams[] = {
    {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
    {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glParams},
    {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  if (mpv_render_context_create(&m_pRender, m_pMpv, renderParams) < 0)
  {
    mpv_terminate_destroy(m_pMpv);
    m_pMpv = nullptr;
    return false;
  }
  mpv_render_context_set_update_callback(m_pRender, &VideoPlayer::onRenderUpdate, this);

  setVolume(m_volume);
  return true;
}

void VideoPlayer::shutdown()
{
  if (m_pRender != nullptr)
  {
    mpv_render_context_free(m_pRender);
    m_pRender = nullptr;
  }
  if (m_pMpv != nullptr)
  {
    mpv_terminate_destroy(m_pMpv);
    m_pMpv = nullptr;
  }
  m_state = VideoPlayerState::Idle;
  m_currentFile.clear();
}

void VideoPlayer::onRenderUpdate(void* ctx)
{
  VideoPlayer* self = static_cast<VideoPlayer*>(ctx);
  if (self != nullptr)
  {
    self->m_renderUpdate = true;
  }
}

bool VideoPlayer::open(const std::string& path)
{
  if (m_pMpv == nullptr)
  {
    return false;
  }
  std::string resolvedPath = path;
  std::string audioUrl;
  if (isYoutubeUrl(path))
  {
    std::string videoUrl;
    if (!resolveYoutubeUrl(path, videoUrl, audioUrl))
    {
      return false;
    }
    resolvedPath = videoUrl;
  }
  if (!audioUrl.empty())
  {
    std::string audioOpt = "audio-file=" + audioUrl;
    const char* cmd[] = {"loadfile", resolvedPath.c_str(), "replace", "-1",
                         audioOpt.c_str(), nullptr};
    if (mpv_command(m_pMpv, cmd) < 0)
    {
      return false;
    }
  }
  else
  {
    const char* cmd[] = {"loadfile", resolvedPath.c_str(), "replace", nullptr};
    if (mpv_command(m_pMpv, cmd) < 0)
    {
      return false;
    }
  }
  m_currentFile = path;
  m_state = VideoPlayerState::Loading;
  m_renderUpdate = true;
  return true;
}

void VideoPlayer::close()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  const char* cmd[] = {"stop", nullptr};
  mpv_command(m_pMpv, cmd);
  m_state = VideoPlayerState::Idle;
  m_currentFile.clear();
}

void VideoPlayer::update()
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
    if (ev->event_id == MPV_EVENT_FILE_LOADED)
    {
      m_state = VideoPlayerState::Playing;
    }
    else if (ev->event_id == MPV_EVENT_END_FILE)
    {
      mpv_event_end_file* end = static_cast<mpv_event_end_file*>(ev->data);
      if (end != nullptr && end->reason == MPV_END_FILE_REASON_ERROR)
      {
        m_state = VideoPlayerState::Failed;
      }
      else
      {
        m_state = VideoPlayerState::Ended;
      }
    }
    else if (ev->event_id == MPV_EVENT_SHUTDOWN)
    {
      m_state = VideoPlayerState::Idle;
    }
  }
}

void VideoPlayer::togglePause()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  if (m_state == VideoPlayerState::Playing || m_state == VideoPlayerState::Paused)
  {
    int paused = (m_state == VideoPlayerState::Paused) ? 0 : 1;
    mpv_set_property(m_pMpv, "pause", MPV_FORMAT_FLAG, &paused);
    m_state = (paused == 1) ? VideoPlayerState::Paused : VideoPlayerState::Playing;
    return;
  }
  play();
}

bool VideoPlayer::play()
{
  if (m_pMpv == nullptr || m_currentFile.empty())
  {
    return false;
  }
  if (m_state == VideoPlayerState::Playing || m_state == VideoPlayerState::Paused)
  {
    return true;
  }
  return open(m_currentFile);
}

void VideoPlayer::setSubtitleFont(const std::string& font)
{
  if (font.empty())
  {
    return;
  }
  m_subFont = font;
  if (m_pMpv == nullptr)
  {
    return;
  }
  mpv_set_property_string(m_pMpv, "sub-font", font.c_str());
}

void VideoPlayer::setSubtitleFontSize(int size)
{
  if (size <= 0)
  {
    return;
  }
  m_subFontSize = size;
  if (m_pMpv == nullptr)
  {
    return;
  }
  int64_t s = static_cast<int64_t>(size);
  mpv_set_property(m_pMpv, "sub-font-size", MPV_FORMAT_INT64, &s);
}

const std::string& VideoPlayer::subtitleFont() const
{
  return m_subFont;
}

int VideoPlayer::subtitleFontSize() const
{
  return m_subFontSize;
}

void VideoPlayer::stop()
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  const char* cmd[] = {"stop", nullptr};
  mpv_command(m_pMpv, cmd);
  m_state = VideoPlayerState::Idle;
}

void VideoPlayer::setVolume(int percent)
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

int VideoPlayer::volume() const
{
  return m_volume;
}

void VideoPlayer::seekAbsolute(double seconds)
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

double VideoPlayer::position() const
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

double VideoPlayer::duration() const
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

bool VideoPlayer::isActive() const
{
  return m_state != VideoPlayerState::Idle;
}

bool VideoPlayer::isPlaying() const
{
  return m_state == VideoPlayerState::Playing;
}

VideoPlayerState VideoPlayer::state() const
{
  return m_state;
}

const std::string& VideoPlayer::currentFile() const
{
  return m_currentFile;
}

bool VideoPlayer::needsRender() const
{
  return m_renderUpdate;
}

void VideoPlayer::render(unsigned int fbo, int width, int height)
{
  if (m_pRender == nullptr)
  {
    return;
  }
  if (!m_renderUpdate)
  {
    return;
  }
  m_renderUpdate = false;

  uint64_t flags = mpv_render_context_update(m_pRender);
  if (!(flags & MPV_RENDER_UPDATE_FRAME))
  {
    return;
  }

  mpv_opengl_fbo fboParam{};
  fboParam.fbo = static_cast<int>(fbo);
  fboParam.w = width;
  fboParam.h = height;
  fboParam.internal_format = 0;

  int flipY = 1;
  mpv_render_param params[] = {
    {MPV_RENDER_PARAM_OPENGL_FBO, &fboParam},
    {MPV_RENDER_PARAM_FLIP_Y, &flipY},
    {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  mpv_render_context_render(m_pRender, params);
}

void VideoPlayer::reportSwap()
{
  if (m_pRender != nullptr)
  {
    mpv_render_context_report_swap(m_pRender);
  }
}
