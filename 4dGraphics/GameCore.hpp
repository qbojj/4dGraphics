#pragma once

#include <SDL2/SDL.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <imgui.h>

#include <cstddef>
#include <utility>

struct GLFWwindow;
struct ImGuiContext;
struct ImFontAtlas;

namespace v4dg {
class ImGuiRAIIContext {
public:
  explicit ImGuiRAIIContext(ImFontAtlas *font = nullptr);
  ImGuiRAIIContext(const ImGuiRAIIContext &) = delete;
  ImGuiRAIIContext &operator=(const ImGuiRAIIContext &) = delete;
  ImGuiRAIIContext(ImGuiRAIIContext &&) noexcept;
  ImGuiRAIIContext &operator=(ImGuiRAIIContext &&) noexcept;
  ~ImGuiRAIIContext();

  operator ::ImGuiContext *() const { return context; }

private:
  ::ImGuiContext *context;
};

// calls SDL_Quit() on destruction
class SDL_GlobalContext {
public:
  explicit SDL_GlobalContext() = default;
  SDL_GlobalContext(const SDL_GlobalContext &) = delete;
  SDL_GlobalContext &operator=(const SDL_GlobalContext &) = delete;
  SDL_GlobalContext(SDL_GlobalContext &&) = delete;
  SDL_GlobalContext &operator=(SDL_GlobalContext &&) = delete;
  ~SDL_GlobalContext();
};

class SDL_Context {
public:
  SDL_Context() noexcept : subsystems(0) {};
  explicit SDL_Context(Uint32 subsystems);

  SDL_Context(const SDL_Context &) = delete;
  SDL_Context(SDL_Context &&o) noexcept;

  SDL_Context &operator=(const SDL_Context &) = delete;
  SDL_Context &operator=(SDL_Context &&o) noexcept;
  ~SDL_Context();

private:
  Uint32 subsystems;
};

class Window {
public:
  using native_type = ::SDL_Window *;

  Window() = delete;
  explicit Window(std::nullptr_t) : window(nullptr) {}
  explicit Window(native_type window) : window(window) {}
  Window(int width, int height, const char *title);

  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;
  Window(Window &&o) noexcept;
  Window &operator=(Window &&) noexcept;
  ~Window();

  operator native_type() const { return window; }

  native_type release() { return std::exchange(window, nullptr); }

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
  static constexpr auto default_width = 1024;
  static constexpr auto default_height = 768;

  explicit GameEngine(Window window = {default_width, default_height, "4dGraphics"});
  virtual ~GameEngine();

  GameEngine(const GameEngine &) = delete;
  GameEngine &operator=(const GameEngine &) = delete;
  GameEngine(GameEngine &&) = delete;
  GameEngine &operator=(GameEngine &&) = delete;

  Window &window() { return m_window; }
  ::ImGuiContext *imgui_context() { return m_ImGuiCtx; }

private:
  Window m_window;
  ImGuiRAIIContext m_ImGuiCtx;
  ImGui_SDLImpl m_ImGuiSdlImpl;
};

} // namespace v4dg
