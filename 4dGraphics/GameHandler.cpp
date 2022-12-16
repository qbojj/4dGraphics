#include "GameHandler.h"

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_events.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_vulkan.h>

#include <exception>

#include "cppHelpers.hpp"
#include "Shader.h"
#include "Debug.h"

cpph::destroy_helper MyGameHandler::InitImgui()
{
    if( !IMGUI_CHECKVERSION() ) 
        throw std::runtime_error("Imgui runtime version not equal to what version program was compiled with");

    if( !ImGui::GetIO().Fonts->AddFontDefault() ||
	    !ImGui::GetIO().Fonts->Build() )
        throw std::runtime_error("Could not initialize imgui font");

    if( !ImGui_ImplSDL2_InitForVulkan( m_hWindow ) )
        throw std::runtime_error("Could not init imgui for SDL2");
    cpph::destroy_helper res(ImGui_ImplSDL2_Shutdown);
    
    return res;
}

MyGameHandler::MyGameHandler()
    : GameEngine()
    , executor()
    , imgui_sdl2_( InitImgui() )
    , tickHandler( m_hWindow )
    , renHandler( m_hWindow )
{}

MyGameHandler::~MyGameHandler()
{}

int MyGameHandler::Run()
{
    bool closing = false;

    void *FData = tickHandler.NewFData();
    cpph::destroy_helper td_([&]{ tickHandler.DeleteFData(FData); });
    
    while( !closing )
    {
        ImGui_ImplSDL2_NewFrame();

        SDL_Event event;
        while( SDL_PollEvent( &event ) )
        {
            if( event.type == SDL_QUIT ) closing = true;

            if( ImGui_ImplSDL2_ProcessEvent( &event ) ) continue;
        }

        tickHandler.OnTick( FData );
        renHandler.OnDraw( FData );
    }

    return 0;
}