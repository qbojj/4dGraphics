#pragma once

#include "cppHelpers.hpp"

#include <SDL2/SDL.h>
#include <imgui.h>

#include <string>
#include <utility>

struct GLFWwindow;
struct ImGuiContext;
struct ImFontAtlas;

namespace v4dg {
class ImGuiRAIIContext {
public:
  explicit ImGuiRAIIContext(ImGuiRAIIContext &&o) : context(std::exchange(o.context, nullptr)) {}
  explicit ImGuiRAIIContext(ImFontAtlas *font = nullptr);
  ~ImGuiRAIIContext();

  ImGuiRAIIContext(const ImGuiRAIIContext &) = delete;
  
  ImGuiRAIIContext &operator=(ImGuiRAIIContext o) { std::swap(context, o.context); return *this; }
  operator ::ImGuiContext *() const { return context; }

private:
  ::ImGuiContext *context;
};

// calls SDL_Quit() on destruction
class SDL_GlobalContext {
public:
  ~SDL_GlobalContext();
};

class SDL_Context {
public:
  SDL_Context() = delete;
  explicit SDL_Context(Uint32 subsystems);
  ~SDL_Context();

  SDL_Context(const SDL_Context &) = delete;
  SDL_Context(SDL_Context &&o) : subsystems(std::exchange(o.subsystems, {})) {}

  SDL_Context &operator=(SDL_Context o) {
    std::swap(subsystems, o.subsystems);
    return *this;
  }

private:
  Uint32 subsystems;
};

class Window {
public:
  using native_type = ::SDL_Window *;

  Window() = delete;
  explicit Window(nullptr_t) : window(nullptr) {}
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
  SDL_Context sdlCtx{SDL_INIT_VIDEO};
  native_type window;
};

class ImGui_SDLImpl {
public:
  ImGui_SDLImpl() = delete;
  explicit ImGui_SDLImpl(const Window &window);
  ~ImGui_SDLImpl();

  ImGui_SDLImpl(const ImGui_SDLImpl &) = delete;
  ImGui_SDLImpl(ImGui_SDLImpl &&) = delete;
  ImGui_SDLImpl &operator=(const ImGui_SDLImpl &) = delete;
  ImGui_SDLImpl &operator=(ImGui_SDLImpl &&) = delete;
};

class GameEngine {
public:
  explicit GameEngine(Window window = {1024, 768, "4dGraphics"});
  virtual ~GameEngine();

  Window m_window;
  ImGuiRAIIContext m_ImGuiCtx;
  ImGui_SDLImpl m_ImGuiSdlImpl;
};

} // namespace v4dg