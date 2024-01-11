#include "GameRenderHandler.hpp"

#include "Debug.hpp"
#include "GameTickHandler.hpp"
#include "VulkanHelpers.hpp"

/*
uint64_t GameRenderHandler::getDeviceViability(VkPhysicalDevice pd) {
  VulkanPDInfo pdi(pd);

  VkPhysicalDeviceProperties &props = pdi.props.properties;
  VkPhysicalDeviceFeatures &feats = pdi.feats.features;
  VkPhysicalDeviceLimits &limits = props.limits;

  // check if vulkan physical device has all requirements:
  if (VK_API_VERSION_VARIANT(props.apiVersion) != 0)
    return 0;
  if (props.apiVersion < VK_API_VERSION_1_3)
    return 0;

  if (!feats.shaderFloat64)
    return 0;

  // assign viability to allow choise of the best device
  uint64_t viability = 0;

  switch (props.deviceType) {
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    // possibly much more powerfull
    viability += 1 << 20;
    break;

  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    viability += 1 << 10;
    break;

  case VK_PHYSICAL_DEVICE_TYPE_CPU:
  case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    break;
    // nothing

  default:
    [[assume(false)]]
  }

  viability += (uint64_t)(limits.maxImageDimension2D);

  return viability;
}

GameRenderHandler::GameRenderHandler(tf::Subflow &sf, SDL_Window *window) {
  tf::Task initVolk =
      sf.emplace([] {
          auto getprocaddr =
              (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
          ASSERT_LOG(getprocaddr, "Could not get vkGetInstanceProcAddr");
          volkInitializeCustom(getprocaddr);
        }).name("initialize volk");

  tf::Task initInstance =
      sf.emplace([&] {
          std::vector<const char *> instanceLayers;
          std::vector<const char *> instanceExtensions;

          {
            const char *wantedLayers[]{
                "VK_LAYER_KHRONOS_validation",
                "VK_LAYER_KHRONOS_shader_object",
                "VK_LAYER_KHRONOS_synchronization2",
            };

            std::vector<VkLayerProperties> layerProps;
            CHECK_LOG(vulkan_helpers::get_vector(
                          layerProps, vkEnumerateInstanceLayerProperties),
                      "Cannot enumerate layers");

            for (const char *layer : wantedLayers)
              for (const auto &prop : layerProps)
                if (strcmp(layer, prop.layerName) == 0) {
                  instanceLayers.push_back(layer);
                  break;
                }

            const char *wantedExts[]{
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            };

            std::vector<VkExtensionProperties> instanceExts;
            CHECK_LOG(vulkan_helpers::enumerate_instance_extensions(
                          instanceExts, instanceLayers),
                      "Cannot enumerate instance extensions");

            for (const char *ext : wantedExts)
              if (vulkan_helpers::is_extension_present(instanceExts, ext))
                instanceExtensions.push_back(ext);

            // those are required, add them even if they are not present
            std::vector<const char *> exts;
            {
              unsigned int sdlextcount = 0;

              ASSERT_LOG(SDL_Vulkan_GetInstanceExtensions(window, &sdlextcount,
                                                          nullptr),
                         "Could not get instance surface exts");
              do {
                exts.resize(sdlextcount);
              } while (!SDL_Vulkan_GetInstanceExtensions(window, &sdlextcount,
                                                         exts.data()));
              exts.resize(sdlextcount);
            }

            for (const char *ext : exts)
              if (!vulkan_helpers::is_extension_present(instanceExtensions,
                                                        ext))
                instanceExtensions.push_back(ext);
          }

          const VkDebugUtilsMessengerCreateInfoEXT messengerCI = {
              .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
              .pNext = nullptr,
              .flags = 0,
              .messageSeverity =
                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                  //  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                  0,
              .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                             0,
              .pfnUserCallback = VulkanDebugCallback,
              .pUserData = nullptr};

          bool debUtils = vulkan_helpers::is_extension_present(
              instanceExtensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

          const void *pCreateInstancePNext = debUtils ? &messengerCI : nullptr;

          VkApplicationInfo ai{};
          ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
          ai.pApplicationName = "4dGraphics";
          ai.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
          ai.apiVersion = VK_API_VERSION_1_3;

          CHECK_LOG(CreateInstance(instanceLayers, instanceExtensions, &ai,
                                   true, pCreateInstancePNext, &vk.instance),
                    "Could not create vulkan instance");
          vk.enabledLayers = move(instanceLayers);
          vk.enabledExts = move(instanceExtensions);
          vk.apiVersion = ai.apiVersion;

          if (debUtils)
            CHECK_LOG(vkCreateDebugUtilsMessengerEXT(vk.instance, &messengerCI,
                                                     nullptr, &vk.messenger),
                      "Could not setup debug callbacks");
          else
            vk.messenger = VK_NULL_HANDLE;

          ASSERT_LOG(SDL_Vulkan_CreateSurface(window, vk.instance, &vk.surface),
                     "Could not create surface");
        })
          .name("create vulkan instance")
          .succeed(initVolk);

  std::vector<uint32_t> queueFamilies;
  uint32_t graphicsQueueIdx, transferQueueIdx, asyncComputeQueueIdx;

  tf::Task initDevice =
      sf.emplace([&] {
          std::vector<VkPhysicalDevice> devices;
          CHECK_LOG(vulkan_helpers::get_vector(
                        devices, vkEnumeratePhysicalDevices, vk.instance),
                    "Could not enumerate physical devices");
          std::vector<uint64_t> scores(devices.size());

          std::transform(
              devices.begin(), devices.end(), scores.begin(),
              [this](VkPhysicalDevice a) { return getDeviceViability(a); });

          uint32_t bestDevIndex =
              (uint32_t)(std::max_element(scores.begin(), scores.end()) -
                         scores.begin());
          VkPhysicalDevice best = devices[bestDevIndex];

          ASSERT_LOG(scores[bestDevIndex] != 0,
                     "Could not get viable physical device");
          VulkanPDInfo pdi(best);

          TRACE(DebugLevel::Log, "Selected %s\n",
                pdi.props.properties.deviceName);

          uint32_t familyCount = 0;

          {
            auto families =
                vulkan_helpers::get_vector_noerror<VkQueueFamilyProperties>(
                    vkGetPhysicalDeviceQueueFamilyProperties, best);
            for (auto &f : families)
              f.queueFlags |=
                  f.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
                      ? VK_QUEUE_TRANSFER_BIT
                      : 0;

            familyCount = (uint32_t)families.size();

            uint32_t graphicsFamily = FindQueueFamilies(
                families, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

            ASSERT_LOG(graphicsFamily != UINT32_MAX,
                       "Could not find graphics+compute queue family");
            families[graphicsFamily].queueCount -= 1;

            uint32_t asyncComputeFamily = FindQueueFamilies(
                families, VK_QUEUE_COMPUTE_BIT, 0, VK_QUEUE_GRAPHICS_BIT);
            if (asyncComputeFamily != UINT32_MAX)
              families[asyncComputeFamily].queueCount -= 1;

            uint32_t transferOnlyFlags =
                VK_QUEUE_SPARSE_BINDING_BIT | VK_QUEUE_TRANSFER_BIT;
            uint32_t transferFamily = FindQueueFamilies(
                families, VK_QUEUE_TRANSFER_BIT, 0, 0, ~transferOnlyFlags);
            if (transferFamily != UINT32_MAX)
              families[transferFamily].queueCount -= 1;

            graphicsQueueIdx = (uint32_t)queues.size();
            queues.push_back(VulkanQueue{.family = graphicsFamily});

            if (asyncComputeFamily != UINT32_MAX) {
              asyncComputeQueueIdx = (uint32_t)queues.size();
              queues.push_back(VulkanQueue{.family = asyncComputeFamily});
            } else
              asyncComputeQueueIdx = UINT32_MAX;

            if (transferFamily != UINT32_MAX) {
              transferQueueIdx = (uint32_t)queues.size();
              queues.push_back(VulkanQueue{.family = transferFamily});
            } else
              transferQueueIdx = UINT32_MAX;
          }

          std::vector<VkDeviceQueueCreateInfo> queueCI;

          std::vector<uint32_t> qCnt(familyCount);
          std::vector<float> priorities(queues.size(), 1.0f);
          for (auto &q : queues)
            qCnt[q.family] += 1;

          for (uint32_t family = 0; family < familyCount; family++) {
            if (qCnt[family] == 0)
              continue;

            queueCI.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = family,
                .queueCount = qCnt[family],
                .pQueuePriorities = priorities.data()});
          }

          std::vector<VkExtensionProperties> devExtensions;
          CHECK_LOG(vulkan_helpers::enumerate_device_extensions(
                        devExtensions, best, vk.enabledLayers),
                    "Could not enumerate extensions");

          VulkanPDInfo enable = pdi;
          enable.clear_core_features();

          std::vector<const char *> deviceExtensions{
              VK_KHR_SWAPCHAIN_EXTENSION_NAME};

          if (vulkan_helpers::is_extension_present(
                  devExtensions, VK_EXT_VALIDATION_CACHE_EXTENSION_NAME))
            deviceExtensions.push_back(VK_EXT_VALIDATION_CACHE_EXTENSION_NAME);

          if (vulkan_helpers::is_extension_present(
                  devExtensions, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
            deviceExtensions.push_back(VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);

            if (vulkan_helpers::is_extension_present(
                    devExtensions,
                    VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME))
              deviceExtensions.push_back(
                  VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
          }

#define ENABLE_IF_PRESENT(o) enable.o = pdi.o;

#define PREFIX feats.features
// enable core features (1.0)
#define EFC(o) ENABLE_IF_PRESENT(feats.features.o)

          // EFC( robustBufferAccess ) // required
          EFC(fullDrawIndexUint32)
          // EFC( imageCubeArray )
          // EFC( independentBlend )
          EFC(geometryShader)
          EFC(tessellationShader)
          // EFC( sampleRateShading )
          // EFC( dualSrcBlend )
          // EFC( logicOp )
          // EFC( multiDrawIndirect )
          // EFC( drawIndirectFirstInstance )
          // EFC( depthClamp )
          // EFC( depthBiasClamp )
          // EFC( fillModeNonSolid )
          // EFC( depthBounds )
          // EFC( wideLines )
          // EFC( largePoints )
          // EFC( alphaToOne )
          // EFC( multiViewport )
          EFC(samplerAnisotropy)
          EFC(textureCompressionETC2)
          EFC(textureCompressionASTC_LDR)
          EFC(textureCompressionBC)
          // EFC( occlusionQueryPrecise )
          // EFC( pipelineStatisticsQuery )
          // EFC( vertexPipelineStoresAndAtomics )
          // EFC( fragmentStoresAndAtomics )
          // EFC( shaderTessellationAndGeometryPointSize )
          // EFC( shaderImageGatherExtended )
          // EFC( shaderStorageImageExtendedFormats )
          // EFC( shaderStorageImageMultisample )
          EFC(shaderStorageImageReadWithoutFormat)
          EFC(shaderStorageImageWriteWithoutFormat)
          // EFC( shaderUniformBufferArrayDynamicIndexing )
          // EFC( shaderSampledImageArrayDynamicIndexing )
          // EFC( shaderStorageBufferArrayDynamicIndexing )
          // EFC( shaderStorageImageArrayDynamicIndexing )
          // EFC( shaderClipDistance )
          // EFC( shaderCullDistance )
          EFC(shaderFloat64)
    // EFC( shaderInt64 )
    // EFC( shaderInt16 )
    // EFC( shaderResourceResidency )
    // EFC( shaderResourceMinLod )
    // EFC( sparseBinding )
    // EFC( sparseResidencyBuffer )
    // EFC( sparseResidencyImage2D )
    // EFC( sparseResidencyImage3D )
    // EFC( sparseResidency2Samples )
    // EFC( sparseResidency4Samples )
    // EFC( sparseResidency8Samples )
    // EFC( sparseResidency16Samples )
    // EFC( sparseResidencyAliased )
    // EFC( variableMultisampleRate )
    // EFC( inheritedQueries )

#undef EFC

// enable core features (1.1)
#define EFC1(o) ENABLE_IF_PRESENT(feats11.o)
          // EFC1( storageBuffer16BitAccess )
          // EFC1( uniformAndStorageBuffer16BitAccess )
          // EFC1( storagePushConstant16 )
          // EFC1( storageInputOutput16 )
          EFC1(multiview)
          // EFC1( multiviewGeometryShader )
          // EFC1( multiviewTessellationShader )
          EFC1(variablePointersStorageBuffer)
          EFC1(variablePointers)
          // EFC1( protectedMemory )
          // EFC1( samplerYcbcrConversion )
          EFC1(shaderDrawParameters)
#undef EFC1

// enable core features (1.2)
#define EFC2(o) ENABLE_IF_PRESENT(feats12.o)
          // EFC2( samplerMirrorClampToEdge )
          // EFC2( drawIndirectCount )
          // EFC2( storageBuffer8BitAccess )
          // EFC2( uniformAndStorageBuffer8BitAccess )
          // EFC2( storagePushConstant8 )
          // EFC2( shaderBufferInt64Atomics )
          // EFC2( shaderSharedInt64Atomics )
          // EFC2( shaderFloat16 )
          // EFC2( shaderInt8 )
          // EFC2( descriptorIndexing )
          // EFC2( shaderInputAttachmentArrayDynamicIndexing )
          // EFC2( shaderUniformTexelBufferArrayDynamicIndexing )
          // EFC2( shaderStorageTexelBufferArrayDynamicIndexing )
          // EFC2( shaderUniformBufferArrayNonUniformIndexing )
          // EFC2( shaderSampledImageArrayNonUniformIndexing )
          // EFC2( shaderStorageBufferArrayNonUniformIndexing )
          // EFC2( shaderStorageImageArrayNonUniformIndexing )
          // EFC2( shaderInputAttachmentArrayNonUniformIndexing )
          // EFC2( shaderUniformTexelBufferArrayNonUniformIndexing )
          // EFC2( shaderStorageTexelBufferArrayNonUniformIndexing )
          // EFC2( descriptorBindingUniformBufferUpdateAfterBind )
          // EFC2( descriptorBindingSampledImageUpdateAfterBind )
          // EFC2( descriptorBindingStorageImageUpdateAfterBind )
          // EFC2( descriptorBindingStorageBufferUpdateAfterBind )
          // EFC2( descriptorBindingUniformTexelBufferUpdateAfterBind )
          // EFC2( descriptorBindingStorageTexelBufferUpdateAfterBind )
          // EFC2( descriptorBindingUpdateUnusedWhilePending )
          // EFC2( descriptorBindingPartiallyBound )
          // EFC2( descriptorBindingVariableDescriptorCount )
          // EFC2( runtimeDescriptorArray )
          // EFC2( samplerFilterMinmax )
          // EFC2( scalarBlockLayout )
          EFC2(imagelessFramebuffer)
          EFC2(uniformBufferStandardLayout)
          EFC2(shaderSubgroupExtendedTypes)
          EFC2(separateDepthStencilLayouts)
          EFC2(hostQueryReset)
          EFC2(timelineSemaphore)
          EFC2(bufferDeviceAddress)
          // EFC2( bufferDeviceAddressCaptureReplay )
          // EFC2( bufferDeviceAddressMultiDevice )
          EFC2(vulkanMemoryModel)
          EFC2(vulkanMemoryModelDeviceScope)
          // EFC2( vulkanMemoryModelAvailabilityVisibilityChains )
          // EFC2( shaderOutputViewportIndex )
          // EFC2( shaderOutputLayer )
          EFC2(subgroupBroadcastDynamicId)
#undef EFC2

#define EFC3(o) ENABLE_IF_PRESENT(feats13.o)

          // EFC3( robustImageAccess ) // required
          EFC3(inlineUniformBlock)
          // EFC3( descriptorBindingInlineUniformBlockUpdateAfterBind )
          EFC3(pipelineCreationCacheControl)
          // EFC3( privateData ) // required
          EFC3(shaderDemoteToHelperInvocation)
          EFC3(shaderTerminateInvocation)
          EFC3(subgroupSizeControl)
          EFC3(computeFullSubgroups)
          EFC3(synchronization2)
          EFC3(textureCompressionASTC_HDR)
          EFC3(shaderZeroInitializeWorkgroupMemory)
          EFC3(dynamicRendering)
          EFC3(shaderIntegerDotProduct)
          EFC3(maintenance4)

#undef EFC3
#undef ENABLE_IF_PRESENT

          TRACE(DebugLevel::Log, "Before device creation\n");
          CHECK_LOG(InitVulkanDevice(vk, best, queueCI, deviceExtensions,
                                     &enable.feats, vkDev),
                    "Could not create device");
        })
          .name("init vulkan device")
          .succeed(initInstance);

  uint32_t kScreenWidth, kScreenHeight;
  SDL_Vulkan_GetDrawableSize(window, (int *)&kScreenWidth,
                             (int *)&kScreenHeight);

  tf::Task initVulkanContext =
      sf.emplace([&] {
          TRACE(DebugLevel::Log, "Before vulkan context creation\n");
          auto &graphicsQueue = queues[graphicsQueueIdx];
          vkGetDeviceQueue(vkDev.device, graphicsQueue.family, 0,
                           &graphicsQueue.queue);
          CHECK_LOG(
              InitVulkanContext(vk, vkDev, 3, {kScreenWidth, kScreenHeight},
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_STORAGE_BIT | 0,
                                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                    VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | 0,
                                4, vkCtx),
              "Could not init vulkan context");

#if USE_OPTICK
          TRACE(DebugLevel::Log, "Setting up optick for vulkan\n");
          Optick::VulkanFunctions vulkanFunctions = {
              vkGetPhysicalDeviceProperties,
              (PFN_vkCreateQueryPool_)vkCreateQueryPool,
              (PFN_vkCreateCommandPool_)vkCreateCommandPool,
              (PFN_vkAllocateCommandBuffers_)vkAllocateCommandBuffers,
              (PFN_vkCreateFence_)vkCreateFence,
              vkCmdResetQueryPool,
              (PFN_vkQueueSubmit_)vkQueueSubmit,
              (PFN_vkWaitForFences_)vkWaitForFences,
              (PFN_vkResetCommandBuffer_)vkResetCommandBuffer,
              (PFN_vkCmdWriteTimestamp_)vkCmdWriteTimestamp,
              (PFN_vkGetQueryPoolResults_)vkGetQueryPoolResults,
              (PFN_vkBeginCommandBuffer_)vkBeginCommandBuffer,
              (PFN_vkEndCommandBuffer_)vkEndCommandBuffer,
              (PFN_vkResetFences_)vkResetFences,
              vkDestroyCommandPool,
              vkDestroyQueryPool,
              vkDestroyFence,
              vkFreeCommandBuffers,
          };
          OPTICK_GPU_INIT_VULKAN(
              &vkDev.device, &vkDev.physicalDevice, &vkRDev.graphicsQueue.queue,
              &vkRDev.graphicsQueue.family, 1, &vulkanFunctions);
#endif
        })
          .name("init vulkan context")
          .succeed(initDevice);

  tf::CriticalSection singleTimeCommands{1};

  tf::Task initLayouts =
      sf.emplace([&] {
          TRACE(DebugLevel::Log, "Before pipeline layout creation\n");
          CHECK_LOG(CreateEngineDescriptorSetLayout(
                        vkDev.device, &vkState.descriptorSetLayout),
                    "Could not create descriptor set layout");
          CHECK_LOG(CreatePipelineLayout(vkDev.device, 1,
                                         &vkState.descriptorSetLayout, 0,
                                         nullptr, &vkState.layout),
                    "Could not create pipeline layout");
        })
          .name("init layouts")
          .succeed(initDevice);

  VkFormat depthFormat;
  tf::Task chooseDepthFormat =
      sf.emplace([&] { depthFormat = FindDepthFormat(vkDev.physicalDevice); })
          .name("choose depth format")
          .succeed(initDevice);

  VkShaderModule vertShader = VK_NULL_HANDLE, fragShader = VK_NULL_HANDLE;
  cpph::destroy_helper destrVert, destrFrag;

  auto [vert, frag] = sf.emplace(
      [&] {
        CHECK_LOG(CreateShaderModule(vkDev.device, "Shaders/Simple.vert", 0,
                                     nullptr, &vertShader),
                  "Could not create vert shader");
        destrVert = [&] {
          vkDestroyShaderModule(vkDev.device, vertShader, nullptr);
        };
      },
      [&] {
        CHECK_LOG(CreateShaderModule(vkDev.device, "Shaders/Simple.frag", 0,
                                     nullptr, &fragShader),
                  "Could not create frag shader");
        destrFrag = [&] {
          vkDestroyShaderModule(vkDev.device, fragShader, nullptr);
        };
      });

  initDevice.precede(vert.name("compile vert shader"),
                     frag.name("compile frag shader"));

  tf::Task compileGraphPipeline =
      sf.emplace([&] {
          TRACE(DebugLevel::Log, "Before pipeline creation\n");
          VkGraphicsPipelineCreateInfo gpci{};
          FillGraphicsPipelineDefaults(&gpci);

          std::vector<VkPipelineShaderStageCreateInfo> shaders{
              FillShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main"),
              FillShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main"),
          };

          VkDynamicState dyncamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                             VK_DYNAMIC_STATE_SCISSOR};
          VkPipelineDynamicStateCreateInfo dsci{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .dynamicStateCount = (uint32_t)size(dyncamicStates),
              .pDynamicStates = data(dyncamicStates)};

          VkFormat colorFromats[] = {vkRDev.swapchain.format.format};

          VkPipelineRenderingCreateInfo prci{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
              .pNext = nullptr,
              .viewMask = 0,
              .colorAttachmentCount = (uint32_t)size(colorFromats),
              .pColorAttachmentFormats = data(colorFromats),
              .depthAttachmentFormat = depthFormat,
              .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
          };

          gpci.stageCount = (uint32_t)size(shaders);
          gpci.pStages = data(shaders);
          gpci.pDynamicState = &dsci;
          gpci.layout = vkState.layout;

          gpci.pNext = &prci;

          TRACE(DebugLevel::Log, "Before vkCreateGraphicsPipelines\n");
          CHECK_LOG(vkCreateGraphicsPipelines(vkDev.device, vkDev.pipelineCache,
                                              1, &gpci, nullptr,
                                              &vkState.graphicsPipeline),
                    "Could not compile graphics pipeline");
        })
          .name("compile graphics pipeline")
          .succeed(initRenderDevice, chooseDepthFormat, vert, frag);

  tf::Task initUBOS =
      sf.emplace([&] {
          const VkPhysicalDeviceProperties *props;
          vmaGetPhysicalDeviceProperties(vkDev.allocator, &props);

          VkDeviceSize uniformSize = 0;
          GetSuballocatedBufferSize(
              std::vector<VkDeviceSize>(vkRDev.swapchain.images.size(),
                                        sizeof(glm::mat4)),
              props->limits.minUniformBufferOffsetAlignment, &uniformSize,
              vkState.uniformBuffers);

          CHECK_LOG(CreateBuffer(
                        vkDev.allocator, uniformSize, 0,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                        VMA_MEMORY_USAGE_AUTO,
                        &vkState.uniformBufferMemory.buffer,
                        &vkState.uniformBufferMemory.bufferAllocation, nullptr),
                    "Could not create unifrom buffers");
        })
          .name("init ubos")
          .succeed(initRenderDevice);

  tf::Task loadModel =
      sf.emplace([&] {
          CHECK_LOG(CreateSSBOVertexBuffer(
                        vkRDev, "data/3dModels/SpaceShuttle.obj",
                        &vkState.modelBuffer.buffer,
                        &vkState.modelBuffer.bufferAllocation,
                        &vkState.vertexBuffer, &vkState.indexBuffer),
                    "Could not crate model");
        })
          .name("load model")
          .succeed(initRenderDevice);
  singleTimeCommands.add(loadModel);

  tf::Task createModelTexture =
      sf.emplace([&] {
          const VkPhysicalDeviceProperties *props;
          vmaGetPhysicalDeviceProperties(vkDev.allocator, &props);

          CHECK_LOG(CreateTextureSampler(vkDev.device, VK_FILTER_LINEAR,
                                         VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                         props->limits.maxSamplerAnisotropy,
                                         &vkState.textureSampler),
                    "Could not create sampler");

          CHECK_LOG(CreateTextureImage(
                        vkRDev, "data/3dModels/SpaceShuttle_BaseColor.png",
                        &vkState.texture),
                    "Could not create texture");
        })
          .name("init model texture")
          .succeed(initRenderDevice);
  singleTimeCommands.add(createModelTexture);

  tf::Task initDescSets =
      sf.emplace([&] {
          CHECK_LOG(CreateDescriptorSetHelper(
                        vkDev.device, 0,
                        (uint32_t)vkRDev.swapchain.images.size(), 10, 10, 10,
                        &vkState.descriptorPool),
                    "Could not create descriptor pool");

          CHECK_LOG(CreateEngineDescriptorSets(vkRDev, vkState),
                    "Cannot create descriptor sets");
        })
          .name("init descr sets")
          .succeed(initRenderDevice, initLayouts, loadModel, createModelTexture,
                   initUBOS);

  tf::Task initDepthRes =
      sf.emplace([&] {
          VkResult res = CreateImageResource(
              vkDev, depthFormat, VK_IMAGE_TYPE_2D,
              {kScreenWidth, kScreenHeight, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT,
              VK_IMAGE_TILING_OPTIMAL, 0,
              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
              0, VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED, &vkState.depthResource);

          if (res >= 0) {
            VkCommandBuffer cmdBuffer;
            res = BeginSingleTimeCommands(vkDev.device, vkRDev.commandPool,
                                          &cmdBuffer);

            if (res >= 0) {
              TransitionImageLayoutCmd(
                  cmdBuffer, vkState.depthResource.image,
                  FormatGetAspects(depthFormat), VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                  VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

              res =
                  EndSingleTimeCommands(vkDev.device, vkRDev.commandPool,
                                        vkRDev.graphicsQueue.queue, cmdBuffer);
            }
          }

          CHECK_LOG(res, "Could not create depth resource");
        })
          .name("init depth resouce")
          .succeed(initRenderDevice, chooseDepthFormat);
  singleTimeCommands.add(initDepthRes);

  // tmp

  tf::Task initComputeLayouts =
      sf.emplace([&] {
          const VkDescriptorSetLayoutBinding binding{
              .binding = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              .pImmutableSamplers = nullptr};

          const VkDescriptorSetLayoutCreateInfo dslci{
              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .bindingCount = 1,
              .pBindings = &binding,
          };

          CHECK_LOG(vkCreateDescriptorSetLayout(vkDev.device, &dslci, nullptr,
                                                &computeSetLayout),
                    "Cannot create compute set layout");

          const VkPushConstantRange pcr{
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              .offset = 0,
              .size = sizeof(computePushConstants),
          };

          const VkPipelineLayoutCreateInfo plci{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .setLayoutCount = 1,
              .pSetLayouts = &computeSetLayout,
              .pushConstantRangeCount = 1,
              .pPushConstantRanges = &pcr};
          CHECK_LOG(vkCreatePipelineLayout(vkDev.device, &plci, nullptr,
                                           &computeLayout),
                    "Cannot create compute layout");
        })
          .name("init compute layouts")
          .succeed(initDevice);

  tf::Task compileComputePipeline =
      sf.emplace([&] {
          VkShaderModule compModule = VK_NULL_HANDLE;
          CHECK_LOG(CreateShaderModule(vkDev.device, "Shaders/Mandelbrot.comp",
                                       0, nullptr, &compModule),
                    "Cannot create compute shader module");

          const VkComputePipelineCreateInfo cpci{
              .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .stage = {.sType =
                            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                        .module = compModule,
                        .pName = "main",
                        .pSpecializationInfo = nullptr},
              .layout = computeLayout,
              .basePipelineHandle = VK_NULL_HANDLE,
              .basePipelineIndex = -1};

          VkResult res = vkCreateComputePipelines(
              vkDev.device, vkDev.pipelineCache, 1, &cpci, nullptr, &compute);

          vkDestroyShaderModule(vkDev.device, compModule, nullptr);
          CHECK_LOG(res, "Cannot compile compute pipeline");
        })
          .name("compile compute pipeline")
          .succeed(initComputeLayouts);

  tf::Task initComputeDS =
      sf.emplace([&] {
          const VkDescriptorPoolSize dps{
              .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 10,
          };

          CHECK_LOG(
              CreateDescriptorPool(vkDev.device, nullptr, 0,
                                   (uint32_t)vkRDev.swapchain.images.size(), 1,
                                   &dps, &computePool),
              "Cannot create compute pool");

          computeDescriptors.resize(vkRDev.swapchain.images.size());
          std::vector<VkDescriptorSetLayout> layout(computeDescriptors.size(),
                                                    computeSetLayout);
          const VkDescriptorSetAllocateInfo dsai{
              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
              .pNext = nullptr,
              .descriptorPool = computePool,
              .descriptorSetCount = (uint32_t)computeDescriptors.size(),
              .pSetLayouts = layout.data()};

          CHECK_LOG(vkAllocateDescriptorSets(vkDev.device, &dsai,
                                             computeDescriptors.data()),
                    "Cannot allocate compute ds");

          std::vector<VkWriteDescriptorSet> writes;
          std::vector<VkDescriptorImageInfo> imageInfos;
          for (uint32_t i = 0; i < (uint32_t)computeDescriptors.size(); i++) {
            imageInfos.push_back(VkDescriptorImageInfo{
                .sampler = VK_NULL_HANDLE,
                .imageView = vkRDev.swapchain.imageViews[i],
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
          }

          for (uint32_t i = 0; i < (uint32_t)computeDescriptors.size(); i++) {
            writes.push_back(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = computeDescriptors[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &imageInfos[i],
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr});
          }

          vkUpdateDescriptorSets(vkDev.device, (uint32_t)writes.size(),
                                 writes.data(), 0, nullptr);
        })
          .name("init compute descriptor sets")
          .succeed(initRenderDevice, initComputeLayouts);

  // this try-catch does not work with taskflow :(
  try {
    sf.join();
  } catch (const std::exception &e) {
    TRACE(DebugLevel::FatalError, "%s\n", e.what());
    throw e;
  }
}

VkResult GameRenderHandler::RecreateSwapchain() {
  VkResult swpErr = VK_SUCCESS;

  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkPresentModeKHR> presentModes;
  swpErr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDev.physicalDevice,
                                                     vk.surface, &capabilities);
  if (swpErr >= 0)
    swpErr = vulkan_helpers::get_vector(
        presentModes, vkGetPhysicalDeviceSurfacePresentModesKHR,
        vkDev.physicalDevice, vk.surface);

  if (swpErr >= 0) {
    VkCompositeAlphaFlagBitsKHR compositeAlpha =
        (VkCompositeAlphaFlagBitsKHR)(1 << glm::findLSB(
                                          (int)capabilities
                                              .supportedCompositeAlpha));
    VkExtent2D newExtent =
        ChooseSwapchainExtent(vkRDev.swapchain.extent, capabilities);
    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VulkanSwapchain oldSwapchain = std::move(vkRDev.swapchain);
    VulkanSwapchain &swapchain = vkRDev.swapchain;

    swpErr = CreateSwapchain(
        vkDev.device, vk.surface, newExtent, 1,
        ChooseSwapImageCount(capabilities), oldSwapchain.format,
        oldSwapchain.usages, capabilities.currentTransform, compositeAlpha,
        ChooseSwapPresentMode(vkDev.physicalDevice, vk.surface, true, false),
        VK_TRUE, oldSwapchain.swapchain, &newSwapchain);

    swapchain.extent = newExtent;
    swapchain.format = oldSwapchain.format;
    swapchain.usages = oldSwapchain.usages;

    // old swapchain is retired even when CreateSwapchain failed
    vkRDev.EndOfUsageHandlers.push(vulkan_helpers::make_eofCallback(
        [dev = vkDev.device, swapchain = std::move(oldSwapchain)]() mutable {
          DestroyVulkanSwapchain(dev, swapchain);
        }));

    if (swpErr >= 0) {
      swapchain.swapchain = newSwapchain;
      swpErr = CreateSwapchainImages(vkDev.device, swapchain.swapchain, nullptr,
                                     swapchain.format.format, swapchain.images,
                                     swapchain.imageViews);
    }
  }

  return swpErr;
}

void GameRenderHandler::ClearDestructionQueue(uint64_t until) {
  OPTICK_EVENT();
  while (!vkRDev.EndOfUsageHandlers.empty()) {
    const EndOfFrameQueueItem &item = vkRDev.EndOfUsageHandlers.front();
    const EndOfFrameQueueItemFlags type = item.type;
    const EndOfFrameQueueItem::payload_t &payload = item.payload;

    if (type == WAIT_BEGIN_FRAME) {
      // don't advance if there is wait flag for greater frame and
      // leave it there for next frames
      if (payload.waitBeginFrame.frameIdx > until)
        break;
    } else if (type == END_OF_FRAME_CALLBACK) {
      payload.EndOfFrameCallback.callback(payload.EndOfFrameCallback.userPtr);
    } else {
      OutputDebug(DebugLevel::Error, "eof unknown flag %d\n", type);
    }

    vkRDev.EndOfUsageHandlers.pop();
  }
}

VkResult GameRenderHandler::AdvanceFrame(uint32_t *imageIdx) {
  OPTICK_EVENT();
  // static double filteredDT = 1;
  // auto t = glfwGetTime();
  // auto dt = t - lt;
  // lt = t;
  // constexpr double alpha = 0.1;
  // filteredDT = filteredDT * ( 1 - alpha ) + dt * alpha;

  vkRDev.currentFrameId++;
  vkRDev.frameId = (vkRDev.frameId + 1) % vkRDev.framesInFlight;

  {
    OPTICK_EVENT("Wait for gpu fence");
    // OutputDebug( DebugLevel::Log, "%llu: %8.4fms (%6.2f)\n",
    // vkRDev.currentFrameId, dt * 1000, 1. / filteredDT );
    VK_CHECK_RET(vkWaitForFences(vkDev.device, 1,
                                 &vkRDev.resourcesUnusedFence[vkRDev.frameId],
                                 VK_FALSE, UINT64_MAX));
    ClearDestructionQueue(vkRDev.currentFrameId - vkRDev.framesInFlight);

    VK_CHECK_RET(vkResetFences(vkDev.device, 1,
                               &vkRDev.resourcesUnusedFence[vkRDev.frameId]));
  }

  OPTICK_GPU_FLIP(vkRDev.swapchain.swapchain);

  {
    OPTICK_EVENT("acquire next frame");

    VkResult res = VK_SUCCESS;

    do {
      if (res == VK_ERROR_OUT_OF_DATE_KHR)
        VK_CHECK_RET(RecreateSwapchain());

      res = vkAcquireNextImageKHR(vkDev.device, vkRDev.swapchain.swapchain,
                                  UINT64_MAX,
                                  vkRDev.imageReadySemaphores[vkRDev.frameId],
                                  VK_NULL_HANDLE, imageIdx);
    } while (res == VK_ERROR_OUT_OF_DATE_KHR); // should recreate swapchain

    VK_CHECK_RET(res);
  }

  return VK_SUCCESS;
}

VkResult GameRenderHandler::EndFrame(uint32_t imageIdx) {
  VkPresentInfoKHR pi{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                      .pNext = nullptr,
                      .waitSemaphoreCount = 1,
                      .pWaitSemaphores =
                          &vkRDev.renderingFinishedSemaphores[vkRDev.frameId],
                      .swapchainCount = 1,
                      .pSwapchains = &vkRDev.swapchain.swapchain,
                      .pImageIndices = &imageIdx,
                      .pResults = nullptr};

  VkResult res = vkQueuePresentKHR(vkRDev.graphicsQueue.queue, &pi);
  vkRDev.EndOfUsageHandlers.push(EndOfFrameQueueItem{
      .type = WAIT_BEGIN_FRAME,
      .payload = {.waitBeginFrame = {.frameIdx = vkRDev.currentFrameId}}});

  // recreate swapchain AFTER frame
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    res = RecreateSwapchain();

  return res;
}

void GameRenderHandler::OnDraw(const void *dat) {
  uint32_t imageIdx;
  CHECK_LOG(AdvanceFrame(&imageIdx), "Cannot advance frame");

  {
          void *BufferMemory;
          CHECK_LOG_RETURN_NOVAL( vmaMapMemory( vkDev.allocator,
                  vkState.uniformBufferMemory.bufferAllocation, &BufferMemory
  ), "could not map uniform buffer" );

          {

                  void *memory = (char *)BufferMemory +
  vkState.uniformBuffers[ imageIdx ].offset;

                  const glm::mat4 reverseDepthMatrrix(
                          1.f, 0.f, 0.f, 0.f,
                          0.f, 1.f, 0.f, 0.f,
                          0.f, 0.f, -1.f, 0.f,
                          0.f, 0.f, 1.f, 1.f
                  );

                  glm::mat4 persp = reverseDepthMatrrix *
  glm::infinitePerspective( glm::pi<float>() * .5f, (
  (float)vkRDev.swapchain.extent.width / vkRDev.swapchain.extent.height ), 1.f
  ); glm::mat4 view = glm::lookAt( glm::vec3{ 100.f, 100.f, 0.f }, { 0.f, 0.f,
  0.f }, { 0.f, 1.f, 0.f } ); glm::mat4 model = glm::rotate( glm::scale(
  glm::mat4( 1.0f ), glm::vec3( 0.8f ) ), (float)glfwGetTime() *
  glm::pi<float>() * 2.f, glm::vec3( 0.0f, 1.0f, 0.0f ) );

                  glm::mat4 mvp = persp * view * model;

                  *( (glm::mat4 *)memory ) = mvp;
          }

          vmaUnmapMemory( vkDev.allocator,
  vkState.uniformBufferMemory.bufferAllocation ); CHECK_LOG_RETURN_NOVAL(
  vmaFlushAllocation( vkDev.allocator,
  vkState.uniformBufferMemory.bufferAllocation, vkState.uniformBuffers[
  imageIdx ].offset, vkState.uniformBuffers[ imageIdx ].size ), "Could not
  flush unifrom buffers" );
  }
  
  pPC = (const computePushConstants *)dat;
  CHECK_LOG(FillCommandBuffers(imageIdx), "Cannot fill cmd buffer");

  const VkPipelineStageFlags waitFlags[] = {
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
  VkSubmitInfo si{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &vkRDev.imageReadySemaphores[vkRDev.frameId],
      .pWaitDstStageMask = waitFlags,
      .commandBufferCount = 1,
      .pCommandBuffers = &vkRDev.commandBuffers[vkRDev.frameId],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &vkRDev.renderingFinishedSemaphores[vkRDev.frameId],
  };

  {
    OPTICK_EVENT("Queue Submit");
    CHECK_LOG(vkQueueSubmit(vkRDev.graphicsQueue.queue, 1, &si,
                            vkRDev.resourcesUnusedFence[vkRDev.frameId]),
              "Cannot enqueue cmdBuffers");
  }

  CHECK_LOG(EndFrame(imageIdx), "Cannot present");
}

GameRenderHandler::~GameRenderHandler() {
  if (vkDev.device) {
    vkDeviceWaitIdle(vkDev.device);
    ClearDestructionQueue(UINT64_MAX);
    vkDestroyDescriptorSetLayout(vkDev.device, computeSetLayout, nullptr);
    vkDestroyPipelineLayout(vkDev.device, computeLayout, nullptr);
    vkDestroyPipeline(vkDev.device, compute, nullptr);
    vkDestroyDescriptorPool(vkDev.device, computePool, nullptr);

    DestroyVulkanState(vkDev, vkState);

    OPTICK_SHUTDOWN();
    DestroyVulkanRendererDevice(vkRDev);
  }
  DestroyVulkanDevice(vkDev);
  DestroyVulkanInstance(vk);
}

VkResult GameRenderHandler::FillCommandBuffers(uint32_t index) {
  const VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr};

  VkCommandBuffer CmdBuffer = vkRDev.commandBuffers[vkRDev.frameId];
  VK_CHECK_RET(vkBeginCommandBuffer(CmdBuffer, &bi));

  OPTICK_GPU_CONTEXT(CmdBuffer, Optick::GPU_QUEUE_COMPUTE, 0);

  TransitionImageLayoutCmd(
      CmdBuffer, vkRDev.swapchain.images[index], VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_GENERAL, // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
  // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );
  
  VkRenderingAttachmentInfo colorAttachment = {
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .pNext = nullptr,
          .imageView = vkRDev.swapchainImageViews[ index ],
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .resolveMode = VK_RESOLVE_MODE_NONE,
          .resolveImageView = VK_NULL_HANDLE,
          .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue = { .color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } }
  },
  };

  VkRenderingAttachmentInfo depthAttachment = {
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .pNext = nullptr,
          .imageView = vkState.depthResource.imageView,
          .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .resolveMode = VK_RESOLVE_MODE_NONE,
          .resolveImageView = VK_NULL_HANDLE,
          .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .clearValue = { .depthStencil = { .depth = 0.0f, .stencil = 0 } },
  };

  VkRenderingInfo rinfo = {
          .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .pNext = nullptr,
          .flags = 0,
          .renderArea = { {0, 0}, {kScreenWidth, kScreenHeight} },
          .layerCount = 1,
          .viewMask = 0,
          .colorAttachmentCount = 1,
          .pColorAttachments = &colorAttachment,
          .pDepthAttachment = &depthAttachment,
          .pStencilAttachment = nullptr
  };

  vkCmdBeginRenderingKHR( CmdBuffer, &rinfo );

          VkViewport vp{
                  .x = 0, .y = 0,
                  .width = (float)kScreenWidth, .height =
  (float)kScreenHeight, .minDepth = 0.0f, .maxDepth = 1.0f
          };
          vkCmdSetViewport( CmdBuffer, 0, 1, &vp );

          VkRect2D sc{
                  .offset = {0,0},
                  .extent = {kScreenWidth, kScreenHeight}
          };
          vkCmdSetScissor( CmdBuffer, 0, 1, &sc );

          uint32_t offset = (uint32_t)vkState.uniformBuffers[index].offset;
          vkCmdBindPipeline( CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
  vkState.graphicsPipeline ); vkCmdBindDescriptorSets( CmdBuffer,
  VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.layout, 0, 1,
  &vkState.descriptorSet, 1, &offset );//&vkState.descriptorSets[ index ], 0,
  nullptr );

          vkCmdBindIndexBuffer( CmdBuffer, vkState.modelBuffer.buffer,
  vkState.indexBuffer.offset, VK_INDEX_TYPE_UINT32 ); vkCmdDrawIndexed(
  CmdBuffer, (uint32_t)( vkState.indexBuffer.size / sizeof( uint32_t ) ), 1,
  0, 0, 0 );

  vkCmdEndRenderingKHR( CmdBuffer );

  {
    OPTICK_GPU_EVENT("Draw MandelBrotSet");

    VkDescriptorSet dset = computeDescriptors[vkRDev.frameId];

    const VkDescriptorImageInfo dii{.sampler = VK_NULL_HANDLE,
                                    .imageView =
                                        vkRDev.swapchain.imageViews[index],
                                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL};

    const VkWriteDescriptorSet wds{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = dset,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &dii,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(vkDev.device, 1, &wds, 0, nullptr);

    vkCmdBindPipeline(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute);
    vkCmdBindDescriptorSets(CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computeLayout, 0, 1, &dset, 0, nullptr);

    vkCmdPushConstants(CmdBuffer, computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(*pPC), pPC);
    const int blockSize = 8;
    vkCmdDispatch(
        CmdBuffer,
        AlignUp(vkRDev.swapchain.extent.width, blockSize) / blockSize,
        AlignUp(vkRDev.swapchain.extent.height, blockSize) / blockSize, 1);
  }
  TransitionImageLayoutCmd(
      CmdBuffer, vkRDev.swapchain.images[index], VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_GENERAL, // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_ACCESS_SHADER_WRITE_BIT,
      // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);

  return vkEndCommandBuffer(CmdBuffer);
}
*/