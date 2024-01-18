#pragma once

#include "GameCore.hpp"

#include <imgui.h>
#include <taskflow/taskflow.hpp>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

struct FrameData {
  glm::dvec2 start, increment;
  // glm::uvec2 WndSize;
  // bool bVSync;
  // float fTime;
  //
  // ImDrawDataBuffered ImGuiDrawData;
};

class GameTickHandler {
public:
  void *NewFData();
  void DeleteFData(void *FData);

  //GameTickHandler(tf::Subflow &, SDL_Window *);
  void OnTick(void *_FData);

  glm::quat CamRot = glm::identity<glm::quat>();
  glm::vec3 CamPos{0, 0, 10};

  struct computePushConstants {
    glm::dvec2 start;
    glm::dvec2 increment;
  } pc;

  glm::vec2 eulerCameraRot{0, 0};

  float lightForce = 0;
  int iShadowMapSizeId = 0;

  bool bVSync = false;
  bool bBoundingBoxes = false;

  glm::vec3 spotPos{0, 10, 10};
  glm::vec3 spotDir{0, -10, -10};

  uint64_t lastTimer;
  float timerInvFreq;

  float FPS;

  glm::vec2 speed{};

  std::chrono::high_resolution_clock::time_point TimeStart;

private:
  void Move(glm::vec3 v);
};
