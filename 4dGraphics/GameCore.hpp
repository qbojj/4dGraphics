#pragma once

#include "cppHelpers.hpp"

#include <GLFW/glfw3.h>

#include <string>

struct SDL_Window;
struct ImGuiContext;
struct ImFontAtlas;

namespace v4dg {
class ImGuiRAIIContext {
public:
  ImGuiRAIIContext(ImGuiRAIIContext &&o) : context(std::exchange(o.context, nullptr)) {}
  ImGuiRAIIContext(ImFontAtlas *font = nullptr);
  ~ImGuiRAIIContext();

  ImGuiRAIIContext &operator=(ImGuiRAIIContext o) { std::swap(context, o.context); return *this; }
  operator ::ImGuiContext *() const { return context; }

  ImGuiRAIIContext(const ImGuiRAIIContext &) = delete;
  ImGuiRAIIContext &operator=(const ImGuiRAIIContext &) = delete;

private:
  ::ImGuiContext *context;
};

class Window {
public:
  using native_type = ::GLFWwindow *;

  Window() = delete;
  Window(nullptr_t) : window(nullptr) {}
  explicit Window(native_type window) : window(window) {}
  Window(int width, int height, const char *title);

  Window(const Window &) = delete;
  Window(Window &&o) : window(o.release()) {}
  ~Window();

  Window &operator=(Window o) { std::swap(window, o.window); return *this; } 

  operator native_type() const { return window; }

  native_type release() {
    return std::exchange(window, nullptr);
  }

private:
  native_type window;
};

class ImGui_GlfwImpl {
public:
  ImGui_GlfwImpl() = delete;
  ImGui_GlfwImpl(const Window &window);
  ~ImGui_GlfwImpl();

  ImGui_GlfwImpl(const ImGui_GlfwImpl &) = delete;
  ImGui_GlfwImpl(ImGui_GlfwImpl &&) = delete;
  ImGui_GlfwImpl &operator=(const ImGui_GlfwImpl &) = delete;
  ImGui_GlfwImpl &operator=(ImGui_GlfwImpl &&) = delete;
};

class GameEngine {
public:
  GameEngine(Window window = {1024, 768, "4dGraphics"});
  virtual ~GameEngine();

  Window m_window;
  ImGuiRAIIContext m_ImGuiCtx;
  ImGui_GlfwImpl m_ImGuiGlfwImpl;
};

} // namespace v4dg