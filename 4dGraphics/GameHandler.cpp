#include "GameHandler.hpp"

#include "Debug.hpp"
#include "cppHelpers.hpp"
#include "Device.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <taskflow/taskflow.hpp>

#include <exception>
#include <fstream>
#include <memory>

namespace v4dg {
MyGameHandler::MyGameHandler() : GameEngine() {
}

MyGameHandler::~MyGameHandler() {
}

int MyGameHandler::Run() {
  auto instance = make_handle<Instance>();
  auto device = make_handle<Device>(instance);
  /*
  tf::Executor executor;

  std::unique_ptr<GameRenderHandler> pRenderHandler;
  std::unique_ptr<GameTickHandler> pTickHandler;
  tf::CriticalSection ImGuiLock{1};

  {
    tf::Taskflow tf("Initialize");

    auto [ren, tick] = tf.emplace(
        [&](tf::Subflow &sf) {
          pRenderHandler = std::make_unique<GameRenderHandler>(sf, m_hWindow);
        },
        [&](tf::Subflow &sf) {
          pTickHandler = std::make_unique<GameTickHandler>(sf, m_hWindow);
        });

    ren.name("Init render handler");
    tick.name("Init tick handler");

    executor.run(tf).wait();

    std::ofstream initf("init.dot");
    tf.dump(initf);
  }

  OPTICK_APP("4dGraphics");

  bool closing = false;

  void *FData = pTickHandler->NewFData();
  if (!FData)
    throw std::runtime_error("could not allocate FData");
  cpph::destroy_helper td_([&] { pTickHandler->DeleteFData(FData); });

  SDL_ShowWindow(m_hWindow);
  while (!closing) {
    OPTICK_FRAME("Main Thread");
    ImGui_ImplSDL2_NewFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        closing = true;

      if (ImGui_ImplSDL2_ProcessEvent(&event))
        continue;
    }

    pTickHandler->OnTick(FData);
    pRenderHandler->OnDraw(FData);
  }
  */
  return 0;
}
} // namespace v4dg