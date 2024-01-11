#pragma once
#include <imgui.h>

struct ImDrawDataBuffered : ImDrawData {
  ImVector<ImDrawList> CmdListData;
  ImVector<ImDrawList *> CmdListPointers;

  ImDrawDataBuffered() = default;
  ImDrawDataBuffered(const ImDrawDataBuffered &) = delete;
  ImDrawDataBuffered(ImDrawDataBuffered &&) = delete;

  ~ImDrawDataBuffered();

  void CopyDrawData(
      const ImDrawData *source); // efficiently backup draw data into a new
                                 // container for multi buffer rendering
};