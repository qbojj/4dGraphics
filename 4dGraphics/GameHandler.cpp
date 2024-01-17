#include "GameHandler.hpp"

#include "Context.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "cppHelpers.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <taskflow/taskflow.hpp>

#include <exception>
#include <fstream>
#include <memory>

namespace {
using namespace v4dg;

vk::raii::SurfaceKHR glfw_get_surface(const vk::raii::Instance &instance,
                                      GLFWwindow *window) {
  VkSurfaceKHR raw_surface;
  vk::resultCheck(static_cast<vk::Result>(glfwCreateWindowSurface(
                      *instance, window, nullptr, &raw_surface)),
                  "glfwCreateWindowSurface");
  return {instance, raw_surface};
}

class ImGui_VulkanImpl {
public:
  ImGui_VulkanImpl(const Swapchain &swapchain, Context &ctx) : pool(nullptr) {
    auto &queue = *ctx.get_queue(Context::QueueType::Graphics);

    std::vector<vk::DescriptorPoolSize> pool_sizes{
        {vk::DescriptorType::eCombinedImageSampler, 4096},
    };

    pool = {ctx.vkDevice(),
            {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1024,
             pool_sizes}};

    ctx.device().setDebugName(pool, "imgui descriptor pool");

    ImGui_ImplVulkan_InitInfo ii{
        .Instance = *ctx.vkInstance(),
        .PhysicalDevice = *ctx.vkPhysicalDevice(),
        .Device = *ctx.vkDevice(),
        .QueueFamily = queue.queue()->family(),
        .Queue = *queue.queue()->queue(),
        .PipelineCache = {}, // TODO
        .DescriptorPool = *pool,
        .Subpass = {},
        .MinImageCount = 2,
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

  ~ImGui_VulkanImpl() { ImGui_ImplVulkan_Shutdown(); }

private:
  vk::raii::DescriptorPool pool;
};
} // namespace

namespace v4dg {
MyGameHandler::MyGameHandler() : GameEngine() {}

MyGameHandler::~MyGameHandler() {}

int MyGameHandler::Run() {
  auto instance = make_handle<Instance>();
  auto surface = glfw_get_surface(instance->instance(), m_window);

  auto device = make_handle<Device>(instance, *surface);

  Context context(instance, device);

  vk::Extent2D swapchain_wanted_extent;
  auto update_wanted_extent = [&] {
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    swapchain_wanted_extent = vk::Extent2D{uint32_t(w), uint32_t(h)};
  };
  update_wanted_extent();

  Swapchain swapchain =
      SwapchainBuilder{
          .surface = *surface,
          .extent = swapchain_wanted_extent,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                        vk::ImageUsageFlagBits::eTransferDst |
                        vk::ImageUsageFlagBits::eStorage,
          .image_count = 3,
      }.build(context);
  
  auto recreate_swapchain = [&]{
    auto builder = swapchain.recreate_builder();
    builder.fallback_extent = builder.extent;
    builder.extent = swapchain_wanted_extent;

    builder.surface = *surface;

    context.get_thread_frame_ctx().m_destruction_stack.push(swapchain.move_out());
    swapchain = builder.build(context);
  };

  ImGui_VulkanImpl imgui_vulkan_backend(swapchain, context);

  glfwShowWindow(m_window);

  while (!glfwWindowShouldClose(m_window)) {
    context.next_frame();

    tf::Taskflow tf;

    glfwPollEvents();
    ImGui_ImplGlfw_NewFrame();
    update_wanted_extent();

    auto gui = tf.emplace([&] {
                   ImGui_ImplVulkan_NewFrame();

                   ImGui::NewFrame();
                   ImGui::ShowAboutWindow();
                   ImGui::Render();
                 }).name("gui");

    uint32_t image_idx = 0;

    auto acquire_image =
        tf.emplace([&] {
            if (swapchain_wanted_extent != swapchain.extent()) {
              logger.Debug("Recreating swapchain (window resize)");
              recreate_swapchain();
            }

            bool recreated = false;

            do {
              try {
                auto [res, idx] = swapchain.swapchain().acquireNextImage(
                    std::numeric_limits<uint64_t>::max(),
                    *context.get_frame_ctx().m_image_ready);

                image_idx = idx;
                recreated = false;
              } catch (vk::OutOfDateKHRError &e) {
                logger.Debug("Recreating swapchain (out of date)");
                recreate_swapchain();
                recreated = true;
              }
            } while (recreated);
          }).name("acquire image");

    auto drawSubmit =
        tf.emplace([&] {
            auto cb =
                context.get_thread_frame_ctx().m_command_buffer_manager.get(
                    vk::CommandBufferLevel::ePrimary,
                    command_buffer_manager::category::c0_100);

            cb->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

            cb->pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                vk::ImageMemoryBarrier{
                    vk::AccessFlagBits::eNone,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    swapchain.images()[image_idx],
                    {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

            vk::RenderingAttachmentInfo rai(
                *swapchain.imageViews()[image_idx],
                vk::ImageLayout::eColorAttachmentOptimal,
                vk::ResolveModeFlagBits::eNone, {}, {},
                vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, {},
                {});

            cb->beginRendering({{}, {{}, swapchain.extent()}, 1, 0, rai});
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cb);

            cb->endRendering();

            cb->pipelineBarrier(
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
                vk::ImageMemoryBarrier{
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits::eMemoryRead,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::ePresentSrcKHR,
                    vk::QueueFamilyIgnored,
                    vk::QueueFamilyIgnored,
                    swapchain.images()[image_idx],
                    {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

            cb->end();

            auto &queue_g = context.get_queue(Context::QueueType::Graphics);

            std::vector<vk::SemaphoreSubmitInfo> waitSSI, signalSSI;

            waitSSI.emplace_back(
                *context.get_frame_ctx().m_image_ready, 0,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            waitSSI.emplace_back(
                *queue_g->semaphore(), queue_g->semaphoreValue(),
                vk::PipelineStageFlagBits2::eColorAttachmentOutput);

            signalSSI.emplace_back(
                *context.get_frame_ctx().m_render_finished, 0,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            signalSSI.emplace_back(
                *queue_g->semaphore(), queue_g->semaphoreValue() + 1,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput);

            queue_g->setSemaphoreValue(queue_g->semaphoreValue() + 1);

            std::vector<vk::CommandBufferSubmitInfo> cbs;
            cbs.emplace_back(*cb);

            std::unique_lock<std::mutex> lock;
            queue_g->queue()->lock(lock).submit2(
                vk::SubmitInfo2{{}, waitSSI, cbs, signalSSI});
          })
            .name("draw submit")
            .succeed(acquire_image)
            .succeed(gui);

    auto preset = tf.emplace([&] {
                      std::unique_lock<std::mutex> lock;

                      auto result =
                          context.get_queue(Context::QueueType::Graphics)
                              ->queue()
                              ->lock(lock)
                              .presentKHR(vk::PresentInfoKHR{
                                  *context.get_frame_ctx().m_render_finished,
                                  *swapchain.swapchain(),
                                  image_idx,
                              });
                      lock.unlock();

                      if (result == vk::Result::eSuboptimalKHR) {
                        logger.Log("Recreating swapchain (suboptimal)");
                        recreate_swapchain();
                      }
                    })
                      .name("present")
                      .succeed(drawSubmit);

    (void)preset;

    auto fut = context.executor().run(tf);

    while (fut.wait_for(std::chrono::milliseconds(5)) !=
           std::future_status::ready)
      glfwPollEvents();
  }

  context.executor().wait_for_all();
  context.vkDevice().waitIdle();

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