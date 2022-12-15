#pragma once
#include "cppHelpers.hpp"
#include "GameCore.h"

class MyGameHandler final : public GameEngine
{
public:
    MyGameHandler();
    ~MyGameHandler();

    int Run();
private:
    cpph::destroy_helper initImguiForSDL2();

    cpph::destroy_helper imgui_sdl2_, imgui_vulkan_;
};