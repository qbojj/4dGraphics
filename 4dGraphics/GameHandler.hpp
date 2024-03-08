#pragma once
#include "GameCore.hpp"
#include "cppHelpers.hpp"
#include "Context.hpp"
#include "PipelineBuilder.hpp"
#include "VulkanConstructs.hpp"
#include "BindlessManager.hpp"
#include "VulkanResources.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <array>

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

  Texture texture;

  bool should_close{false};
  bool has_focus{true};

  vk::Extent2D wanted_extent{0, 0};

  vk::raii::DescriptorSetLayout descriptor_set_layout;
  vk::raii::PipelineLayout pipeline_layout;
  std::array<vk::raii::Pipeline,3> pipeline;
  int current_pipeline{2};

  struct MandelbrotPushConstants {
    glm::dvec2 center{};
    glm::dvec2 scale{1. / 128};
    BindlessResource image_idx;
  } mandelbrot_push_constants;

  void recreate_swapchain();
  std::uint32_t wait_for_image();
  bool handle_events();

  void gui();
  void record_gui(CommandBuffer &cb, vk::Image,vk::ImageView);
  void submit(vk::CommandBuffer cmd, std::uint32_t image_idx);
  void present(std::uint32_t image_idx);
};
} // namespace v4dg