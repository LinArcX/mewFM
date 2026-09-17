#include <GLFW/glfw3.h>

#include <GL/gl.h>
#include <GL/glext.h>

#define NANOVG_GL2
#include "../third_party/nanovg/nanovg.h"
#include "../third_party/nanovg/nanovg_gl.h"
#include "../third_party/oui-blendish/blendish.h"
#include "FileManager.hpp"

#include <iostream>
#include <string>
#include "DejaVuSansFont.hpp"
#include "BlenderIcons.hpp"

static BNDwidgetTheme makeDefaultWidgetTheme()
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

static void applyTheme()
{
  BNDwidgetTheme w = makeDefaultWidgetTheme();
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
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
  {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }
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

  GLFWwindow* window = glfwCreateWindow(800, 600, "File Manager", nullptr, nullptr);
  if (!window)
  {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  glfwSetKeyCallback(window, keyCallback);

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

  while (!glfwWindowShouldClose(window))
  {
    int winWidth, winHeight;
    glfwGetFramebufferSize(window, &winWidth, &winHeight);

    glViewport(0, 0, winWidth, winHeight);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg, winWidth, winHeight, 1.0f);

    float y = 20.0f;
    const auto& entries = fileManager.entries();
    for (const auto& entry : entries)
    {
      bndOptionButton(vg, 20.0f, y, 200.0f, 24.0f, BND_DEFAULT, entry.c_str());
      y += 28.0f;
    }

    nvgEndFrame(vg);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  nvgDeleteGL2(vg);
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
