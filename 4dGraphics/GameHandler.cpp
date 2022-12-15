#include "GameHandler.h"

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_events.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

#include <exception>

#include "cppHelpers.hpp"
#include "Shader.h"
#include "Debug.h"

cpph::destroy_helper MyGameHandler::initImguiForSDL2()
{
    if( !ImGui_ImplSDL2_InitForVulkan( m_hWindow ) )
        throw std::runtime_error("Could not init imgui for SDL2");
    
    return { []{ TRACE(DebugLevel::Log,"destr\n"); ImGui_ImplSDL2_Shutdown(); } };
}

MyGameHandler::MyGameHandler()
    : GameEngine()
    , imgui_sdl2_( initImguiForSDL2() )
{
}

MyGameHandler::~MyGameHandler()
{
}

int MyGameHandler::Run()
{
    ImGui_ImplSDL2_NewFrame();
    //ImGui_ImplVulkan_NewFrame();

    bool closing = false;

    while( !closing )
    {
        SDL_Event event;
        while( SDL_PollEvent( &event ) )
        {
            if( event.type == SDL_QUIT )
            {
                closing = true;
            }

            if( ImGui_ImplSDL2_ProcessEvent( &event ) ) continue;
        }
    }

    return 0;
}