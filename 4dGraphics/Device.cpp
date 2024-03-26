#include "Device.hpp"
#include "Debug.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_extension_inspection.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <SDL2/SDL_vulkan.h>

#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>

namespace v4dg {
namespace {
vk::Bool32
debugMessageFuncCpp(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                    vk::DebugUtilsMessageTypeFlagsEXT types,
                    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void * /*pUserData*/) noexcept {

  try {
    std::string message =
        std::format("\n"
                    "{}: {}:\n"
                    "\tmessageIDName   = <{}>\n"
                    "\tmessageIdNumber = {:x}\n"
                    "\tmessage         = <{}>\n",
                    severity, types, pCallbackData->pMessageIdName,
                    static_cast<uint32_t>(pCallbackData->messageIdNumber),
                    pCallbackData->pMessage);

    auto format_append = [&]<typename... Ts>(std::format_string<Ts...> fmt,
                                             Ts &&...args) {
      std::format_to(std::back_inserter(message), fmt,
                     std::forward<Ts>(args)...);
    };

    if (0 < pCallbackData->queueLabelCount) {
      message.append("\tQueue Labels:\n");

      for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
        format_append("\t\tlabelName = <{}>\n",
                      pCallbackData->pQueueLabels[i].pLabelName);
    }
    if (0 < pCallbackData->cmdBufLabelCount) {
      message.append("\tCommandBuffer Labels:\n");

      for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
        format_append("\t\tlabelName = <{}>\n",
                      pCallbackData->pCmdBufLabels[i].pLabelName);
    }

    for (uint32_t i = 0; i < pCallbackData->objectCount; i++) {
      format_append(
          "\tObject {}\n"
          "\t\tobjectType   = {}\n"
          "\t\tobjectHandle = {:x}\n",
          i, static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType),
          pCallbackData->pObjects[i].objectHandle);

      if (pCallbackData->pObjects[i].pObjectName)
        format_append("\t\tobjectName   = <{}>\n",
                      pCallbackData->pObjects[i].pObjectName);
    }

    message.pop_back(); // remove trailing newline

    Logger::LogLevel level;

    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
      level = Logger::LogLevel::Error;
    else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
      level = Logger::LogLevel::Warning;
    else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
      level = Logger::LogLevel::Log;
    else
      level = Logger::LogLevel::Debug;

    logger.GenericLog(level, "{}", message);
  } catch (const std::exception &e) {
    logger.Error("Exception caught in debug message callback: {}", e.what());
  }

  return vk::False;
};

VKAPI_ATTR VkBool32 VKAPI_CALL
debugMessageFunc(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                 VkDebugUtilsMessengerCallbackDataEXT const *pCallbackData,
                 void *pUserData) noexcept {
  auto severity =
      static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity);
  auto types = static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes);

  return static_cast<VkBool32>(debugMessageFuncCpp(
      severity, types,
      reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT *>(
          pCallbackData),
      pUserData));
}

using std::ranges::contains;

constexpr auto make_ext_storage(std::string_view ext) {
  extension_storage storage;
  assert(ext.size() < storage.size());

  std::ranges::fill(storage, '\0');
  std::ranges::copy(ext, storage.begin());
  return storage;
}

constexpr void unique_add(auto &dst, const auto &ext) {
  if (!contains(dst, ext)) {
    dst.push_back(ext);
  }
}

constexpr void unique_add_if_present(auto &dst, auto &src, const auto &ext) {
  if (contains(src, ext))
    unique_add(dst, ext);
}

std::vector<const char *> to_c_vector(auto &strings) {
  auto rg =
      strings | std::views::transform([](const auto &s) { return s.data(); });
  return {rg.begin(), rg.end()};
}

constexpr auto transform_to_ext_storage = std::views::transform(
    [](const auto &ext) { return make_ext_storage(ext); });

constexpr std::array required_instance_exts{
    vk::KHRSurfaceExtensionName,
};

constexpr std::array wanted_instance_exts{
    vk::EXTDebugUtilsExtensionName,
    vk::KHRPortabilityEnumerationExtensionName,
};

constexpr std::array required_device_exts{
    vk::KHRSwapchainExtensionName,

    // VP_KHR_roadmap_2022
    vk::KHRGlobalPriorityExtensionName,

    // VP_KHR_roadmap_2024
    // vk::KHRDynamicRenderingLocalReadExtensionName,
    // vk::KHRLoadStoreOpNoneExtensionName,
    // vk::KHRShaderQuadControlExtensionName,
    // vk::KHRShaderMaximalReconvergenceExtensionName,
    vk::KHRShaderSubgroupUniformControlFlowExtensionName,
    // vk::KHRShaderSubgroupRotateExtensionName,
    // vk::KHRShaderFloatControls2ExtensionName,
    // vk::KHRShaderExpectAssumeExtensionName,
    // vk::KHRLineRasterizationExtensionName,
    // vk::KHRVertexAttributeDivisorExtensionName,
    // vk::KHRIndexTypeUint8ExtensionName,
    // vk::KHRMapMemory2ExtensionName,
    vk::KHRMaintenance5ExtensionName,
    vk::KHRPushDescriptorExtensionName,
};

constexpr std::array wanted_device_exts{
    vk::EXTMemoryBudgetExtensionName,
    vk::EXTMemoryPriorityExtensionName,
    vk::EXTSwapchainColorSpaceExtensionName,
    vk::EXTDeviceFaultExtensionName,
};

} // namespace

Instance::Instance(vk::raii::Context context,
                   vk::Optional<const vk::AllocationCallbacks> allocator)
    : m_context(std::move(context)), m_maxApiVer(vk::ApiVersion13),
      m_apiVer(std::min(m_maxApiVer, m_context.enumerateInstanceVersion())),
      m_layers(chooseLayers()), m_extensions(chooseExtensions()),
      m_debugUtilsEnabled(contains(
          m_extensions, make_ext_storage(vk::EXTDebugUtilsExtensionName))),
      m_instance(initInstance(allocator)),
      m_debugMessenger(m_debugUtilsEnabled
                           ? instance().createDebugUtilsMessengerEXT(
                                 debugMessengerCreateInfo())
                           : vk::raii::DebugUtilsMessengerEXT{nullptr}) {

  logger.Log("vulkan header version: {}.{}.{}",
             vk::apiVersionMajor(vk::HeaderVersionComplete),
             vk::apiVersionMinor(vk::HeaderVersionComplete),
             vk::apiVersionPatch(vk::HeaderVersionComplete));
}

std::vector<extension_storage> Instance::chooseLayers() const {
  std::vector<extension_storage> layers;

  std::vector<std::string_view> requestedLayers{
      "VK_LAYER_KHRONOS_shader_object",
      "VK_LAYER_KHRONOS_memory_decompression",
  };

  if (!is_production)
    requestedLayers.push_back("VK_LAYER_KHRONOS_validation");

  for (const auto &layer_p : m_context.enumerateInstanceLayerProperties())
    if (contains(requestedLayers, std::string_view{layer_p.layerName}))
      unique_add(layers, layer_p.layerName);

  return layers;
}

std::vector<extension_storage> Instance::chooseExtensions() const {
  std::vector<extension_storage> extensions;

  std::vector<std::string_view> requiredInstanceExts, wantedInstanceExts;

  for (const auto &ext : required_instance_exts)
    unique_add(requiredInstanceExts, ext);

  for (const auto &ext : wanted_instance_exts)
    unique_add(wantedInstanceExts, ext);

  uint32_t window_ext_count;
  SDL_Vulkan_GetInstanceExtensions(nullptr, &window_ext_count, nullptr);
  std::vector<const char *> window_exts(window_ext_count);
  SDL_Vulkan_GetInstanceExtensions(nullptr, &window_ext_count,
                                   window_exts.data());

  logger.Debug("Required window extensions:");
  for (const auto &ext : window_exts)
    logger.Debug("\t{}", std::string_view{ext});

  for (const auto &ext : window_exts)
    unique_add(requiredInstanceExts, ext);

  std::vector<extension_storage> avaiable_exts;

  for (const auto &ext : m_context.enumerateInstanceExtensionProperties())
    unique_add(avaiable_exts, ext.extensionName);

  for (const auto &layer : m_layers)
    for (const auto &ext :
         m_context.enumerateInstanceExtensionProperties(std::string{layer}))
      unique_add(avaiable_exts, ext.extensionName);

  for (const auto &ext : wantedInstanceExts | transform_to_ext_storage) {
    unique_add_if_present(extensions, avaiable_exts, ext);
  }

  for (const auto &ext : requiredInstanceExts | transform_to_ext_storage) {
    if (!contains(avaiable_exts, ext))
      throw exception("Required instance extension {} not supported", ext);
    unique_add(extensions, ext);
  }

  return extensions;
}

vk::raii::Instance
Instance::initInstance(const vk::AllocationCallbacks *allocator) const {
  if (vk::apiVersionVariant(m_context.enumerateInstanceVersion()) != 0)
    throw exception("Non-variant Vulkan API required (variant {})",
                    m_context.enumerateInstanceVersion());

  if (m_apiVer < vk::ApiVersion13)
    throw exception("Vulkan 1.3 or higher is required");

  vk::ApplicationInfo ai{nullptr, 0, "4dGraphics",
                         vk::makeApiVersion(0, 0, 0, 1), vk::ApiVersion13};

  vk::InstanceCreateFlags flags{};
  if (contains(m_extensions,
               make_ext_storage(vk::KHRPortabilityEnumerationExtensionName)))
    flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

  auto layers_c = to_c_vector(m_layers);
  auto instance_exts_c = to_c_vector(m_extensions);

  vk::StructureChain<vk::InstanceCreateInfo,
                     vk::DebugUtilsMessengerCreateInfoEXT>
      ici{{flags, &ai, layers_c, instance_exts_c}, debugMessengerCreateInfo()};

  if (!debugUtilsEnabled())
    ici.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();

  logger.Log("Creating Vulkan instance with {} layers and {} extensions",
             m_layers.size(), m_extensions.size());

  logger.Log("Layers:");
  for (const auto &layer : m_layers)
    logger.Log("\t{}", std::string_view{layer});

  logger.Log("Extensions:");
  for (const auto &ext : m_extensions)
    logger.Log("\t{}", std::string_view{ext});

  return m_context.createInstance(ici.get<>(), allocator);
}

vk::DebugUtilsMessengerCreateInfoEXT Instance::debugMessengerCreateInfo() {
  return {{},
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
          vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
          &debugMessageFunc};
}

Device::Device(const Instance &instance, vk::SurfaceKHR surface)
    : m_instance(instance), m_physicalDevice(choosePhysicalDevice(surface)),
      m_apiVer(std::min(instance.maxApiVer(),
                        m_physicalDevice.getProperties().apiVersion)),
      m_extensions(chooseExtensions()), m_features(chooseFeatures()),
      m_device(initDevice()), m_allocator(initAllocator()),
      m_queues(initQueues()) {

  m_rayTracing = true;

  auto *pdasf = getVkStructureFromChain<
      vk::PhysicalDeviceAccelerationStructureFeaturesKHR>(m_features.get());
  m_rayTracing &= pdasf && pdasf->accelerationStructure;

  auto *rtpf =
      getVkStructureFromChain<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>(
          m_features.get());
  m_rayTracing &= rtpf && rtpf->rayTracingPipeline;

  auto *rtaf = getVkStructureFromChain<vk::PhysicalDeviceRayQueryFeaturesKHR>(
      m_features.get());
  m_rayTracing &= rtaf && rtaf->rayQuery;

  auto *msf = getVkStructureFromChain<vk::PhysicalDeviceMeshShaderFeaturesEXT>(
      m_features.get());
  m_meshShader = msf && msf->meshShader && msf->taskShader;

  m_deviceFault =
      contains(extensions(), make_ext_storage(vk::EXTDeviceFaultExtensionName));
}

std::vector<extension_storage>
Device::enumerateExtensions(const vk::raii::PhysicalDevice &pd) const {
  std::vector<extension_storage> avaiable_exts;

  for (const auto &ext : pd.enumerateDeviceExtensionProperties())
    unique_add(avaiable_exts, ext.extensionName);

  for (const auto &lay_exts : instance().layers())
    for (const auto &ext :
         pd.enumerateDeviceExtensionProperties(std::string{lay_exts}))
      unique_add(avaiable_exts, ext.extensionName);

  // remove promoted extensions to core
  std::ranges::remove_if(avaiable_exts, [](const auto &ext) {
    if (!vk::isPromotedExtension(ext))
      return false;

    auto promoted = vk::getExtensionPromotedTo(ext);
    return contains(
        std::span<const char *const>{
            {"VK_VERSION_1_3", "VK_VERSION_1_2", "VK_VERSION_1_1"}},
        promoted);
  });

  return avaiable_exts;
}

bool Device::physicalDeviceSuitable(const vk::raii::PhysicalDevice &pd,
                                    vk::SurfaceKHR surface) const {
  auto props = pd.getProperties();
  logger.Debug("Checking physical device {}", props.deviceName);

  if (props.apiVersion < vk::ApiVersion13) {
    logger.Debug("Physical device {} does not support Vulkan 1.3",
                 props.deviceName);
    return false;
  }

  auto exts = enumerateExtensions(pd);

  auto ext_present = [&](auto &ext) {
    if (!contains(exts, make_ext_storage(ext))) {
      logger.Debug("Physical device {} does not support required extension {}",
                   props.deviceName, ext);
      return false;
    } else {
      return true;
    }
  };

  auto families = pd.getQueueFamilyProperties();

  return std::ranges::all_of(required_device_exts, ext_present) &&
         std::ranges::any_of(families,
                             [](auto &qfp) {
                               return static_cast<bool>(
                                   qfp.queueFlags &
                                   vk::QueueFlagBits::eGraphics);
                             }) &&
         (!surface ||
          std::ranges::any_of(std::views::iota(0u, families.size()),
                              [&](uint32_t family) {
                                return pd.getSurfaceSupportKHR(family, surface);
                              }));
}

vk::raii::PhysicalDevice
Device::choosePhysicalDevice(vk::SurfaceKHR surface) const {
  auto phys_devices_view = instance().instance().enumeratePhysicalDevices() |
                           std::views::filter([&](auto &pd) {
                             return physicalDeviceSuitable(pd, surface);
                           });

  std::vector<vk::raii::PhysicalDevice> phys_devices{phys_devices_view.begin(),
                                                     phys_devices_view.end()};

  if (phys_devices.empty())
    throw exception("No suitable physical devices found");

  std::ranges::sort(phys_devices, [&](const vk::raii::PhysicalDevice &a,
                                      const vk::raii::PhysicalDevice &b) {
    auto a_type = a.getProperties().deviceType;
    auto b_type = b.getProperties().deviceType;

    static constexpr std::array prefered{
        vk::PhysicalDeviceType::eDiscreteGpu,
        vk::PhysicalDeviceType::eIntegratedGpu,
        vk::PhysicalDeviceType::eVirtualGpu,
    };

    auto a_it = std::ranges::find(prefered, a_type);
    auto b_it = std::ranges::find(prefered, b_type);

    return a_it < b_it;
  });

  logger.Debug("Found {} suitable physical devices", phys_devices.size());
  for (auto &pd : phys_devices) {
    auto props = pd.getProperties();
    logger.Debug("\tSuitable physical device: {} ({})", props.deviceName,
                 props.deviceType);
  }

  logger.Log("Choosing physical device {}",
             phys_devices.front().getProperties().deviceName);
  return phys_devices.front();
}

std::vector<extension_storage> Device::chooseExtensions() const {
  std::vector<extension_storage> extensions;

  auto avaiable_exts = enumerateExtensions(m_physicalDevice);

  for (const auto &ext : wanted_device_exts | transform_to_ext_storage)
    unique_add_if_present(extensions, avaiable_exts, ext);

  for (const auto &ext : required_device_exts | transform_to_ext_storage) {
    if (!contains(avaiable_exts, ext))
      throw exception("Required device extension {} not supported", ext);
    unique_add(extensions, ext);
  }

  return extensions;
}

std::shared_ptr<const vk::PhysicalDeviceFeatures2>
Device::chooseFeatures() const {
  auto all_features = std::make_shared<vk::StructureChain<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
#ifdef VK_KHR_portability_subset
      vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
#endif
      vk::PhysicalDeviceMaintenance5FeaturesKHR,
      vk::PhysicalDeviceMemoryPriorityFeaturesEXT,
      vk::PhysicalDeviceFaultFeaturesEXT>>();

  auto avaiable_features = m_physicalDevice.getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
#ifdef VK_KHR_portability_subset
      vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
#endif
      vk::PhysicalDeviceMemoryPriorityFeaturesEXT,
      vk::PhysicalDeviceFaultFeaturesEXT>();

  auto &feature_chain = *all_features;

  feature_chain.get<vk::PhysicalDeviceFeatures2>()
      .features

      .setShaderFloat64(vk::True)
      .setShaderInt64(vk::True)
      .setShaderInt16(vk::True)

      // VK_KHR_roadmap_2022
      .setFullDrawIndexUint32(vk::True)
      .setImageCubeArray(vk::True)
      .setIndependentBlend(vk::True)
      .setSampleRateShading(vk::True)
      .setDrawIndirectFirstInstance(vk::True)
      .setDepthClamp(vk::True)
      .setDepthBiasClamp(vk::True)
      .setSamplerAnisotropy(vk::True)
      .setOcclusionQueryPrecise(vk::True)
      .setFragmentStoresAndAtomics(vk::True)
      .setShaderStorageImageExtendedFormats(vk::True)
      .setShaderUniformBufferArrayDynamicIndexing(vk::True)
      .setShaderSampledImageArrayDynamicIndexing(vk::True)
      .setShaderStorageBufferArrayDynamicIndexing(vk::True)
      .setShaderStorageImageArrayDynamicIndexing(vk::True)

      // VK_KHR_roadmap_2024
      .setMultiDrawIndirect(vk::True)
      .setShaderImageGatherExtended(vk::True)
      .setShaderInt16(vk::True);

  feature_chain
      .get<vk::PhysicalDeviceVulkan11Features>()

      // 1.1 required
      .setMultiview(vk::True)

      // VK_KHR_roadmap_2022
      // VK_KHR_roadmap_2024
      .setShaderDrawParameters(vk::True)
      .setStorageBuffer16BitAccess(vk::True);

  feature_chain
      .get<vk::PhysicalDeviceVulkan12Features>()

      // 1.2 required
      .setUniformBufferStandardLayout(vk::True)

      .setShaderSubgroupExtendedTypes(vk::True)
      .setSeparateDepthStencilLayouts(vk::True)

      .setHostQueryReset(vk::True)
      .setTimelineSemaphore(vk::True)
      .setImagelessFramebuffer(vk::True)

      // 1.3 required
      .setBufferDeviceAddress(vk::True)
      .setVulkanMemoryModel(vk::True)
      .setVulkanMemoryModelDeviceScope(vk::True)

      // VK_KHR_roadmap_2022
      .setSamplerMirrorClampToEdge(vk::True)
      .setDescriptorIndexing(vk::True)
      .setShaderUniformTexelBufferArrayDynamicIndexing(vk::True)
      .setShaderStorageTexelBufferArrayDynamicIndexing(vk::True)

      .setShaderUniformBufferArrayNonUniformIndexing(vk::True)
      .setShaderSampledImageArrayNonUniformIndexing(vk::True)
      .setShaderStorageBufferArrayNonUniformIndexing(vk::True)
      .setShaderStorageImageArrayNonUniformIndexing(vk::True)
      .setShaderUniformTexelBufferArrayNonUniformIndexing(vk::True)
      .setShaderStorageTexelBufferArrayNonUniformIndexing(vk::True)

      .setDescriptorBindingSampledImageUpdateAfterBind(vk::True)
      .setDescriptorBindingStorageImageUpdateAfterBind(vk::True)
      .setDescriptorBindingStorageBufferUpdateAfterBind(vk::True)
      .setDescriptorBindingUniformTexelBufferUpdateAfterBind(vk::True)
      .setDescriptorBindingStorageTexelBufferUpdateAfterBind(vk::True)

      .setDescriptorBindingUpdateUnusedWhilePending(vk::True)
      .setDescriptorBindingPartiallyBound(vk::True)
      .setDescriptorBindingVariableDescriptorCount(vk::True)
      .setRuntimeDescriptorArray(vk::True)
      .setScalarBlockLayout(vk::True)

      // VK_KHR_roadmap_2024
      .setShaderInt8(vk::True)
      .setShaderFloat16(vk::True)
      .setStorageBuffer8BitAccess(vk::True);

  feature_chain
      .get<vk::PhysicalDeviceVulkan13Features>()

      // Vulkan 1.3 required
      .setInlineUniformBlock(vk::True)

      .setDescriptorBindingInlineUniformBlockUpdateAfterBind(vk::True)
      .setPipelineCreationCacheControl(vk::True)
      .setPrivateData(vk::True)

      .setShaderDemoteToHelperInvocation(vk::True)
      .setShaderTerminateInvocation(vk::True)

      .setSubgroupSizeControl(vk::True)
      .setComputeFullSubgroups(vk::True)

      .setSynchronization2(vk::True)

      .setShaderZeroInitializeWorkgroupMemory(vk::True)

      .setDynamicRendering(vk::True)

      .setShaderIntegerDotProduct(vk::True)
      .setMaintenance4(vk::True);

  feature_chain.get<vk::PhysicalDeviceMaintenance5FeaturesKHR>()
      .setMaintenance5(vk::True);

#ifdef VK_KHR_portability_subset
  // portability subset
  feature_chain.get<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();

  if (!contains(m_extensions,
                make_ext_storage(vk::KHRPortabilitySubsetExtensionName)))
    feature_chain.unlink<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>();
#endif

  // optional extensions
  feature_chain.get<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>()
      .setMemoryPriority(vk::True);

  if (!contains(m_extensions,
                make_ext_storage(vk::EXTMemoryPriorityExtensionName)))
    feature_chain.unlink<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>();

  feature_chain.get<vk::PhysicalDeviceFaultFeaturesEXT>()
      .setDeviceFault(vk::True)
      .setDeviceFaultVendorBinary(
          avaiable_features.get<vk::PhysicalDeviceFaultFeaturesEXT>()
              .deviceFaultVendorBinary);

  if (!contains(m_extensions,
                make_ext_storage(vk::EXTDeviceFaultExtensionName)))
    feature_chain.unlink<vk::PhysicalDeviceFaultFeaturesEXT>();

  return {std::move(all_features), &all_features->get<>()};
}

vk::raii::Device Device::initDevice() const {
  float prio = 0.5f;
  auto rg = m_physicalDevice.getQueueFamilyProperties() |
            std::views::transform([&, family = 0u](auto) mutable {
              return vk::DeviceQueueCreateInfo{{}, family++, 1, &prio};
            });

  std::vector<vk::DeviceQueueCreateInfo> qcis{rg.begin(), rg.end()};

  logger.Log("Creating Vulkan device with {} extensions", m_extensions.size());
  for (const auto &ext : m_extensions)
    logger.Log("\t{}", std::string_view{ext});

  auto extensions_c = to_c_vector(m_extensions);
  return {m_physicalDevice, {{}, qcis, {}, extensions_c, nullptr, features()}};
}

vma::UniqueAllocator Device::initAllocator() const {
  const vk::PhysicalDeviceFeatures2 *device_features = features();

  vma::AllocatorCreateFlags flags{};
  if (contains(m_extensions,
               make_ext_storage(vk::EXTMemoryBudgetExtensionName)))
    flags |= vma::AllocatorCreateFlagBits::eExtMemoryBudget;

  {
    auto *phdcmf =
        getVkStructureFromChain<vk::PhysicalDeviceCoherentMemoryFeaturesAMD>(
            device_features);

    if (phdcmf && phdcmf->deviceCoherentMemory)
      flags |= vma::AllocatorCreateFlagBits::eAmdDeviceCoherentMemory;
  }

  {
    const auto *pdbdaf =
        getVkStructureFromChain<vk::PhysicalDeviceBufferDeviceAddressFeatures>(
            device_features);
    const auto *pdv12f =
        getVkStructureFromChain<vk::PhysicalDeviceVulkan12Features>(
            device_features);

    if ((pdbdaf && pdbdaf->bufferDeviceAddress) ||
        (pdv12f && pdv12f->bufferDeviceAddress))
      flags |= vma::AllocatorCreateFlagBits::eBufferDeviceAddress;
  }

  {
    const auto *pdmpf =
        getVkStructureFromChain<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>(
            device_features);

    if (pdmpf && pdmpf->memoryPriority)
      flags |= vma::AllocatorCreateFlagBits::eExtMemoryPriority;
  }

  vma::AllocatorCreateInfo aci{};

  aci.flags = flags;
  aci.instance = *instance().instance();
  aci.physicalDevice = *m_physicalDevice;
  aci.device = *m_device;
  aci.vulkanApiVersion = m_apiVer;

  vma::VulkanFunctions functions = vma::functionsFromDispatcher(
      instance().instance().getDispatcher(), device().getDispatcher());

  aci.pVulkanFunctions = &functions;

  return vma::createAllocatorUnique(aci);
}

std::vector<std::vector<Queue>> Device::initQueues() const {
  std::vector<std::vector<Queue>> queues;

  auto props = m_physicalDevice.getQueueFamilyProperties();

  for (const auto &[family_, qfp] : props | std::views::enumerate) {
    uint32_t family = family_;
    auto flags = qfp.queueFlags;

    // graphics and compute queues can also do transfer operations
    //  but are not required to have a transfer flag
    if (flags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
      flags |= vk::QueueFlagBits::eTransfer;

    std::vector<Queue> family_queues;
    family_queues.push_back(Queue{vk::raii::Queue{device(), family, 0u}, family,
                                  0u, flags, qfp.timestampValidBits,
                                  qfp.minImageTransferGranularity});

    queues.push_back(std::move(family_queues));
  }

  return queues;
}
} // namespace v4dg