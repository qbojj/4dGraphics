module;

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <SDL2/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <exception>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

module v4dg;

import vulkan_hpp;

using namespace v4dg;

namespace {
vk::Bool32
debugMessageFuncCpp(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                    vk::DebugUtilsMessageTypeFlagsEXT types,
                    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void * /*pUserData*/) noexcept {

  try {
    std::string message;
    auto format_append = [&]<typename... Ts>(std::format_string<Ts...> fmt,
                                             Ts &&...args) {
      std::format_to(std::back_inserter(message), fmt,
                     std::forward<Ts>(args)...);
    };

    format_append("\n"
                  "{}: {}:\n"
                  "\tmessageIDName   = <{}>\n"
                  "\tmessageIdNumber = {:x}\n"
                  "\tmessage         = <{}>\n",
                  severity, types, pCallbackData->pMessageIdName,
                  static_cast<std::uint32_t>(pCallbackData->messageIdNumber),
                  pCallbackData->pMessage);

    if (0 < pCallbackData->queueLabelCount) {
      message.append("\tQueue Labels:\n");

      auto labels = std::span{pCallbackData->pQueueLabels,
                              pCallbackData->queueLabelCount};

      for (const auto &label : labels) {
        format_append("\t\tlabelName = <{}>\n", label.pLabelName);
      }
    }

    if (0 < pCallbackData->cmdBufLabelCount) {
      message.append("\tCommandBuffer Labels:\n");

      auto labels = std::span{pCallbackData->pCmdBufLabels,
                              pCallbackData->cmdBufLabelCount};

      for (const auto &label : labels) {
        format_append("\t\tlabelName = <{}>\n", label.pLabelName);
      }
    }

    auto objects =
        std::span{pCallbackData->pObjects, pCallbackData->objectCount};

    for (auto &&[i, object] : objects | std::views::enumerate) {
      format_append("\tObject {}\n"
                    "\t\tobjectType   = {}\n"
                    "\t\tobjectHandle = {:x}\n",
                    i, object.objectType, object.objectHandle);

      if (object.pObjectName != nullptr) {
        format_append("\t\tobjectName   = <{}>\n", object.pObjectName);
      }
    }

    message.pop_back(); // remove trailing newline

    Logger::LogLevel level = Logger::LogLevel::Debug;

    if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
      level = Logger::LogLevel::Error;
    } else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
      level = Logger::LogLevel::Warning;
    } else if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
      level = Logger::LogLevel::Log;
    } else {
      level = Logger::LogLevel::Debug;
    }

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

  const auto *callback_data =
      reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT *>(
          pCallbackData);

  return debugMessageFuncCpp(severity, types, callback_data, pUserData);
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
  if (contains(src, ext)) {
    unique_add(dst, ext);
  }
}

std::vector<const char *> to_c_vector(auto &strings) {
  auto rg =
      strings | std::views::transform([](const auto &s) { return s.data(); });
  return {rg.begin(), rg.end()};
}

constexpr auto transform_to_ext_storage = std::views::transform(
    [](const auto &ext) { return make_ext_storage(ext); });

using DeviceStats_ext_adder = void (DeviceStats::*)(std::string_view);
using DeviceStats_adder_map = std::map<std::string_view, DeviceStats_ext_adder>;
using DeviceStats_ext_mapping = DeviceStats_adder_map::value_type;

constexpr DeviceStats_ext_mapping make_ext_adder(std::string_view name) {
  return {name, &DeviceStats::add_extension};
}

template <vulkan_struct_extends<vk::PhysicalDeviceFeatures2> Tf>
constexpr DeviceStats_ext_mapping make_ext_adder(std::string_view name) {
  return {name, &DeviceStats::add_extension<Tf>};
}

template <vulkan_struct_extends<vk::PhysicalDeviceProperties2> Tp>
constexpr DeviceStats_ext_mapping make_ext_adder(std::string_view name) {
  return {name, &DeviceStats::add_extension<Tp>};
}

template <vulkan_struct_extends<vk::PhysicalDeviceFeatures2> Tf,
          vulkan_struct_extends<vk::PhysicalDeviceProperties2> Tp>
constexpr DeviceStats_ext_mapping make_ext_adder(std::string_view name) {
  return {name, &DeviceStats::add_extension<Tf, Tp>};
}

const std::map<std::string_view, DeviceStats_ext_adder> device_ext_spec{
    // VP_KHR_roadmap_2022
    make_ext_adder<vk::PhysicalDeviceGlobalPriorityQueryFeaturesKHR>(
        vk::KHRGlobalPriorityExtensionName),

    // VP_KHR_roadmap_2024
    make_ext_adder<vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR>(
        vk::KHRDynamicRenderingLocalReadExtensionName),
    make_ext_adder(vk::KHRLoadStoreOpNoneExtensionName),
    make_ext_adder<vk::PhysicalDeviceShaderQuadControlFeaturesKHR>(
        vk::KHRShaderQuadControlExtensionName),
    make_ext_adder<vk::PhysicalDeviceShaderMaximalReconvergenceFeaturesKHR>(
        vk::KHRShaderMaximalReconvergenceExtensionName),
    make_ext_adder<
        vk::PhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR>(
        vk::KHRShaderSubgroupUniformControlFlowExtensionName),
    make_ext_adder<vk::PhysicalDeviceShaderSubgroupRotateFeaturesKHR>(
        vk::KHRShaderSubgroupRotateExtensionName),
    make_ext_adder<vk::PhysicalDeviceShaderFloatControls2FeaturesKHR>(
        vk::KHRShaderFloatControls2ExtensionName),
    make_ext_adder<vk::PhysicalDeviceShaderExpectAssumeFeaturesKHR>(
        vk::KHRShaderExpectAssumeExtensionName),
    make_ext_adder<vk::PhysicalDeviceLineRasterizationFeaturesKHR,
                   vk::PhysicalDeviceLineRasterizationPropertiesKHR>(
        vk::KHRLineRasterizationExtensionName),
    make_ext_adder<vk::PhysicalDeviceVertexAttributeDivisorFeaturesKHR,
                   vk::PhysicalDeviceVertexAttributeDivisorPropertiesKHR>(
        vk::KHRVertexAttributeDivisorExtensionName),
    make_ext_adder<vk::PhysicalDeviceIndexTypeUint8FeaturesKHR>(
        vk::KHRIndexTypeUint8ExtensionName),
    make_ext_adder(vk::KHRMapMemory2ExtensionName),
    make_ext_adder<vk::PhysicalDeviceMaintenance5FeaturesKHR,
                   vk::PhysicalDeviceMaintenance5PropertiesKHR>(
        vk::KHRMaintenance5ExtensionName),
    make_ext_adder<vk::PhysicalDevicePushDescriptorPropertiesKHR>(
        vk::KHRPushDescriptorExtensionName),

    // other
    make_ext_adder(vk::KHRSwapchainExtensionName),
    make_ext_adder(vk::EXTMemoryBudgetExtensionName),
    make_ext_adder<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>(
        vk::EXTMemoryPriorityExtensionName),
    make_ext_adder(vk::EXTSwapchainColorSpaceExtensionName),
    make_ext_adder<vk::PhysicalDeviceFaultFeaturesEXT>(
        vk::EXTDeviceFaultExtensionName),
#ifdef VK_KHR_portability_subset
    make_ext_adder<vk::PhysicalDevicePortabilitySubsetFeaturesKHR,
                   vk::PhysicalDevicePortabilitySubsetPropertiesKHR>(
        vk::KHRPortabilitySubsetExtensionName),
#endif
    make_ext_adder<vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                   vk::PhysicalDeviceAccelerationStructurePropertiesKHR>(
        vk::KHRAccelerationStructureExtensionName),
    make_ext_adder<vk::PhysicalDeviceRayQueryFeaturesKHR>(
        vk::KHRRayQueryExtensionName),
    make_ext_adder<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
                   vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>(
        vk::KHRRayTracingPipelineExtensionName),
    make_ext_adder<vk::PhysicalDeviceRayTracingMaintenance1FeaturesKHR>(
        vk::KHRRayTracingMaintenance1ExtensionName),
    make_ext_adder(vk::KHRDeferredHostOperationsExtensionName),
};

constexpr std::array required_instance_exts{
    vk::KHRSurfaceExtensionName,
};

constexpr std::array wanted_instance_exts{
    vk::EXTDebugUtilsExtensionName,
    vk::KHRPortabilityEnumerationExtensionName,
};

const std::array required_device_exts{
    // VP_KHR_roadmap_2022
    // vk::KHRGlobalPriorityExtensionName,

    // VP_KHR_roadmap_2024
    vk::KHRLoadStoreOpNoneExtensionName,
    vk::KHRShaderQuadControlExtensionName,
    vk::KHRShaderMaximalReconvergenceExtensionName,
    vk::KHRShaderSubgroupUniformControlFlowExtensionName,
    // vk::KHRShaderSubgroupRotateExtensionName,
    // vk::KHRShaderFloatControls2ExtensionName,
    vk::KHRShaderExpectAssumeExtensionName,
    // vk::KHRLineRasterizationExtensionName,
    vk::KHRVertexAttributeDivisorExtensionName,
    vk::KHRIndexTypeUint8ExtensionName,
    vk::KHRMapMemory2ExtensionName,
    vk::KHRMaintenance5ExtensionName,
    vk::KHRPushDescriptorExtensionName,

    // other
    vk::KHRSwapchainExtensionName,
    vk::KHRDynamicRenderingLocalReadExtensionName,
};

const std::array wanted_device_exts{
    vk::EXTMemoryBudgetExtensionName,
    vk::EXTMemoryPriorityExtensionName,
    vk::EXTSwapchainColorSpaceExtensionName,
    vk::EXTDeviceFaultExtensionName,
#ifdef VK_KHR_portability_subset
    vk::KHRPortabilitySubsetExtensionName,
#endif
    vk::KHRDeferredHostOperationsExtensionName,
    vk::KHRAccelerationStructureExtensionName,
    vk::KHRRayQueryExtensionName,
    vk::KHRRayTracingPipelineExtensionName,
    vk::KHRRayTracingMaintenance1ExtensionName,
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

  if (!is_production) {
    requestedLayers.emplace_back("VK_LAYER_KHRONOS_validation");
  }

  for (const auto &layer_p : m_context.enumerateInstanceLayerProperties()) {
    if (contains(requestedLayers, std::string_view{layer_p.layerName})) {
      unique_add(layers, layer_p.layerName);
    }
  }

  return layers;
}

std::vector<extension_storage> Instance::chooseExtensions() const {
  std::vector<extension_storage> extensions;

  std::vector<std::string_view> requiredInstanceExts;
  std::vector<std::string_view> wantedInstanceExts;

  for (const auto &ext : required_instance_exts) {
    unique_add(requiredInstanceExts, ext);
  }

  for (const auto &ext : wanted_instance_exts) {
    unique_add(wantedInstanceExts, ext);
  }

  uint32_t window_ext_count = 0;
  SDL_Vulkan_GetInstanceExtensions(nullptr, &window_ext_count, nullptr);
  std::vector<const char *> window_exts(window_ext_count);
  SDL_Vulkan_GetInstanceExtensions(nullptr, &window_ext_count,
                                   window_exts.data());

  logger.Debug("Required window extensions:");
  for (const auto &ext : window_exts) {
    logger.Debug("\t{}", std::string_view{ext});
  }

  for (const auto &ext : window_exts) {
    unique_add(requiredInstanceExts, ext);
  }

  std::vector<extension_storage> avaiable_exts;

  for (const auto &ext : m_context.enumerateInstanceExtensionProperties()) {
    unique_add(avaiable_exts, ext.extensionName);
  }

  for (const auto &layer : m_layers) {
    for (const auto &ext :
         m_context.enumerateInstanceExtensionProperties(std::string{layer})) {
      unique_add(avaiable_exts, ext.extensionName);
    }
  }

  for (const auto &ext : wantedInstanceExts | transform_to_ext_storage) {
    unique_add_if_present(extensions, avaiable_exts, ext);
  }

  for (const auto &ext : requiredInstanceExts | transform_to_ext_storage) {
    if (!contains(avaiable_exts, ext)) {
      throw exception("Required instance extension {} not supported", ext);
    }
    unique_add(extensions, ext);
  }

  return extensions;
}

vk::raii::Instance
Instance::initInstance(const vk::AllocationCallbacks *allocator) const {
  if (vk::apiVersionVariant(m_context.enumerateInstanceVersion()) != 0) {
    throw exception("Non-variant Vulkan API required (variant {})",
                    m_context.enumerateInstanceVersion());
  }

  if (m_apiVer < vk::ApiVersion13) {
    throw exception("Vulkan 1.3 or higher is required");
  }

  vk::ApplicationInfo const ai{nullptr, 0, "4dGraphics",
                               vk::makeApiVersion(0, 0, 0, 1),
                               vk::ApiVersion13};

  vk::InstanceCreateFlags flags{};
  if (contains(m_extensions,
               make_ext_storage(vk::KHRPortabilityEnumerationExtensionName))) {
    flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
  }

  auto layers_c = to_c_vector(m_layers);
  auto instance_exts_c = to_c_vector(m_extensions);

  vk::StructureChain<vk::InstanceCreateInfo,
                     vk::DebugUtilsMessengerCreateInfoEXT>
      ici{{flags, &ai, layers_c, instance_exts_c}, debugMessengerCreateInfo()};

  if (!debugUtilsEnabled()) {
    ici.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
  }

  logger.Log("Creating Vulkan instance with {} layers and {} extensions",
             m_layers.size(), m_extensions.size());

  logger.Log("Layers:");
  for (const auto &layer : m_layers) {
    logger.Log("\t{}", std::string_view{layer});
  }

  logger.Log("Extensions:");
  for (const auto &ext : m_extensions) {
    logger.Log("\t{}", std::string_view{ext});
  }

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

bool DeviceStats::has_extension(std::string_view name) const noexcept {
  return std::ranges::contains(extensions, make_ext_storage(name));
}

void DeviceStats::add_extension(std::string_view name) {
  assert(!has_extension(name));
  extensions.push_back(make_ext_storage(name));
}

DeviceStats::DeviceStats(const vk::raii::PhysicalDevice &pd,
                         std::span<const extension_storage> layers) {
  std::vector<extension_storage> exts;

  for (const auto &ext : pd.enumerateDeviceExtensionProperties()) {
    unique_add(exts, ext.extensionName);
  }

  for (const auto &lay_exts : layers) {
    for (const auto &ext :
         pd.enumerateDeviceExtensionProperties(std::string{lay_exts})) {
      unique_add(exts, ext.extensionName);
    }
  }

  // fill everything (filters out unknown extensions)
  for (std::string_view const e : exts) {
    if (device_ext_spec.contains(e)) {
      std::invoke(device_ext_spec.at(e), *this, e);
    }
  }

  extensions.shrink_to_fit();
  features.shrink_to_fit();
  properties.shrink_to_fit();

  const auto &disp = *pd.getDispatcher();
  (*pd).getProperties2(properties.get<>(), disp);
  (*pd).getFeatures2(features.get<>(), disp);
}

Device::Device(const Instance &instance, vk::SurfaceKHR surface)
    : m_instance(instance), m_physicalDevice(nullptr), m_device(nullptr),
      m_allocator(nullptr) {
  DeviceStats pd_stats;
  std::tie(pd_stats, m_physicalDevice) = choosePhysicalDevice(surface);
  m_stats = chooseFeatures(pd_stats);
  m_device = initDevice();
  m_allocator = initAllocator();
  m_queues = initQueues();
}

std::optional<float>
Device::rankPhysicalDevice(const DeviceStats &stats,
                           const vk::raii::PhysicalDevice &pd,
                           vk::SurfaceKHR surface) {
  const auto &props =
      stats.properties.get<vk::PhysicalDeviceProperties2>()->properties;
  logger.Debug("Checking physical device {}", props.deviceName);

  if (props.apiVersion < vk::ApiVersion13) {
    logger.Debug("Physical device {} does not support Vulkan 1.3",
                 props.deviceName);
    return std::nullopt;
  }

  auto has_ext = std::bind_front(&DeviceStats::has_extension, stats);

  if (!std::ranges::all_of(required_device_exts, has_ext)) {
    std::string missing_exts;
    for (const auto &ext : required_device_exts) {
      if (!has_ext(ext)) {
        missing_exts += std::format("\n\t{}", ext);
      }
    }

    logger.Debug("Physical device {} does not support required extensions: {}",
                 props.deviceName, missing_exts);
    return std::nullopt;
  }

  auto families = pd.getQueueFamilyProperties();

  auto is_graphics_family = [](auto &qfp) {
    return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
  };

  if (!std::ranges::any_of(families, is_graphics_family)) {
    logger.Debug(
        "Physical device {} does not have a graphics capable queue family",
        props.deviceName);
    return std::nullopt;
  }

  auto queue_families = std::views::iota(
      std::uint32_t{0U}, static_cast<std::uint32_t>(families.size()));

  auto family_supports_present = [&](std::uint32_t family) -> bool {
    return pd.getSurfaceSupportKHR(family, surface);
  };

  if (surface &&
      !std::ranges::any_of(queue_families, family_supports_present)) {
    logger.Debug(
        "Physical device {} does not support present for specified surface",
        props.deviceName);
    return std::nullopt;
  }

  // we have a suitable device, now grade it

  // NOLINTBEGIN(*-magic-numbers): rank is a magic number by itself
  float rank = 0;

  static constexpr std::array prefered{
      vk::PhysicalDeviceType::eDiscreteGpu,
      vk::PhysicalDeviceType::eIntegratedGpu,
      vk::PhysicalDeviceType::eVirtualGpu,
  };

  auto type = props.deviceType;
  const auto *type_it = std::ranges::find(prefered, type);

  if (type_it != prefered.end()) {
    rank += 1000.F /
            static_cast<float>(1 + std::distance(prefered.begin(), type_it));
  }

  rank += static_cast<float>(props.limits.maxImageDimension2D) / 4096.F;

#ifdef VK_KHR_portability_subset
  // prefere non-portability subset devices
  if (stats.has_extension(vk::KHRPortabilitySubsetExtensionName)) {
    rank -= 100000;
  }
#endif

  logger.Debug("Physical device {} has rank {}", props.deviceName, rank);
  return rank;
  // NOLINTEND(*-magic-numbers)
}

std::pair<DeviceStats, vk::raii::PhysicalDevice>
Device::choosePhysicalDevice(vk::SurfaceKHR surface) const {
  struct pd_info {
    vk::raii::PhysicalDevice pd;
    DeviceStats stats;
    std::optional<float> rank;
  };

  auto phys_devices_view =
      vkInstance().enumeratePhysicalDevices() |
      std::views::transform([&](const vk::raii::PhysicalDevice &pd) -> pd_info {
        DeviceStats stats{pd, instance().layers()};
        auto rank = rankPhysicalDevice(stats, pd, surface);
        return pd_info{.pd = pd, .stats = std::move(stats), .rank = rank};
      });

  std::vector<pd_info> phys_devices{phys_devices_view.begin(),
                                    phys_devices_view.end()};

  std::ranges::sort(phys_devices, std::greater{}, &pd_info::rank);

  std::string devices_str;
  for (const auto &pd : phys_devices) {
    devices_str += std::format(
        "\n\t{}: {}", pd.stats.properties.get<>()->properties.deviceName,
        pd.rank.value_or(std::numeric_limits<float>::quiet_NaN()));
  }
  logger.Debug("Physical device rankings: {}", devices_str);

  if (phys_devices.empty()) {
    throw exception("No suitable physical devices found");
  }

  pd_info &best = phys_devices.front();
  logger.Log("Choosing physical device {}",
             best.stats.properties.get<>()->properties.deviceName);
  return {best.stats, best.pd};
}

DeviceStats Device::chooseFeatures(const DeviceStats &avaiable) const {
  DeviceStats enabled;

  for (std::string_view e : required_device_exts) {
    if (!device_ext_spec.contains(e)) {
      throw exception("No feature mapping for extension {}", e);
    }

    if (enabled.has_extension(e)) {
      continue;
    }

    if (!avaiable.has_extension(e)) {
      throw exception("Required device extension {} not supported", e);
    }

    enabled.add_extension(e);
  }

  for (std::string_view e : wanted_device_exts) {
    if (!device_ext_spec.contains(e)) {
      throw exception("No feature mapping for extension {}", e);
    }

    if (enabled.has_extension(e)) {
      continue;
    }

    if (!avaiable.has_extension(e)) {
      continue;
    }

    std::invoke(device_ext_spec.at(e), enabled, e);
  }

  // get properties and leave features empty
  enabled.extensions.shrink_to_fit();
  enabled.properties.shrink_to_fit();

  // remove all features (they are only added if needed)
  enabled.features = {};

  const auto &disp = *m_physicalDevice.getDispatcher();
  (*m_physicalDevice).getProperties2(enabled.properties.get<>(), disp);

  // choose features to enable

  const auto &avaiable_f = avaiable.features;
  auto &enabled_f = enabled.features;

  [[maybe_unused]] const auto *a_features2 =
      avaiable_f.get<vk::PhysicalDeviceFeatures2>();
  [[maybe_unused]] auto &e_features2 =
      enabled_f.get_or_add<vk::PhysicalDeviceFeatures2>();

  assert(a_features2);
  [[maybe_unused]] const auto &a_features = a_features2->features;
  [[maybe_unused]] auto &e_features = e_features2.features;

  e_features2.features.setShaderFloat64(vk::True)
      .setShaderInt64(vk::True)
      .setShaderInt16(vk::True)

      .setTextureCompressionASTC_LDR(a_features.textureCompressionASTC_LDR)
      .setTextureCompressionBC(a_features.textureCompressionBC)
      .setTextureCompressionETC2(a_features.textureCompressionETC2)

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

  [[maybe_unused]] const auto *a_features11 =
      avaiable_f.get<vk::PhysicalDeviceVulkan11Features>();
  [[maybe_unused]] auto &e_features11 =
      enabled_f.get_or_add<vk::PhysicalDeviceVulkan11Features>();

  assert(a_features11);

  e_features11

      // 1.1 required
      .setMultiview(vk::True)

      // VK_KHR_roadmap_2022
      // VK_KHR_roadmap_2024
      .setShaderDrawParameters(vk::True)
      .setStorageBuffer16BitAccess(vk::True);

  [[maybe_unused]] const auto *a_features12 =
      avaiable_f.get<vk::PhysicalDeviceVulkan12Features>();
  [[maybe_unused]] auto &e_features12 =
      enabled_f.get_or_add<vk::PhysicalDeviceVulkan12Features>();

  assert(a_features12);

  e_features12

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

  [[maybe_unused]] const auto *a_features13 =
      avaiable_f.get<vk::PhysicalDeviceVulkan13Features>();
  [[maybe_unused]] auto &e_features13 =
      enabled_f.get_or_add<vk::PhysicalDeviceVulkan13Features>();

  assert(a_features13);

  e_features13
      .setTextureCompressionASTC_HDR(a_features13->textureCompressionASTC_HDR)

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

  // VP_KHR_roadmap_2022
  // enabled_f.assign(vk::PhysicalDeviceGlobalPriorityQueryFeaturesKHR{vk::True});

  // VP_KHR_roadmap_2024
  enabled_f.assign(vk::PhysicalDeviceShaderQuadControlFeaturesKHR{vk::True});
  enabled_f.assign(
      vk::PhysicalDeviceShaderMaximalReconvergenceFeaturesKHR{vk::True});
  enabled_f.assign(
      vk::PhysicalDeviceShaderSubgroupUniformControlFlowFeaturesKHR{vk::True});
  // enabled_f.assign(vk::PhysicalDeviceShaderSubgroupRotateFeaturesKHR{vk::True});
  // enabled_f.assign(vk::PhysicalDeviceShaderFloatControls2FeaturesKHR{vk::True});
  enabled_f.assign(vk::PhysicalDeviceShaderExpectAssumeFeaturesKHR{vk::True});
  // enabled_f.assign(vk::PhysicalDeviceLineRasterizationFeaturesKHR{vk::True});
  // enabled_f.assign(vk::PhysicalDeviceVertexAttributeDivisorFeaturesKHR{vk::True});
  enabled_f.assign(vk::PhysicalDeviceIndexTypeUint8FeaturesKHR{vk::True});
  enabled_f.assign(vk::PhysicalDeviceMaintenance5FeaturesKHR{vk::True});

  // other
  enabled_f.assign(
      vk::PhysicalDeviceDynamicRenderingLocalReadFeaturesKHR{vk::True});

  // optional extensions
  auto copy_if_present =
      [&]<vulkan_struct_chainable<vk::PhysicalDeviceFeatures2> T>(
          const T & /*unused*/) {
        if (const auto *available = avaiable_f.get<T>()) {
          enabled_f.assign(*available);
        }
      };

  copy_if_present(vk::PhysicalDeviceMemoryPriorityFeaturesEXT{});
  copy_if_present(vk::PhysicalDeviceFaultFeaturesEXT{});

#ifdef VK_KHR_portability_subset

  // should enable all features that are needed by the app
  //  so will fix this when someone needs to run on a portability subset device
  if ([[maybe_unused]] const auto *a_portability_subset =
          avaiable_f.get<vk::PhysicalDevicePortabilitySubsetFeaturesKHR>()) {
    throw exception("Portability subset not implemented");
  }
#endif

  // ray tracing
  copy_if_present(vk::PhysicalDeviceAccelerationStructureFeaturesKHR{});
  copy_if_present(vk::PhysicalDeviceRayQueryFeaturesKHR{});
  copy_if_present(vk::PhysicalDeviceRayTracingPipelineFeaturesKHR{});
  copy_if_present(vk::PhysicalDeviceRayTracingMaintenance1FeaturesKHR{});

  enabled.features.shrink_to_fit();
  return enabled;
}

vk::raii::Device Device::initDevice() const {
  float prio = 0.5F; // NOLINT(*-magic-numbers)
  auto rg = m_physicalDevice.getQueueFamilyProperties() |
            std::views::transform([&, family = 0U](auto) mutable {
              return vk::DeviceQueueCreateInfo{{}, family++, 1, &prio};
            });

  std::vector<vk::DeviceQueueCreateInfo> qcis{rg.begin(), rg.end()};

  std::string exts_str;
  for (const auto &ext : stats().extensions) {
    exts_str += std::format("\n\t{}", ext);
  }
  logger.Log("Creating Vulkan device with extensions: {}", exts_str);

  auto extensions_c = to_c_vector(stats().extensions);
  return {m_physicalDevice,
          {{}, qcis, {}, extensions_c, nullptr, stats().features.get<>()}};
}

vma::UniqueAllocator Device::initAllocator() const {
  vma::AllocatorCreateFlags flags{};
  if (stats().has_extension(vk::EXTMemoryBudgetExtensionName)) {
    flags |= vma::AllocatorCreateFlagBits::eExtMemoryBudget;
  }

  if (const auto *phdcmf =
          stats().features.get<vk::PhysicalDeviceCoherentMemoryFeaturesAMD>()) {
    if (phdcmf->deviceCoherentMemory != 0U) {
      flags |= vma::AllocatorCreateFlagBits::eAmdDeviceCoherentMemory;
    }
  }

  if (const auto *pdbdaf =
          stats()
              .features.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>()) {
    if (pdbdaf->bufferDeviceAddress != 0U) {
      flags |= vma::AllocatorCreateFlagBits::eBufferDeviceAddress;
    }
  }

  if (const auto *pdv12f =
          stats().features.get<vk::PhysicalDeviceVulkan12Features>()) {
    if (pdv12f->bufferDeviceAddress != 0U) {
      flags |= vma::AllocatorCreateFlagBits::eBufferDeviceAddress;
    }
  }

  if (const auto *pdmpf =
          stats().features.get<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>()) {
    if (pdmpf->memoryPriority != 0U) {
      flags |= vma::AllocatorCreateFlagBits::eExtMemoryPriority;
    }
  }

  if (const auto *m5pf =
          stats().features.get<vk::PhysicalDeviceMaintenance5FeaturesKHR>()) {
    if (m5pf->maintenance5 != 0U) {
      flags |= vma::AllocatorCreateFlagBits::eKhrMaintenance5;
    }
  }

  vma::AllocatorCreateInfo aci{};

  aci.flags = flags;
  aci.instance = *vkInstance();
  aci.physicalDevice = *m_physicalDevice;
  aci.device = *m_device;
  aci.vulkanApiVersion = std::min(
      instance().apiVer(), stats().properties.get<>()->properties.apiVersion);

  vma::VulkanFunctions const functions = vma::functionsFromDispatcher(
      vkInstance().getDispatcher(), device().getDispatcher());

  aci.pVulkanFunctions = &functions;

  return vma::createAllocatorUnique(aci);
}

std::vector<std::vector<Queue>> Device::initQueues() const {
  std::vector<std::vector<Queue>> queues;

  auto props = m_physicalDevice.getQueueFamilyProperties();

  for (const auto &[family_, qfp] : props | std::views::enumerate) {
    uint32_t const family = family_;
    auto flags = qfp.queueFlags;

    // graphics and compute queues can also do transfer operations
    //  but are not required to have a transfer flag
    if (flags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) {
      flags |= vk::QueueFlagBits::eTransfer;
    }

    std::vector<Queue> family_queues;
    family_queues.emplace_back(vk::raii::Queue{device(), family, 0U}, family,
                               0U, flags, qfp.timestampValidBits,
                               qfp.minImageTransferGranularity);

    for (auto &q : family_queues) {
      setDebugName(q.queue(), "queue fam-{} idx-{}", q.family(), q.index());
    }

    queues.push_back(std::move(family_queues));
  }

  return queues;
}

void Device::make_device_lost_dump(const Config &cfg,
                                   const vk::DeviceLostError &error) const {
  logger.FatalError("Device lost: {}", error.what());

  if (!stats().has_extension(vk::EXTDeviceFaultExtensionName)) {
    logger.Warning("Device fault extension not available");
    return;
  }

  auto [counts, info] = device().getFaultInfoEXT();

  std::span addressInfos{info.pAddressInfos, counts.addressInfoCount};
  std::span vendorInfos{info.pVendorInfos, counts.vendorInfoCount};
  std::span<const std::byte> const vendorData{
      static_cast<std::byte *>(info.pVendorBinaryData),
      counts.vendorBinarySize};

  auto make_append_to = [](auto &&it) {
    return [it]<typename... Args>(std::format_string<Args...> fmt,
                                  Args &&...args) {
      std::format_to(it, fmt, std::forward<Args>(args)...);
    };
  };

  auto date = std::chrono::system_clock::now();

  auto [props2, id_props, driver_props] =
      physicalDevice()
          .getProperties2<vk::PhysicalDeviceProperties2,
                          vk::PhysicalDeviceIDProperties,
                          vk::PhysicalDeviceDriverProperties>();

  auto &props = props2.properties;

  std::string message;

  {
    auto append = make_append_to(std::back_inserter(message));

    append("Device fault: {}\n", info.description);
    append("  addressInfos:\n");
    for (auto &ai : addressInfos) {
      append("    address type: {}\n", ai.addressType);
      append("    reported address: 0x{:016x}\n", ai.reportedAddress);
      append("    address mask: 0x{:016x}\n", ~(ai.addressPrecision - 1));
      append("\n");
    }

    append("  vendorInfos:\n");
    for (auto &vi : vendorInfos) {
      append("    description: {}\n", vi.description);
      append("    vendor fault code: 0x{:016x}\n", vi.vendorFaultCode);
      append("    vendor fault data: 0x{:016x}\n", vi.vendorFaultData);
      append("\n");
    }

    if (!vendorData.empty()) {
      auto dump_path = cfg.user_data_dir() / "crash_dumps" /
                       std::format("{:%Y-%m-%d_%H-%M-%S}_{}_{:8x}.bin", date,
                                   props.deviceName.data(), props.deviceID);

      append("  binary dump saved to {}\n", dump_path.string());
      std::ofstream(dump_path, std::ios::binary)
          .write(reinterpret_cast<const char *>(vendorData.data()),
                 static_cast<std::streamsize>(vendorData.size()));
    }

    logger.FatalError("{}", message);
  }

  {
    std::ofstream const crash_dump(
        cfg.user_data_dir() / "crash_dumps" /
        std::format("{:%Y-%m-%d_%H-%M-%S}.txt", date));

    auto append =
        make_append_to(std::ostreambuf_iterator<char>(crash_dump.rdbuf()));

    auto format_uuid =
        [&](const vk::ArrayWrapper1D<uint8_t, vk::UuidSize> &uuid) {
          // format uuid like XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
          constexpr static auto dash_locations = {3, 5, 7, 9};

          for (auto [i, v] : std::views::enumerate(uuid)) {
            append("{:02x}", v);
            if (std::ranges::contains(dash_locations, i)) {
              append("-");
            }
          }
          append("\n");
        };

    append("Device name: {}\n", props.deviceName.data());
    append("Device type: {}\n", props.deviceType);
    append("Vendor ID: 0x{:08x}\n", props.vendorID);
    append("Device ID: 0x{:08x}\n", props.deviceID);
    append("Driver ID: {}\n", driver_props.driverID);
    append("Driver name: {}\n", driver_props.driverName.data());
    append("Driver version: {}\n", props.driverVersion);
    append("Driver info: {}\n", driver_props.driverInfo.data());
    append("Device UUID: ");
    format_uuid(id_props.deviceUUID);
    append("Driver UUID: ");
    format_uuid(id_props.driverUUID);

    append("{}\n", message);
  }
}
