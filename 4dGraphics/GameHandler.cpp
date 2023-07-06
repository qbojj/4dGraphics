#include "GameHandler.h"

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_events.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <taskflow/taskflow.hpp>
#include <exception>
#include <memory>
#include <fstream>

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

    return cpph::destroy_helper(ImGui_ImplSDL2_Shutdown);
}

MyGameHandler::MyGameHandler()
    : GameEngine()
    , imgui_sdl2_( InitImgui() )
{}

MyGameHandler::~MyGameHandler()
{}

int MyGameHandler::Run()
{
    tf::Executor executor;

    std::unique_ptr<GameRenderHandler> pRenderHandler;
    std::unique_ptr<GameTickHandler> pTickHandler;
    tf::CriticalSection ImGuiLock{1};
    
    {
        tf::Taskflow tf("Initialize");

        auto [ren, tick] = tf.emplace(
            [&]( tf::Subflow &sf ){ pRenderHandler = std::make_unique<GameRenderHandler>( sf, m_hWindow ); },
            [&]( tf::Subflow &sf ){ pTickHandler = std::make_unique<GameTickHandler>( sf, m_hWindow ); }
        );

        ren.name("Init render handler");
        tick.name("Init tick handler");

        executor.run( tf ).wait();
        
        std::ofstream initf("init.dot");
        tf.dump(initf);
    }

    OPTICK_APP( "4dGraphics" );
    
    bool closing = false;

    void *FData = pTickHandler->NewFData();
    if( !FData ) throw std::runtime_error("could not allocate FData");
    cpph::destroy_helper td_([&]{ pTickHandler->DeleteFData(FData); });

    SDL_ShowWindow( m_hWindow );   
    while( !closing )
    {
        OPTICK_FRAME("Main Thread");
        ImGui_ImplSDL2_NewFrame();

        SDL_Event event;
        while( SDL_PollEvent( &event ) )
        {
            if( event.type == SDL_QUIT ) closing = true;

            if( ImGui_ImplSDL2_ProcessEvent( &event ) ) continue;
        }

        pTickHandler->OnTick( FData );
        pRenderHandler->OnDraw( FData );
    }

    return 0;
}