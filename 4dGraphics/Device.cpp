#include "Device.hpp"
#include "Debug.hpp"
#include "VulkanHelpers.hpp"
#include "v4dgVulkan.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <GLFW/glfw3.h>

#include <format>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>

namespace {
using namespace v4dg;
vk::Bool32
debugMessageFuncCpp(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                    vk::DebugUtilsMessageTypeFlagsEXT types,
                    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                    void * /*pUserData*/) noexcept {

  try {
    std::string message =
        std::format("{}: {}:\n"
                    "\tmessageIDName   = <{}>\n"
                    "\tmessageIdNumber = {}\n"
                    "\tmessage         = <{}>\n",
                    severity, types, pCallbackData->pMessageIdName,
                    pCallbackData->messageIdNumber, pCallbackData->pMessage);

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
          "\t\tobjectHandle = {}\n",
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

bool contains(auto &container, const auto &ext) {
  return std::find(container.begin(), container.end(), ext) != container.end();
}

auto make_ext_storage(const auto &ext) {
  extension_storage storage;
  std::fill(storage.begin(), storage.end(), 0);
  std::strncpy(storage.data(), &ext[0], storage.size());
  return storage;
}

void unique_add(auto &dst, const auto &ext_) {
  auto ext = make_ext_storage(ext_);
  if (!contains(dst, ext)) {
    dst.push_back(ext);
  }
}

void unique_add_if_present(auto &dst, auto &src, const auto &ext) {
  if (contains(src, ext))
    unique_add(dst, ext);
}

std::vector<const char *> to_c_vector(auto &strings) {
  auto rg =
      strings | std::views::transform([](const auto &s) { return s.data(); });
  return {rg.begin(), rg.end()};
}

auto transform_ext_props_to_string = std::views::transform(
    [](const vk::ExtensionProperties &exp) -> std::string_view {
      return exp.extensionName;
    });
} // namespace

namespace v4dg {
Instance::Instance(vk::Optional<const vk::AllocationCallbacks> allocator)
    : m_context(), m_maxApiVer(vk::ApiVersion13),
      m_apiVer(std::min(m_maxApiVer, m_context.enumerateInstanceVersion())),
      m_layers(chooseLayers()), m_extensions(chooseExtensions()),
      m_debugUtilsEnabled(
          contains(m_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)),
      m_instance(initInstance(allocator)),
      m_debugMessenger(m_debugUtilsEnabled
                           ? m_instance.createDebugUtilsMessengerEXT(
                                 debugMessengerCreateInfo())
                           : vk::raii::DebugUtilsMessengerEXT{nullptr}) {}

std::vector<extension_storage>
Instance::chooseLayers() const {
  std::vector<extension_storage> layers;

  std::vector<std::string_view> requestedLayers{
      "VK_LAYER_KHRONOS_shader_object",
      "VK_LAYER_KHRONOS_memory_decompression",
  };

  if (!is_production)
    requestedLayers.push_back("VK_LAYER_KHRONOS_validation");

  for (const auto &layer_p : m_context.enumerateInstanceLayerProperties())
    unique_add_if_present(layers, requestedLayers, layer_p.layerName);

  return layers;
}

std::vector<extension_storage> Instance::chooseExtensions() const {
  std::vector<extension_storage> extensions;

  std::vector<std::string_view> requiredInstanceExts = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
      VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
      VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
  };

  std::vector<std::string_view> wantedInstanceExts{
      VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
  };

  auto window_exts = [] {
    uint32_t count;
    const char **exts = glfwGetRequiredInstanceExtensions(&count);
    return std::span{exts, count};
  }();

  for (const auto &ext : window_exts)
    unique_add(requiredInstanceExts, ext);

  for (const auto &ext : m_context.enumerateInstanceExtensionProperties() |
                             transform_ext_props_to_string)
    unique_add_if_present(extensions, wantedInstanceExts, ext);

  for (const auto &layer : m_layers)
    for (const auto &ext :
         m_context.enumerateInstanceExtensionProperties(std::string{layer}) |
             transform_ext_props_to_string)
      unique_add_if_present(extensions, wantedInstanceExts, ext);

  for (const auto &ext : requiredInstanceExts)
    unique_add(extensions, make_ext_storage(ext));

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
  if (contains(m_extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
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
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
              vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
          vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
          &debugMessageFunc};
}

Device::Device(Handle<Instance> instance)
    : m_instance(std::move(instance)), m_physicalDevice(choosePhysicalDevice()),
      m_apiVer(std::min(m_instance->maxApiVer(),
                        m_physicalDevice.getProperties().apiVersion)),
      m_extensions(chooseExtensions()), m_features(chooseFeatures()),
      m_device(initDevice()), m_allocator(initAllocator()),
      m_queues(initQueues()) {

  auto *pdasf = getVkStructureFromChain<
      vk::PhysicalDeviceAccelerationStructureFeaturesKHR>(m_features.get());
  m_accelerationStructure = pdasf && pdasf->accelerationStructure;

  auto *rtpf =
      getVkStructureFromChain<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>(
          m_features.get());
  m_rayTracingPipeline = rtpf && rtpf->rayTracingPipeline;

  auto *rtaf = getVkStructureFromChain<vk::PhysicalDeviceRayQueryFeaturesKHR>(
      m_features.get());
  m_rayQuery = rtaf && rtaf->rayQuery;

  auto *msf = getVkStructureFromChain<vk::PhysicalDeviceMeshShaderFeaturesEXT>(
      m_features.get());
  m_meshShader = msf && msf->meshShader;
}

bool Device::physicalDeviceSuitable(const vk::raii::PhysicalDevice &pd) const {
  auto props = pd.getProperties();
  logger.Debug("Checking physical device {}", props.deviceName);

  if (props.apiVersion < vk::ApiVersion13) {
    logger.Debug("Physical device {} does not support Vulkan 1.3",
                 props.deviceName);
    return false;
  }

  std::array requiredExtensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
  };

  auto exts =
      pd.enumerateDeviceExtensionProperties() | transform_ext_props_to_string;

  auto ext_present = [&](auto &ext) {
    if (!contains(exts, ext)) {
      logger.Debug("Physical device {} does not support required extension {}",
                   props.deviceName, ext);
      return false;
    } else {
      return true;
    }
  };

  bool ok = std::ranges::all_of(requiredExtensions, ext_present);
  logger.Debug("Physical device {} {} suitable", props.deviceName,
               ok ? "is" : "is not");
  return ok;
}

vk::raii::PhysicalDevice Device::choosePhysicalDevice() const {
  auto phys_devices_view =
      m_instance->instance().enumeratePhysicalDevices() |
      std::views::filter([&](auto &pd) { return physicalDeviceSuitable(pd); });

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

    auto a_it = std::find(prefered.begin(), prefered.end(), a_type);
    auto b_it = std::find(prefered.begin(), prefered.end(), b_type);

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

  std::array requiredExtensions{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
  };

  std::array wantedExtensions{
      VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
      VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
  };

  auto lay_exts = m_instance->layers() |
                  std::views::transform([&](const extension_storage &layer) {
                    return m_physicalDevice.enumerateDeviceExtensionProperties(
                               std::string{layer}) |
                           transform_ext_props_to_string;
                  });

  for (const auto &ext : m_physicalDevice.enumerateDeviceExtensionProperties() |
                             transform_ext_props_to_string)
    unique_add_if_present(extensions, wantedExtensions, ext);

  for (const auto &lay_exts : lay_exts)
    for (const auto &ext : lay_exts)
      unique_add_if_present(extensions, wantedExtensions, ext);

  for (const auto &ext : requiredExtensions)
    unique_add(extensions, ext);

  return extensions;
}

std::shared_ptr<const vk::PhysicalDeviceFeatures2>
Device::chooseFeatures() const {
  auto all_features = std::make_shared<vk::StructureChain<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceMemoryPriorityFeaturesEXT,
      vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>>();

  auto &feature_chain = *all_features;

  feature_chain.get<vk::PhysicalDeviceFeatures2>()
      .features.setFullDrawIndexUint32(vk::True)
      .setSampleRateShading(vk::True)

      .setSamplerAnisotropy(vk::True)

      .setVertexPipelineStoresAndAtomics(vk::True)
      .setFragmentStoresAndAtomics(vk::True)

      .setShaderStorageImageExtendedFormats(vk::True)

      .setShaderUniformBufferArrayDynamicIndexing(vk::True)
      .setShaderSampledImageArrayDynamicIndexing(vk::True)
      .setShaderStorageBufferArrayDynamicIndexing(vk::True)
      .setShaderStorageImageArrayDynamicIndexing(vk::True)

      .setShaderResourceMinLod(vk::True);

  feature_chain.get<vk::PhysicalDeviceVulkan11Features>()
      .setShaderDrawParameters(vk::True);

  feature_chain.get<vk::PhysicalDeviceVulkan12Features>()
      .setShaderInputAttachmentArrayDynamicIndexing(vk::True)
      .setShaderUniformTexelBufferArrayDynamicIndexing(vk::True)
      .setShaderStorageTexelBufferArrayDynamicIndexing(vk::True)

      .setShaderUniformBufferArrayNonUniformIndexing(vk::True)
      .setShaderSampledImageArrayNonUniformIndexing(vk::True)
      .setShaderStorageBufferArrayNonUniformIndexing(vk::True)
      .setShaderStorageImageArrayNonUniformIndexing(vk::True)
      .setShaderInputAttachmentArrayNonUniformIndexing(vk::True)
      .setShaderUniformTexelBufferArrayNonUniformIndexing(vk::True)
      .setShaderStorageTexelBufferArrayNonUniformIndexing(vk::True)

      .setDescriptorBindingUpdateUnusedWhilePending(vk::True)
      .setDescriptorBindingPartiallyBound(vk::True)
      .setDescriptorBindingVariableDescriptorCount(vk::True)
      .setRuntimeDescriptorArray(vk::True)

      .setScalarBlockLayout(vk::True)
      .setUniformBufferStandardLayout(vk::True)

      .setShaderSubgroupExtendedTypes(vk::True)
      .setSeparateDepthStencilLayouts(vk::True)

      .setHostQueryReset(vk::True)
      .setTimelineSemaphore(vk::True)
      .setImagelessFramebuffer(vk::True)

      // 1.3 required
      .setBufferDeviceAddress(vk::True)
      .setVulkanMemoryModel(vk::True)
      .setVulkanMemoryModelDeviceScope(vk::True);

  feature_chain.get<vk::PhysicalDeviceVulkan13Features>()
      .setInlineUniformBlock(vk::True)

      .setShaderDemoteToHelperInvocation(vk::True)
      .setShaderTerminateInvocation(vk::True)

      .setSubgroupSizeControl(vk::True)
      .setComputeFullSubgroups(vk::True)

      .setSynchronization2(vk::True)

      .setShaderZeroInitializeWorkgroupMemory(vk::True)

      .setDynamicRendering(vk::True)

      .setShaderIntegerDotProduct(vk::True)
      .setMaintenance4(vk::True);

  feature_chain.get<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>()
      .setMemoryPriority(vk::True);

  if (!contains(m_extensions, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME))
    feature_chain.unlink<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>();

  feature_chain.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>()
      .setSwapchainMaintenance1(vk::True);

  if (!contains(m_extensions, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME))
    feature_chain.unlink<vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT>();

  return {std::move(all_features), &all_features->get<>()};
}

vk::raii::Device Device::initDevice() const {
  std::vector<std::vector<float>> priorities;

  auto rg = m_physicalDevice.getQueueFamilyProperties() |
            std::views::transform([&, family = 0u](auto qfp) mutable {
              priorities.push_back(std::vector<float>(qfp.queueCount, 0.5f));
              return vk::DeviceQueueCreateInfo{{}, family++, priorities.back()};
            });

  std::vector<vk::DeviceQueueCreateInfo> qcis{rg.begin(), rg.end()};

  auto extensions_c = to_c_vector(m_extensions);
  return {m_physicalDevice, {{}, qcis, {}, extensions_c, nullptr, features()}};
}

vma::UniqueAllocator Device::initAllocator() const {
  const vk::PhysicalDeviceFeatures2 *device_features = features();

  vma::AllocatorCreateFlags flags{};
  if (contains(m_extensions, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
    flags |= vma::AllocatorCreateFlagBits::eExtMemoryBudget;

  if (contains(m_extensions, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)) {
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

  if (contains(m_extensions, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME)) {
    const auto *pdmpf =
        getVkStructureFromChain<vk::PhysicalDeviceMemoryPriorityFeaturesEXT>(
            device_features);

    if (pdmpf && pdmpf->memoryPriority)
      flags |= vma::AllocatorCreateFlagBits::eExtMemoryPriority;
  }

  vma::AllocatorCreateInfo aci{};

  aci.flags = flags;
  aci.instance = *m_instance->instance();
  aci.physicalDevice = *m_physicalDevice;
  aci.device = *m_device;
  aci.vulkanApiVersion = m_apiVer;

  vma::VulkanFunctions functions{
      m_instance->instance().getDispatcher()->vkGetInstanceProcAddr,
      m_device.getDispatcher()->vkGetDeviceProcAddr,
  };

  aci.pVulkanFunctions = &functions;

  return vma::createAllocatorUnique(aci);
}

std::vector<std::vector<Handle<Queue>>> Device::initQueues() const {
  auto rg = m_physicalDevice.getQueueFamilyProperties() |
            std::views::enumerate |
            std::views::transform(
                [&](const auto &pair) -> std::vector<Handle<Queue>> {
                  auto &[family_, qfp] = pair;
                  uint32_t family = family_;

                  auto rg2 = std::views::iota(0u, qfp.queueCount) |
                             std::views::transform([&](uint32_t i) {
                               return make_handle<Queue>(
                                   vk::raii::Queue{m_device, family, i},
                                   qfp.queueFlags, family, i);
                             });

                  return {rg2.begin(), rg2.end()};
                });

  return {rg.begin(), rg.end()};
}
} // namespace v4dg