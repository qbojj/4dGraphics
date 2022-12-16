#pragma once
#include "cppHelpers.hpp"
#include "GameCore.h"

#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include <taskflow/taskflow.hpp>

class MyGameHandler final : public GameEngine
{
public:
    MyGameHandler();
    ~MyGameHandler();

    int Run();

    tf::Executor executor;
private:
    cpph::destroy_helper InitImgui();

    cpph::destroy_helper imgui_sdl2_;
    GameTickHandler tickHandler;
    GameRenderHandler renHandler;
};