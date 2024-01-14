#include "GameCore.hpp"

#include "Debug.hpp"

#include <GLFW/glfw3.h>

#include <exception>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stdexcept>
#include <string.h>

namespace v4dg {
std::mutex GlfwContext::glfwInitMutex;
int GlfwContext::glfwInitCount = 0;

GlfwContext::GlfwContext() {
  std::scoped_lock lock(glfwInitMutex);
  if (glfwInitCount++ == 0) {
    if (!glfwInit()) {
      glfwSetErrorCallback([](int error, const char *description) {
        logger.Error("GLFW error {}: {}", error, description);
      });
      glfwInitCount--;
      throw std::runtime_error("Cannot initialize GLFW");
    }
  }
}

GlfwContext::~GlfwContext() {
  try {
    std::scoped_lock lock(glfwInitMutex);
    if (--glfwInitCount == 0)
      glfwTerminate();
  } catch (...) {}
}

ImGuiRAIIContext::ImGuiRAIIContext(ImFontAtlas *font)
    : context(ImGui::CreateContext(font)) {
  if (!context)
    throw std::runtime_error("Cannot initialize ImGui");
  if (!IMGUI_CHECKVERSION())
    throw std::runtime_error("Imgui runtime version not equal to what version "
                             "program was compiled with");
  ImGui::SetCurrentContext(context);
}

ImGuiRAIIContext::~ImGuiRAIIContext() { ImGui::DestroyContext(context); }

Window::Window(int width, int height, const char *title) : window(nullptr) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!window)
    throw std::runtime_error("Could not create window");
}

Window::~Window() { glfwDestroyWindow(window); }

ImGui_GlfwImpl::ImGui_GlfwImpl(const Window &window) {
  auto &io = ImGui::GetIO();

  if (!io.Fonts->AddFontDefault() ||
      !io.Fonts->Build())
    throw std::runtime_error("Could not initialize imgui font");

  ImGui_ImplGlfw_InitForVulkan(window, true);
}

ImGui_GlfwImpl::~ImGui_GlfwImpl() { ImGui_ImplGlfw_Shutdown(); }

GameEngine::GameEngine(Window window)
    : m_window(std::move(window)), m_ImGuiCtx(),
      m_ImGuiGlfwImpl(m_window) {}

GameEngine::~GameEngine() {}

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

} // namespace v4dg