#pragma once
#include <imgui.h>

struct ImDrawDataBuffered : ImDrawData
{
    ImVector<ImDrawList> CmdListData;
    ImVector<ImDrawList*> CmdListPointers;

    ~ImDrawDataBuffered();

    IMGUI_API void  CopyDrawData(const ImDrawData* source); // efficiently backup draw data into a new container for multi buffer rendering
};