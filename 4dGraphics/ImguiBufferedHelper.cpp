#include "ImguiBufferedHelper.h"

ImDrawDataBuffered::~ImDrawDataBuffered()
{
    for (int i = 0; i < CmdListData.size(); ++i)
    {
        CmdListData[i].~ImDrawList();
    }
}

// copy state and swap memory with existing draw lists
void ImDrawDataBuffered::CopyDrawData(const ImDrawData* source)
{
    CmdListData.resize(source->CmdListsCount);
    CmdListPointers.resize(source->CmdListsCount, NULL);

    Valid = source->Valid;
    CmdLists = source->CmdListsCount ? CmdListPointers.Data : NULL;
    CmdListsCount = source->CmdListsCount;
    TotalIdxCount = source->TotalIdxCount;
    TotalVtxCount = source->TotalVtxCount;
    DisplayPos = source->DisplayPos;
    DisplaySize = source->DisplaySize;

    for (int i = 0; i < CmdListsCount; ++i)
    {
        ImDrawList* src_list = source->CmdLists[i];

        if (CmdListPointers[i] == NULL)
        {
            IM_PLACEMENT_NEW(&CmdListData[i]) ImDrawList(src_list->_Data);
        }

        // always copy pointer in case the data list gets reallocated
        CmdListPointers[i] = &CmdListData[i];

        ImDrawList* dst_list = CmdListPointers[i];

        // pre-allocate to the same size so we don't suffer multiple allocations next frame
        dst_list->CmdBuffer.reserve(src_list->CmdBuffer.size());
        dst_list->IdxBuffer.reserve(src_list->IdxBuffer.size());
        dst_list->VtxBuffer.reserve(src_list->VtxBuffer.size());

        dst_list->CmdBuffer.swap(src_list->CmdBuffer);
        dst_list->IdxBuffer.swap(src_list->IdxBuffer);
        dst_list->VtxBuffer.swap(src_list->VtxBuffer);
        dst_list->Flags = src_list->Flags;
    }
}