#include "GameHandler.hpp"

#include "Context.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "cppHelpers.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>

#include <exception>
#include <fstream>
#include <memory>

namespace v4dg {
namespace {
vk::raii::SurfaceKHR sdl_get_surface(const vk::raii::Instance &instance,
                                     SDL_Window *window) {
  VkSurfaceKHR raw_surface;
  if (SDL_Vulkan_CreateSurface(window, *instance, &raw_surface) == SDL_FALSE)
    throw exception("Could not create vulkan surface: {}", SDL_GetError());

  return {instance, raw_surface};
}
} // namespace

ImGui_VulkanImpl::ImGui_VulkanImpl(const Swapchain &swapchain, Context &ctx)
    : pool(nullptr) {
  auto &queue = *ctx.get_queue(Context::QueueType::Graphics);

  std::vector<vk::DescriptorPoolSize> pool_sizes{
      {vk::DescriptorType::eCombinedImageSampler, 4096},
  };

  pool = {
      ctx.vkDevice(),
      {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1024, pool_sizes}};

  ctx.device().setDebugName(pool, "imgui descriptor pool");

  uint32_t min_image_count =
      swapchain.presentMode() == vk::PresentModeKHR::eMailbox ? 3 : 2;

  ImGui_ImplVulkan_InitInfo ii{
      .Instance = *ctx.vkInstance(),
      .PhysicalDevice = *ctx.vkPhysicalDevice(),
      .Device = *ctx.vkDevice(),
      .QueueFamily = queue.queue()->family(),
      .Queue = *queue.queue()->queue(),
      .PipelineCache = *ctx.pipeline_cache(),
      .DescriptorPool = *pool,
      .Subpass = {},
      .MinImageCount = min_image_count,
      .ImageCount = uint32_t(swapchain.images().size()),
      .MSAASamples =
          static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1),

      .UseDynamicRendering = true,
      .ColorAttachmentFormat = static_cast<VkFormat>(swapchain.format()),

      .Allocator = {},
      .CheckVkResultFn = {},
  };

  if (!ImGui_ImplVulkan_Init(&ii, {}))
    throw exception("Could not init imgui vulkan backend");
}

ImGui_VulkanImpl::~ImGui_VulkanImpl() { ImGui_ImplVulkan_Shutdown(); }

MyGameHandler::MyGameHandler()
    : GameEngine(), instance(make_handle<Instance>()),
      surface(sdl_get_surface(instance->instance(), m_window)),
      device(make_handle<Device>(instance, *surface)),
      context(instance, device),
      swapchain(SwapchainBuilder{
          .surface = *surface,
          .preferred_present_mode = vk::PresentModeKHR::eMailbox,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                        vk::ImageUsageFlagBits::eTransferDst |
                        vk::ImageUsageFlagBits::eStorage,
          .image_count = 3,
      }
                    .build(context)),
      imguiVulkanImpl(swapchain, context) {}

MyGameHandler::~MyGameHandler() {}

void MyGameHandler::recreate_swapchain() {
  auto builder = swapchain.recreate_builder();
  builder.fallback_extent = builder.extent;
  builder.extent = wanted_extent;

  builder.surface = *surface;

  context.get_destruction_stack().push(swapchain.move_out());
  swapchain = builder.build(context);
}

uint32_t MyGameHandler::wait_for_image() {
  ZoneScoped;

  do {
    int w, h;
    SDL_Vulkan_GetDrawableSize(m_window, &w, &h);
    wanted_extent = vk::Extent2D{uint32_t(w), uint32_t(h)};

    if (wanted_extent != swapchain.extent()) {
      logger.Debug("Recreating swapchain (window resize)");
      recreate_swapchain();
    }

    try {
      auto [res, idx] = swapchain.swapchain().acquireNextImage(
          std::numeric_limits<uint64_t>::max(),
          *context.get_frame_ctx().m_image_ready);

      return idx;
    } catch (vk::OutOfDateKHRError &e) {
      logger.Debug("Recreating swapchain (out of date)");
      recreate_swapchain();
    }
  } while (true);
}

bool MyGameHandler::handle_events() {
  ZoneScoped;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      should_close = true;
      return false;
    case SDL_WINDOWEVENT:
      switch (event.window.event) {
      case SDL_WINDOWEVENT_FOCUS_GAINED:
        has_focus = true;
        break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
        has_focus = false;
        break;
      }
      break;
    }

    ImGui_ImplSDL2_ProcessEvent(&event);
  }

  ImGui_ImplSDL2_NewFrame(m_window);
  ImGui_ImplVulkan_NewFrame();

  return true;
}

void MyGameHandler::gui() {
  ZoneScoped;
  ImGui::NewFrame();

  ImGui::ShowDemoWindow();
  ImGui::ShowMetricsWindow();

  ImGui::Render();
}

vk::CommandBuffer MyGameHandler::record_gui(vk::Image image,
                                            vk::ImageView view) {
  ZoneScoped;

  auto cb = context.get_thread_frame_ctx().m_command_buffer_manager.get(
      vk::CommandBufferLevel::ePrimary,
      command_buffer_manager::category::c0_100);

  TracyMessageL("command buffer acquired");

  cb->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  TracyMessageL("command buffer begin");

  cb->pipelineBarrier(
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
      vk::ImageMemoryBarrier{vk::AccessFlagBits::eNone,
                             vk::AccessFlagBits::eColorAttachmentWrite,
                             vk::ImageLayout::eUndefined,
                             vk::ImageLayout::eColorAttachmentOptimal,
                             vk::QueueFamilyIgnored,
                             vk::QueueFamilyIgnored,
                             image,
                             {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

  vk::RenderingAttachmentInfo rai(
      view, vk::ImageLayout::eColorAttachmentOptimal,
      vk::ResolveModeFlagBits::eNone, {}, {}, vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore, {}, {});

  cb->beginRendering({{}, {{}, swapchain.extent()}, 1, 0, rai});
  {
    ZoneScopedN("render imgui");
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cb);
  }
  cb->endRendering();

  cb->pipelineBarrier(
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
      vk::ImageMemoryBarrier{vk::AccessFlagBits::eColorAttachmentWrite,
                             vk::AccessFlagBits::eMemoryRead,
                             vk::ImageLayout::eColorAttachmentOptimal,
                             vk::ImageLayout::ePresentSrcKHR,
                             vk::QueueFamilyIgnored,
                             vk::QueueFamilyIgnored,
                             image,
                             {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

  cb->end();

  return *cb;
}

void MyGameHandler::submit(vk::CommandBuffer cb) {
  ZoneScoped;
  std::unique_lock<std::mutex> lock;
  auto &queue_g = context.get_queue(Context::QueueType::Graphics);

  std::vector<vk::SemaphoreSubmitInfo> waitSSI, signalSSI;

  waitSSI.emplace_back(*context.get_frame_ctx().m_image_ready, 0,
                       vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  waitSSI.emplace_back(*queue_g->semaphore(), queue_g->semaphoreValue(),
                       vk::PipelineStageFlagBits2::eColorAttachmentOutput);

  signalSSI.emplace_back(*context.get_frame_ctx().m_render_finished, 0,
                         vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  signalSSI.emplace_back(*queue_g->semaphore(), queue_g->semaphoreValue() + 1,
                         vk::PipelineStageFlagBits2::eColorAttachmentOutput);

  queue_g->setSemaphoreValue(queue_g->semaphoreValue() + 1);

  std::vector<vk::CommandBufferSubmitInfo> cbs;
  cbs.emplace_back(cb);

  auto &queue = queue_g->queue()->lock(lock);
  queue.submit2(vk::SubmitInfo2{{}, waitSSI, cbs, signalSSI});
}

void MyGameHandler::present(uint32_t image_idx) {
  ZoneScoped;
  auto &queue_g = context.get_queue(Context::QueueType::Graphics);

  std::unique_lock<std::mutex> lock;
  auto &queue = queue_g->queue()->lock(lock);

  try {
    (void)queue.presentKHR({
        *context.get_frame_ctx().m_render_finished,
        *swapchain.swapchain(),
        image_idx,
    });
  } catch (const vk::OutOfDateKHRError &e) {
    // ignore as if we wanted to use the swapchain once more
    //  we will recreate it on acquire
  }
}

int MyGameHandler::Run() {
  SDL_ShowWindow(m_window);

  auto last_frame = std::chrono::high_resolution_clock::now();

  while (!should_close) {
    auto now = std::chrono::high_resolution_clock::now();
    auto delta = std::chrono::duration<double>(now - last_frame).count();
    last_frame = now;

    (void)delta;
    
    if (!has_focus) // Don't waste CPU time when not focused
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

    FrameMark;

    {
      ZoneScopedN("advance frame");
      context.next_frame();
    }

    uint32_t image_idx = wait_for_image();
    if (!handle_events())
      break;

    tf::Taskflow tf;

    {
      ZoneScopedN("build taskflow");

      auto gui_task = tf.emplace([&] { gui(); }).name("gui");

      vk::CommandBuffer recorded;

      auto record = tf.emplace([&] {
                        recorded =
                            record_gui(swapchain.images()[image_idx],
                                       *swapchain.imageViews()[image_idx]);
                      })
                        .name("record")
                        .succeed(gui_task);

      tf.emplace([&] { submit(recorded); }).name("submit").succeed(record);
    }

    context.executor().run(tf).wait();
    present(image_idx);
  }

  context.executor().wait_for_all();
  context.vkDevice().waitIdle();

  return 0;
}
} // namespace v4dg