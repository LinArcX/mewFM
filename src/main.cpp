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

#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <vector>

namespace
{
  constexpr float kTopBarHeight = 40.0f;
  constexpr float kSidebarWidth = 220.0f;
  constexpr float kRowHeight    = 24.0f;
  constexpr float kHeaderHeight = 26.0f;
  constexpr float kColNameW     = 260.0f;
  constexpr float kColSizeW     =  90.0f;
  constexpr float kColTypeW     = 110.0f;
  constexpr float kColOwnerW    = 100.0f;
  constexpr float kColPermW     = 110.0f;
  constexpr float kPadX         =   8.0f;
  constexpr float kBtnSize      =  28.0f;
  constexpr float kBtnGap       =   4.0f;
  constexpr float kBtnY         = (kTopBarHeight - kBtnSize) * 0.5f;

  float g_mouseX = 0.0f;
  float g_mouseY = 0.0f;
  bool  g_mouseClicked = false;
  float g_scrollY = 0.0f;
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

namespace
{
  struct Place
  {
    std::string label;
    std::string path;
  };
}

static std::vector<Place> buildPlaces()
{
  std::vector<Place> all;
  const char* home = std::getenv("HOME");
  std::string h = (home != nullptr) ? std::string(home) : std::string("/");
  all.push_back({"Home",        h});
  all.push_back({"Desktop",     h + "/Desktop"});
  all.push_back({"Documents",   h + "/Documents"});
  all.push_back({"Downloads",   h + "/Downloads"});
  all.push_back({"Pictures",    h + "/Pictures"});
  all.push_back({"Music",       h + "/Music"});
  all.push_back({"Videos",      h + "/Videos"});
  all.push_back({"File System", "/"});

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
  }
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
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
  {
    g_mouseClicked = true;
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

static void drawTopBar(NVGcontext* vg, FileManager& fm, float w)
{
  bndBackground(vg, 0.0f, 0.0f, w, kTopBarHeight);

  float x = kPadX;
  const char* labels[3] = {"<", ">", "^"};
  const bool enabled[3] = {fm.canGoBack(), fm.canGoForward(), true};
  int action = -1;
  for (int i = 0; i < 3; i++)
  {
    bool hover = enabled[i] && inRect(g_mouseX, g_mouseY, x, kBtnY, kBtnSize, kBtnSize);
    BNDwidgetState st = hover ? BND_HOVER : BND_DEFAULT;
    bndOptionButton(vg, x, kBtnY, kBtnSize, kBtnSize, st, labels[i]);
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
    float bounds[4];
    nvgTextBounds(vg, 0.0f, 0.0f, crumbs[k].first.c_str(), nullptr, bounds);
    float tw = bounds[2] - bounds[0];
    float segW = tw + 12.0f;
    bool hover = inRect(g_mouseX, g_mouseY, x, 0.0f, segW, kTopBarHeight);
    if (hover)
    {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, x, kBtnY, segW, kBtnSize, 3.0f);
      nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
      nvgFill(vg);
    }
    nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
    nvgText(vg, x + 6.0f, cy, crumbs[k].first.c_str(), nullptr);
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
    bndOptionButton(vg, itemX, y, itemW, itemH, st, p.label.c_str());
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

static void drawMainHeader(NVGcontext* vg, float x, float y, float w)
{
  bndBackground(vg, x, y, w, kHeaderHeight);
  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 13.0f);
  nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float cy = y + kHeaderHeight * 0.5f;
  float cx = x + kPadX;
  nvgText(vg, cx, cy, "Name", nullptr);        cx += kColNameW;
  nvgText(vg, cx, cy, "Size", nullptr);        cx += kColSizeW;
  nvgText(vg, cx, cy, "Type", nullptr);        cx += kColTypeW;
  nvgText(vg, cx, cy, "Owner", nullptr);       cx += kColOwnerW;
  nvgText(vg, cx, cy, "Permissions", nullptr);
}

static void drawRows(NVGcontext* vg,
                     const FileManager& fm,
                     float x, float y, float w, float h,
                     float scrollOffset)
{
  nvgFontFace(vg, "sans");
  nvgFontSize(vg, 13.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  const auto& entries = fm.entries();

  nvgSave(vg);
  nvgScissor(vg, x, y, w, h);
  for (size_t i = 0; i < entries.size(); i++)
  {
    float rowY = y + static_cast<float>(i) * kRowHeight - scrollOffset;
    if (rowY + kRowHeight < y)
    {
      continue;
    }
    if (rowY > y + h)
    {
      break;
    }
    const Entry& e = entries[i];
    if (inRect(g_mouseX, g_mouseY, x, rowY, w, kRowHeight))
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, kRowHeight);
      nvgFillColor(vg, nvgRGBf(0.25f, 0.25f, 0.25f));
      nvgFill(vg);
    }
    nvgFillColor(vg, nvgRGBf(0.88f, 0.88f, 0.88f));
    float cy = rowY + kRowHeight * 0.5f;
    float cx = x + kPadX;
    nvgText(vg, cx, cy, e.name.c_str(), nullptr);      cx += kColNameW;
    nvgText(vg, cx, cy, e.sizeText.c_str(), nullptr);  cx += kColSizeW;
    nvgText(vg, cx, cy, e.typeText.c_str(), nullptr);  cx += kColTypeW;
    nvgText(vg, cx, cy, e.ownerText.c_str(), nullptr); cx += kColOwnerW;
    nvgText(vg, cx, cy, e.permText.c_str(), nullptr);
  }
  nvgRestore(vg);
}

int main()
{
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

  FileManager fileManager;
  std::vector<Place> places = buildPlaces();
  float scrollOffset = 0.0f;

  while (!glfwWindowShouldClose(window))
  {
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

    drawTopBar(vg, fileManager, w);

    float mainX = kSidebarWidth;
    float mainY = kTopBarHeight;
    float mainW = w - kSidebarWidth;
    float listTop = mainY + kHeaderHeight;
    float listH = h - listTop;

    float contentH = static_cast<float>(fileManager.entries().size()) * kRowHeight;
    float maxScroll = contentH - listH;
    if (maxScroll < 0.0f)
    {
      maxScroll = 0.0f;
    }
    scrollOffset -= g_scrollY * 40.0f;
    if (scrollOffset < 0.0f)
    {
      scrollOffset = 0.0f;
    }
    if (scrollOffset > maxScroll)
    {
      scrollOffset = maxScroll;
    }
    g_scrollY = 0.0f;

    if (g_mouseClicked && inRect(g_mouseX, g_mouseY, mainX, listTop, mainW, listH))
    {
      int idx = static_cast<int>((g_mouseY - listTop + scrollOffset) / kRowHeight);
      const auto& entries = fileManager.entries();
      if (idx >= 0 && idx < static_cast<int>(entries.size()))
      {
        const Entry& e = entries[idx];
        if (e.isDirectory)
        {
          fileManager.setPath(joinPath(fileManager.currentPath(), e.name));
          scrollOffset = 0.0f;
        }
      }
    }

    drawSidebar(vg, fileManager, places, h);
    drawMainHeader(vg, mainX, mainY, mainW);
    drawRows(vg, fileManager, mainX, listTop, mainW, listH, scrollOffset);

    nvgEndFrame(vg);

    glfwSwapBuffers(window);
    glfwPollEvents();
    g_mouseClicked = false;
  }

  nvgDeleteGL2(vg);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
