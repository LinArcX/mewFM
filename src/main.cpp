#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glext.h>

#define NANOVG_GL2
#include "../third_party/nanovg/nanovg.h"
#include "../third_party/nanovg/nanovg_gl.h"
#include "../third_party/oui-blendish/blendish.h"
#include "FileManager.hpp"
#include "DejaVuSansFont.hpp"
#include "BlenderIcons.hpp"

#include <fstream>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace
{
  constexpr float kTopBarHeight = 40.0f;
  constexpr float kSidebarWidth = 220.0f;
  constexpr float kRowHeight    = 24.0f;
  constexpr float kHeaderHeight = 26.0f;
  constexpr float kPadX         =   8.0f;
  constexpr float kBtnSize      =  28.0f;
  constexpr float kBtnGap       =   4.0f;
  constexpr float kBtnY         = (kTopBarHeight - kBtnSize) * 0.5f;
  constexpr double kDoubleClickTime = 0.4;
  constexpr float kIconSize     = 16.0f;
  constexpr float kIconGap      = 4.0f;
  constexpr float kMinColWidth  = 40.0f;
  constexpr int   kNumCols      = 5;

  struct Place
  {
    std::string label;
    std::string path;
    int icon;
  };

  struct AppState
  {
    FileManager fm;
    std::vector<Place> places;
    float scrollOffset = 0.0f;
    int selectedIndex = -1;
    std::string lastPath;
    double lastClickTime = 0.0;
    int lastClickIndex = -1;
    float colWidths[kNumCols] = {260.0f, 90.0f, 110.0f, 100.0f, 110.0f};
    int dragColumn = -1;
    float dragStartMouseX = 0.0f;
    float dragStartWidth = 0.0f;
  };

  float g_mouseX = 0.0f;
  float g_mouseY = 0.0f;
  bool  g_mouseClicked = false;
  bool  g_mouseDown = false;
  float g_scrollY = 0.0f;
  bool  g_navUp = false;
  bool  g_navDown = false;
  bool  g_navEnter = false;
  bool  g_navBack = false;
}

static BNDwidgetTheme makeWidgetTheme()
{
  BNDwidgetTheme w{};
  w.outlineColor = nvgRGBf(0.098f, 0.098f, 0.098f);
  w.itemColor = nvgRGBf(0.098f, 0.098f, 0.098f);
  w.innerColor = nvgRGBf(0.3f, 0.3f, 0.3f);
  w.innerSelectedColor = nvgRGBf(0.4f, 0.4f, 0.4f);
  w.textColor = nvgRGBf(0.9f, 0.9f, 0.9f);
  w.textSelectedColor = nvgRGBf(1.0f, 1.0f, 1.0f);
  w.shadeTop = 100;
  w.shadeDown = 0;
  return w;
}

static std::string configFilePath()
{
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  std::string base;
  if (xdg != nullptr && xdg[0] != '\0')
  {
    base = xdg;
  }
  else
  {
    const char* home = std::getenv("HOME");
    base = (home != nullptr) ? std::string(home) + "/.config" : ".";
  }
  return base + "/rah/config";
}

static void loadConfig(AppState& app)
{
  std::ifstream in(configFilePath());
  if (!in)
  {
    return;
  }
  std::string line;
  while (std::getline(in, line))
  {
    size_t eq = line.find('=');
    if (eq == std::string::npos)
    {
      continue;
    }
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    if (key.size() >= 4 && key.compare(0, 3, "col") == 0)
    {
      int idx = std::atoi(key.c_str() + 3);
      if (idx >= 0 && idx < kNumCols)
      {
        float w = static_cast<float>(std::atof(val.c_str()));
        if (w >= kMinColWidth)
        {
          app.colWidths[idx] = w;
        }
      }
    }
  }
}

static void saveConfig(const AppState& app)
{
  std::string path = configFilePath();
  std::error_code ec;
  std::filesystem::create_directories(
    std::filesystem::path(path).parent_path(), ec);
  std::ofstream out(path);
  if (!out)
  {
    return;
  }
  for (int i = 0; i < kNumCols; i++)
  {
    out << "col" << i << "=" << app.colWidths[i] << "\n";
  }
}

static void applyTheme()
{
  BNDwidgetTheme w = makeWidgetTheme();
  BNDtheme t{};
  t.backgroundColor = nvgRGBf(0.2f, 0.2f, 0.2f);
  t.regularTheme = w;
  t.toolTheme = w;
  t.radioTheme = w;
  t.textFieldTheme = w;
  t.optionTheme = w;
  t.choiceTheme = w;
  t.numberFieldTheme = w;
  t.sliderTheme = w;
  t.scrollBarTheme = w;
  t.tooltipTheme = w;
  t.menuTheme = w;
  t.menuItemTheme = w;
  bndSetTheme(t);
}

static std::vector<Place> buildPlaces()
{
  std::vector<Place> all;
  const char* home = std::getenv("HOME");
  std::string h = (home != nullptr) ? std::string(home) : std::string("/");
  all.push_back({"Home",        h,                  BND_ICON_FILE_FOLDER});
  all.push_back({"Desktop",     h + "/Desktop",     BND_ICON_FILE_FOLDER});
  all.push_back({"Documents",   h + "/Documents",   BND_ICON_FILE_FOLDER});
  all.push_back({"Downloads",   h + "/Downloads",   BND_ICON_FILE_FOLDER});
  all.push_back({"Pictures",    h + "/Pictures",    BND_ICON_FILE_IMAGE});
  all.push_back({"Music",       h + "/Music",       BND_ICON_FILE_SOUND});
  all.push_back({"Videos",      h + "/Videos",      BND_ICON_FILE_MOVIE});
  all.push_back({"File System", "/",                BND_ICON_DISK_DRIVE});

  //all.push_back({"Home",        h});
  //all.push_back({"Desktop",     h + "/Desktop"});
  //all.push_back({"Documents",   h + "/Documents"});
  //all.push_back({"Downloads",   h + "/Downloads"});
  //all.push_back({"Pictures",    h + "/Pictures"});
  //all.push_back({"Music",       h + "/Music"});
  //all.push_back({"Videos",      h + "/Videos"});
  //all.push_back({"File System", "/"});

  std::vector<Place> filtered;
  std::error_code ec;
  for (const auto& p : all)
  {
    ec.clear();
    if (std::filesystem::is_directory(p.path, ec))
    {
      filtered.push_back(p);
    }
  }
  return filtered;
}

static void openWithDefaultApp(const std::string& path)
{
  pid_t pid = fork();
  if (pid == 0)
  {
    execlp("xdg-open", "xdg-open", path.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
}

static void runExecutable(const std::string& path)
{
  pid_t pid = fork();
  if (pid == 0)
  {
    execl(path.c_str(), path.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
}

static void errorCallback(int error, const char* description)
{
  std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  (void)scancode;
  (void)mods;
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
  {
    return;
  }
  if (key == GLFW_KEY_UP)    g_navUp = true;
  if (key == GLFW_KEY_DOWN)  g_navDown = true;
  if (key == GLFW_KEY_ENTER) g_navEnter = true;
  if (key == GLFW_KEY_BACKSPACE) g_navBack = true;
}

static void cursorPosCallback(GLFWwindow* window, double x, double y)
{
  (void)window;
  g_mouseX = static_cast<float>(x);
  g_mouseY = static_cast<float>(y);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  (void)window;
  (void)mods;
  if (button != GLFW_MOUSE_BUTTON_LEFT)
  {
    return;
  }
  if (action == GLFW_PRESS)
  {
    g_mouseClicked = true;
    g_mouseDown = true;
  }
  else if (action == GLFW_RELEASE)
  {
    g_mouseDown = false;
  }
}

static void scrollCallback(GLFWwindow* window, double x, double y)
{
  (void)window;
  (void)x;
  g_scrollY += static_cast<float>(y);
}

static bool inRect(float mx, float my, float x, float y, float w, float h)
{
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

static std::string joinPath(const std::string& base, const std::string& name)
{
  if (base.empty())
  {
    return name;
  }
  if (base.back() == '/')
  {
    return base + name;
  }
  return base + "/" + name;
}

static int iconForEntry(const Entry& e)
{
  if (e.isDirectory)
  {
    return BND_ICON_FILE_FOLDER;
  }
  std::string ext;
  size_t dot = e.name.find_last_of('.');
  if (dot != std::string::npos && dot + 1 < e.name.size())
  {
    ext = e.name.substr(dot + 1);
    for (char& c : ext)
    {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
  }
  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" ||
      ext == "bmp" || ext == "webp" || ext == "svg" || ext == "tiff" ||
      ext == "ico")
  {
    return BND_ICON_FILE_IMAGE;
  }
  if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
      ext == "webm" || ext == "flv" || ext == "wmv" || ext == "m4v")
  {
    return BND_ICON_FILE_MOVIE;
  }
  if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg" ||
      ext == "m4a" || ext == "opus" || ext == "aac")
  {
    return BND_ICON_FILE_SOUND;
  }
  if (ext == "txt" || ext == "md" || ext == "log" || ext == "pdf" ||
      ext == "doc" || ext == "docx" || ext == "odt" || ext == "rtf")
  {
    return BND_ICON_FILE_TEXT;
  }
  if (ext == "ttf" || ext == "otf" || ext == "woff" || ext == "woff2")
  {
    return BND_ICON_FILE_FONT;
  }
  if (ext == "sh" || ext == "py" || ext == "cpp" || ext == "cc" ||
      ext == "c" || ext == "h" || ext == "hpp" || ext == "js" ||
      ext == "ts" || ext == "rb" || ext == "rs" || ext == "go" ||
      ext == "java" || ext == "lua" || ext == "pl")
  {
    return BND_ICON_FILE_SCRIPT;
  }
  if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "bz2" ||
      ext == "xz" || ext == "rar" || ext == "7z" || ext == "jar" ||
      ext == "apk" || ext == "deb" || ext == "rpm" || ext == "iso")
  {
    return BND_ICON_FILE_BACKUP;
  }
  if (ext == "o" || ext == "so" || ext == "a" || ext == "out" ||
      ext == "exe" || ext == "dll" || ext == "dylib" || ext == "bin")
  {
    return BND_ICON_DISK_DRIVE;
  }
  return BND_ICON_FILE_BLANK;
}

static void openEntry(AppState& app, int index)
{
  const auto& entries = app.fm.entries();
  if (index < 0 || index >= static_cast<int>(entries.size()))
  {
    return;
  }
  const Entry e = entries[index];
  std::string full = joinPath(app.fm.currentPath(), e.name);
  if (e.isDirectory)
  {
    app.fm.setPath(full);
    app.selectedIndex = -1;
    app.scrollOffset = 0.0f;
  }
  else if (e.isExecutable)
  {
    runExecutable(full);
  }
  else
  {
    openWithDefaultApp(full);
  }
}

static void drawTopBar(NVGcontext* vg, FileManager& fm, float w)
{
  bndBackground(vg, 0.0f, 0.0f, w, kTopBarHeight);
  float x = kPadX;

  bool homeHover = inRect(g_mouseX, g_mouseY, x, kBtnY, kBtnSize, kBtnSize);
  bndToolButton(vg, x, kBtnY, kBtnSize, kBtnSize, BND_CENTER,
                homeHover ? BND_HOVER : BND_DEFAULT,
                BND_ICON_DISK_DRIVE, "");
  if (homeHover && g_mouseClicked)
  {
    const char* home = std::getenv("HOME");
    if (home != nullptr)
    {
      fm.setPath(home);
    }
  }
  x += kBtnSize + kBtnGap;
  x += 8.0f;

  const int icons[3] = {BND_ICON_TRIA_LEFT, BND_ICON_TRIA_RIGHT, BND_ICON_TRIA_UP};
  const bool enabled[3] = {fm.canGoBack(), fm.canGoForward(), true};
  int action = -1;
  for (int i = 0; i < 3; i++)
  {
    bool hover = enabled[i] && inRect(g_mouseX, g_mouseY, x, kBtnY, kBtnSize, kBtnSize);
    BNDwidgetState st = BND_DEFAULT;
    if (enabled[i] && hover)
    {
      st = BND_HOVER;
    }
    bndToolButton(vg, x, kBtnY, kBtnSize, kBtnSize, BND_CENTER, st, icons[i], "");
    if (hover && g_mouseClicked)
    {
      action = i;
    }
    x += kBtnSize + kBtnGap;
  }
  x += 8.0f;

  if (action == 0)
  {
    fm.goBack();
  }
  else if (action == 1)
  {
    fm.goForward();
  }
  else if (action == 2)
  {
    fm.goUp();
  }

  //float x = kPadX;
  //const int icons[3] = {BND_ICON_TRIA_LEFT, BND_ICON_TRIA_RIGHT, BND_ICON_TRIA_UP};
  //const bool enabled[3] = {fm.canGoBack(), fm.canGoForward(), true};
  //int action = -1;
  //for (int i = 0; i < 3; i++)
  //{
  //  bool hover = enabled[i] && inRect(g_mouseX, g_mouseY, x, kBtnY, kBtnSize, kBtnSize);
  //  BNDwidgetState st = BND_DEFAULT;
  //  if (enabled[i] && hover)
  //  {
  //    st = BND_HOVER;
  //  }
  //  bndToolButton(vg, x, kBtnY, kBtnSize, kBtnSize, BND_CENTER, st, icons[i], "");

  //  if (hover && g_mouseClicked)
  //  {
  //    action = i;
  //  }
  //  x += kBtnSize + kBtnGap;
  //}
  //x += 8.0f;

  //if (action == 0)
  //{
  //  fm.goBack();
  //}
  //else if (action == 1)
  //{
  //  fm.goForward();
  //}
  //else if (action == 2)
  //{
  //  fm.goUp();
  //}

  std::string path = fm.currentPath();
  std::vector<std::pair<std::string, std::string>> crumbs;
  crumbs.push_back(std::make_pair(std::string("/"), std::string("/")));
  std::string acc;
  size_t i = (!path.empty() && path[0] == '/') ? 1 : 0;
  while (i < path.size())
  {
    size_t j = path.find('/', i);
    if (j == std::string::npos)
    {
      j = path.size();
    }
    std::string seg = path.substr(i, j - i);
    if (!seg.empty())
    {
      acc = acc.empty() ? ("/" + seg) : (acc + "/" + seg);
      crumbs.push_back(std::make_pair(seg, acc));
    }
    i = j + 1;
  }

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 14.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float cy = kTopBarHeight * 0.5f;

    std::string navTo;
  for (size_t k = 0; k < crumbs.size(); k++)
  {
    const bool isRoot = (k == 0);
    float segW;
    if (isRoot)
    {
      segW = kBtnSize;
    }
    else
    {
      float bounds[4];
      nvgTextBounds(vg, 0.0f, 0.0f, crumbs[k].first.c_str(), nullptr, bounds);
      segW = (bounds[2] - bounds[0]) + 12.0f;
    }
    bool hover = inRect(g_mouseX, g_mouseY, x, 0.0f, segW, kTopBarHeight);
    if (hover)
    {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x, kBtnY, segW, kBtnSize, 3.0f);
      nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
      nvgFill(vg);
    }
    if (isRoot)
    {
      bndIcon(vg, x + (segW - kIconSize) * 0.5f, cy - kIconSize * 0.5f,
              BND_ICON_DISK_DRIVE);
    }
    else
    {
      nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
      nvgText(vg, x + 6.0f, cy, crumbs[k].first.c_str(), nullptr);
    }
    if (hover && g_mouseClicked)
    {
      navTo = crumbs[k].second;
    }
    x += segW;
    if (k + 1 < crumbs.size())
    {
      nvgFillColor(vg, nvgRGBf(0.5f, 0.5f, 0.5f));
      nvgText(vg, x, cy, "/", nullptr);
      float sb[4];
      nvgTextBounds(vg, 0.0f, 0.0f, "/", nullptr, sb);
      x += (sb[2] - sb[0]) + 4.0f;
    }
  }

  //std::string navTo;
  //for (size_t k = 0; k < crumbs.size(); k++)
  //{
  //  float bounds[4];
  //  nvgTextBounds(vg, 0.0f, 0.0f, crumbs[k].first.c_str(), nullptr, bounds);
  //  float tw = bounds[2] - bounds[0];
  //  float segW = tw + 12.0f;
  //  bool hover = inRect(g_mouseX, g_mouseY, x, 0.0f, segW, kTopBarHeight);
  //  if (hover)
  //  {
  //    nvgBeginPath(vg);
  //    nvgRoundedRect(vg, x, kBtnY, segW, kBtnSize, 3.0f);
  //    nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
  //    nvgFill(vg);
  //  }
  //  nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
  //  nvgText(vg, x + 6.0f, cy, crumbs[k].first.c_str(), nullptr);
  //  if (hover && g_mouseClicked)
  //  {
  //    navTo = crumbs[k].second;
  //  }
  //  x += segW;
  //  if (k + 1 < crumbs.size())
  //  {
  //    nvgFillColor(vg, nvgRGBf(0.5f, 0.5f, 0.5f));
  //    nvgText(vg, x, cy, "/", nullptr);
  //    float sb[4];
  //    nvgTextBounds(vg, 0.0f, 0.0f, "/", nullptr, sb);
  //    x += (sb[2] - sb[0]) + 4.0f;
  //  }
  //}
  if (!navTo.empty())
  {
    fm.setPath(navTo);
  }
}

static void drawSidebar(NVGcontext* vg,
                        FileManager& fm,
                        const std::vector<Place>& places,
                        float h)
{
  bndBackground(vg, 0.0f, kTopBarHeight, kSidebarWidth, h - kTopBarHeight);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 14.0f);
  nvgFillColor(vg, nvgRGBf(0.85f, 0.85f, 0.85f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(vg, kPadX, kTopBarHeight + 20.0f, "Places", nullptr);

  const float itemH   = 26.0f;
  const float itemGap = 2.0f;
  const float itemX   = 8.0f;
  const float itemW   = kSidebarWidth - 16.0f;
  float y = kTopBarHeight + 40.0f;

  std::string navTo;
  for (const auto& p : places)
  {
    bool hover   = inRect(g_mouseX, g_mouseY, itemX, y, itemW, itemH);
    bool current = (fm.currentPath() == p.path);
    BNDwidgetState st = BND_DEFAULT;
    if (current)
    {
      st = BND_ACTIVE;
    }
    else if (hover)
    {
      st = BND_HOVER;
    }
    bndToolButton(vg, itemX, y, itemW, itemH, BND_LEFT, st, p.icon, p.label.c_str());

    if (hover && g_mouseClicked)
    {
      navTo = p.path;
    }
    y += itemH + itemGap;
  }
  if (!navTo.empty())
  {
    fm.setPath(navTo);
  }
}

static float columnX(const AppState& app, float listX, int col)
{
  float x = listX + kPadX;
  for (int i = 0; i < col; i++)
  {
    x += app.colWidths[i];
  }
  return x;
}

static void drawMainHeader(NVGcontext* vg, AppState& app, float x, float y, float w)
{
  bndBackground(vg, x, y, w, kHeaderHeight);

  const float sepTol = 4.0f;

  if (app.dragColumn >= 0)
  {
    if (!g_mouseDown)
    {
      app.dragColumn = -1;
      saveConfig(app);
    }
    else
    {
      float delta = g_mouseX - app.dragStartMouseX;
      float nw = app.dragStartWidth + delta;
      if (nw < kMinColWidth)
      {
        nw = kMinColWidth;
      }
      app.colWidths[app.dragColumn] = nw;
    }
  }

  float seps[4];
  {
    float cx = x + kPadX;
    for (int i = 0; i < 4; i++)
    {
      cx += app.colWidths[i];
      seps[i] = cx;
    }
  }

  int hoverSep = -1;
  if (g_mouseY >= y && g_mouseY < y + kHeaderHeight)
  {
    for (int i = 0; i < 4; i++)
    {
      if (g_mouseX >= seps[i] - sepTol && g_mouseX <= seps[i] + sepTol)
      {
        hoverSep = i;
        break;
      }
    }
  }
  if (hoverSep >= 0 && g_mouseClicked && app.dragColumn < 0)
  {
    app.dragColumn = hoverSep;
    app.dragStartMouseX = g_mouseX;
    app.dragStartWidth = app.colWidths[hoverSep];
  }

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 13.0f);
  nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float cy = y + kHeaderHeight * 0.5f;

  nvgText(vg, columnX(app, x, 0) + kIconSize + kIconGap, cy, "Name", nullptr);
  nvgText(vg, columnX(app, x, 1), cy, "Size", nullptr);
  nvgText(vg, columnX(app, x, 2), cy, "Type", nullptr);
  nvgText(vg, columnX(app, x, 3), cy, "Owner", nullptr);
  nvgText(vg, columnX(app, x, 4), cy, "Permissions", nullptr);

  for (int i = 0; i < 4; i++)
  {
    bool active = (hoverSep == i || app.dragColumn == i);
    nvgBeginPath(vg);
    nvgMoveTo(vg, seps[i], y + 4.0f);
    nvgLineTo(vg, seps[i], y + kHeaderHeight - 4.0f);
    nvgStrokeColor(vg, active ? nvgRGBf(0.7f, 0.7f, 0.7f)
                              : nvgRGBf(0.35f, 0.35f, 0.35f));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
  }
}

static void drawRows(NVGcontext* vg,
                     const AppState& app,
                     float x, float y, float w, float h)
{
  const auto& entries = app.fm.entries();

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 13.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  nvgSave(vg);
  nvgScissor(vg, x, y, w, h);
  for (size_t i = 0; i < entries.size(); i++)
  {
    float rowY = y + static_cast<float>(i) * kRowHeight - app.scrollOffset;
    if (rowY + kRowHeight < y)
    {
      continue;
    }
    if (rowY > y + h)
    {
      break;
    }
    bool selected = (static_cast<int>(i) == app.selectedIndex);
    bool hover = inRect(g_mouseX, g_mouseY, x, rowY, w, kRowHeight);
    if (selected)
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, kRowHeight);
      nvgFillColor(vg, nvgRGBf(0.2f, 0.35f, 0.55f));
      nvgFill(vg);
    }
    else if (hover)
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, kRowHeight);
      nvgFillColor(vg, nvgRGBf(0.25f, 0.25f, 0.25f));
      nvgFill(vg);
    }
    const Entry& e = entries[i];
    float cy = rowY + kRowHeight * 0.5f;
    float iconX = columnX(app, x, 0);
    float iconY = cy - kIconSize * 0.5f;
    bndIcon(vg, iconX, iconY, iconForEntry(e));

    nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
    nvgText(vg, iconX + kIconSize + kIconGap, cy, e.name.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 1), cy, e.sizeText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 2), cy, e.typeText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 3), cy, e.ownerText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 4), cy, e.permText.c_str(), nullptr);
  }
  nvgRestore(vg);
}

static void resetOnPathChange(AppState& app)
{
  if (app.fm.currentPath() == app.lastPath)
  {
    return;
  }
  app.lastPath = app.fm.currentPath();
  loadConfig(app);
  app.selectedIndex = -1;
  app.scrollOffset = 0.0f;
}

static void handleListClick(AppState& app, float listX, float listTop, float listW, float listH)
{
  if (!g_mouseClicked || !inRect(g_mouseX, g_mouseY, listX, listTop, listW, listH))
  {
    return;
  }
  int idx = static_cast<int>((g_mouseY - listTop + app.scrollOffset) / kRowHeight);
  int count = static_cast<int>(app.fm.entries().size());
  if (idx < 0 || idx >= count)
  {
    return;
  }
  double now = glfwGetTime();
  bool isDouble = (idx == app.lastClickIndex) && (now - app.lastClickTime < kDoubleClickTime);
  app.lastClickTime = now;
  app.lastClickIndex = idx;

  if (isDouble)
  {
    openEntry(app, idx);
    app.lastClickIndex = -1;
  }
  else
  {
    app.selectedIndex = idx;
  }
}

static void handleKeyboardNav(AppState& app, float listH)
{
  int count = static_cast<int>(app.fm.entries().size());
  if (g_navUp && count > 0)
  {
    if (app.selectedIndex < 0)
    {
      app.selectedIndex = 0;
    }
    else if (app.selectedIndex > 0)
    {
      app.selectedIndex--;
    }
  }
  if (g_navDown && count > 0)
  {
    if (app.selectedIndex < 0)
    {
      app.selectedIndex = 0;
    }
    else if (app.selectedIndex < count - 1)
    {
      app.selectedIndex++;
    }
  }
  if (g_navEnter && app.selectedIndex >= 0)
  {
    openEntry(app, app.selectedIndex);
  }
  if (g_navBack)
  {
    app.fm.goUp();
    g_navBack = false;
  }
  g_navUp = false;
  g_navDown = false;
  g_navEnter = false;

  if (app.selectedIndex >= 0)
  {
    float rowTop = static_cast<float>(app.selectedIndex) * kRowHeight;
    if (rowTop < app.scrollOffset)
    {
      app.scrollOffset = rowTop;
    }
    else if (rowTop + kRowHeight > app.scrollOffset + listH)
    {
      app.scrollOffset = rowTop + kRowHeight - listH;
    }
  }
}

static void applyScroll(AppState& app, float listH)
{
  float contentH = static_cast<float>(app.fm.entries().size()) * kRowHeight;
  float maxScroll = contentH - listH;
  if (maxScroll < 0.0f)
  {
    maxScroll = 0.0f;
  }
  app.scrollOffset -= g_scrollY * 40.0f;
  if (app.scrollOffset < 0.0f)
  {
    app.scrollOffset = 0.0f;
  }
  if (app.scrollOffset > maxScroll)
  {
    app.scrollOffset = maxScroll;
  }
  g_scrollY = 0.0f;
}

int main()
{
  std::signal(SIGCHLD, SIG_IGN);

  glfwSetErrorCallback(errorCallback);
  if (!glfwInit())
  {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_STENCIL_BITS, 8);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(1024, 640, "rah", nullptr, nullptr);
  if (!window)
  {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetScrollCallback(window, scrollCallback);

  NVGcontext* vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  if (!vg)
  {
    glfwTerminate();
    return 1;
  }

  int font = nvgCreateFontMem(vg, "sans", fontData, fontData_len, 0);
  if (font == -1)
  {
    std::cerr << "Could not load font." << std::endl;
  }
  bndSetFont(font);

  int iconImage = nvgCreateImageMem(vg, 0, iconData, iconData_len);
  if (iconImage == -1)
  {
    std::cerr << "Could not load icon sheet." << std::endl;
  }
  bndSetIconImage(iconImage);

  applyTheme();

  AppState app;
  app.places = buildPlaces();
  app.lastPath = app.fm.currentPath();

  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();

    int winW = 0;
    int winH = 0;
    int fbW = 0;
    int fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    float pxRatio = (winW > 0) ? (static_cast<float>(fbW) / static_cast<float>(winW)) : 1.0f;

    glViewport(0, 0, fbW, fbH);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg, static_cast<float>(winW), static_cast<float>(winH), pxRatio);

    float w = static_cast<float>(winW);
    float h = static_cast<float>(winH);
    float mainX = kSidebarWidth;
    float mainY = kTopBarHeight;
    float mainW = w - kSidebarWidth;
    float listTop = mainY + kHeaderHeight;
    float listH = h - listTop;

    resetOnPathChange(app);
    applyScroll(app, listH);
    handleListClick(app, mainX, listTop, mainW, listH);
    resetOnPathChange(app);
    handleKeyboardNav(app, listH);

    drawTopBar(vg, app.fm, w);
    resetOnPathChange(app);
    drawSidebar(vg, app.fm, app.places, h);
    resetOnPathChange(app);
    drawMainHeader(vg, app, mainX, mainY, mainW);
    drawRows(vg, app, mainX, listTop, mainW, listH);

    nvgEndFrame(vg);

    glfwSwapBuffers(window);
    g_mouseClicked = false;
  }

  nvgDeleteGL2(vg);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
