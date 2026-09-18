#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glext.h>

#define NANOVG_GL2
#include "../third_party/nanovg/nanovg.h"
#include "../third_party/nanovg/nanovg_gl.h"
#include "../third_party/oui-blendish/blendish.h"
#include "FileManager.hpp"
#include "Modal.hpp"
#include "TextInput.hpp"
#include "HurmitFont.hpp"
#include "BlenderIcons.hpp"

#include <fstream>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>
#include <sys/statvfs.h>

namespace
{
  constexpr float kTabBarHeight = 26.0f;
  constexpr float kToolBarHeight = 40.0f;
  constexpr float kTopBarHeight = kTabBarHeight + kToolBarHeight;
  constexpr float kSidebarWidth = 220.0f;
  constexpr float kPreviewWidth = 280.0f;
  constexpr float kHeaderHeight = 26.0f;
  constexpr float kStatusBarHeight = 22.0f;
  float g_fontSize  = 16.0f;
  float g_rowHeight = 24.0f;
  constexpr float kPadX         =   8.0f;
  constexpr float kBtnSize      =  28.0f;
  constexpr float kBtnGap       =   4.0f;
  constexpr float kBtnY         = kTabBarHeight + (kToolBarHeight - kBtnSize) * 0.5f;
  constexpr double kDoubleClickTime = 0.4;
  constexpr float kIconSize     = 16.0f;
  constexpr float kIconGap      = 4.0f;
  constexpr float kMinColWidth  = 40.0f;
  constexpr int   kNumCols      = 5;
  constexpr float kMenuWidth      = 160.0f;
  constexpr float kMenuItemHeight =  26.0f;
  constexpr float kMenuPadY       =   4.0f;
  constexpr float kFilterBoxW     = 200.0f;
  constexpr float kFilterBoxH     =  24.0f;

  struct Place
  {
    std::string label;
    std::string path;
    int icon;
  };

  struct Section
  {
    std::string key;
    std::string title;
    bool collapsed = false;
    std::vector<Place> items;
  };

  enum class PendingInput
  {
    None,
    NewFolder,
    NewFile,
    Rename,
    GoToPath,
    Filter,
  };

  struct Editor
  {
    bool active = false;
    std::string path;
    std::vector<std::string> lines;
    int cursorLine = 0;
    int cursorCol = 0;
    int scrollLine = 0;
    int visibleLines = 20;
    bool dirty = false;
  };

  enum class MenuKind
  {
    None,
    Row,
    Empty,
  };

  enum class MenuAction
  {
    None,
    Open,
    EditHere,
    Restore,
    Copy,
    Cut,
    Paste,
    Rename,
    Delete,
    NewFolder,
    NewFile,
    Refresh,
    EmptyTrash,
    Properties,
    AddBookmark,
    RemoveBookmark,
  };

  struct MenuItem
  {
    const char* label;
    MenuAction action;
    bool enabled;
  };

  enum class PendingConfirm
  {
    None,
    DeleteEntry,
    RestoreEntry,
    EmptyTrash,
    EditorClose,
  };

  struct TabSnapshot
  {
    FileManager fm;
    int selectedIndex = -1;
    int selectionAnchor = -1;
    std::vector<int> selectedIndices;
    float scrollOffset = 0.0f;
    std::string lastPath;
    double lastClickTime = 0.0;
    int lastClickIndex = -1;
  };

  struct Theme
  {
    NVGcolor bg          = nvgRGBf(0.15f, 0.15f, 0.15f);
    NVGcolor text        = nvgRGBf(0.9f, 0.9f, 0.9f);
    NVGcolor rowSelected = nvgRGBf(0.2f, 0.35f, 0.55f);
    NVGcolor rowHover    = nvgRGBf(0.25f, 0.25f, 0.25f);
    NVGcolor rowStripe   = nvgRGBAf(1.0f, 1.0f, 1.0f, 0.035f);
  };

  struct Toast
  {
    std::string message;
    double shownAt = 0.0;
    double duration = 3.0;

    void show(const std::string& msg)
    {
      message = msg;
      shownAt = glfwGetTime();
    }

    bool active() const
    {
      if (message.empty())
      {
        return false;
      }
      return (glfwGetTime() - shownAt) < duration;
    }
  };

  struct AppState
  {
    FileManager fm;
    std::vector<Section> sections;
    Modal modal;
    TextInput textInput;
    PendingInput pendingInput = PendingInput::None;
    std::string renameOldName;
    PendingConfirm pendingConfirm = PendingConfirm::None;
  bool pendingDeleteIsTrash = true;
    std::vector<std::string> pendingDeleteNames;
    std::vector<std::string> pendingRestoreNames;
    MenuKind menuKind = MenuKind::None;
    float menuX = 0.0f;
    float menuY = 0.0f;
    int menuRowIndex = -1;
    float scrollOffset = 0.0f;
    int selectedIndex = -1;
    int selectionAnchor = -1;
    std::vector<int> selectedIndices;
    std::string lastPath;
    double lastClickTime = 0.0;
    int lastClickIndex = -1;
    float colWidths[kNumCols] = {260.0f, 90.0f, 110.0f, 100.0f, 110.0f};
    int dragColumn = -1;
    int hoveredSep = -1;
    float dragStartMouseX = 0.0f;
    float dragStartWidth = 0.0f;
    std::vector<TabSnapshot> tabs;
    int activeTab = 0;
    float tabScrollOffset = 0.0f;
    bool tabScrollToActive = true;
    Theme theme;
    Toast toast;
    bool previewVisible = true;
    int previewImage = -1;
    std::string previewPath;
    std::string previewText;
    Editor editor;
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
  bool  g_toggleHidden = false;
  bool  g_newFolder = false;
  bool  g_newFile = false;
  bool  g_rename = false;
  bool  g_delete = false;
  bool  g_forceDelete = false;
  bool  g_copy = false;
  bool  g_cut = false;
  bool  g_paste = false;
  bool  g_selectAll = false;
  bool  g_gotoPath = false;
  bool  g_filter = false;
  bool  g_refresh = false;
  bool  g_togglePreview = false;
  bool  g_newTab = false;
  bool  g_closeTab = false;
  bool  g_nextTab = false;
  bool  g_prevTab = false;
  int   g_mouseMods = 0;
  bool  g_rightClicked = false;
  bool g_sidebarDirty = false;
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

static bool parseThemeColor(const std::string& s, NVGcolor& out)
{
  float v[3] = {0.0f, 0.0f, 0.0f};
  size_t start = 0;
  for (int i = 0; i < 3; i++)
  {
    std::string tok;
    if (i < 2)
    {
      size_t comma = s.find(',', start);
      if (comma == std::string::npos)
      {
        return false;
      }
      tok = s.substr(start, comma - start);
      start = comma + 1;
    }
    else
    {
      tok = s.substr(start);
    }
    char* pEnd = nullptr;
    v[i] = std::strtof(tok.c_str(), &pEnd);
    if (pEnd == tok.c_str() || *pEnd != '\0')
    {
      return false;
    }
    if (v[i] < 0.0f)
    {
      v[i] = 0.0f;
    }
    if (v[i] > 1.0f)
    {
      v[i] = 1.0f;
    }
  }
  out = nvgRGBf(v[0], v[1], v[2]);
  return true;
}

static void loadConfig(AppState& app)
{
  std::ifstream in(configFilePath());
  if (!in)
  {
    return;
  }
  std::string savedPath;
  bool savedHidden = false;
  int savedSortField = -1;
  int savedSortDir = -1;
  std::vector<std::string> savedBookmarks;
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
    if (key.size() >= 4 && key.compare(0, 3, "col") == 0 &&
        key[3] >= '0' && key[3] <= '9')
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
    else if (key == "path")
    {
      savedPath = val;
    }
    else if (key == "hidden")
    {
      savedHidden = (val == "1");
    }
    else if (key == "sortField")
    {
      savedSortField = std::atoi(val.c_str());
    }
    else if (key == "sortDir")
    {
      savedSortDir = std::atoi(val.c_str());
    }
    else if (key == "fontSize")
    {
      float s = static_cast<float>(std::atof(val.c_str()));
      if (s >= 8.0f && s <= 48.0f)
      {
        g_fontSize = s;
      }
    }
    else if (key == "theme.bg")
    {
      parseThemeColor(val, app.theme.bg);
    }
    else if (key == "theme.text")
    {
      parseThemeColor(val, app.theme.text);
    }
    else if (key == "theme.rowSelected")
    {
      parseThemeColor(val, app.theme.rowSelected);
    }
    else if (key == "theme.rowHover")
    {
      parseThemeColor(val, app.theme.rowHover);
    }
    else if (key == "theme.rowStripe")
    {
      NVGcolor c;
      if (parseThemeColor(val, c))
      {
        app.theme.rowStripe = nvgRGBAf(c.r, c.g, c.b, 0.035f);
      }
    }
    else if (key.compare(0, 10, "collapsed_") == 0)
    {
      std::string secKey = key.substr(10);
      for (auto& s : app.sections)
      {
        if (s.key == secKey)
        {
          s.collapsed = (val == "1");
        }
      }
    }
    else if (key == "bookmark")
    {
      savedBookmarks.push_back(val);
    }
  }

  for (size_t i = 0; i < savedBookmarks.size(); i++)
  {
    const std::string& path = savedBookmarks[i];
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec))
    {
      continue;
    }
    for (auto& s : app.sections)
    {
      if (s.key != "bookmarks")
      {
        continue;
      }
      Place p;
      std::filesystem::path fp(path);
      p.label = fp.filename().string();
      if (p.label.empty())
      {
        p.label = path;
      }
      p.path = path;
      p.icon = BND_ICON_FILE_FOLDER;
      s.items.push_back(p);
      break;
    }
  }

  if (savedSortField >= 0 && savedSortField <= 4 && savedSortDir >= 0)
  {
    app.fm.setSort(static_cast<SortField>(savedSortField), savedSortDir == 0);
  }

  if (savedHidden)
  {
    app.fm.setShowHidden(true);
  }

  std::error_code ec;
  if (!savedPath.empty() && std::filesystem::is_directory(savedPath, ec))
  {
    app.fm.resetTo(savedPath);
  }
  g_rowHeight = g_fontSize + 10.0f;
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
  char colorBuf[64];
  std::snprintf(colorBuf, sizeof(colorBuf), "%.3f,%.3f,%.3f",
                app.theme.bg.r, app.theme.bg.g, app.theme.bg.b);
  out << "theme.bg=" << colorBuf << "\n";
  std::snprintf(colorBuf, sizeof(colorBuf), "%.3f,%.3f,%.3f",
                app.theme.text.r, app.theme.text.g, app.theme.text.b);
  out << "theme.text=" << colorBuf << "\n";
  std::snprintf(colorBuf, sizeof(colorBuf), "%.3f,%.3f,%.3f",
                app.theme.rowSelected.r, app.theme.rowSelected.g, app.theme.rowSelected.b);
  out << "theme.rowSelected=" << colorBuf << "\n";
  std::snprintf(colorBuf, sizeof(colorBuf), "%.3f,%.3f,%.3f",
                app.theme.rowHover.r, app.theme.rowHover.g, app.theme.rowHover.b);
  out << "theme.rowHover=" << colorBuf << "\n";
  std::snprintf(colorBuf, sizeof(colorBuf), "%.3f,%.3f,%.3f",
                app.theme.rowStripe.r, app.theme.rowStripe.g, app.theme.rowStripe.b);
  out << "theme.rowStripe=" << colorBuf << "\n";
  out << "path=" << app.fm.currentPath() << "\n";
  out << "hidden=" << (app.fm.showHidden() ? "1" : "0") << "\n";
  out << "sortField=" << static_cast<int>(app.fm.sortField()) << "\n";
  out << "sortDir=" << (app.fm.sortAscending() ? "0" : "1") << "\n";
  for (const auto& s : app.sections)
  {
    out << "collapsed_" << s.key << "=" << (s.collapsed ? "1" : "0") << "\n";
    if (s.key == "bookmarks")
    {
      for (const auto& bm : s.items)
      {
        out << "bookmark=" << bm.path << "\n";
      }
    }
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

static std::vector<Section> buildSections()
{
  const char* home = std::getenv("HOME");
  std::string h = (home != nullptr) ? std::string(home) : std::string("/");

  Section places;
  places.key = "places";
  places.title = "Places";
  places.items.push_back({"Home",      h,                BND_ICON_FILE_FOLDER});
  places.items.push_back({"Desktop",   h + "/Desktop",   BND_ICON_FILE_FOLDER});
  places.items.push_back({"Documents", h + "/Documents", BND_ICON_FILE_FOLDER});
  places.items.push_back({"Downloads", h + "/Downloads", BND_ICON_FILE_FOLDER});
  places.items.push_back({"Pictures",  h + "/Pictures",  BND_ICON_FILE_IMAGE});
  places.items.push_back({"Music",     h + "/Music",     BND_ICON_FILE_SOUND});
  places.items.push_back({"Videos",    h + "/Videos",    BND_ICON_FILE_MOVIE});
  std::string trashRoot = FileManager::trashRootDir();
  if (!trashRoot.empty())
  {
    places.items.push_back({"Trash", trashRoot + "/files", BND_ICON_FILE_BACKUP});
  }

  Section devices;
  devices.key = "devices";
  devices.title = "Devices";
  devices.items.push_back({"File System", "/", BND_ICON_DISK_DRIVE});

  std::vector<Section> all;
  std::error_code ec;
  for (auto& sec : {places, devices})
  {
    Section filtered;
    filtered.key = sec.key;
    filtered.title = sec.title;
    for (const auto& p : sec.items)
    {
      ec.clear();
      if (std::filesystem::is_directory(p.path, ec))
      {
        filtered.items.push_back(p);
      }
    }
    if (!filtered.items.empty())
    {
      all.push_back(filtered);
    }
  }
  Section bookmarks;
  bookmarks.key = "bookmarks";
  bookmarks.title = "Bookmarks";
  all.push_back(bookmarks);
  return all;
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

static void textInputBackspace(TextInput& t)
{
  if (t.cursor == 0 || t.value.empty())
  {
    return;
  }
  t.value.erase(t.cursor - 1, 1);
  t.cursor--;
}

static void textInputDelete(TextInput& t)
{
  if (t.cursor >= t.value.size())
  {
    return;
  }
  t.value.erase(t.cursor, 1);
}

static void textInputInsert(TextInput& t, unsigned int codepoint)
{
  if (codepoint < 32 || codepoint > 126)
  {
    return;
  }
  if (t.value.size() >= 255)
  {
    return;
  }
  t.value.insert(t.cursor, 1, static_cast<char>(codepoint));
  t.cursor++;
}

static bool loadEditorFile(const std::string& path, Editor& ed)
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return false;
  }
  ed.lines.clear();
  std::string line;
  while (std::getline(in, line))
  {
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }
    ed.lines.push_back(line);
  }
  if (ed.lines.empty())
  {
    ed.lines.push_back(std::string());
  }
  return true;
}

static bool saveEditorFile(const std::string& path, const Editor& ed)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
  {
    return false;
  }
  for (size_t i = 0; i < ed.lines.size(); i++)
  {
    out << ed.lines[i];
    if (i + 1 < ed.lines.size())
    {
      out << "\n";
    }
  }
  return out.good();
}

static void ensureEditorCursorVisible(Editor& ed)
{
  if (ed.visibleLines <= 0)
  {
    return;
  }
  if (ed.cursorLine < ed.scrollLine)
  {
    ed.scrollLine = ed.cursorLine;
  }
  else if (ed.cursorLine >= ed.scrollLine + ed.visibleLines)
  {
    ed.scrollLine = ed.cursorLine - ed.visibleLines + 1;
  }
}

static void handleEditorKey(AppState& app, int key, int mods)
{
  Editor& ed = app.editor;
  if (ed.lines.empty())
  {
    ed.lines.push_back(std::string());
  }
  if (ed.cursorLine < 0)
  {
    ed.cursorLine = 0;
  }
  if (ed.cursorLine >= static_cast<int>(ed.lines.size()))
  {
    ed.cursorLine = static_cast<int>(ed.lines.size()) - 1;
  }
  std::string& line = ed.lines[static_cast<size_t>(ed.cursorLine)];
  if (ed.cursorCol < 0)
  {
    ed.cursorCol = 0;
  }
  if (ed.cursorCol > static_cast<int>(line.size()))
  {
    ed.cursorCol = static_cast<int>(line.size());
  }

  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_S)
  {
    if (saveEditorFile(ed.path, ed))
    {
      ed.dirty = false;
      app.toast.show("Saved");
    }
    else
    {
      app.modal.openInfo("Error", "Could not save file.");
    }
    return;
  }
  if (key == GLFW_KEY_ESCAPE)
  {
    if (ed.dirty)
    {
      app.modal.openConfirm("Unsaved Changes", "Save before closing?");
      app.pendingConfirm = PendingConfirm::EditorClose;
    }
    else
    {
      ed.active = false;
      ed.lines.clear();
      ed.path.clear();
    }
    return;
  }
  if (key == GLFW_KEY_BACKSPACE)
  {
    if (ed.cursorCol > 0)
    {
      line.erase(line.begin() + (ed.cursorCol - 1));
      ed.cursorCol--;
      ed.dirty = true;
    }
    else if (ed.cursorLine > 0)
    {
      int prevLen = static_cast<int>(ed.lines[ed.cursorLine - 1].size());
      ed.lines[ed.cursorLine - 1] += line;
      ed.lines.erase(ed.lines.begin() + ed.cursorLine);
      ed.cursorLine--;
      ed.cursorCol = prevLen;
      ed.dirty = true;
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_DELETE)
  {
    if (ed.cursorCol < static_cast<int>(line.size()))
    {
      line.erase(line.begin() + ed.cursorCol);
      ed.dirty = true;
    }
    else if (ed.cursorLine + 1 < static_cast<int>(ed.lines.size()))
    {
      line += ed.lines[ed.cursorLine + 1];
      ed.lines.erase(ed.lines.begin() + (ed.cursorLine + 1));
      ed.dirty = true;
    }
    return;
  }
  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
  {
    std::string rest = line.substr(static_cast<size_t>(ed.cursorCol));
    line = line.substr(0, static_cast<size_t>(ed.cursorCol));
    ed.lines.insert(ed.lines.begin() + (ed.cursorLine + 1), rest);
    ed.cursorLine++;
    ed.cursorCol = 0;
    ed.dirty = true;
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_TAB)
  {
    line.insert(line.begin() + ed.cursorCol, ' ');
    line.insert(line.begin() + ed.cursorCol + 1, ' ');
    ed.cursorCol += 2;
    ed.dirty = true;
    return;
  }
  if (key == GLFW_KEY_LEFT)
  {
    if (ed.cursorCol > 0)
    {
      ed.cursorCol--;
    }
    else if (ed.cursorLine > 0)
    {
      ed.cursorLine--;
      ed.cursorCol = static_cast<int>(ed.lines[static_cast<size_t>(ed.cursorLine)].size());
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_RIGHT)
  {
    if (ed.cursorCol < static_cast<int>(line.size()))
    {
      ed.cursorCol++;
    }
    else if (ed.cursorLine + 1 < static_cast<int>(ed.lines.size()))
    {
      ed.cursorLine++;
      ed.cursorCol = 0;
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_UP)
  {
    if (ed.cursorLine > 0)
    {
      ed.cursorLine--;
      int len = static_cast<int>(ed.lines[static_cast<size_t>(ed.cursorLine)].size());
      if (ed.cursorCol > len)
      {
        ed.cursorCol = len;
      }
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_DOWN)
  {
    if (ed.cursorLine + 1 < static_cast<int>(ed.lines.size()))
    {
      ed.cursorLine++;
      int len = static_cast<int>(ed.lines[static_cast<size_t>(ed.cursorLine)].size());
      if (ed.cursorCol > len)
      {
        ed.cursorCol = len;
      }
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_HOME)
  {
    ed.cursorCol = 0;
    return;
  }
  if (key == GLFW_KEY_END)
  {
    ed.cursorCol = static_cast<int>(line.size());
    return;
  }
  if (key == GLFW_KEY_PAGE_UP)
  {
    ed.cursorLine -= ed.visibleLines;
    if (ed.cursorLine < 0)
    {
      ed.cursorLine = 0;
    }
    int len = static_cast<int>(ed.lines[static_cast<size_t>(ed.cursorLine)].size());
    if (ed.cursorCol > len)
    {
      ed.cursorCol = len;
    }
    ensureEditorCursorVisible(ed);
    return;
  }
  if (key == GLFW_KEY_PAGE_DOWN)
  {
    ed.cursorLine += ed.visibleLines;
    if (ed.cursorLine >= static_cast<int>(ed.lines.size()))
    {
      ed.cursorLine = static_cast<int>(ed.lines.size()) - 1;
    }
    int len = static_cast<int>(ed.lines[static_cast<size_t>(ed.cursorLine)].size());
    if (ed.cursorCol > len)
    {
      ed.cursorCol = len;
    }
    ensureEditorCursorVisible(ed);
    return;
  }
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  (void)scancode;
  AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  bool modalActive = (app != nullptr) && app->modal.active;
  bool inputActive = (app != nullptr) && app->textInput.active;
  bool editorActive = (app != nullptr) && app->editor.active;
  if (editorActive && !modalActive)
  {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
    {
      return;
    }
    handleEditorKey(*app, key, mods);
    return;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && app != nullptr &&
      app->menuKind != MenuKind::None)
  {
    app->menuKind = MenuKind::None;
    return;
  }
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
  {
    if (inputActive)
    {
      if (app->pendingInput == PendingInput::Filter)
      {
        app->fm.setFilter("");
        app->pendingInput = PendingInput::None;
        app->selectedIndices.clear();
        app->selectedIndex = -1;
        app->selectionAnchor = -1;
        app->scrollOffset = 0.0f;
      }
      app->textInput.close();
      return;
    }
    if (modalActive)
    {
      app->modal.close();
      return;
    }
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }
  if (inputActive)
  {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
    {
      return;
    }
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
    {
      app->textInput.result = TextInputResult::Ok;
      app->textInput.active = false;
    }
    else if (key == GLFW_KEY_BACKSPACE) textInputBackspace(app->textInput);
    else if (key == GLFW_KEY_DELETE)    textInputDelete(app->textInput);
    else if (key == GLFW_KEY_LEFT  && app->textInput.cursor > 0)
      app->textInput.cursor--;
    else if (key == GLFW_KEY_RIGHT && app->textInput.cursor < app->textInput.value.size())
      app->textInput.cursor++;
    else if (key == GLFW_KEY_HOME) app->textInput.cursor = 0;
    else if (key == GLFW_KEY_END)  app->textInput.cursor = app->textInput.value.size();
    if (app->pendingInput == PendingInput::Filter)
    {
      app->fm.setFilter(app->textInput.value);
      app->selectedIndices.clear();
      app->selectedIndex = -1;
      app->selectionAnchor = -1;
      app->scrollOffset = 0.0f;
    }
    return;
  }
  if (modalActive)
  {
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
    {
      return;
    }
    if (app->modal.type == ModalType::Confirm)
    {
      if (key == GLFW_KEY_TAB || key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT)
      {
        if (app->modal.focus < 0)
        {
          app->modal.focus = 0;
        }
        else
        {
          app->modal.focus = (app->modal.focus == 0) ? 1 : 0;
        }
      }
      else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
      {
        if (app->modal.focus >= 0)
        {
          app->modal.result = (app->modal.focus == 0) ? ModalResult::No : ModalResult::Yes;
          app->modal.active = false;
        }
      }
    }
    else if (app->modal.type == ModalType::Info)
    {
      if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
      {
        app->modal.result = ModalResult::Ok;
        app->modal.active = false;
      }
    }
    return;
  }
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
  {
    return;
  }
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_H)
  {
    g_toggleHidden = true;
  }

  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_N)
  {
    g_newFolder = true;
  }

  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_C) g_copy = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_X) g_cut = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_V) g_paste = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_A) g_selectAll = true;

  if (key == GLFW_KEY_UP)    g_navUp = true;
  if (key == GLFW_KEY_DOWN)  g_navDown = true;
  if (key == GLFW_KEY_ENTER) g_navEnter = true;
  if (key == GLFW_KEY_BACKSPACE) g_navBack = true;
  if (key == GLFW_KEY_F2) g_rename = true;
  if (key == GLFW_KEY_DELETE)
  {
    if ((mods & GLFW_MOD_SHIFT) != 0)
    {
      g_forceDelete = true;
    }
    else
    {
      g_delete = true;
    }
  }
  if (key == GLFW_KEY_F5) g_refresh = true;
  if (key == GLFW_KEY_F3) g_togglePreview = true;

  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_L) g_gotoPath = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_F) g_filter = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_T) g_newTab = true;
  if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_W) g_closeTab = true;
  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_TAB) g_prevTab = true;
  else if ((mods & GLFW_MOD_CONTROL) && key == GLFW_KEY_TAB) g_nextTab = true;
}

static void charCallback(GLFWwindow* window, unsigned int codepoint)
{
  AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (app == nullptr)
  {
    return;
  }
  if (app->editor.active)
  {
    if (codepoint >= 32 && codepoint <= 126)
    {
      Editor& ed = app->editor;
      if (ed.lines.empty())
      {
        ed.lines.push_back(std::string());
      }
      if (ed.cursorLine < 0)
      {
        ed.cursorLine = 0;
      }
      if (ed.cursorLine >= static_cast<int>(ed.lines.size()))
      {
        ed.cursorLine = static_cast<int>(ed.lines.size()) - 1;
      }
      std::string& line = ed.lines[static_cast<size_t>(ed.cursorLine)];
      if (ed.cursorCol < 0)
      {
        ed.cursorCol = 0;
      }
      if (ed.cursorCol > static_cast<int>(line.size()))
      {
        ed.cursorCol = static_cast<int>(line.size());
      }
      line.insert(line.begin() + ed.cursorCol, static_cast<char>(codepoint));
      ed.cursorCol++;
      ed.dirty = true;
    }
    return;
  }
  if (!app->textInput.active)
  {
    return;
  }
  textInputInsert(app->textInput, codepoint);
  if (app->pendingInput == PendingInput::Filter)
  {
    app->fm.setFilter(app->textInput.value);
    app->selectedIndices.clear();
    app->selectedIndex = -1;
    app->selectionAnchor = -1;
    app->scrollOffset = 0.0f;
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
  if (button == GLFW_MOUSE_BUTTON_LEFT)
  {
    if (action == GLFW_PRESS)
    {
      g_mouseClicked = true;
      g_mouseDown = true;
      g_mouseMods = mods;
    }
    else if (action == GLFW_RELEASE)
    {
      g_mouseDown = false;
    }
  }
  else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
  {
    g_rightClicked = true;
  }
}

static void scrollCallback(GLFWwindow* window, double x, double y)
{
  (void)x;
  AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
  if (app != nullptr && app->editor.active)
  {
    app->editor.scrollLine -= static_cast<int>(y);
    if (app->editor.scrollLine < 0)
    {
      app->editor.scrollLine = 0;
    }
    int maxScroll = static_cast<int>(app->editor.lines.size()) - app->editor.visibleLines;
    if (maxScroll < 0)
    {
      maxScroll = 0;
    }
    if (app->editor.scrollLine > maxScroll)
    {
      app->editor.scrollLine = maxScroll;
    }
    return;
  }
  if (g_mouseY < kTabBarHeight)
  {
    if (app != nullptr)
    {
      app->tabScrollOffset -= static_cast<float>(y) * 40.0f;
      if (app->tabScrollOffset < 0.0f)
      {
        app->tabScrollOffset = 0.0f;
      }
    }
    return;
  }
  g_scrollY += static_cast<float>(y);
}

static bool inRect(float mx, float my, float x, float y, float w, float h)
{
  return mx >= x && mx < x + w && my >= y && my < y + h;
}

static bool isEntrySelected(const AppState& app, int idx)
{
  for (size_t i = 0; i < app.selectedIndices.size(); i++)
  {
    if (app.selectedIndices[i] == idx)
    {
      return true;
    }
  }
  return false;
}

static void setSingleSelection(AppState& app, int idx)
{
  app.selectedIndices.clear();
  if (idx >= 0)
  {
    app.selectedIndices.push_back(idx);
  }
  app.selectedIndex = idx;
  app.selectionAnchor = idx;
}

static void toggleSelection(AppState& app, int idx)
{
  if (idx < 0)
  {
    return;
  }
  for (size_t i = 0; i < app.selectedIndices.size(); i++)
  {
    if (app.selectedIndices[i] == idx)
    {
      app.selectedIndices.erase(app.selectedIndices.begin() + static_cast<long>(i));
      app.selectedIndex = idx;
      return;
    }
  }
  app.selectedIndices.push_back(idx);
  app.selectedIndex = idx;
  app.selectionAnchor = idx;
}

static void selectRange(AppState& app, int anchor, int idx)
{
  if (anchor < 0 || idx < 0)
  {
    return;
  }
  int a = anchor;
  int b = idx;
  if (a > b)
  {
    int t = a;
    a = b;
    b = t;
  }
  app.selectedIndices.clear();
  for (int i = a; i <= b; i++)
  {
    app.selectedIndices.push_back(i);
  }
  app.selectedIndex = idx;
}

static void selectAllEntries(AppState& app)
{
  app.selectedIndices.clear();
  int n = static_cast<int>(app.fm.entries().size());
  for (int i = 0; i < n; i++)
  {
    app.selectedIndices.push_back(i);
  }
}

static void clearSelection(AppState& app)
{
  app.selectedIndices.clear();
  app.selectedIndex = -1;
  app.selectionAnchor = -1;
}

static void saveActiveIntoTab(AppState& app)
{
  if (app.activeTab < 0 || app.activeTab >= static_cast<int>(app.tabs.size()))
  {
    return;
  }
  TabSnapshot& t = app.tabs[app.activeTab];
  t.fm = app.fm;
  t.selectedIndex = app.selectedIndex;
  t.selectionAnchor = app.selectionAnchor;
  t.selectedIndices = app.selectedIndices;
  t.scrollOffset = app.scrollOffset;
  t.lastPath = app.lastPath;
  t.lastClickTime = app.lastClickTime;
  t.lastClickIndex = app.lastClickIndex;
}

static void loadActiveFromTab(AppState& app)
{
  if (app.activeTab < 0 || app.activeTab >= static_cast<int>(app.tabs.size()))
  {
    return;
  }
  const TabSnapshot& t = app.tabs[app.activeTab];
  app.fm = t.fm;
  app.selectedIndex = t.selectedIndex;
  app.selectionAnchor = t.selectionAnchor;
  app.selectedIndices = t.selectedIndices;
  app.scrollOffset = t.scrollOffset;
  app.lastPath = t.lastPath;
  app.lastClickTime = t.lastClickTime;
  app.lastClickIndex = t.lastClickIndex;
}

static void switchTab(AppState& app, int newIndex)
{
  if (newIndex < 0 || newIndex >= static_cast<int>(app.tabs.size()))
  {
    return;
  }
  if (newIndex == app.activeTab)
  {
    return;
  }
  saveActiveIntoTab(app);
  app.activeTab = newIndex;
  loadActiveFromTab(app);
  app.tabScrollToActive = true;
}

static void createNewTab(AppState& app)
{
  saveActiveIntoTab(app);
  TabSnapshot t;
  t.fm.resetTo(app.fm.currentPath());
  t.lastPath = app.fm.currentPath();
  app.tabs.push_back(t);
  app.activeTab = static_cast<int>(app.tabs.size()) - 1;
  loadActiveFromTab(app);
  app.tabScrollToActive = true;
}

static void closeTab(AppState& app, int index)
{
  if (app.tabs.size() <= 1)
  {
    return;
  }
  if (index < 0 || index >= static_cast<int>(app.tabs.size()))
  {
    return;
  }
  if (index == app.activeTab)
  {
    app.tabs.erase(app.tabs.begin() + index);
    if (app.activeTab >= static_cast<int>(app.tabs.size()))
    {
      app.activeTab = static_cast<int>(app.tabs.size()) - 1;
    }
    loadActiveFromTab(app);
    app.tabScrollToActive = true;
    return;
  }
  app.tabs.erase(app.tabs.begin() + index);
  if (app.activeTab > index)
  {
    app.activeTab--;
  }
}

static std::string tabLabelFor(const AppState& app, int index)
{
  std::string path;
  if (index == app.activeTab)
  {
    path = app.fm.currentPath();
  }
  else if (index >= 0 && index < static_cast<int>(app.tabs.size()))
  {
    path = app.tabs[index].fm.currentPath();
  }
  if (path.empty())
  {
    return std::string("/");
  }
  std::filesystem::path p(path);
  std::string name = p.filename().string();
  if (name.empty())
  {
    name = path;
  }
  return name;
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


static void showProperties(AppState& app, int index)
{
  const auto& entries = app.fm.entries();
  if (index < 0 || index >= static_cast<int>(entries.size()))
  {
    return;
  }
  const Entry& e = entries[index];
  std::string full = joinPath(app.fm.currentPath(), e.name);

  std::string msg;
  msg += "Name: " + e.name + "\n";
  msg += "Path: " + full + "\n";
  msg += "Type: " + e.typeText + "\n";
  if (e.isDirectory)
  {
    msg += "Size: " + e.sizeText + "\n";
  }
  else
  {
    msg += "Size: " + e.sizeText + " (" + std::to_string(e.sizeBytes) + " bytes)\n";
  }
  msg += "Owner: " + e.ownerText + "\n";
  msg += "Permissions: " + e.permText + "\n";
  msg += std::string("Executable: ") + (e.isExecutable ? "yes" : "no");

  app.modal.openInfo("Properties", msg);
}

static void drawSeparator(NVGcontext* vg, float x1, float y1, float x2, float y2)
{
  nvgBeginPath(vg);
  nvgMoveTo(vg, x1, y1);
  nvgLineTo(vg, x2, y2);
  nvgStrokeColor(vg, nvgRGBf(0.08f, 0.08f, 0.08f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);
}

static constexpr float kModalWidth  = 380.0f;
static constexpr float kModalHeight = 150.0f;
static constexpr float kModalBtnW   =  90.0f;
static constexpr float kModalBtnH   =  28.0f;
static constexpr float kModalBtnGap =  10.0f;

static void drawModalButton(
  NVGcontext* vg,
  float x,
  float y,
  float w,
  float h,
  const char* label,
  bool hover,
  bool focused = false)
{
  nvgBeginPath(vg);
  nvgRoundedRect(vg, x, y, w, h, 4.0f);
  nvgFillColor(vg, hover ? nvgRGBf(0.35f, 0.35f, 0.35f)
                         : nvgRGBf(0.27f, 0.27f, 0.27f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.12f, 0.12f, 0.12f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  if (focused)
  {
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 3.0f);
    nvgStrokeColor(vg, nvgRGBf(0.55f, 0.65f, 0.85f));
    nvgStrokeWidth(vg, 1.5f);
    nvgStroke(vg);
  }

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize);
  nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + w * 0.5f, y + h * 0.5f, label, nullptr);
}

static void drawModal(
  NVGcontext* vg,
  Modal& modal,
  float w,
  float h)
{
  if (!modal.active || modal.type == ModalType::None)
  {
    return;
  }

  nvgBeginPath(vg);
  nvgRect(vg, 0.0f, 0.0f, w, h);
  nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.55f));
  nvgFill(vg);

  int lineCount = 1;
  for (size_t i = 0; i < modal.message.size(); i++)
  {
    if (modal.message[i] == '\n')
    {
      lineCount++;
    }
  }
  float boxH = 94.0f + static_cast<float>(lineCount) * (g_fontSize + 6.0f);
  if (boxH < kModalHeight)
  {
    boxH = kModalHeight;
  }
  float px = (w - kModalWidth) * 0.5f;
  float py = (h - boxH) * 0.5f;

  nvgBeginPath(vg);
  nvgRoundedRect(vg, px, py, kModalWidth, boxH, 6.0f);
  nvgFillColor(vg, nvgRGBf(0.22f, 0.22f, 0.22f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize + 1.0f);
  nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, px + kModalWidth * 0.5f, py + 26.0f, modal.title.c_str(), nullptr);

  nvgFontSize(vg, g_fontSize);
  nvgFillColor(vg, nvgRGBf(0.85f, 0.85f, 0.85f));
  if (lineCount > 1)
  {
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, px + 16.0f, py + 50.0f, kModalWidth - 32.0f, modal.message.c_str(), nullptr);
  }
  else
  {
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, px + kModalWidth * 0.5f, py + 64.0f, modal.message.c_str(), nullptr);
  }

  float by = py + boxH - kModalBtnH - 16.0f;
  ModalResult clicked = ModalResult::None;

  if (modal.type == ModalType::Confirm)
  {
    float total = kModalBtnW * 2.0f + kModalBtnGap;
    float bx = px + (kModalWidth - total) * 0.5f;

    bool hoverNo = inRect(g_mouseX, g_mouseY, bx, by, kModalBtnW, kModalBtnH);
    if (hoverNo)
    {
      modal.focus = 0;
    }
    drawModalButton(vg, bx, by, kModalBtnW, kModalBtnH, "No", hoverNo, modal.focus == 0);
    if (hoverNo && g_mouseClicked)
    {
      clicked = ModalResult::No;
    }
    bx += kModalBtnW + kModalBtnGap;

    bool hoverYes = inRect(g_mouseX, g_mouseY, bx, by, kModalBtnW, kModalBtnH);
    if (hoverYes)
    {
      modal.focus = 1;
    }
    drawModalButton(vg, bx, by, kModalBtnW, kModalBtnH, "Yes", hoverYes, modal.focus == 1);
    if (hoverYes && g_mouseClicked)
    {
      clicked = ModalResult::Yes;
    }
  }
  else
  {
    float bx = px + (kModalWidth - kModalBtnW) * 0.5f;
    bool hoverOk = inRect(g_mouseX, g_mouseY, bx, by, kModalBtnW, kModalBtnH);
    if (hoverOk)
    {
      modal.focus = 0;
    }
    drawModalButton(vg, bx, by, kModalBtnW, kModalBtnH, "OK", hoverOk, modal.focus == 0);
    if (hoverOk && g_mouseClicked)
    {
      clicked = ModalResult::Ok;
    }
  }

  if (clicked != ModalResult::None)
  {
    modal.result = clicked;
    modal.active = false;
  }
}

static constexpr float kInputWidth  = 380.0f;
static constexpr float kInputHeight = 170.0f;

static void drawTextInput(NVGcontext* vg, TextInput& t, float w, float h)
{
  if (!t.active)
  {
    return;
  }

  nvgBeginPath(vg);
  nvgRect(vg, 0.0f, 0.0f, w, h);
  nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.55f));
  nvgFill(vg);

  float px = (w - kInputWidth) * 0.5f;
  float py = (h - kInputHeight) * 0.5f;

  nvgBeginPath(vg);
  nvgRoundedRect(vg, px, py, kInputWidth, kInputHeight, 6.0f);
  nvgFillColor(vg, nvgRGBf(0.22f, 0.22f, 0.22f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize + 1.0f);
  nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, px + kInputWidth * 0.5f, py + 26.0f, t.label.c_str(), nullptr);

  const float fieldPad = 10.0f;
  float fx = px + fieldPad;
  float fy = py + 50.0f;
  float fw = kInputWidth - fieldPad * 2.0f;
  float fh = 30.0f;

  nvgBeginPath(vg);
  nvgRoundedRect(vg, fx, fy, fw, fh, 3.0f);
  nvgFillColor(vg, nvgRGBf(0.15f, 0.15f, 0.15f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.35f, 0.35f, 0.35f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFontSize(vg, g_fontSize);
  nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float textX = fx + 8.0f;
  float textY = fy + fh * 0.5f;
  nvgText(vg, textX, textY, t.value.c_str(), nullptr);

  float bounds[4];
  std::string upToCursor = t.value.substr(0, t.cursor);
  nvgTextBounds(vg, 0.0f, 0.0f, upToCursor.c_str(), nullptr, bounds);
  float cursorX = textX + (bounds[2] - bounds[0]);

  nvgBeginPath(vg);
  nvgMoveTo(vg, cursorX, fy + 5.0f);
  nvgLineTo(vg, cursorX, fy + fh - 5.0f);
  nvgStrokeColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
  nvgStrokeWidth(vg, 1.5f);
  nvgStroke(vg);

  float by = py + kInputHeight - kModalBtnH - 16.0f;
  float total = kModalBtnW * 2.0f + kModalBtnGap;
  float bx = px + (kInputWidth - total) * 0.5f;

  bool hoverCancel = inRect(g_mouseX, g_mouseY, bx, by, kModalBtnW, kModalBtnH);
  drawModalButton(vg, bx, by, kModalBtnW, kModalBtnH, "Cancel", hoverCancel);
  if (hoverCancel && g_mouseClicked)
  {
    t.result = TextInputResult::Cancel;
    t.active = false;
    return;
  }
  bx += kModalBtnW + kModalBtnGap;

  bool hoverOk = inRect(g_mouseX, g_mouseY, bx, by, kModalBtnW, kModalBtnH);
  drawModalButton(vg, bx, by, kModalBtnW, kModalBtnH, "OK", hoverOk);
  if (hoverOk && g_mouseClicked)
  {
    t.result = TextInputResult::Ok;
    t.active = false;
  }
}

static std::string truncateToWidth(NVGcontext* vg, const std::string& text, float maxWidth);

static void drawTabBar(NVGcontext* vg, AppState& app, float w)
{
  bndBackground(vg, 0.0f, 0.0f, w, kTabBarHeight);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  const float tabH = kTabBarHeight - 4.0f;
  const float tabY = 2.0f;
  const float padX = 10.0f;
  const float gap = 2.0f;
  const float closeReserve = 18.0f;
  const float maxTabW = 200.0f;
  const float minTabW = 70.0f;
  const float plusW = 24.0f;
  const float plusGap = 4.0f;

  const int tabCount = static_cast<int>(app.tabs.size());

  std::vector<float> tabWidths;
  tabWidths.reserve(static_cast<size_t>(tabCount));
  float totalW = 0.0f;
  for (int i = 0; i < tabCount; i++)
  {
    std::string label = tabLabelFor(app, i);
    float bounds[4];
    nvgTextBounds(vg, 0.0f, 0.0f, label.c_str(), nullptr, bounds);
    float textW = bounds[2] - bounds[0];
    float tabW = textW + padX * 2.0f + closeReserve;
    if (tabW > maxTabW)
    {
      tabW = maxTabW;
    }
    if (tabW < minTabW)
    {
      tabW = minTabW;
    }
    tabWidths.push_back(tabW);
    totalW += tabW + gap;
  }

  const float barLeft = kPadX;
  const float barRight = w - kPadX - plusW - plusGap;
  const float availW = barRight - barLeft;
  if (availW <= 0.0f)
  {
    return;
  }

  float maxScroll = totalW - availW;
  if (maxScroll < 0.0f)
  {
    maxScroll = 0.0f;
  }
  if (app.tabScrollOffset < 0.0f)
  {
    app.tabScrollOffset = 0.0f;
  }
  if (app.tabScrollOffset > maxScroll)
  {
    app.tabScrollOffset = maxScroll;
  }

  if (app.tabScrollToActive && tabCount > 0)
  {
    float activeX = 0.0f;
    for (int i = 0; i < app.activeTab && i < tabCount; i++)
    {
      activeX += tabWidths[static_cast<size_t>(i)] + gap;
    }
    float activeW = (app.activeTab >= 0 && app.activeTab < tabCount)
      ? tabWidths[static_cast<size_t>(app.activeTab)] : 0.0f;
    if (activeX < app.tabScrollOffset)
    {
      app.tabScrollOffset = activeX;
    }
    if (activeX + activeW > app.tabScrollOffset + availW)
    {
      app.tabScrollOffset = activeX + activeW - availW;
    }
    if (app.tabScrollOffset < 0.0f)
    {
      app.tabScrollOffset = 0.0f;
    }
    if (app.tabScrollOffset > maxScroll)
    {
      app.tabScrollOffset = maxScroll;
    }
    app.tabScrollToActive = false;
  }

  nvgSave(vg);
  nvgScissor(vg, barLeft, 0.0f, availW, kTabBarHeight);

  float x = barLeft - app.tabScrollOffset;
  int closeIndex = -1;
  int switchTo = -1;

  for (int i = 0; i < tabCount; i++)
  {
    float tabW = tabWidths[static_cast<size_t>(i)];
    std::string label = tabLabelFor(app, i);
    float maxTextW = tabW - padX * 2.0f - closeReserve;
    if (maxTextW < 0.0f)
    {
      maxTextW = 0.0f;
    }
    std::string shown = truncateToWidth(vg, label, maxTextW);

    bool active = (i == app.activeTab);
    bool visible = (x + tabW > barLeft) && (x < barRight);
    bool hover = visible && inRect(g_mouseX, g_mouseY, x, tabY, tabW, tabH);
    bool closeHover = false;
    float closeX = x + tabW - closeReserve + 4.0f;
    if (hover)
    {
      closeHover = inRect(g_mouseX, g_mouseY, closeX, tabY + 4.0f, 12.0f, tabH - 8.0f);
    }

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, tabY, tabW, tabH, 4.0f);
    if (active)
    {
      nvgFillColor(vg, nvgRGBf(0.28f, 0.28f, 0.28f));
    }
    else if (hover)
    {
      nvgFillColor(vg, nvgRGBf(0.22f, 0.22f, 0.22f));
    }
    else
    {
      nvgFillColor(vg, nvgRGBf(0.18f, 0.18f, 0.18f));
    }
    nvgFill(vg);

    nvgFillColor(vg, active ? nvgRGBf(1.0f, 1.0f, 1.0f) : nvgRGBf(0.75f, 0.75f, 0.75f));
    nvgText(vg, x + padX, tabY + tabH * 0.5f, shown.c_str(), nullptr);

    if (tabCount > 1 && hover)
    {
      nvgFillColor(vg, closeHover ? nvgRGBf(1.0f, 0.4f, 0.4f) : nvgRGBf(0.7f, 0.7f, 0.7f));
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(vg, closeX + 6.0f, tabY + tabH * 0.5f, "x", nullptr);
      nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }

    if (hover && g_mouseClicked)
    {
      if (closeHover && tabCount > 1)
      {
        closeIndex = i;
      }
      else
      {
        switchTo = i;
      }
    }

    x += tabW + gap;
  }

  nvgRestore(vg);

  const float plusX = w - kPadX - plusW;
  bool plusHover = inRect(g_mouseX, g_mouseY, plusX, tabY, plusW, tabH);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, plusX, tabY, plusW, tabH, 4.0f);
  nvgFillColor(vg, plusHover ? nvgRGBf(0.3f, 0.3f, 0.3f) : nvgRGBf(0.2f, 0.2f, 0.2f));
  nvgFill(vg);
  nvgFillColor(vg, nvgRGBf(0.85f, 0.85f, 0.85f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, plusX + plusW * 0.5f, tabY + tabH * 0.5f, "+", nullptr);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  bool newTabRequested = (plusHover && g_mouseClicked);

  if (closeIndex >= 0)
  {
    closeTab(app, closeIndex);
  }
  else if (switchTo >= 0)
  {
    switchTab(app, switchTo);
  }
  else if (newTabRequested)
  {
    createNewTab(app);
  }
}

static void drawTopBar(NVGcontext* vg, AppState& app, float w)
{
  FileManager& fm = app.fm;
  bndBackground(vg, 0.0f, kTabBarHeight, w, kToolBarHeight);
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
  nvgFontSize(vg, g_fontSize);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float cy = kTabBarHeight + kToolBarHeight * 0.5f;

    std::string navTo;
  const float filterBoxX = w - kFilterBoxW - kPadX;
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
    if (filterBoxX > 0.0f && x + segW > filterBoxX)
    {
      break;
    }
    bool hover = inRect(g_mouseX, g_mouseY, x, kTabBarHeight, segW, kToolBarHeight);
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

  const float filterBoxY = kTabBarHeight + (kToolBarHeight - kFilterBoxH) * 0.5f;
  bool filterHover = inRect(g_mouseX, g_mouseY, filterBoxX, filterBoxY, kFilterBoxW, kFilterBoxH);
  nvgBeginPath(vg);
  nvgRoundedRect(vg, filterBoxX, filterBoxY, kFilterBoxW, kFilterBoxH, 3.0f);
  nvgFillColor(vg, filterHover ? nvgRGBf(0.28f, 0.28f, 0.28f) : nvgRGBf(0.18f, 0.18f, 0.18f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  const std::string& filterText = fm.filter();
  float filterTextY = filterBoxY + kFilterBoxH * 0.5f;
  nvgSave(vg);
  nvgScissor(vg, filterBoxX + 2.0f, filterBoxY, kFilterBoxW - 4.0f, kFilterBoxH);
  if (filterText.empty())
  {
    nvgFillColor(vg, nvgRGBf(0.5f, 0.5f, 0.5f));
    nvgText(vg, filterBoxX + 8.0f, filterTextY, "Filter...", nullptr);
  }
  else
  {
    nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
    nvgText(vg, filterBoxX + 8.0f, filterTextY, filterText.c_str(), nullptr);
  }
  nvgRestore(vg);

  if (filterHover && g_mouseClicked)
  {
    app.textInput.open("Filter", app.fm.filter());
    app.pendingInput = PendingInput::Filter;
  }

  if (!navTo.empty())
  {
    fm.setPath(navTo);
  }
}

static void drawTriangle(NVGcontext* vg, float cx, float cy, float size, bool pointingDown)
{
  float s = size * 0.5f;
  nvgBeginPath(vg);
  if (pointingDown)
  {
    nvgMoveTo(vg, cx - s, cy - s * 0.6f);
    nvgLineTo(vg, cx + s, cy - s * 0.6f);
    nvgLineTo(vg, cx,     cy + s * 0.6f);
  }
  else
  {
    nvgMoveTo(vg, cx - s * 0.6f, cy - s);
    nvgLineTo(vg, cx - s * 0.6f, cy + s);
    nvgLineTo(vg, cx + s * 0.6f, cy);
  }
  nvgClosePath(vg);
  nvgFillColor(vg, nvgRGBf(0.75f, 0.75f, 0.75f));
  nvgFill(vg);
}

static void drawSidebar(NVGcontext* vg,
                        FileManager& fm,
                        std::vector<Section>& sections,
                        float h)
{
  bndBackground(vg, 0.0f, kTopBarHeight, kSidebarWidth, h - kTopBarHeight);

  const float headerH = 24.0f;
  const float itemH   = 26.0f;
  const float itemGap = 2.0f;
  const float itemX   = 8.0f;
  const float itemW   = kSidebarWidth - 16.0f;

  float y = kTopBarHeight + 6.0f;
  std::string navTo;

  for (auto& sec : sections)
  {
    if (sec.items.empty())
    {
      continue;
    }
    bool headerHover = inRect(g_mouseX, g_mouseY, itemX, y, itemW, headerH);
    if (headerHover)
    {
      nvgBeginPath(vg);
      nvgRoundedRect(vg, itemX, y, itemW, headerH, 3.0f);
      nvgFillColor(vg, nvgRGBf(0.25f, 0.25f, 0.25f));
      nvgFill(vg);
    }

    const float triSize = 10.0f;
    float triCx = itemX + 6.0f + triSize * 0.5f;
    float triCy = y + headerH * 0.5f;
    drawTriangle(vg, triCx, triCy, triSize, !sec.collapsed);

    nvgFontFace(vg, "sans");
    nvgFontSize(vg, g_fontSize - 1.0f);
    nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, itemX + 6.0f + triSize + kIconGap, y + headerH * 0.5f,
            sec.title.c_str(), nullptr);

    if (headerHover && g_mouseClicked)
    {
      sec.collapsed = !sec.collapsed;
      g_sidebarDirty = true;
    }
    y += headerH + 2.0f;

    if (sec.collapsed)
    {
      y += 4.0f;
      continue;
    }

    for (const auto& p : sec.items)
    {
      bool hover   = inRect(g_mouseX, g_mouseY, itemX, y, itemW, itemH);
      bool current = (fm.currentPath() == p.path);
      BNDwidgetState st = BND_DEFAULT;
      if (current) st = BND_ACTIVE;
      else if (hover) st = BND_HOVER;
      bndToolButton(vg, itemX, y, itemW, itemH, BND_LEFT, st, p.icon, p.label.c_str());
      if (hover && g_mouseClicked)
      {
        navTo = p.path;
      }
      y += itemH + itemGap;
    }
    y += 6.0f;
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

static SortField columnSortField(int col)
{
  switch (col)
  {
    case 0: return SortField::Name;
    case 1: return SortField::Size;
    case 2: return SortField::Type;
    case 3: return SortField::Owner;
    default: return SortField::Permissions;
  }
}

static const char* columnLabel(int col)
{
  switch (col)
  {
    case 0: return " Name";
    case 1: return " Size";
    case 2: return " Type";
    case 3: return " Owner";
    default: return " Permissions";
  }
}

static void drawSortArrow(NVGcontext* vg, float cx, float cy, bool ascending)
{
  float s = 4.0f;
  nvgBeginPath(vg);
  if (ascending)
  {
    nvgMoveTo(vg, cx - s, cy + s * 0.6f);
    nvgLineTo(vg, cx + s, cy + s * 0.6f);
    nvgLineTo(vg, cx,     cy - s * 0.6f);
  }
  else
  {
    nvgMoveTo(vg, cx - s, cy - s * 0.6f);
    nvgLineTo(vg, cx + s, cy - s * 0.6f);
    nvgLineTo(vg, cx,     cy + s * 0.6f);
  }
  nvgClosePath(vg);
  nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
  nvgFill(vg);
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
  app.hoveredSep = hoverSep;
  if (hoverSep >= 0 && g_mouseClicked && app.dragColumn < 0)
  {
    app.dragColumn = hoverSep;
    app.dragStartMouseX = g_mouseX;
    app.dragStartWidth = app.colWidths[hoverSep];
  }

  if (g_mouseClicked && app.dragColumn < 0 && hoverSep < 0 &&
      g_mouseY >= y && g_mouseY < y + kHeaderHeight)
  {
    float colX = x + kPadX;
    for (int i = 0; i < kNumCols; i++)
    {
      float left = colX;
      colX += app.colWidths[i];
      if (g_mouseX >= left && g_mouseX < colX)
      {
        SortField clickedField = columnSortField(i);
        if (app.fm.sortField() == clickedField)
        {
          app.fm.setSort(clickedField, !app.fm.sortAscending());
        }
        else
        {
          app.fm.setSort(clickedField, true);
        }
        saveConfig(app);
        break;
      }
    }
  }

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  float cy = y + kHeaderHeight * 0.5f;

  for (int i = 0; i < kNumCols; i++)
  {
    float textX = columnX(app, x, i);
    if (i == 0)
    {
      textX += kIconSize + kIconGap;
    }
    nvgText(vg, textX, cy, columnLabel(i), nullptr);
    if (app.fm.sortField() == columnSortField(i))
    {
      float bounds[4];
      nvgTextBounds(vg, 0.0f, 0.0f, columnLabel(i), nullptr, bounds);
      drawSortArrow(vg, textX + (bounds[2] - bounds[0]) + 8.0f, cy, app.fm.sortAscending());
    }
  }

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

static std::string truncateToWidth(NVGcontext* vg, const std::string& text, float maxWidth)
{
  if (maxWidth <= 0.0f)
  {
    return std::string();
  }
  float bounds[4];
  nvgTextBounds(vg, 0.0f, 0.0f, text.c_str(), nullptr, bounds);
  if (bounds[2] - bounds[0] <= maxWidth)
  {
    return text;
  }
  const std::string ellipsis = "...";
  nvgTextBounds(vg, 0.0f, 0.0f, ellipsis.c_str(), nullptr, bounds);
  float ellipsisW = bounds[2] - bounds[0];
  float available = maxWidth - ellipsisW;
  if (available <= 0.0f)
  {
    return ellipsis;
  }
  size_t lo = 0;
  size_t hi = text.size();
  while (lo < hi)
  {
    size_t mid = (lo + hi + 1) / 2;
    std::string sub = text.substr(0, mid);
    nvgTextBounds(vg, 0.0f, 0.0f, sub.c_str(), nullptr, bounds);
    if (bounds[2] - bounds[0] <= available)
    {
      lo = mid;
    }
    else
    {
      hi = mid - 1;
    }
  }
  if (lo == 0)
  {
    return ellipsis;
  }
  return text.substr(0, lo) + ellipsis;
}

static std::string lowercaseExtension(const std::string& name)
{
  size_t dot = name.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= name.size())
  {
    return std::string();
  }
  std::string ext = name.substr(dot + 1);
  for (char& c : ext)
  {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return ext;
}

static bool isImageExtension(const std::string& ext)
{
  return ext == "png" || ext == "jpg" || ext == "jpeg" ||
         ext == "bmp" || ext == "gif" || ext == "tga"  ||
         ext == "psd" || ext == "hdr" || ext == "pic"  ||
         ext == "pnm";
}

static bool isTextExtension(const std::string& ext)
{
  return ext == "txt"  || ext == "md"   || ext == "log"  ||
         ext == "cpp"  || ext == "hpp"  || ext == "h"    ||
         ext == "c"    || ext == "cc"   || ext == "py"   ||
         ext == "sh"   || ext == "json" || ext == "xml"  ||
         ext == "yml"  || ext == "yaml" || ext == "toml" ||
         ext == "ini"  || ext == "cfg"  || ext == "conf" ||
         ext == "ts"   || ext == "js"   || ext == "rs"   ||
         ext == "go"   || ext == "java" || ext == "lua"  ||
         ext == "pl"   || ext == "rb"   || ext == "html" ||
         ext == "css";
}

static std::string currentSelectionPath(const AppState& app)
{
  const auto& entries = app.fm.entries();
  if (app.selectedIndex < 0 || app.selectedIndex >= static_cast<int>(entries.size()))
  {
    return std::string();
  }
  return joinPath(app.fm.currentPath(), entries[app.selectedIndex].name);
}

static void updatePreview(AppState& app, NVGcontext* vg)
{
  if (app.editor.active)
  {
    if (app.previewImage >= 0)
    {
      nvgDeleteImage(vg, app.previewImage);
      app.previewImage = -1;
    }
    app.previewPath.clear();
    app.previewText.clear();
    return;
  }
  if (!app.previewVisible)
  {
    if (app.previewImage >= 0)
    {
      nvgDeleteImage(vg, app.previewImage);
      app.previewImage = -1;
    }
    app.previewPath.clear();
    app.previewText.clear();
    return;
  }
  std::string path = currentSelectionPath(app);
  if (path == app.previewPath)
  {
    return;
  }
  if (app.previewImage >= 0)
  {
    nvgDeleteImage(vg, app.previewImage);
    app.previewImage = -1;
  }
  app.previewText.clear();
  app.previewPath = path;
  if (path.empty())
  {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec))
  {
    return;
  }
  std::filesystem::path p(path);
  std::string ext = lowercaseExtension(p.filename().string());
  if (isImageExtension(ext))
  {
    int img = nvgCreateImage(vg, path.c_str(), 0);
    if (img >= 0)
    {
      app.previewImage = img;
    }
    return;
  }
  if (!isTextExtension(ext))
  {
    return;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in)
  {
    return;
  }
  const size_t kMaxPreviewBytes = 64 * 1024;
  std::string buf;
  buf.resize(kMaxPreviewBytes);
  in.read(&buf[0], static_cast<std::streamsize>(kMaxPreviewBytes));
  buf.resize(static_cast<size_t>(in.gcount()));
  std::string cleaned;
  cleaned.reserve(buf.size());
  for (size_t i = 0; i < buf.size(); i++)
  {
    if (buf[i] != '\r')
    {
      cleaned.push_back(buf[i]);
    }
  }
  app.previewText = cleaned;
}

static void drawPreviewPanel(
  NVGcontext* vg,
  const AppState& app,
  float x,
  float y,
  float w,
  float h)
{
  bndBackground(vg, x, y, w, h);

  const float pad = 8.0f;
  const float headerH = 24.0f;

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  std::string title = "Preview";
  if (!app.previewPath.empty())
  {
    std::filesystem::path fp(app.previewPath);
    std::string name = fp.filename().string();
    if (!name.empty())
    {
      title = name;
    }
    else
    {
      title = app.previewPath;
    }
  }
  std::string shownTitle = truncateToWidth(vg, title, w - pad * 2.0f);
  nvgText(vg, x + pad, y + headerH * 0.5f, shownTitle.c_str(), nullptr);

  float contentY = y + headerH + pad;
  float contentH = h - headerH - pad * 2.0f;
  float contentW = w - pad * 2.0f;

  if (app.previewPath.empty())
  {
    nvgFillColor(vg, nvgRGBf(0.5f, 0.5f, 0.5f));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w * 0.5f, y + h * 0.5f, "No selection", nullptr);
    return;
  }

  if (app.previewImage >= 0)
  {
    int iw = 0;
    int ih = 0;
    nvgImageSize(vg, app.previewImage, &iw, &ih);
    if (iw > 0 && ih > 0)
    {
      float sx = contentW / static_cast<float>(iw);
      float sy = contentH / static_cast<float>(ih);
      float scale = sx < sy ? sx : sy;
      if (scale > 1.0f)
      {
        scale = 1.0f;
      }
      float dw = static_cast<float>(iw) * scale;
      float dh = static_cast<float>(ih) * scale;
      float dx = x + (w - dw) * 0.5f;
      float dy = contentY + (contentH - dh) * 0.5f;
      NVGpaint imgPaint = nvgImagePattern(vg, dx, dy, dw, dh, 0.0f, app.previewImage, 1.0f);
      nvgBeginPath(vg);
      nvgRect(vg, dx, dy, dw, dh);
      nvgFillPaint(vg, imgPaint);
      nvgFill(vg);
    }
    return;
  }

  if (!app.previewText.empty())
  {
    nvgSave(vg);
    nvgScissor(vg, x + pad, contentY, contentW, contentH);
    nvgFontSize(vg, g_fontSize - 2.0f);
    nvgFillColor(vg, nvgRGBf(0.85f, 0.85f, 0.85f));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float lineH = g_fontSize + 2.0f;
    int maxLines = static_cast<int>(contentH / lineH) + 1;
    int drawn = 0;
    float ty = contentY + lineH * 0.5f;
    size_t start = 0;
    while (start < app.previewText.size() && drawn < maxLines)
    {
      size_t nl = app.previewText.find('\n', start);
      if (nl == std::string::npos)
      {
        nl = app.previewText.size();
      }
      std::string line = app.previewText.substr(start, nl - start);
      std::string shown = truncateToWidth(vg, line, contentW);
      nvgText(vg, x + pad, ty, shown.c_str(), nullptr);
      ty += lineH;
      drawn++;
      start = nl + 1;
    }
    nvgRestore(vg);
    return;
  }

  nvgFillColor(vg, nvgRGBf(0.5f, 0.5f, 0.5f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + w * 0.5f, y + h * 0.5f, "No preview available", nullptr);
}

static void drawRows(NVGcontext* vg,
                     const AppState& app,
                     float x, float y, float w, float h)
{
  const auto& entries = app.fm.entries();

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  nvgSave(vg);
  nvgScissor(vg, x, y, w, h);
  for (size_t i = 0; i < entries.size(); i++)
  {
    float rowY = y + static_cast<float>(i) * g_rowHeight - app.scrollOffset;
    if (rowY + g_rowHeight < y)
    {
      continue;
    }
    if (rowY > y + h)
    {
      break;
    }
    if ((i % 2) == 0)
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, g_rowHeight);
      nvgFillColor(vg, app.theme.rowStripe);
      nvgFill(vg);
    }
    bool selected = isEntrySelected(app, static_cast<int>(i));
    bool hover = inRect(g_mouseX, g_mouseY, x, rowY, w, g_rowHeight);
    if (selected)
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, g_rowHeight);
      nvgFillColor(vg, app.theme.rowSelected);
      nvgFill(vg);
    }
    else if (hover)
    {
      nvgBeginPath(vg);
      nvgRect(vg, x, rowY, w, g_rowHeight);
      nvgFillColor(vg, app.theme.rowHover);
      nvgFill(vg);
    }
    const Entry& e = entries[i];
    float cy = rowY + g_rowHeight * 0.5f;
    float iconX = columnX(app, x, 0);
    float iconY = cy - kIconSize * 0.5f;
    bndIcon(vg, iconX, iconY, iconForEntry(e));

    const float cellPad = 4.0f;
    std::string nameText = truncateToWidth(vg, e.name,
                          app.colWidths[0] - kIconSize - kIconGap - cellPad);
    std::string sizeText = truncateToWidth(vg, e.sizeText,  app.colWidths[1] - cellPad);
    std::string typeText = truncateToWidth(vg, e.typeText,  app.colWidths[2] - cellPad);
    std::string ownerText = truncateToWidth(vg, e.ownerText, app.colWidths[3] - cellPad);

    nvgFillColor(vg, app.theme.text);
    nvgText(vg, iconX + kIconSize + kIconGap, cy, nameText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 1), cy, sizeText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 2), cy, typeText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 3), cy, ownerText.c_str(), nullptr);
    nvgText(vg, columnX(app, x, 4), cy, e.permText.c_str(), nullptr);
  }
  nvgRestore(vg);
}

static void drawEditor(NVGcontext* vg, AppState& app, float x, float y, float w, float h)
{
  Editor& ed = app.editor;

  nvgBeginPath(vg);
  nvgRect(vg, x, y, w, h);
  nvgFillColor(vg, nvgRGBf(0.12f, 0.12f, 0.12f));
  nvgFill(vg);

  if (ed.lines.empty())
  {
    ed.lines.push_back(std::string());
  }

  const float pad = 8.0f;
  const float headerH = 32.0f;
  const float lineH = g_fontSize + 4.0f;
  const float gutterW = 54.0f;

  nvgBeginPath(vg);
  nvgRect(vg, x, y, w, headerH);
  nvgFillColor(vg, nvgRGBf(0.18f, 0.18f, 0.18f));
  nvgFill(vg);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  std::filesystem::path fp(ed.path);
  std::string label = fp.filename().string();
  if (label.empty())
  {
    label = ed.path;
  }
  if (ed.dirty)
  {
    label += " *";
  }
  float labelMaxW = w - pad * 2.0f - 200.0f;
  if (labelMaxW < 40.0f)
  {
    labelMaxW = 40.0f;
  }
  std::string shownLabel = truncateToWidth(vg, label, labelMaxW);
  nvgText(vg, x + pad, y + headerH * 0.5f, shownLabel.c_str(), nullptr);

  const float btnW = 70.0f;
  const float btnH = 22.0f;
  const float btnGap = 6.0f;
  float btnY = y + (headerH - btnH) * 0.5f;

  float closeX = x + w - pad - btnW;
  bool closeHover = inRect(g_mouseX, g_mouseY, closeX, btnY, btnW, btnH);
  drawModalButton(vg, closeX, btnY, btnW, btnH, "Close", closeHover, false);
  if (closeHover && g_mouseClicked)
  {
    if (ed.dirty)
    {
      app.modal.openConfirm("Unsaved Changes", "Save before closing?");
      app.pendingConfirm = PendingConfirm::EditorClose;
    }
    else
    {
      ed.active = false;
      ed.lines.clear();
      ed.path.clear();
    }
  }

  float saveX = closeX - btnW - btnGap;
  bool saveHover = inRect(g_mouseX, g_mouseY, saveX, btnY, btnW, btnH);
  drawModalButton(vg, saveX, btnY, btnW, btnH, "Save", saveHover, false);
  if (saveHover && g_mouseClicked)
  {
    if (saveEditorFile(ed.path, ed))
    {
      ed.dirty = false;
      app.toast.show("Saved");
    }
    else
    {
      app.modal.openInfo("Error", "Could not save file.");
    }
  }

  float contentY = y + headerH;
  float contentH = h - headerH;

  nvgBeginPath(vg);
  nvgRect(vg, x, contentY, w, contentH);
  nvgFillColor(vg, nvgRGBf(0.12f, 0.12f, 0.12f));
  nvgFill(vg);

  int maxLines = static_cast<int>((contentH - pad * 2.0f) / lineH);
  if (maxLines < 1)
  {
    maxLines = 1;
  }
  ed.visibleLines = maxLines;

  if (ed.cursorLine < ed.scrollLine)
  {
    ed.scrollLine = ed.cursorLine;
  }
  if (ed.cursorLine >= ed.scrollLine + maxLines)
  {
    ed.scrollLine = ed.cursorLine - maxLines + 1;
  }
  if (ed.scrollLine < 0)
  {
    ed.scrollLine = 0;
  }
  int maxScroll = static_cast<int>(ed.lines.size()) - maxLines;
  if (maxScroll < 0)
  {
    maxScroll = 0;
  }
  if (ed.scrollLine > maxScroll)
  {
    ed.scrollLine = maxScroll;
  }

  nvgSave(vg);
  nvgScissor(vg, x, contentY, w, contentH);

  nvgFontSize(vg, g_fontSize - 1.0f);

  for (int i = 0; i < maxLines; i++)
  {
    int lineIdx = ed.scrollLine + i;
    if (lineIdx >= static_cast<int>(ed.lines.size()))
    {
      break;
    }
    float lineY = contentY + pad + static_cast<float>(i) * lineH + lineH * 0.5f;

    char numBuf[16];
    std::snprintf(numBuf, sizeof(numBuf), "%4d", lineIdx + 1);
    nvgFillColor(vg, nvgRGBf(0.45f, 0.45f, 0.45f));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + gutterW - 8.0f, lineY, numBuf, nullptr);

    nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 0.9f));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + gutterW, lineY, ed.lines[static_cast<size_t>(lineIdx)].c_str(), nullptr);

    if (lineIdx == ed.cursorLine)
    {
      std::string before = ed.lines[static_cast<size_t>(lineIdx)].substr(
        0, static_cast<size_t>(ed.cursorCol));
      float bounds[4];
      nvgTextBounds(vg, 0.0f, 0.0f, before.c_str(), nullptr, bounds);
      float cx = x + gutterW + (bounds[2] - bounds[0]);

      nvgBeginPath(vg);
      nvgMoveTo(vg, cx, lineY - lineH * 0.4f);
      nvgLineTo(vg, cx, lineY + lineH * 0.4f);
      nvgStrokeColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
      nvgStrokeWidth(vg, 1.5f);
      nvgStroke(vg);
    }
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
  clearSelection(app);
  app.scrollOffset = 0.0f;
  saveConfig(app);
}

static void handleListClick(AppState& app, float listX, float listTop, float listW, float listH)
{
  if (!g_mouseClicked || !inRect(g_mouseX, g_mouseY, listX, listTop, listW, listH))
  {
    return;
  }
  int idx = static_cast<int>((g_mouseY - listTop + app.scrollOffset) / g_rowHeight);
  int count = static_cast<int>(app.fm.entries().size());
  if (idx < 0 || idx >= count)
  {
    return;
  }

  const bool ctrl = (g_mouseMods & GLFW_MOD_CONTROL) != 0;
  const bool shift = (g_mouseMods & GLFW_MOD_SHIFT) != 0;

  if (ctrl || shift)
  {
    if (shift && app.selectionAnchor >= 0)
    {
      selectRange(app, app.selectionAnchor, idx);
    }
    else if (ctrl)
    {
      toggleSelection(app, idx);
    }
    app.lastClickIndex = -1;
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
    return;
  }
  setSingleSelection(app, idx);
}

static void handleKeyboardNav(AppState& app, float listH)
{
  int count = static_cast<int>(app.fm.entries().size());
  if (g_navUp && count > 0)
  {
    int newIdx = (app.selectedIndex <= 0) ? 0 : app.selectedIndex - 1;
    setSingleSelection(app, newIdx);
  }
  if (g_navDown && count > 0)
  {
    int newIdx = 0;
    if (app.selectedIndex >= 0 && app.selectedIndex < count - 1)
    {
      newIdx = app.selectedIndex + 1;
    }
    else if (app.selectedIndex >= count - 1)
    {
      newIdx = count - 1;
    }
    setSingleSelection(app, newIdx);
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
  if (g_toggleHidden)
  {
    app.fm.setShowHidden(!app.fm.showHidden());
    clearSelection(app);
    app.scrollOffset = 0.0f;
    g_toggleHidden = false;
  }
  if (g_newFolder)
  {
    app.textInput.open("New Folder", "");
    app.pendingInput = PendingInput::NewFolder;
    g_newFolder = false;
  }
  if (g_newFile)
  {
    app.textInput.open("New File", "");
    app.pendingInput = PendingInput::NewFile;
    g_newFile = false;
  }
  if (g_rename)
  {
    const auto& entries = app.fm.entries();
    if (app.selectedIndex >= 0 && app.selectedIndex < static_cast<int>(entries.size()))
    {
      app.renameOldName = entries[app.selectedIndex].name;
      app.textInput.open("Rename", app.renameOldName);
      app.pendingInput = PendingInput::Rename;
    }
    g_rename = false;
  }
  if (g_delete || g_forceDelete)
  {
    const auto& entries = app.fm.entries();
    std::vector<std::string> names;
    if (!app.selectedIndices.empty())
    {
      for (size_t i = 0; i < app.selectedIndices.size(); i++)
      {
        int sel = app.selectedIndices[i];
        if (sel >= 0 && sel < static_cast<int>(entries.size()))
        {
          names.push_back(entries[sel].name);
        }
      }
    }
    else if (app.selectedIndex >= 0 && app.selectedIndex < static_cast<int>(entries.size()))
    {
      names.push_back(entries[app.selectedIndex].name);
    }
    if (!names.empty())
    {
      bool isTrash = !g_forceDelete;
      app.pendingDeleteIsTrash = isTrash;
      const char* verb = isTrash ? "Move to trash" : "Permanently delete";
      std::string msg;
      if (names.size() == 1)
      {
        msg = std::string(verb) + ": \"" + names[0] + "\"?";
      }
      else
      {
        msg = std::string(verb) + " " + std::to_string(names.size()) + " items?";
      }
      app.pendingDeleteNames = names;
      app.modal.openConfirm(isTrash ? "Trash" : "Delete", msg);
      app.pendingConfirm = PendingConfirm::DeleteEntry;
    }
    g_delete = false;
    g_forceDelete = false;
  }

  if (g_copy)
  {
    const auto& entries = app.fm.entries();
    std::vector<std::string> names;
    if (!app.selectedIndices.empty())
    {
      for (size_t i = 0; i < app.selectedIndices.size(); i++)
      {
        int sel = app.selectedIndices[i];
        if (sel >= 0 && sel < static_cast<int>(entries.size()))
        {
          names.push_back(entries[sel].name);
        }
      }
    }
    else if (app.selectedIndex >= 0 && app.selectedIndex < static_cast<int>(entries.size()))
    {
      names.push_back(entries[app.selectedIndex].name);
    }
    if (!names.empty())
    {
      if (!app.fm.copyEntries(names))
      {
        app.modal.openInfo("Error", "Could not copy.");
      }
      else
      {
        app.toast.show("Copied");
      }
    }
    g_copy = false;
  }

  if (g_cut)
  {
    const auto& entries = app.fm.entries();
    std::vector<std::string> names;
    if (!app.selectedIndices.empty())
    {
      for (size_t i = 0; i < app.selectedIndices.size(); i++)
      {
        int sel = app.selectedIndices[i];
        if (sel >= 0 && sel < static_cast<int>(entries.size()))
        {
          names.push_back(entries[sel].name);
        }
      }
    }
    else if (app.selectedIndex >= 0 && app.selectedIndex < static_cast<int>(entries.size()))
    {
      names.push_back(entries[app.selectedIndex].name);
    }
    if (!names.empty())
    {
      if (!app.fm.cutEntries(names))
      {
        app.modal.openInfo("Error", "Could not cut.");
      }
      else
      {
        app.toast.show("Cut");
      }
    }
    g_cut = false;
  }

  if (g_paste)
  {
    if (app.fm.clipboardMode() != ClipboardMode::None)
    {
      if (!app.fm.paste())
      {
        app.modal.openInfo("Error", "Could not paste.");
      }
      else
      {
        app.toast.show("Pasted");
      }
      app.selectedIndex = -1;
      app.scrollOffset = 0.0f;
    }
    g_paste = false;
  }

  if (g_gotoPath)
  {
    app.textInput.open("Go to Path", app.fm.currentPath());
    app.pendingInput = PendingInput::GoToPath;
    g_gotoPath = false;
  }

  if (g_filter)
  {
    app.textInput.open("Filter", app.fm.filter());
    app.pendingInput = PendingInput::Filter;
    g_filter = false;
  }

  if (g_refresh)
  {
    if (!app.fm.refresh())
    {
      app.modal.openInfo("Error", "Could not refresh.");
    }
    clearSelection(app);
    app.scrollOffset = 0.0f;
    g_refresh = false;
  }

  if (g_togglePreview)
  {
    app.previewVisible = !app.previewVisible;
    g_togglePreview = false;
  }

  if (g_selectAll)
  {
    selectAllEntries(app);
    g_selectAll = false;
  }

  if (g_newTab)
  {
    createNewTab(app);
    g_newTab = false;
  }
  if (g_closeTab)
  {
    closeTab(app, app.activeTab);
    g_closeTab = false;
  }
  if (g_nextTab)
  {
    int n = static_cast<int>(app.tabs.size());
    if (n > 1)
    {
      int next = (app.activeTab + 1) % n;
      switchTab(app, next);
    }
    g_nextTab = false;
  }
  if (g_prevTab)
  {
    int n = static_cast<int>(app.tabs.size());
    if (n > 1)
    {
      int prev = (app.activeTab - 1 + n) % n;
      switchTab(app, prev);
    }
    g_prevTab = false;
  }

  g_navUp = false;
  g_navDown = false;
  g_navEnter = false;

  if (app.selectedIndex >= 0)
  {
    float rowTop = static_cast<float>(app.selectedIndex) * g_rowHeight;
    if (rowTop < app.scrollOffset)
    {
      app.scrollOffset = rowTop;
    }
    else if (rowTop + g_rowHeight > app.scrollOffset + listH)
    {
      app.scrollOffset = rowTop + g_rowHeight - listH;
    }
  }
}

static void handleTextInputResult(AppState& app)
{
  if (app.textInput.result == TextInputResult::None)
  {
    return;
  }
  TextInputResult r = app.textInput.result;
  app.textInput.result = TextInputResult::None;

  PendingInput pending = app.pendingInput;
  app.pendingInput = PendingInput::None;

  if (r != TextInputResult::Ok)
  {
    if (pending == PendingInput::Filter)
    {
      app.fm.setFilter("");
      clearSelection(app);
      app.scrollOffset = 0.0f;
    }
    return;
  }
  if (pending == PendingInput::NewFolder)
  {
    if (!app.fm.createDirectory(app.textInput.value))
    {
      app.modal.openInfo("Error", "Could not create folder.");
    }
    else
    {
      app.toast.show("Folder created: " + app.textInput.value);
    }
    app.selectedIndex = -1;
    app.scrollOffset = 0.0f;
  }
  if (pending == PendingInput::NewFile)
  {
    if (!app.fm.createFile(app.textInput.value))
    {
      app.modal.openInfo("Error", "Could not create file.");
    }
    else
    {
      app.toast.show("File created: " + app.textInput.value);
    }
    app.selectedIndex = -1;
    app.scrollOffset = 0.0f;
  }
  if (pending == PendingInput::Rename)
  {
    if (!app.fm.renameEntry(app.renameOldName, app.textInput.value))
    {
      app.modal.openInfo("Error", "Could not rename.");
    }
    else
    {
      app.toast.show("Renamed to " + app.textInput.value);
    }
    app.selectedIndex = -1;
    app.scrollOffset = 0.0f;
  }

  if (pending == PendingInput::GoToPath)
  {
    if (!app.fm.setPath(app.textInput.value))
    {
      app.modal.openInfo("Error", "Could not open path.");
    }
  }
}

static void handleModalResult(AppState& app)
{
  ModalResult r = app.modal.result;
  if (r == ModalResult::None)
  {
    return;
  }
  app.modal.result = ModalResult::None;
  PendingConfirm pending = app.pendingConfirm;
  app.pendingConfirm = PendingConfirm::None;

  if (pending == PendingConfirm::EditorClose)
  {
    if (r == ModalResult::Yes)
    {
      if (saveEditorFile(app.editor.path, app.editor))
      {
        app.toast.show("Saved");
      }
      else
      {
        app.modal.openInfo("Error", "Could not save file.");
        return;
      }
    }
    app.editor.active = false;
    app.editor.lines.clear();
    app.editor.path.clear();
    app.editor.dirty = false;
    return;
  }

  if (pending == PendingConfirm::RestoreEntry)
  {
    if (r == ModalResult::Yes)
    {
      bool anyFailed = false;
      for (size_t i = 0; i < app.pendingRestoreNames.size(); i++)
      {
        if (!app.fm.restoreEntry(app.pendingRestoreNames[i], true))
        {
          anyFailed = true;
        }
      }
      if (anyFailed)
      {
        app.modal.openInfo("Error", "Could not restore some items.");
      }
      else
      {
        app.toast.show("Restored");
      }
      clearSelection(app);
      app.scrollOffset = 0.0f;
    }
    app.pendingRestoreNames.clear();
    return;
  }

  if (pending == PendingConfirm::EmptyTrash)
  {
    if (r == ModalResult::Yes)
    {
      if (!app.fm.emptyTrash())
      {
        app.modal.openInfo("Error", "Could not empty trash.");
      }
      else
      {
        app.toast.show("Trash emptied");
      }
      clearSelection(app);
      app.scrollOffset = 0.0f;
    }
    return;
  }

  if (r != ModalResult::Yes)
  {
    app.pendingDeleteNames.clear();
    return;
  }
  if (pending == PendingConfirm::DeleteEntry)
  {
    bool anyFailed = false;
    const bool isTrash = app.pendingDeleteIsTrash;
    for (size_t i = 0; i < app.pendingDeleteNames.size(); i++)
    {
      const bool ok = isTrash
        ? app.fm.trashEntry(app.pendingDeleteNames[i])
        : app.fm.deleteEntry(app.pendingDeleteNames[i]);
      if (!ok)
      {
        anyFailed = true;
      }
    }
    if (anyFailed)
    {
      app.modal.openInfo("Error", isTrash
        ? "Could not move some items to trash."
        : "Could not delete some items.");
    }
    else
    {
      app.toast.show(isTrash ? "Moved to trash" : "Deleted");
    }
    app.pendingDeleteNames.clear();
    clearSelection(app);
    app.scrollOffset = 0.0f;
  }
}

static void applyScroll(AppState& app, float listH)
{
  float contentH = static_cast<float>(app.fm.entries().size()) * g_rowHeight;
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

static bool isBookmarked(const AppState& app, const std::string& fullPath)
{
  if (fullPath.empty())
  {
    return false;
  }
  for (const auto& s : app.sections)
  {
    if (s.key != "bookmarks")
    {
      continue;
    }
    for (const auto& item : s.items)
    {
      if (item.path == fullPath)
      {
        return true;
      }
    }
  }
  return false;
}

static bool menuItemEnabled(const AppState& app, const MenuItem& item, int rowIdx)
{
  if (!item.enabled)
  {
    return false;
  }
  if (item.action == MenuAction::AddBookmark)
  {
    const auto& entries = app.fm.entries();
    if (rowIdx < 0 || rowIdx >= static_cast<int>(entries.size()))
    {
      return false;
    }
    if (!entries[rowIdx].isDirectory)
    {
      return false;
    }
    std::string full = joinPath(app.fm.currentPath(), entries[rowIdx].name);
    return !isBookmarked(app, full);
  }
  if (item.action == MenuAction::RemoveBookmark)
  {
    const auto& entries = app.fm.entries();
    if (rowIdx < 0 || rowIdx >= static_cast<int>(entries.size()))
    {
      return false;
    }
    std::string full = joinPath(app.fm.currentPath(), entries[rowIdx].name);
    return isBookmarked(app, full);
  }
  if (item.action == MenuAction::EditHere)
  {
    const auto& entries = app.fm.entries();
    if (rowIdx < 0 || rowIdx >= static_cast<int>(entries.size()))
    {
      return false;
    }
    if (entries[rowIdx].isDirectory)
    {
      return false;
    }
    std::string ext = lowercaseExtension(entries[rowIdx].name);
    return isTextExtension(ext);
  }
  return true;
}


static bool isInsideTrash(const AppState& app)
{
  std::string root = FileManager::trashRootDir();
  if (root.empty())
  {
    return false;
  }
  return app.fm.currentPath() == (root + "/files");
}

static std::vector<MenuItem> buildRowMenuItems(const AppState& app)
{
  std::vector<MenuItem> items;
  items.push_back({"Open", MenuAction::Open, true});
  items.push_back({"Edit Here", MenuAction::EditHere, true});
  if (isInsideTrash(app))
  {
    items.push_back({"Restore", MenuAction::Restore, true});
  }
  items.push_back({"Copy", MenuAction::Copy, true});
  items.push_back({"Cut", MenuAction::Cut, true});
  items.push_back({"Rename", MenuAction::Rename, true});
  items.push_back({"Delete", MenuAction::Delete, true});
  items.push_back({"Properties", MenuAction::Properties, true});
  items.push_back({"Add to Bookmarks", MenuAction::AddBookmark, true});
  items.push_back({"Remove from Bookmarks", MenuAction::RemoveBookmark, true});
  return items;
}

static std::vector<MenuItem> buildEmptyMenuItems(const AppState& app)
{
  std::vector<MenuItem> items;
  items.push_back({"Paste", MenuAction::Paste, true});
  items.push_back({"New Folder", MenuAction::NewFolder, true});
  items.push_back({"New File", MenuAction::NewFile, true});
  if (isInsideTrash(app))
  {
    items.push_back({"Empty Trash", MenuAction::EmptyTrash, true});
  }
  items.push_back({"Refresh", MenuAction::Refresh, true});
  return items;
}

static MenuAction handleMenuClick(const AppState& app, float w, float h)
{
  if (app.menuKind == MenuKind::None)
  {
    return MenuAction::None;
  }
  std::vector<MenuItem> items = (app.menuKind == MenuKind::Row)
    ? buildRowMenuItems(app)
    : buildEmptyMenuItems(app);
  const int count = static_cast<int>(items.size());
  float menuH = kMenuPadY * 2.0f + kMenuItemHeight * static_cast<float>(count);

  float mx = app.menuX;
  float my = app.menuY;
  if (mx + kMenuWidth > w)
  {
    mx = w - kMenuWidth;
  }
  if (my + menuH > h)
  {
    my = h - menuH;
  }

  for (int i = 0; i < count; i++)
  {
    float iy = my + kMenuPadY + static_cast<float>(i) * kMenuItemHeight;
    if (inRect(g_mouseX, g_mouseY, mx, iy, kMenuWidth, kMenuItemHeight))
    {
      return menuItemEnabled(app, items[i], app.menuRowIndex) ? items[i].action : MenuAction::None;
    }
  }
  return MenuAction::None;
}

static void openContextMenu(
  AppState& app,
  float listX,
  float listTop,
  float listW,
  float listH)
{
  app.menuX = g_mouseX;
  app.menuY = g_mouseY;
  app.menuRowIndex = -1;
  if (inRect(g_mouseX, g_mouseY, listX, listTop, listW, listH))
  {
    int idx = static_cast<int>((g_mouseY - listTop + app.scrollOffset) / g_rowHeight);
    int count = static_cast<int>(app.fm.entries().size());
    if (idx >= 0 && idx < count)
    {
      if (!isEntrySelected(app, idx))
      {
        setSingleSelection(app, idx);
      }
      else
      {
        app.selectedIndex = idx;
      }
      app.menuKind = MenuKind::Row;
      app.menuRowIndex = idx;
      return;
    }
  }
  app.menuKind = MenuKind::Empty;
}

static void addBookmark(AppState& app, const std::string& fullPath)
{
  if (fullPath.empty())
  {
    return;
  }
  Section* pBookmarks = nullptr;
  for (auto& s : app.sections)
  {
    if (s.key == "bookmarks")
    {
      pBookmarks = &s;
      break;
    }
  }
  if (pBookmarks == nullptr)
  {
    return;
  }
  for (size_t i = 0; i < pBookmarks->items.size(); i++)
  {
    if (pBookmarks->items[i].path == fullPath)
    {
      return;
    }
  }
  Place p;
  std::filesystem::path fp(fullPath);
  p.label = fp.filename().string();
  if (p.label.empty())
  {
    p.label = fullPath;
  }
  p.path = fullPath;
  p.icon = BND_ICON_FILE_FOLDER;
  pBookmarks->items.push_back(p);
}


static void removeBookmark(AppState& app, const std::string& fullPath)
{
  if (fullPath.empty())
  {
    return;
  }
  for (auto& s : app.sections)
  {
    if (s.key != "bookmarks")
    {
      continue;
    }
    for (size_t i = 0; i < s.items.size(); i++)
    {
      if (s.items[i].path == fullPath)
      {
        s.items.erase(s.items.begin() + static_cast<long>(i));
        return;
      }
    }
  }
}

static void beginRestore(AppState& app)
{
  const auto& entries = app.fm.entries();
  std::vector<std::string> names;
  if (!app.selectedIndices.empty())
  {
    for (size_t i = 0; i < app.selectedIndices.size(); i++)
    {
      int sel = app.selectedIndices[i];
      if (sel >= 0 && sel < static_cast<int>(entries.size()))
      {
        names.push_back(entries[sel].name);
      }
    }
  }
  else if (app.selectedIndex >= 0 && app.selectedIndex < static_cast<int>(entries.size()))
  {
    names.push_back(entries[app.selectedIndex].name);
  }
  if (names.empty())
  {
    return;
  }
  bool anyConflict = false;
  for (size_t i = 0; i < names.size(); i++)
  {
    if (app.fm.isRestoreConflict(names[i]))
    {
      anyConflict = true;
      break;
    }
  }
  if (anyConflict)
  {
    app.pendingRestoreNames = names;
    std::string msg = "Some items already exist at their destinations. Overwrite?";
    if (names.size() == 1)
    {
      msg = "\"" + names[0] + "\" already exists at its destination. Overwrite?";
    }
    app.modal.openConfirm("Restore", msg);
    app.pendingConfirm = PendingConfirm::RestoreEntry;
    return;
  }
  bool anyFailed = false;
  for (size_t i = 0; i < names.size(); i++)
  {
    if (!app.fm.restoreEntry(names[i], false))
    {
      anyFailed = true;
    }
  }
  if (anyFailed)
  {
    app.modal.openInfo("Error", "Could not restore some items.");
  }
  else
  {
    app.toast.show("Restored");
  }
  clearSelection(app);
  app.scrollOffset = 0.0f;
}

static void beginEdit(AppState& app, int rowIdx)
{
  const auto& entries = app.fm.entries();
  if (rowIdx < 0 || rowIdx >= static_cast<int>(entries.size()))
  {
    return;
  }
  if (entries[rowIdx].isDirectory)
  {
    return;
  }
  std::string ext = lowercaseExtension(entries[rowIdx].name);
  if (!isTextExtension(ext))
  {
    return;
  }
  std::string full = joinPath(app.fm.currentPath(), entries[rowIdx].name);
  if (!loadEditorFile(full, app.editor))
  {
    app.modal.openInfo("Error", "Could not open file for editing.");
    return;
  }
  app.editor.active = true;
  app.editor.path = full;
  app.editor.cursorLine = 0;
  app.editor.cursorCol = 0;
  app.editor.scrollLine = 0;
  app.editor.dirty = false;
}

static void executeMenuAction(AppState& app, MenuAction action, int rowIdx)
{
  switch (action)
  {
    case MenuAction::Open:
      if (rowIdx >= 0)
      {
        openEntry(app, rowIdx);
      }
      break;
    case MenuAction::EditHere:
      beginEdit(app, rowIdx);
      break;
    case MenuAction::Restore:
      beginRestore(app);
      break;
    case MenuAction::Copy:      g_copy = true; break;
    case MenuAction::Cut:       g_cut = true; break;
    case MenuAction::Paste:     g_paste = true; break;
    case MenuAction::Rename:    g_rename = true; break;
    case MenuAction::Delete:    g_delete = true; break;
    case MenuAction::Properties:
      if (rowIdx >= 0)
      {
        showProperties(app, rowIdx);
      }
      break;
    case MenuAction::NewFolder: g_newFolder = true; break;
    case MenuAction::NewFile:   g_newFile = true; break;
    case MenuAction::EmptyTrash:
      app.modal.openConfirm("Empty Trash", "Permanently delete all items in Trash?");
      app.pendingConfirm = PendingConfirm::EmptyTrash;
      break;
    case MenuAction::Refresh:
      if (!app.fm.refresh())
      {
        app.modal.openInfo("Error", "Could not refresh.");
      }
      clearSelection(app);
      app.scrollOffset = 0.0f;
      break;
    case MenuAction::AddBookmark:
      if (rowIdx >= 0)
      {
        const auto& entries = app.fm.entries();
        if (rowIdx < static_cast<int>(entries.size()) && entries[rowIdx].isDirectory)
        {
          std::string full = joinPath(app.fm.currentPath(), entries[rowIdx].name);
          addBookmark(app, full);
          saveConfig(app);
        }
      }
      break;
    case MenuAction::RemoveBookmark:
      if (rowIdx >= 0)
      {
        const auto& entries = app.fm.entries();
        if (rowIdx < static_cast<int>(entries.size()))
        {
          std::string full = joinPath(app.fm.currentPath(), entries[rowIdx].name);
          removeBookmark(app, full);
          saveConfig(app);
        }
      }
      break;
    default:
      break;
  }
}

static void drawContextMenu(
  NVGcontext* vg,
  const AppState& app,
  float w,
  float h)
{
  if (app.menuKind == MenuKind::None)
  {
    return;
  }
  std::vector<MenuItem> items = (app.menuKind == MenuKind::Row)
    ? buildRowMenuItems(app)
    : buildEmptyMenuItems(app);
  const int count = static_cast<int>(items.size());
  float menuH = kMenuPadY * 2.0f + kMenuItemHeight * static_cast<float>(count);

  float mx = app.menuX;
  float my = app.menuY;
  if (mx + kMenuWidth > w)
  {
    mx = w - kMenuWidth;
  }
  if (my + menuH > h)
  {
    my = h - menuH;
  }
  if (mx < 0.0f)
  {
    mx = 0.0f;
  }
  if (my < 0.0f)
  {
    my = 0.0f;
  }

  nvgBeginPath(vg);
  nvgRoundedRect(vg, mx, my, kMenuWidth, menuH, 4.0f);
  nvgFillColor(vg, nvgRGBf(0.22f, 0.22f, 0.22f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize);
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  for (int i = 0; i < count; i++)
  {
    float iy = my + kMenuPadY + static_cast<float>(i) * kMenuItemHeight;
    bool hover = inRect(g_mouseX, g_mouseY, mx, iy, kMenuWidth, kMenuItemHeight);
    bool enabled = menuItemEnabled(app, items[i], app.menuRowIndex);
    if (hover && enabled)
    {
      nvgBeginPath(vg);
      nvgRect(vg, mx + 1.0f, iy, kMenuWidth - 2.0f, kMenuItemHeight);
      nvgFillColor(vg, nvgRGBf(0.3f, 0.3f, 0.3f));
      nvgFill(vg);
    }
    nvgFillColor(vg, enabled ? nvgRGBf(0.9f, 0.9f, 0.9f)
                             : nvgRGBf(0.5f, 0.5f, 0.5f));
    nvgText(vg, mx + 10.0f, iy + kMenuItemHeight * 0.5f, items[i].label, nullptr);
  }
}

static std::string humanSize(unsigned long long bytes)
{
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4)
  {
    value /= 1024.0;
    unit++;
  }
  char buf[32];
  if (unit == 0)
  {
    std::snprintf(buf, sizeof(buf), "%llu %s", bytes, units[unit]);
  }
  else
  {
    std::snprintf(buf, sizeof(buf), "%.1f %s", value, units[unit]);
  }
  return std::string(buf);
}


static void drawToast(NVGcontext* vg, const AppState& app, float w, float h)
{
  if (!app.toast.active())
  {
    return;
  }
  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 1.0f);
  float bounds[4];
  nvgTextBounds(vg, 0.0f, 0.0f, app.toast.message.c_str(), nullptr, bounds);
  float textW = bounds[2] - bounds[0];
  const float padX = 14.0f;
  const float padY = 8.0f;
  float boxW = textW + padX * 2.0f;
  float boxH = g_fontSize + padY * 2.0f;
  float boxX = (w - boxW) * 0.5f;
  float boxY = h - kStatusBarHeight - boxH - 12.0f;

  nvgBeginPath(vg);
  nvgRoundedRect(vg, boxX, boxY, boxW, boxH, 6.0f);
  nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.75f));
  nvgFill(vg);
  nvgStrokeColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.15f));
  nvgStrokeWidth(vg, 1.0f);
  nvgStroke(vg);

  nvgFillColor(vg, nvgRGBf(0.95f, 0.95f, 0.95f));
  nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(vg, boxX + boxW * 0.5f, boxY + boxH * 0.5f, app.toast.message.c_str(), nullptr);
}

static void drawStatusBar(NVGcontext* vg, const AppState& app, float x, float y, float w)
{
  bndBackground(vg, x, y, w, kStatusBarHeight);

  const size_t total = app.fm.entries().size();
  const size_t selCount = app.selectedIndices.size();

  std::string left;
  if (!app.fm.filter().empty())
  {
    left = "Filter \"" + app.fm.filter() + "\": ";
    if (selCount > 0)
    {
      left += std::to_string(selCount) + " of " + std::to_string(total) + " selected";
    }
    else
    {
      left += std::to_string(total) + " matches";
    }
  }
  else if (selCount > 0)
  {
    left = std::to_string(selCount) + " of " + std::to_string(total) + " selected";
  }
  else if (total == 1)
  {
    left = "1 item";
  }
  else
  {
    left = std::to_string(total) + " items";
  }

  nvgFontFace(vg, "sans");
  nvgFontSize(vg, g_fontSize - 2.0f);
  nvgFillColor(vg, nvgRGBf(0.7f, 0.7f, 0.7f));

  const float cy = y + kStatusBarHeight * 0.5f;
  nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(vg, x + kPadX, cy, left.c_str(), nullptr);

  struct statvfs st;
  if (statvfs(app.fm.currentPath().c_str(), &st) == 0)
  {
    unsigned long long freeBytes = static_cast<unsigned long long>(st.f_bavail) *
                                   static_cast<unsigned long long>(st.f_frsize);
    std::string right = humanSize(freeBytes) + " free";
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w - kPadX, cy, right.c_str(), nullptr);
  }
}


static std::string expandTilde(const std::string& p)
{
  if (p.empty() || p[0] != '~')
  {
    return p;
  }
  const char* home = std::getenv("HOME");
  if (home == nullptr)
  {
    return p;
  }
  if (p.size() == 1)
  {
    return std::string(home);
  }
  if (p[1] == '/')
  {
    return std::string(home) + p.substr(1);
  }
  return p;
}

int main(int argc, char** argv)
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
  GLFWcursor* resizeCursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
  glfwSwapInterval(1);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCharCallback(window, charCallback);
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
  glfwSetWindowUserPointer(window, &app);
  app.sections = buildSections();
  app.lastPath = app.fm.currentPath();
  loadConfig(app);
  app.tabs.push_back(TabSnapshot());
  saveActiveIntoTab(app);

  if (argc >= 2)
  {
    std::string requested = expandTilde(argv[1]);
    std::error_code ec;
    if (std::filesystem::is_directory(requested, ec))
    {
      app.fm.resetTo(std::filesystem::canonical(requested, ec).string());
      app.lastPath = app.fm.currentPath();
    }
    else if (std::filesystem::is_regular_file(requested, ec))
    {
      std::filesystem::path fp = std::filesystem::canonical(requested, ec);
      app.fm.resetTo(fp.parent_path().string());
      app.lastPath = app.fm.currentPath();
    }
    else
    {
      std::cerr << "rah: invalid path: " << argv[1] << std::endl;
    }
  }

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
    glClearColor(app.theme.bg.r, app.theme.bg.g, app.theme.bg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg, static_cast<float>(winW), static_cast<float>(winH), pxRatio);

    float w = static_cast<float>(winW);
    float h = static_cast<float>(winH);
    float previewW = (app.previewVisible && !app.editor.active) ? kPreviewWidth : 0.0f;
    float mainX = kSidebarWidth;
    float mainY = kTopBarHeight;
    float mainW = w - kSidebarWidth - previewW;
    float listTop = mainY + kHeaderHeight;
    float listH = h - listTop - kStatusBarHeight;

    resetOnPathChange(app);
    applyScroll(app, listH);
    bool popupActive = app.modal.active || app.textInput.active;
    bool clickBefore = g_mouseClicked;
    if (popupActive)
    {
      g_mouseClicked = false;
    }
    bool hadMenu = (app.menuKind != MenuKind::None);
    if (!popupActive)
    {
      if (g_rightClicked)
      {
        openContextMenu(
          app,
          mainX,
          listTop,
          mainW,
          listH);
        g_rightClicked = false;
      }
      else if (hadMenu && g_mouseClicked)
      {
        MenuAction menuAction = handleMenuClick(app, w, h);
        if (menuAction != MenuAction::None)
        {
          executeMenuAction(app, menuAction, app.menuRowIndex);
        }
        app.menuKind = MenuKind::None;
        g_mouseClicked = false;
      }
    }
    handleListClick(app, mainX, listTop, mainW, listH);
    resetOnPathChange(app);
    if (!popupActive)
    {
      handleKeyboardNav(app, listH);
    }
    handleTextInputResult(app);

    updatePreview(app, vg);

    drawTabBar(vg, app, w);
    drawTopBar(vg, app, w);
    resetOnPathChange(app);
    drawSidebar(vg, app.fm, app.sections, h);
    if (g_sidebarDirty)
    {
      saveConfig(app);
      g_sidebarDirty = false;
    }
    resetOnPathChange(app);
    if (app.editor.active)
    {
      drawEditor(vg, app, mainX, mainY, mainW, h - mainY - kStatusBarHeight);
    }
    else
    {
      drawMainHeader(vg, app, mainX, mainY, mainW);
      drawRows(vg, app, mainX, listTop, mainW, listH);
    }
    if (app.previewVisible && !app.editor.active)
    {
      drawPreviewPanel(vg, app, mainX + mainW, mainY, previewW, h - mainY - kStatusBarHeight);
    }
    drawStatusBar(vg, app, 0.0f, h - kStatusBarHeight, w);
    drawToast(vg, app, w, h);

    if (popupActive)
    {
      g_mouseClicked = clickBefore;
      drawModal(vg, app.modal, w, h);
      drawTextInput(vg, app.textInput, w, h);
    }

    handleModalResult(app);

    drawContextMenu(
      vg,
      app,
      w,
      h);

    bool resizeHover = (app.hoveredSep >= 0) || (app.dragColumn >= 0);
    if (app.modal.active || app.textInput.active)
    {
      resizeHover = false;
    }
    if (app.modal.active)
    {
      resizeHover = false;
    }
    glfwSetCursor(window, resizeHover ? resizeCursor : nullptr);
    drawSeparator(vg, 0.0f, kTabBarHeight, w, kTabBarHeight);
    drawSeparator(vg, 0.0f, kTopBarHeight, w, kTopBarHeight);
    drawSeparator(vg, kSidebarWidth, kTopBarHeight, kSidebarWidth, h - kStatusBarHeight);
    if (app.previewVisible && !app.editor.active)
    {
      drawSeparator(vg, mainX + mainW, kTopBarHeight, mainX + mainW, h - kStatusBarHeight);
    }
    if (!app.editor.active)
    {
      drawSeparator(vg, mainX, mainY + kHeaderHeight, mainX + mainW, mainY + kHeaderHeight);
    }
    drawSeparator(vg, 0.0f, h - kStatusBarHeight, w, h - kStatusBarHeight);
    nvgEndFrame(vg);

    glfwSwapBuffers(window);
    g_mouseClicked = false;
    g_rightClicked = false;
  }

  saveConfig(app);
  if (app.previewImage >= 0)
  {
    nvgDeleteImage(vg, app.previewImage);
    app.previewImage = -1;
  }
  nvgDeleteGL2(vg);
  if (resizeCursor != nullptr)
  {
    glfwDestroyCursor(resizeCursor);
  }
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
