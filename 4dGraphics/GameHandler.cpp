#include "GameHandler.hpp"

#include <CommandBuffer.hpp>
#include <Context.hpp>
#include <Debug.hpp>
#include <Device.hpp>
#include <PipelineBuilder.hpp>
#include <Swapchain.hpp>
#include <TransferManager.hpp>
#include <VulkanCaches.hpp>
#include <VulkanConstructs.hpp>
#include <VulkanResources.hpp>
#include <cppHelpers.hpp>

#include <SDL2/SDL_vulkan.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <taskflow/taskflow.hpp>
#include <tracy/Tracy.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

using namespace v4dg;

namespace {
vk::raii::SurfaceKHR sdl_get_surface(const vk::raii::Instance &instance,
                                     SDL_Window *window) {
  VkSurfaceKHR raw_surface{};
  if (SDL_Vulkan_CreateSurface(window, *instance, &raw_surface) == SDL_FALSE) {
    throw exception("Could not create vulkan surface: {}", SDL_GetError());
  }

  return {instance, raw_surface};
}
} // namespace

ImGui_VulkanImpl::ImGui_VulkanImpl(const Swapchain &swapchain, Context &ctx)
    : pool(nullptr), color_format(swapchain.format()) {
  auto &queue = *ctx.get_queue(Context::QueueType::Graphics);

  static constexpr auto max_samplers = 4096;
  static constexpr auto max_sets = 4;

  std::vector<vk::DescriptorPoolSize> pool_sizes{
      {vk::DescriptorType::eCombinedImageSampler, max_samplers},
  };

  pool = {
      ctx.vkDevice(),
      {vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, max_sets,
       pool_sizes},
  };

  ctx.device().setDebugName(pool, "imgui descriptor pool");

  uint32_t const min_image_count =
      swapchain.presentMode() == vk::PresentModeKHR::eMailbox ? 3 : 2;

  ImGui_ImplVulkan_InitInfo ii{};

  ii.Instance = *ctx.vkInstance();
  ii.PhysicalDevice = *ctx.vkPhysicalDevice();
  ii.Device = *ctx.vkDevice();
  ii.QueueFamily = queue.queue().family();
  ii.Queue = *queue.queue().queue();

  ii.PipelineCache = *ctx.pipeline_cache();
  ii.DescriptorPool = *pool;

  ii.MinImageCount = min_image_count;
  ii.ImageCount = static_cast<uint32_t>(swapchain.images().size());
  ii.MSAASamples =
      static_cast<VkSampleCountFlagBits>(vk::SampleCountFlagBits::e1);

  ii.UseDynamicRendering = true;

  ii.PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo{
      0,
      color_format,
      vk::Format::eUndefined,
      vk::Format::eUndefined,
  };

  if (!ImGui_ImplVulkan_LoadFunctions(
          [](const char *name, void *user) {
            return static_cast<Context *>(user)->vkInstance().getProcAddr(name);
          },
          &ctx)) {
    throw exception("Could not load imgui vulkan functions");
  }

  if (!ImGui_ImplVulkan_Init(&ii)) {
    throw exception("Could not init imgui vulkan backend");
  }
}

ImGui_VulkanImpl::~ImGui_VulkanImpl() { ImGui_ImplVulkan_Shutdown(); }

MyGameHandler::MyGameHandler(const Config &cfg)
    : cfg(cfg),
      instance(vk::raii::Context(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
          SDL_Vulkan_GetVkGetInstanceProcAddr()))),
      surface(sdl_get_surface(instance.instance(), window())),
      device(instance, *surface), context(cfg, device),
      transfer_manager(context),
      swapchain(SwapchainBuilder{
          .surface = *surface,
          .preferred_format = vk::Format::eR8G8B8A8Unorm,
          .preferred_present_mode = vk::PresentModeKHR::eMailbox,
          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                        vk::ImageUsageFlagBits::eTransferDst,
          .image_count = 3,
      }
                    .build(context)),
      imguiVulkanImpl(swapchain, context),
      texture(ImageView::createTexture(
          context,
          Image::ImageCreateInfo{
              .format = vk::Format::eR16G16B16A16Sfloat,
              .extent = tex_extent,
              .usage = vk::ImageUsageFlagBits::eStorage |
                       vk::ImageUsageFlagBits::eTransferSrc,
          },
          {{}, vma::MemoryUsage::eAuto})),
      descriptor_set_layout(nullptr),
      pipeline_layout(PipelineLayoutInfo()
                          .add_sets(context.bindlessManager().get_layouts())
                          .add_push({vk::ShaderStageFlagBits::eCompute, 0U,
                                     sizeof(MandelbrotPushConstants)})
                          .create(device)),
      pipeline({nullptr, nullptr, nullptr}) {

  texture->setName(device, "mandelbrot texture");

  auto shader =
      load_shader_code(cfg.data_dir() / "Shaders/Mandelbrot.comp.spv");
  if (!shader) {
    std::visit(
        [&](const auto &e) {
          throw exception("Could not load shader: {}", to_string(e));
        },
        shader.error());
  }

  for (int variant = 0; variant < 3; variant++) {
    ShaderStageData shader_data(vk::ShaderStageFlagBits::eCompute, *shader);

    if (instance.debugUtilsEnabled()) {
      shader_data.set_debug_name(
          std::format("Shaders/Mandelbrot.comp.spv (var {})", variant));
    }

    shader_data.add_specialization(0, variant);
    vk::ComputePipelineCreateInfo const pci{
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

  while (true) {
    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(window(), &w, &h);
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
  }
}

bool MyGameHandler::handle_events() {
  ZoneScoped;
  SDL_Event event;
  while (SDL_PollEvent(&event) != 0) {
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
      default:
        break;
      }
      break;
    default:
      break;
    }

    ImGui_ImplSDL2_ProcessEvent(&event);
  }

  ImGui_ImplSDL2_NewFrame();
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
  ImGui::Text("scale: %f %f", mandelbrot_push_constants.scale.x,
              mandelbrot_push_constants.scale.y);
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
    static constexpr auto zoom_speed = 1.1;
    if (ImGui::GetIO().MouseWheel != 0) {
      auto delta = double{ImGui::GetIO().MouseWheel};

      // zoom into mouse position (position under the mouse stays the same)
      auto mouse_pos = to_glm<double>(ImGui::GetMousePos());
      auto swapchain_size = to_glm<double>(swapchain.extent());

      auto mouse_center_rel =
          mouse_pos - swapchain_size / 2.0; // NOLINT(*-magic-numbers)

      auto world_pos = mandelbrot_push_constants.center +
                       mouse_center_rel * mandelbrot_push_constants.scale;

      // fractal so exponential zoom
      auto scale = mandelbrot_push_constants.scale;
      auto new_scale = scale * std::pow(zoom_speed, delta);

      auto new_center = world_pos - mouse_center_rel * new_scale;

      mandelbrot_push_constants.center = new_center;
      mandelbrot_push_constants.scale = new_scale;
    }
  }

  ImGui::Render();
}

void MyGameHandler::record_gui(CommandBuffer &cb, vk::Image image,
                               vk::ImageView view) {
  ZoneScoped;

  cb->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  cb.barrier({}, {}, {},
             {
                 {
                     vk::PipelineStageFlagBits2::eBlit,
                     vk::AccessFlagBits2::eNone,
                     vk::PipelineStageFlagBits2::eComputeShader,
                     vk::AccessFlagBits2::eShaderStorageWrite,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eGeneral,
                     vk::QueueFamilyIgnored,
                     vk::QueueFamilyIgnored,
                     texture->vkImage(),
                     {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                 },
                 {
                     vk::PipelineStageFlagBits2::eBlit,
                     vk::AccessFlagBits2::eNone,
                     vk::PipelineStageFlagBits2::eBlit,
                     vk::AccessFlagBits2::eTransferWrite,
                     vk::ImageLayout::eUndefined,
                     vk::ImageLayout::eTransferDstOptimal,
                     vk::QueueFamilyIgnored,
                     vk::QueueFamilyIgnored,
                     image,
                     {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                 },
             });

  // render mandelbrot
  {
    ZoneScopedN("mandelbrot");
    auto labal = cb.debugLabelScope("mandelbrot", {0.0F, 1.0F, 0.0F, 1.0F});
    cb->bindPipeline(vk::PipelineBindPoint::eCompute,
                     *pipeline[current_pipeline]);

    context.bindlessManager().bind(cb, *pipeline_layout,
                                   vk::PipelineBindPoint::eCompute);

    mandelbrot_push_constants.image_idx = texture->storageHandle();

    cb->pushConstants<MandelbrotPushConstants>(
        *pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
        mandelbrot_push_constants);

    static constexpr auto workgroup_size_base = 8;
    auto workgroup_size = workgroup_size_base * (current_pipeline == 0 ? 2 : 1);
    cb->dispatch(DivCeil(texture->image()->extent().width, workgroup_size),
                 DivCeil(texture->image()->extent().height, workgroup_size), 1);
  }

  cb.barrier({}, {}, {},
             {
                 {
                     vk::PipelineStageFlagBits2::eComputeShader,
                     vk::AccessFlagBits2::eShaderStorageWrite,
                     vk::PipelineStageFlagBits2::eTransfer,
                     vk::AccessFlagBits2::eTransferRead,
                     vk::ImageLayout::eGeneral,
                     vk::ImageLayout::eTransferSrcOptimal,
                     vk::QueueFamilyIgnored,
                     vk::QueueFamilyIgnored,
                     texture->vkImage(),
                     {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                 },
             });

  {
    ZoneScopedN("blit");

    cb->blitImage(
        texture->vkImage(), vk::ImageLayout::eTransferSrcOptimal, image,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit{
            {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            {
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{int32_t(texture->image()->extent().width),
                             int32_t(texture->image()->extent().height), 1},
            },
            {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            {
                vk::Offset3D{0, 0, 0},
                vk::Offset3D{int32_t(swapchain.extent().width),
                             int32_t(swapchain.extent().height), 1},
            },
        },
        vk::Filter::eLinear);
  }

  cb.barrier({}, {}, {},
             {{vk::PipelineStageFlagBits2::eBlit,
               vk::AccessFlagBits2::eTransferWrite,
               vk::PipelineStageFlagBits2::eColorAttachmentOutput,
               vk::AccessFlagBits2::eColorAttachmentRead,
               vk::ImageLayout::eTransferDstOptimal,
               vk::ImageLayout::eAttachmentOptimal,
               vk::QueueFamilyIgnored,
               vk::QueueFamilyIgnored,
               image,
               {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}});

  {
    ZoneScopedN("imgui");
    auto label = cb.debugLabelScope("imgui", {0.0F, 1.0F, 1.0F, 1.0F});

    vk::RenderingAttachmentInfo rai{};
    rai.setImageView(view)
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setResolveMode(vk::ResolveModeFlagBits::eNone)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(vk::ClearColorValue{std::array{0.0F, 0.0F, 0.0F, 1.0F}});

    cb->beginRendering(
        vk::RenderingInfo{{}, {{}, swapchain.extent()}, 1, 0, rai});
    {
      ZoneScopedN("render");
      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *cb);
    }
    cb->endRendering();
  }

  // to the present engine
  cb.barrier({}, {}, {},
             {{vk::PipelineStageFlagBits2::eColorAttachmentOutput,
               vk::AccessFlagBits2::eColorAttachmentWrite,
               vk::PipelineStageFlagBits2::eColorAttachmentOutput,
               vk::AccessFlagBits2::eNone,
               vk::ImageLayout::eAttachmentOptimal,
               vk::ImageLayout::ePresentSrcKHR,
               vk::QueueFamilyIgnored,
               vk::QueueFamilyIgnored,
               image,
               {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}});

  cb.end();
}

void MyGameHandler::present(uint32_t image_idx) {
  ZoneScoped;
  auto &queue_g = context.get_queue(Context::QueueType::Graphics);
  std::scoped_lock const _{queue_g->queue_mutex()};

  const auto &queue = queue_g->queue().queue();

  try {
    vk::Result const res = queue.presentKHR({
        swapchain.readyToPresent(image_idx),
        *swapchain.swapchain(),
        image_idx,
    });
    if (res == vk::Result::eSuboptimalKHR) {
      logger.Debug("Recreating swapchain (suboptimal)");
      recreate_swapchain();
    }
  } catch (const vk::OutOfDateKHRError &e) {
    logger.Warning("Frame lost - swapchain out of date");
    // ignore as if we wanted to use the swapchain once more
    //  we will recreate it on acquire
  }
}

int MyGameHandler::Run() try {
  SDL_ShowWindow(window());

  auto last_frame = std::chrono::high_resolution_clock::now();

  while (!should_close) {
    auto now = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto delta =
        std::chrono::duration<double>(now - last_frame).count();
    last_frame = now;

    {
      using namespace std::chrono_literals;
      if (!has_focus && false) { // Don't waste CPU time when not focused
        std::this_thread::sleep_for(50ms);
      }
    }

    FrameMarkStart(nullptr);
    detail::destroy_helper const frame_mark_scope{
        [] { FrameMarkEnd(nullptr); }};

    {
      ZoneScopedN("advance frame");
      context.next_frame();
    }

    uint32_t image_idx = wait_for_image();
    if (!handle_events()) {
      break;
    }

    tf::Taskflow tf;
    std::optional<CommandBuffer> recorded;

    {
      ZoneScopedN("build taskflow");

      auto gui_task = tf.emplace([&] { gui(); }).name("gui");

      auto record =
          tf.emplace([&] {
              recorded = context.getGraphicsCommandBuffer();
              recorded->add_wait(context.get_frame_ctx().m_image_ready, 0,
                                 vk::PipelineStageFlagBits2::eBlit);

              record_gui(*recorded, swapchain.image(image_idx),
                         swapchain.imageView(image_idx));

              recorded->add_signal(
                  swapchain.readyToPresent(image_idx), 0,
                  vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            })
              .name("record")
              .succeed(gui_task);

      tf.emplace([&] {
          context.get_queue(Context::QueueType::Graphics)
              ->submit(SubmitionInfo::gather(std::move(recorded).value()));
        })
          .name("submit")
          .succeed(record);

      tf.emplace([&] { transfer_manager.doOutstandingTransfers(); })
          .name("async transfer")
          .succeed(record);
    }

    context.executor().run(tf).wait();

    present(image_idx);
  }

  return 0;
} catch (const vk::DeviceLostError &err) {
  context.device().make_device_lost_dump(cfg, err);
  return 1;
}
