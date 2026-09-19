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

void VideoPlayer::cancelResolve()
{
  if (m_resolveFd >= 0)
  {
    ::close(m_resolveFd);
    m_resolveFd = -1;
  }
  m_resolvePid = -1;
  m_resolveBuffer.clear();
}

bool VideoPlayer::startResolveYoutube(const std::string& ytUrl)
{
  cancelResolve();

  int pipefd[2];
  if (pipe(pipefd) != 0)
  {
    return false;
  }

  int flags = fcntl(pipefd[0], F_GETFL, 0);
  if (flags >= 0)
  {
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
  }

  if (m_preferredHeight > 0)
  {
    m_resolveFormat =
      "bv*[height<=" + std::to_string(m_preferredHeight) +
      "]+ba/b[height<=" + std::to_string(m_preferredHeight) +
      "]/bv*+ba/b";
  }
  else
  {
    m_resolveFormat = "bv*+ba/b";
  }

  pid_t pid = fork();
  if (pid < 0)
  {
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    return false;
  }

  if (pid == 0)
  {
    ::close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    ::close(pipefd[1]);
    int devnull = ::open("/dev/null", O_WRONLY);
    if (devnull >= 0)
    {
      dup2(devnull, STDERR_FILENO);
      ::close(devnull);
    }
    const char* argv[] = {
      "yt-dlp",
      "-f", m_resolveFormat.c_str(),
      "--no-warnings",
      "--get-url",
      ytUrl.c_str(),
      nullptr,
    };
    execvp("yt-dlp", const_cast<char* const*>(argv));
    _exit(127);
  }

  ::close(pipefd[1]);
  m_resolveFd = pipefd[0];
  m_resolvePid = pid;
  m_resolveBuffer.clear();
  return true;
}

bool VideoPlayer::loadResolved(const std::string& videoUrl, const std::string& audioUrl)
{
  if (m_pMpv == nullptr || videoUrl.empty())
  {
    return false;
  }
  if (!audioUrl.empty())
  {
    std::string audioOpt = "audio-file=" + audioUrl;
    const char* cmd[] = {"loadfile", videoUrl.c_str(), "replace", "-1",
                         audioOpt.c_str(), nullptr};
    if (mpv_command(m_pMpv, cmd) < 0)
    {
      return false;
    }
  }
  else
  {
    const char* cmd[] = {"loadfile", videoUrl.c_str(), "replace", nullptr};
    if (mpv_command(m_pMpv, cmd) < 0)
    {
      return false;
    }
  }
  m_renderUpdate = true;
  return true;
}

void VideoPlayer::pollResolve()
{
  if (m_resolveFd < 0)
  {
    return;
  }

  bool eof = false;
  char buf[4096];
  while (true)
  {
    ssize_t n = read(m_resolveFd, buf, sizeof(buf));
    if (n > 0)
    {
      m_resolveBuffer.append(buf, static_cast<size_t>(n));
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
    return;
  }

  ::close(m_resolveFd);
  m_resolveFd = -1;
  m_resolvePid = -1;

  std::string videoUrl;
  std::string audioUrl;
  if (!m_resolveBuffer.empty())
  {
    size_t nl1 = m_resolveBuffer.find('\n');
    std::string line1 = (nl1 == std::string::npos)
      ? m_resolveBuffer
      : m_resolveBuffer.substr(0, nl1);
    if (!line1.empty() && line1.back() == '\r')
    {
      line1.pop_back();
    }
    if (!line1.empty())
    {
      videoUrl = line1;
      if (nl1 != std::string::npos)
      {
        size_t start2 = nl1 + 1;
        size_t nl2 = m_resolveBuffer.find('\n', start2);
        std::string line2 = (nl2 == std::string::npos)
          ? m_resolveBuffer.substr(start2)
          : m_resolveBuffer.substr(start2, nl2 - start2);
        if (!line2.empty() && line2.back() == '\r')
        {
          line2.pop_back();
        }
        if (!line2.empty())
        {
          audioUrl = line2;
        }
      }
    }
  }
  m_resolveBuffer.clear();

  if (videoUrl.empty() || !loadResolved(videoUrl, audioUrl))
  {
    m_state = VideoPlayerState::Failed;
  }
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
  cancelResolve();
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
  cancelResolve();
  if (isYoutubeUrl(path))
  {
    if (m_preferredHeight > 0)
    {
      m_resolveFormat =
        "bv*[height<=" + std::to_string(m_preferredHeight) +
        "]+ba/b[height<=" + std::to_string(m_preferredHeight) +
        "]/bv*+ba/b";
    }
    else
    {
      m_resolveFormat = "bv*+ba/b";
    }
    mpv_set_property_string(m_pMpv, "ytdl-format", m_resolveFormat.c_str());
    mpv_set_property_string(m_pMpv, "sub-auto", "fuzzy");
    mpv_set_property_string(m_pMpv, "ytdl", "yes");
    const char* cmd[] = {"loadfile", path.c_str(), "replace", nullptr};
    if (mpv_command(m_pMpv, cmd) < 0)
    {
      return false;
    }
    m_currentFile = path;
    m_state = VideoPlayerState::Loading;
    m_renderUpdate = true;
    return true;
  }
  const char* cmd[] = {"loadfile", path.c_str(), "replace", nullptr};
  if (mpv_command(m_pMpv, cmd) < 0)
  {
    return false;
  }
  m_currentFile = path;
  m_state = VideoPlayerState::Loading;
  m_renderUpdate = true;
  return true;
}

void VideoPlayer::close()
{
  cancelResolve();
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
  if (m_resolveFd >= 0)
  {
    pollResolve();
  }
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
  (void)play();
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
void VideoPlayer::setPreferredHeight(int height)
{
  if (height < 0)
  {
    height = 0;
  }
  m_preferredHeight = height;
}

int VideoPlayer::preferredHeight() const
{
  return m_preferredHeight;
}

bool VideoPlayer::reopenWithHeight(int height)
{
  if (m_currentFile.empty())
  {
    return false;
  }
  const std::string path = m_currentFile;
  setPreferredHeight(height);
  cancelResolve();
  if (m_pMpv != nullptr)
  {
    const char* cmd[] = {"stop", nullptr};
    mpv_command(m_pMpv, cmd);
  }
  m_state = VideoPlayerState::Idle;
  return open(path);
}

int VideoPlayer::subtitleTrackCount() const
{
  if (m_pMpv == nullptr)
  {
    return 0;
  }
  int64_t count = 0;
  if (mpv_get_property(m_pMpv, "track-list/count", MPV_FORMAT_INT64, &count) < 0)
  {
    return 0;
  }
  int n = 0;
  for (int64_t i = 0; i < count; i++)
  {
    char key[64];
    std::snprintf(key, sizeof(key), "track-list/%lld/type", static_cast<long long>(i));
    char* type = nullptr;
    if (mpv_get_property(m_pMpv, key, MPV_FORMAT_STRING, &type) < 0 || type == nullptr)
    {
      continue;
    }
    const bool isSub = (std::strcmp(type, "sub") == 0);
    mpv_free(type);
    if (isSub)
    {
      n++;
    }
  }
  return n;
}

int VideoPlayer::subtitleTrackIdAt(int index) const
{
  if (m_pMpv == nullptr || index < 0)
  {
    return -1;
  }
  int64_t count = 0;
  if (mpv_get_property(m_pMpv, "track-list/count", MPV_FORMAT_INT64, &count) < 0)
  {
    return -1;
  }
  int n = 0;
  for (int64_t i = 0; i < count; i++)
  {
    char key[64];
    std::snprintf(key, sizeof(key), "track-list/%lld/type", static_cast<long long>(i));
    char* type = nullptr;
    if (mpv_get_property(m_pMpv, key, MPV_FORMAT_STRING, &type) < 0 || type == nullptr)
    {
      continue;
    }
    const bool isSub = (std::strcmp(type, "sub") == 0);
    mpv_free(type);
    if (!isSub)
    {
      continue;
    }
    if (n == index)
    {
      std::snprintf(key, sizeof(key), "track-list/%lld/id", static_cast<long long>(i));
      int64_t id = -1;
      if (mpv_get_property(m_pMpv, key, MPV_FORMAT_INT64, &id) < 0)
      {
        return -1;
      }
      return static_cast<int>(id);
    }
    n++;
  }
  return -1;
}

std::string VideoPlayer::subtitleTrackLabelAt(int index) const
{
  if (m_pMpv == nullptr || index < 0)
  {
    return std::string();
  }
  int64_t count = 0;
  if (mpv_get_property(m_pMpv, "track-list/count", MPV_FORMAT_INT64, &count) < 0)
  {
    return std::string();
  }
  int n = 0;
  for (int64_t i = 0; i < count; i++)
  {
    char key[64];
    std::snprintf(key, sizeof(key), "track-list/%lld/type", static_cast<long long>(i));
    char* type = nullptr;
    if (mpv_get_property(m_pMpv, key, MPV_FORMAT_STRING, &type) < 0 || type == nullptr)
    {
      continue;
    }
    const bool isSub = (std::strcmp(type, "sub") == 0);
    mpv_free(type);
    if (!isSub)
    {
      continue;
    }
    if (n == index)
    {
      std::string label;
      std::snprintf(key, sizeof(key), "track-list/%lld/lang", static_cast<long long>(i));
      char* lang = nullptr;
      if (mpv_get_property(m_pMpv, key, MPV_FORMAT_STRING, &lang) >= 0 && lang != nullptr)
      {
        label = lang;
        mpv_free(lang);
      }
      std::snprintf(key, sizeof(key), "track-list/%lld/title", static_cast<long long>(i));
      char* title = nullptr;
      if (mpv_get_property(m_pMpv, key, MPV_FORMAT_STRING, &title) >= 0 && title != nullptr)
      {
        if (!label.empty())
        {
          label += " — ";
        }
        label += title;
        mpv_free(title);
      }
      if (label.empty())
      {
        label = "Track " + std::to_string(index + 1);
      }
      return label;
    }
    n++;
  }
  return std::string();
}

int VideoPlayer::currentSubtitleId() const
{
  if (m_pMpv == nullptr)
  {
    return -1;
  }
  int64_t id = -1;
  if (mpv_get_property(m_pMpv, "sid", MPV_FORMAT_INT64, &id) < 0)
  {
    return -1;
  }
  return static_cast<int>(id);
}

void VideoPlayer::setSubtitleId(int id)
{
  if (m_pMpv == nullptr)
  {
    return;
  }
  if (id < 0)
  {
    mpv_set_property_string(m_pMpv, "sid", "no");
    return;
  }
  int64_t sid = id;
  mpv_set_property(m_pMpv, "sid", MPV_FORMAT_INT64, &sid);
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
