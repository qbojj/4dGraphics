#pragma once

#include "BindlessManager.hpp"
#include "Context.hpp"
#include "GameCore.hpp"
#include "PipelineBuilder.hpp"
#include "VulkanConstructs.hpp"
#include "VulkanResources.hpp"
#include "cppHelpers.hpp"
#include "TransferManager.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstdint>

namespace v4dg {
class ImGui_VulkanImpl {
public:
  ImGui_VulkanImpl(const Swapchain &swapchain, Context &ctx);
  ImGui_VulkanImpl(const ImGui_VulkanImpl &) = delete;
  ImGui_VulkanImpl &operator=(const ImGui_VulkanImpl &) = delete;
  ImGui_VulkanImpl(ImGui_VulkanImpl &&) = delete;
  ImGui_VulkanImpl &operator=(ImGui_VulkanImpl &&) = delete;
  ~ImGui_VulkanImpl();

private:
  vk::raii::DescriptorPool pool;
  vk::Format color_format;
};

class MyGameHandler final : public GameEngine {
public:
  MyGameHandler();
  MyGameHandler(const MyGameHandler &) = delete;
  MyGameHandler &operator=(const MyGameHandler &) = delete;
  MyGameHandler(MyGameHandler &&) = delete;
  MyGameHandler &operator=(MyGameHandler &&) = delete;
  ~MyGameHandler();

  int Run();

private:
  SDL_Context sdlContext{SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER};
  Instance instance;
  vk::raii::SurfaceKHR surface;
  Device device;
  Context context;
  TransferManager transfer_manager;

  Swapchain swapchain;
  ImGui_VulkanImpl imguiVulkanImpl;

  ImageView texture;

  bool should_close{false};
  bool has_focus{true};

  vk::Extent2D wanted_extent{0, 0};

  vk::raii::DescriptorSetLayout descriptor_set_layout;
  vk::raii::PipelineLayout pipeline_layout;
  std::array<vk::raii::Pipeline, 3> pipeline;
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
  void record_gui(CommandBuffer &cb, vk::Image, vk::ImageView);
  void present(std::uint32_t image_idx);
};
} // namespace v4dg