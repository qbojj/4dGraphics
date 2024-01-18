#pragma once
#include "GameCore.hpp"
#include "GameRenderHandler.hpp"
#include "GameTickHandler.hpp"
#include "cppHelpers.hpp"
#include "Context.hpp"

namespace v4dg {
class ImGui_VulkanImpl {
public:
  ImGui_VulkanImpl(const Swapchain &swapchain, Context &ctx);
  ~ImGui_VulkanImpl();
private:
  vk::raii::DescriptorPool pool;
};

class MyGameHandler final : public GameEngine {
public:
  MyGameHandler();
  ~MyGameHandler();

  int Run();

private:
  SDL_Context sdlContext{SDL_INIT_EVERYTHING};
  Handle<Instance> instance;
  vk::raii::SurfaceKHR surface;
  Handle<Device> device;
  Context context;
  Swapchain swapchain;
  ImGui_VulkanImpl imguiVulkanImpl;

  bool should_close{false};
  bool has_focus{true};

  vk::Extent2D wanted_extent{0, 0};

  void recreate_swapchain();
  uint32_t wait_for_image();
  bool handle_events();

  void gui();
  vk::CommandBuffer record_gui(vk::Image,vk::ImageView);
  void submit(vk::CommandBuffer cmd);
  void present(uint32_t image_idx);
};
} // namespace v4dg