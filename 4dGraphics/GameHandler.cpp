#include "GameHandler.hpp"

#include "Context.hpp"
#include "Debug.hpp"
#include "Device.hpp"
#include "Swapchain.hpp"
#include "VulkanCaches.hpp"
#include "VulkanConstructs.hpp"
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

  ImGui_ImplVulkan_LoadFunctions(
      [](const char *name, void *user) {
        return static_cast<Context *>(user)->vkInstance().getProcAddr(name);
      },
      &ctx);
  if (!ImGui_ImplVulkan_Init(&ii, {}))
    throw exception("Could not init imgui vulkan backend");
}

ImGui_VulkanImpl::~ImGui_VulkanImpl() { ImGui_ImplVulkan_Shutdown(); }

MyGameHandler::MyGameHandler()
    : GameEngine(),
      instance(vk::raii::Context(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
          SDL_Vulkan_GetVkGetInstanceProcAddr()))),
      surface(sdl_get_surface(instance.instance(), m_window)),
      device(instance, *surface), context(device),
      swapchain(SwapchainBuilder{
          .surface = *surface,
          .preferred_format = vk::Format::eR8G8B8A8Unorm,
          .preferred_present_mode = vk::PresentModeKHR::eMailbox,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                        vk::ImageUsageFlagBits::eTransferDst |
                        vk::ImageUsageFlagBits::eStorage,
          .image_count = 3,
      }
                    .build(context)),
      imguiVulkanImpl(swapchain, context),
      texture(device,
              Image::ImageCreateInfo{
                  .format = vk::Format::eR16G16B16A16Sfloat,
                  .extent = {1024, 720, 1},
                  .usage = vk::ImageUsageFlagBits::eStorage |
                           vk::ImageUsageFlagBits::eColorAttachment |
                           vk::ImageUsageFlagBits::eSampled |
                           vk::ImageUsageFlagBits::eTransferSrc,
              },
              {{}, vma::MemoryUsage::eAuto}),
      descriptor_set_layout(DescriptorSetLayoutInfo()
                                .add_binding(0,
                                             vk::DescriptorType::eStorageImage,
                                             vk::ShaderStageFlagBits::eCompute)
                                .create(&device)),
      pipeline_layout(PipelineLayoutInfo()
                          .add_set(*descriptor_set_layout)
                          .add_push({vk::ShaderStageFlagBits::eCompute, 0u,
                                     sizeof(MandelbrotPushConstants)})
                          .create(&device)),
      pipeline({nullptr, nullptr, nullptr}) {

  auto shader =
      load_shader_module("Shaders/Mandelbrot.comp.spv", device.device());
  if (!shader)
    throw exception("Could not load shader module");

  for (int variant = 0; variant < 3; variant++) {
    ShaderStageData shader_data(vk::ShaderStageFlagBits::eCompute, **shader);
    shader_data.add_specialization(0, variant);
    vk::ComputePipelineCreateInfo pci{
        {},
        shader_data.get(),
        *pipeline_layout,
    };

    pipeline[variant] = {device.device(), context.pipeline_cache(), pci};
    device.setDebugName(pipeline[variant], "mandelbrot pipeline ({})", variant);
  }
}

MyGameHandler::~MyGameHandler() { context.cleanup(); }

void MyGameHandler::recreate_swapchain() {
  auto old_size = swapchain.extent();

  auto builder = swapchain.recreate_builder();
  builder.fallback_extent = builder.extent;
  builder.extent = wanted_extent;

  builder.surface = *surface;

  context.get_destruction_stack().push(swapchain.move_out());
  swapchain = builder.build(context);

  mandelbrot_push_constants.scale *=
      glm::dvec2(swapchain.extent().width, swapchain.extent().height) /
      glm::dvec2(old_size.width, old_size.height);
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

  ImGui::Begin("Mandelbrot");
  ImGui::SliderInt("variant", &current_pipeline, 0, 2);
  ImGui::Text("center: %f %f", mandelbrot_push_constants.center.x,
              mandelbrot_push_constants.center.y);
  ImGui::Text("scale: %f %f", mandelbrot_push_constants.scale.x, mandelbrot_push_constants.scale.y);
  ImGui::End();

  auto &io = ImGui::GetIO();
  if (!io.WantCaptureMouse) {
    // move mandelbrot
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      mandelbrot_push_constants.center -=
          glm::dvec2(delta.x, delta.y) * mandelbrot_push_constants.scale;
      ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }

    // zoom mandelbrot
    if (ImGui::GetIO().MouseWheel != 0) {
      auto delta = ImGui::GetIO().MouseWheel;

      // zoom into mouse position (position under the mouse stays the same)
      auto mouse_pos = ImGui::GetMousePos();

      auto mouse_center_rel =
          glm::dvec2(mouse_pos.x, mouse_pos.y) -
          glm::dvec2(swapchain.extent().width, swapchain.extent().height) / 2.0;

      auto world_pos = mandelbrot_push_constants.center +
                       mouse_center_rel * mandelbrot_push_constants.scale;

      // fractal so exponential zoom
      auto scale = mandelbrot_push_constants.scale;
      auto new_scale = scale * std::pow(1.1, (double)delta);

      auto new_center = world_pos - mouse_center_rel * new_scale;

      mandelbrot_push_constants.center = new_center;
      mandelbrot_push_constants.scale = new_scale;
    }
  }

  ImGui::Render();
}

vk::CommandBuffer MyGameHandler::record_gui(vk::Image image,
                                            vk::ImageView view) {
  ZoneScoped;

  auto cb = context.getGraphicsCommandBuffer();

  cb->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  cb->pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eComputeShader, {}, {}, {},
      vk::ImageMemoryBarrier{vk::AccessFlagBits::eTransferRead,
                             vk::AccessFlagBits::eShaderWrite,
                             vk::ImageLayout::eUndefined,
                             vk::ImageLayout::eGeneral,
                             vk::QueueFamilyIgnored,
                             vk::QueueFamilyIgnored,
                             texture.image(),
                             {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

  // render mandelbrot
  {
    cb.beginDebugLabel("mandelbrot", {0.0f, 1.0f, 0.0f, 1.0f});
    cb->bindPipeline(vk::PipelineBindPoint::eCompute,
                     *pipeline[current_pipeline]);

    vk::DescriptorImageInfo dii{
        {},
        texture.imageView(),
        vk::ImageLayout::eGeneral,
    };

    auto ds = cb.ds_allocator().allocate(*descriptor_set_layout);
    context.vkDevice().updateDescriptorSets(
        vk::WriteDescriptorSet{
            ds, 0, 0, vk::DescriptorType::eStorageImage, dii, {}, {}},
        {});

    cb->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipeline_layout, 0,
                           ds, {});
    cb->pushConstants<MandelbrotPushConstants>(
        *pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
        mandelbrot_push_constants);

    auto div = 8 * (current_pipeline == 0 ? 2 : 1);
    cb->dispatch(detail::DivCeil(texture.extent().width, div),
                 detail::DivCeil(texture.extent().height, div), 1);

    cb.endDebugLabel();
  }

  {
    ZoneScopedN("blit");

    std::array image_barriers{
        vk::ImageMemoryBarrier2{vk::PipelineStageFlagBits2::eComputeShader,
                                vk::AccessFlagBits2::eShaderWrite,
                                vk::PipelineStageFlagBits2::eTransfer,
                                vk::AccessFlagBits2::eTransferRead,
                                vk::ImageLayout::eGeneral,
                                vk::ImageLayout::eTransferSrcOptimal,
                                vk::QueueFamilyIgnored,
                                vk::QueueFamilyIgnored,
                                texture.image(),
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}},
        vk::ImageMemoryBarrier2{vk::PipelineStageFlagBits2::eTransfer,
                                vk::AccessFlagBits2::eNone,
                                vk::PipelineStageFlagBits2::eTransfer,
                                vk::AccessFlagBits2::eTransferWrite,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eTransferDstOptimal,
                                vk::QueueFamilyIgnored,
                                vk::QueueFamilyIgnored,
                                image,
                                {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}};
    cb->pipelineBarrier2({{}, {}, {}, image_barriers});
    cb->blitImage(
        texture.image(), vk::ImageLayout::eTransferSrcOptimal, image,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit({vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                      {vk::Offset3D{0, 0, 0},
                       vk::Offset3D{int32_t(texture.extent().width),
                                    int32_t(texture.extent().height), 1}},
                      {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                      {vk::Offset3D{0, 0, 0},
                       vk::Offset3D{int32_t(swapchain.extent().width),
                                    int32_t(swapchain.extent().height), 1}}),
        vk::Filter::eLinear);

    cb->pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {},
        vk::ImageMemoryBarrier{vk::AccessFlagBits::eTransferWrite,
                               vk::AccessFlagBits::eColorAttachmentRead |
                                   vk::AccessFlagBits::eColorAttachmentWrite,
                               vk::ImageLayout::eTransferDstOptimal,
                               vk::ImageLayout::eColorAttachmentOptimal,
                               vk::QueueFamilyIgnored,
                               vk::QueueFamilyIgnored,
                               image,
                               {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});
  }

  vk::RenderingAttachmentInfo rai(view,
                                  vk::ImageLayout::eColorAttachmentOptimal);

  cb->beginRendering({{}, {{}, swapchain.extent()}, 1, 0, rai});
  {
    cb.beginDebugLabel("imgui", {0.0f, 1.0f, 1.0f, 1.0f});
    ZoneScopedN("render imgui");
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cb);
    cb.endDebugLabel();
  }
  cb->endRendering();

  cb->pipelineBarrier(
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {},
      vk::ImageMemoryBarrier{vk::AccessFlagBits::eColorAttachmentWrite,
                             vk::AccessFlagBits::eNone,
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
  auto &queue = queue_g->queue()->lock(lock);

  std::vector<vk::SemaphoreSubmitInfo> waitSSI, signalSSI;

  auto sem_value = queue_g->semaphoreValue();
  waitSSI.emplace_back(*context.get_frame_ctx().m_image_ready, 0,
                       vk::PipelineStageFlagBits2::eTransfer);

  signalSSI.emplace_back(*context.get_frame_ctx().m_render_finished, 0,
                         vk::PipelineStageFlagBits2::eBottomOfPipe);
  signalSSI.emplace_back(*queue_g->semaphore(), sem_value + 1,
                         vk::PipelineStageFlagBits2::eBottomOfPipe);

  queue_g->setSemaphoreValue(sem_value + 1);

  std::vector<vk::CommandBufferSubmitInfo> cbs;
  cbs.emplace_back(cb);

  queue.submit2(vk::SubmitInfo2{{}, waitSSI, cbs, signalSSI});
}

void MyGameHandler::present(uint32_t image_idx) {
  ZoneScoped;
  auto &queue_g = context.get_queue(Context::QueueType::Graphics);

  std::unique_lock<std::mutex> lock;
  auto &queue = queue_g->queue()->lock(lock);

  try {
    vk::Result res = queue.presentKHR({
        *context.get_frame_ctx().m_render_finished,
        *swapchain.swapchain(),
        image_idx,
    });
    if (res == vk::Result::eSuboptimalKHR) {
      logger.Debug("Recreating swapchain (suboptimal)");
      recreate_swapchain();
    }
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

    static const char *ZoneFrameMark = "frame";
    FrameMarkStart(ZoneFrameMark);

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
    FrameMarkEnd(ZoneFrameMark);
  }

  return 0;
}
} // namespace v4dg