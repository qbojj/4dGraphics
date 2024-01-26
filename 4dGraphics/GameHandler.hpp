#pragma once
#include "GameCore.hpp"
#include "GameRenderHandler.hpp"
#include "GameTickHandler.hpp"
#include "cppHelpers.hpp"
#include "Context.hpp"
#include "PipelineBuilder.hpp"

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
  SDL_Context sdlContext{SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER};
  Instance instance;
  vk::raii::SurfaceKHR surface;
  Device device;
  Context context;
  Swapchain swapchain;
  ImGui_VulkanImpl imguiVulkanImpl;

  bool should_close{false};
  bool has_focus{true};

  vk::Extent2D wanted_extent{0, 0};

  vk::raii::DescriptorSetLayout descriptor_set_layout;
  vk::raii::PipelineLayout pipeline_layout;
  vk::raii::Pipeline pipeline;

  struct MandelbrotPushConstants {
    float x, y, scale{1. / 1024};
    uint32_t max_iter{512};
  } mandelbrot_push_constants;

  void recreate_swapchain();
  uint32_t wait_for_image();
  bool handle_events();

  void gui();
  vk::CommandBuffer record_gui(vk::Image,vk::ImageView);
  void submit(vk::CommandBuffer cmd);
  void present(uint32_t image_idx);
};
} // namespace v4dg