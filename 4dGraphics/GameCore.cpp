#include "GameCore.hpp"

#include <Debug.hpp>
#include <v4dgCore.hpp>

#include <SDL2/SDL.h>
#include <SDL_error.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>

#include <cstring>
#include <stdexcept>
#include <utility>

using namespace v4dg;
namespace {
[[noreturn]] void sdl_error_to_exception() {
  throw exception("STL error: {}", SDL_GetError());
}
} // namespace

ImGuiRAIIContext::ImGuiRAIIContext(ImGuiRAIIContext &&o) noexcept
    : context(std::exchange(o.context, nullptr)) {}

ImGuiRAIIContext &ImGuiRAIIContext::operator=(ImGuiRAIIContext &&o) noexcept {
  if (this == &o) {
    return *this;
  }
  ImGui::DestroyContext(context);
  context = std::exchange(o.context, nullptr);
  return *this;
}
ImGuiRAIIContext::ImGuiRAIIContext(ImFontAtlas *font)
    : context(ImGui::CreateContext(font)) {
  if (context == nullptr) {
    throw exception("Cannot initialize ImGui");
  }
  if (!IMGUI_CHECKVERSION()) {
    throw exception("Imgui runtime version not equal to what version "
                    "program was compiled with");
  }
  ImGui::SetCurrentContext(context);
}

ImGuiRAIIContext::~ImGuiRAIIContext() { ImGui::DestroyContext(context); }

SDL_GlobalContext::~SDL_GlobalContext() { SDL_Quit(); }

SDL_Context::SDL_Context(Uint32 subsystems) : subsystems(subsystems) {
  if (SDL_InitSubSystem(subsystems) != 0) {
    sdl_error_to_exception();
  }
}

SDL_Context::SDL_Context(SDL_Context &&o) noexcept
    : subsystems(std::exchange(o.subsystems, 0)) {}

SDL_Context &SDL_Context::operator=(SDL_Context &&o) noexcept {
  if (this == &o) {
    return *this;
  }

  if (subsystems != 0) {
    SDL_QuitSubSystem(subsystems);
  }

  subsystems = std::exchange(o.subsystems, 0);
  return *this;
}

SDL_Context::~SDL_Context() { SDL_QuitSubSystem(subsystems); }

Window::Window(int width, int height, const char *title)
    : window(SDL_CreateWindow(title, 0, 0, width, height,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE)) {

  if (window == nullptr) {
    sdl_error_to_exception();
  }
}

Window::Window(Window &&o) noexcept
    : sdlCtx(std::move(o.sdlCtx)), window(std::exchange(o.window, nullptr)) {}

Window &Window::operator=(Window &&o) noexcept {
  if (this == &o) {
    return *this;
  }
  sdlCtx = std::exchange(o.sdlCtx, SDL_Context{0});
  SDL_DestroyWindow(window);
  window = std::exchange(o.window, nullptr);
  return *this;
}

Window::~Window() { SDL_DestroyWindow(window); }

ImGui_SDLImpl::ImGui_SDLImpl(const Window &window) {
  auto &io = ImGui::GetIO();

  if ((io.Fonts->AddFontDefault() == nullptr) || !io.Fonts->Build()) {
    throw std::runtime_error("Could not initialize imgui font");
  }

  ImGui_ImplSDL2_InitForVulkan(window);
}

ImGui_SDLImpl::~ImGui_SDLImpl() { ImGui_ImplSDL2_Shutdown(); }

GameEngine::GameEngine(Window window)
    : m_window(std::move(window)), m_ImGuiSdlImpl(m_window) {}

GameEngine::~GameEngine() = default;

/*
void GameEngine::EngineLoop(void* wData)
{
        WindowDataContainer* windowData = (WindowDataContainer*)wData;
        GameEngine* gameEngine = windowData->pGameEngine;
        GLFWwindow* window = windowData->window;

        {
                OPTICK_APP( "4dGraphics" );

                void* FData = NULL;
                try
                {
                        TRACE( DebugLevel::Log, "Before OnCreate\n" );
                        if(
                                gameEngine->m_pInputHandler->OnCreate( window )
&& gameEngine->m_pGameHandler->OnCreate( window ) &&
                                gameEngine->m_pRenderHandler->OnCreate( window )
                                )
                        {
                                TRACE( DebugLevel::Log, "After OnCreate\n" );

                                FData = gameEngine->m_pGameHandler->NewFData();

                                if( !FData ) throw runtime_error( "couldn't
create Frame Data" );

                                while( !glfwWindowShouldClose( window ) )
                                {
                                        {
                                                OPTICK_FRAME_EVENT(
Optick::FrameType::CPU );

                                                {
                                                        OPTICK_CATEGORY(
"process messages", Optick::Category::IO ); glfwPollEvents();
                                                }

                                                {
                                                        OPTICK_CATEGORY(
"Process new Frame", Optick::Category::GameLogic );
                                                        gameEngine->m_pInputHandler->OnPreTick();
                                                        gameEngine->m_pGameHandler->OnTick(
FData, gameEngine->m_pInputHandler ); gameEngine->m_pInputHandler->OnPostTick();
                                                }
                                        }


                                        {
                                                OPTICK_FRAME_EVENT(
Optick::FrameType::Render );

                                                {
                                                        OPTICK_CATEGORY( "render
frame", Optick::Category::Rendering ); gameEngine->m_pRenderHandler->OnDraw(
FData );
                                                }
                                        }
                                }
                        }
                        else TRACE( DebugLevel::FatalError, "OnCreate returned
false\n" );
                }
                catch( const std::exception &e ) { TRACE(
DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); } catch(
ShutdownException ) {} catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown
Exception caught\n" ); }

                TRACE( DebugLevel::Log, "Begin closing\n" );
                glfwSetWindowShouldClose( window, true );

                try
                {
                        if (FData)
gameEngine->m_pGameHandler->DeleteFData(FData);
                }
                catch (const std::exception& e) { TRACE(DebugLevel::FatalError,
"Exception caught in FData delete: %s\n", e.what()); } catch (...) {
TRACE(DebugLevel::FatalError, "Unknown Exception caught in FData delete\n"); }

                try
                {
                        gameEngine->m_pInputHandler->OnDestroy( window );
                }
                catch( const std::exception &e ) { TRACE(
DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); } catch( ... ) {
TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

                try
                {
                        gameEngine->m_pGameHandler->OnDestroy();
                }
                catch( const std::exception &e ) { TRACE(
DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); } catch( ... ) {
TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

                try
                {
                        gameEngine->m_pRenderHandler->OnDestroy();
                }
                catch( const std::exception &e ) { TRACE(
DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); } catch( ... ) {
TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

        }
        TRACE( DebugLevel::Log, "After OnDestroy\n" );
        OPTICK_SHUTDOWN();

        try
        {
                delete gameEngine->m_pInputHandler;
                gameEngine->m_pInputHandler = NULL;
        }
        catch( const std::exception &e ) { TRACE( DebugLevel::FatalError,
"Exception caught: %s\n", e.what() ); } catch( ... ) { TRACE(
DebugLevel::FatalError, "Unknown Exception caught\n" ); }


        try
        {
                delete gameEngine->m_pGameHandler;
                gameEngine->m_pGameHandler = NULL;
        }
        catch( const std::exception &e ) { TRACE( DebugLevel::FatalError,
"Exception caught: %s\n", e.what() ); } catch( ... ) { TRACE(
DebugLevel::FatalError, "Unknown Exception caught\n" ); }

        try
        {
                delete gameEngine->m_pRenderHandler;
                gameEngine->m_pRenderHandler = NULL;
        }
        catch( const std::exception &e ) { TRACE( DebugLevel::FatalError,
"Exception caught: %s\n", e.what() ); } catch( ... ) { TRACE(
DebugLevel::FatalError, "Unknown Exception caught\n" ); }
}
*/
