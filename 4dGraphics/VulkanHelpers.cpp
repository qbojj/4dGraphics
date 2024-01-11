/*
#include "VulkanHelpers.hpp"

#include "Debug.hpp"

#include <SDL2/SDL_vulkan.h>
#include <algorithm>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <bit>
#include <cstdarg>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include <vulkan/vulkan.hpp>


VkResult BeginSingleTimeCommands(VkDevice device, VkCommandPool commandPool,
                                 VkCommandBuffer *commandBuffer) {
  VkResult ret = VK_SUCCESS;
  *commandBuffer = VK_NULL_HANDLE;

  const VkCommandBufferAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};

  ret = vkAllocateCommandBuffers(device, &ai, commandBuffer);
  if (ret >= 0) {
    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr};

    ret = vkBeginCommandBuffer(*commandBuffer, &beginInfo);

    if (ret >= 0)
      return VK_SUCCESS;
  }

  vkFreeCommandBuffers(device, commandPool, 1, commandBuffer);
  *commandBuffer = VK_NULL_HANDLE;
  return ret;
}

VkResult EndSingleTimeCommands(VkDevice device, VkCommandPool commandPool,
                               VkQueue queue, VkCommandBuffer commandBuffer) {
  VkResult ret = VK_SUCCESS;

  ret = vkEndCommandBuffer(commandBuffer);
  if (ret >= 0) {
    const VkFenceCreateInfo fci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .pNext = nullptr,
                                   .flags = 0};

    VkFence fence;

    ret = vkCreateFence(device, &fci, nullptr, &fence);
    if (ret >= 0) {
      const VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .pNext = nullptr,
                               .waitSemaphoreCount = 0,
                               .pWaitSemaphores = nullptr,
                               .pWaitDstStageMask = nullptr,
                               .commandBufferCount = 1,
                               .pCommandBuffers = &commandBuffer,
                               .signalSemaphoreCount = 0,
                               .pSignalSemaphores = nullptr};

      ret = vkQueueSubmit(queue, 1, &si, fence);
      if (ret >= 0)
        ret = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT32_MAX);

      vkDestroyFence(device, fence, nullptr);
    }
  }
  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
  return ret;
}

VkResult RemoveUnsupportedLayers(std::vector<const char *> &layers) {
  std::vector<VkLayerProperties> avaiableLayers;
  VK_CHECK_RET(get_vector(avaiableLayers, vkEnumerateInstanceLayerProperties));
  for (auto layer = layers.begin(); layer < layers.end();) {
    if (!std::any_of(avaiableLayers.begin(), avaiableLayers.end(),
                     [layer](VkLayerProperties &l) {
                       return strcmp(*layer, l.layerName) == 0;
                     }))
      layer = layers.erase(layer);
    else
      layer++;
  }

  return VK_SUCCESS;
}

VkResult RemoveUnsupportedExtensions(const std::vector<const char *> &layers,
                                     std::vector<const char *> &exts) {
  std::vector<std::vector<VkExtensionProperties>> avaiableExtensions(
      layers.size() + 1);
  VK_CHECK_RET(get_vector(avaiableExtensions[0],
                          vkEnumerateInstanceExtensionProperties, nullptr));
  for (int i = 0; i < (int)layers.size(); i++)
    VK_CHECK_RET(get_vector(avaiableExtensions[i + 1],
                            vkEnumerateInstanceExtensionProperties, layers[i]));

  for (auto ext = exts.begin(); ext < exts.end();) {
    if (!std::any_of(
            avaiableExtensions.begin(), avaiableExtensions.end(),
            [ext](std::vector<VkExtensionProperties> &avaiableExtsForLayer) {
              return std::any_of(avaiableExtsForLayer.begin(),
                                 avaiableExtsForLayer.end(),
                                 [ext](VkExtensionProperties &l) {
                                   return strcmp(*ext, l.extensionName) == 0;
                                 });
            }))
      ext = exts.erase(ext);
    else
      ext++;
  }

  return VK_SUCCESS;
}

VkResult
FindSuitablePhysicalDevice(VkInstance instance,
                           std::function<bool(VkPhysicalDevice)> selector,
                           VkPhysicalDevice *physicalDevice) {
  std::vector<VkPhysicalDevice> physicalDevices;
  VK_CHECK_RET(
      get_vector(physicalDevices, vkEnumeratePhysicalDevices, instance));

  for (VkPhysicalDevice dev : physicalDevices) {
    if (selector(dev)) {
      *physicalDevice = dev;
      return VK_SUCCESS;
    }
  }

  return VK_ERROR_INITIALIZATION_FAILED;
}

uint32_t FindQueueFamilies(const std::vector<VkQueueFamilyProperties> &families,
                           VkQueueFlags neededFlags, VkQueueFlags wantedFlags,
                           VkQueueFlags unwantedFlags,
                           VkQueueFlags forbiddenFlags) {
  int32_t maxScore = INT32_MIN;
  uint32_t bestFamily = UINT32_MAX;

  for (uint32_t i = 0; i < (uint32_t)families.size(); i++) {
    uint32_t flags = families[i].queueFlags;
    if (families[i].queueCount > 0 && (flags & neededFlags) == neededFlags &&
        (flags & forbiddenFlags) == 0) {
      uint32_t score = 32 * std::popcount(flags & wantedFlags) -
                       std::popcount(flags & unwantedFlags);

      score = score * 256 - std::popcount(flags);
      if (score >= maxScore) {
        maxScore = score;
        bestFamily = i;
      }
    }
  }

  return bestFamily;
}

VkSurfaceFormatKHR ChooseSwapchainFormat(VkPhysicalDevice pd, VkSurfaceKHR surf,
                                         VkFormatFeatureFlags features) {
  const VkSurfaceFormatKHR badFormat = {VK_FORMAT_UNDEFINED,
                                        VK_COLOR_SPACE_PASS_THROUGH_EXT};

  std::vector<VkSurfaceFormatKHR> avaiableFormats;
  if (get_vector(avaiableFormats, vkGetPhysicalDeviceSurfaceFormatsKHR, pd,
                 surf) < 0)
    return badFormat;

  // std::vector<VkSurfaceFormatKHR> formatsForUsage;
  for (VkSurfaceFormatKHR &f : avaiableFormats) {
    VkFormatProperties fp;
    vkGetPhysicalDeviceFormatProperties(pd, f.format, &fp);
    if ((fp.optimalTilingFeatures & features) == features)
      return f; // formatsForUsage.push_back(f);
  }

  return badFormat;
}

VkPresentModeKHR ChooseSwapPresentMode(VkPhysicalDevice pd, VkSurfaceKHR surf,
                                       bool VSync, bool limitFramerate) {
  std::vector<VkPresentModeKHR> avaiablePresentModes;
  if (get_vector(avaiablePresentModes,
                 vkGetPhysicalDeviceSurfacePresentModesKHR, pd, surf) < 0)
    return VK_PRESENT_MODE_MAX_ENUM_KHR;

  auto contains = [&](VkPresentModeKHR mode) {
    return avaiablePresentModes.end() != std::find(avaiablePresentModes.begin(),
                                                   avaiablePresentModes.end(),
                                                   mode);
  };

  if (VSync && !limitFramerate && contains(VK_PRESENT_MODE_MAILBOX_KHR))
    return VK_PRESENT_MODE_MAILBOX_KHR;

  if (VSync && contains(VK_PRESENT_MODE_FIFO_KHR))
    return VK_PRESENT_MODE_FIFO_KHR;

  if (!VSync && !limitFramerate && contains(VK_PRESENT_MODE_IMMEDIATE_KHR))
    return VK_PRESENT_MODE_IMMEDIATE_KHR;

  if (!VSync && contains(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
    return VK_PRESENT_MODE_FIFO_RELAXED_KHR; // no tearing if full framerate
                                             // (if frame arrives late it
                                             // still tears)

  assert(contains(VK_PRESENT_MODE_FIFO_KHR)); // VK_PRESENT_MODE_FIFO_KHR should
                                              // always be avaiable (I think)
  return VK_PRESENT_MODE_FIFO_KHR;
}

uint32_t ChooseSwapImageCount(uint32_t wantedImageCount,
                              const VkSurfaceCapabilitiesKHR &capabilities) {
  return capabilities.maxImageCount == 0
             ? // 0 means that there is no maximum
             wantedImageCount
             : max(wantedImageCount, capabilities.maxImageCount);
}

VkExtent2D ChooseSwapchainExtent(VkExtent2D defaultSize,
                                 const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width == 0xFFFFFFFF &&
      capabilities.currentExtent.height == 0xFFFFFFFF)
    return VkExtent2D{.width = glm::clamp(defaultSize.width,
                                          capabilities.minImageExtent.width,
                                          capabilities.maxImageExtent.width),
                      .height = glm::clamp(defaultSize.height,
                                           capabilities.minImageExtent.height,
                                           capabilities.maxImageExtent.height)};

  return capabilities.currentExtent;
}

VkResult CreateSwapchain(
    VkDevice device, VkSurfaceKHR surface, VkExtent2D swapchainSize,
    uint32_t swapchainLayers, uint32_t minImageCount,
    VkSurfaceFormatKHR surfaceFormat, VkImageUsageFlags swapchainUsage,
    VkSurfaceTransformFlagBitsKHR preTransform,
    VkCompositeAlphaFlagBitsKHR compositeAlpha, VkPresentModeKHR presentMode,
    VkBool32 clipped, VkSwapchainKHR oldSwapchain, VkSwapchainKHR *swapchain) {
  const VkSwapchainCreateInfoKHR swapCI = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface,
      .minImageCount = minImageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = swapchainSize,
      .imageArrayLayers =
          swapchainLayers, // for stereoscopic 3d this should be 2
      .imageUsage = swapchainUsage,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = preTransform,
      .compositeAlpha = compositeAlpha,
      .presentMode = presentMode,
      .clipped = clipped,
      .oldSwapchain = oldSwapchain};

  return vkCreateSwapchainKHR(device, &swapCI, nullptr, swapchain);
}

VkResult CreateSwapchainImages(VkDevice device, VkSwapchainKHR swapchain,
                               uint32_t *swapchainImageCount,
                               VkFormat imageFormat,
                               std::vector<VkImage> &swapchainImages,
                               std::vector<VkImageView> &swapchainImageViews) {
  uint32_t sic;
  if (!swapchainImageCount)
    swapchainImageCount = &sic;

  VK_CHECK_GOTO_INIT;

  VK_CHECK_GOTO(
      vkGetSwapchainImagesKHR(device, swapchain, swapchainImageCount, nullptr));
  swapchainImages.resize(*swapchainImageCount, VK_NULL_HANDLE);
  swapchainImageViews.resize(*swapchainImageCount, VK_NULL_HANDLE);
  VK_CHECK_GOTO(vkGetSwapchainImagesKHR(device, swapchain, swapchainImageCount,
                                        swapchainImages.data()));

  for (uint32_t i = 0; i < *swapchainImageCount; i++) {
    VK_CHECK_GOTO(CreateImageView(device, swapchainImages[i], imageFormat,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  &swapchainImageViews[i]));

    // VK_CHECK_GOTO( SET_VK_NAME( device, swapchainImages[i],
    // VK_OBJECT_TYPE_IMAGE, "swapchain image %d", i ) ); VK_CHECK_GOTO(
    // SET_VK_NAME( device, swapchainImageViews[i],
    // VK_OBJECT_TYPE_IMAGE_VIEW, "swapchain image view %d", i ) );
  }

  return VK_SUCCESS;

  VK_CHECK_GOTO_HANDLE(res);

  for (VkImageView iv : swapchainImageViews)
    vkDestroyImageView(device, iv, nullptr);

  swapchainImages.clear();
  swapchainImageViews.clear();
  return res;
}

void GetSuballocatedBufferSize(
    const std::vector<VkDeviceSize> &sizes, VkDeviceSize alignment,
    VkDeviceSize *bufferSize,
    std::vector<VulkanBufferSuballocation> &suballocations) {
  for (auto size : sizes) {
    suballocations.push_back(
        {.offset = AlignUp(*bufferSize, alignment), .size = size});

    *bufferSize = suballocations.back().offset + size;
  }
}

VkResult CreateBuffer(VmaAllocator allocator, VkDeviceSize size,
                      VkBufferCreateFlags flags, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags,
                      VmaMemoryUsage vmaUsage, VkBuffer *buffer,
                      VmaAllocation *bufferMemory,
                      VmaAllocationInfo *allocationInfo) {
  const VkBufferCreateInfo bufferInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = flags,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr};

  VmaAllocationCreateInfo ai{};

  ai.flags = allocationFlags;
  ai.usage = vmaUsage;

  return vmaCreateBuffer(allocator, &bufferInfo, &ai, buffer, bufferMemory,
                         allocationInfo);
}

VkResult CreateImage(VmaAllocator allocator, VkFormat format,
                     VkImageType imageType, VkExtent3D size, uint32_t mipLevels,
                     uint32_t arrayLayers, VkSampleCountFlagBits samples,
                     VkImageTiling tiling, VkImageCreateFlags flags,
                     VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags,
                     VmaMemoryUsage vmaUsage, VkImage *image,
                     VmaAllocation *imageMemory,
                     VmaAllocationInfo *allocationInfo) {
  const VkImageCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .pNext = nullptr,
                                 .flags = flags,
                                 .imageType = imageType,
                                 .format = format,
                                 .extent = size,
                                 .mipLevels = mipLevels,
                                 .arrayLayers = arrayLayers,
                                 .samples = samples,
                                 .tiling = tiling,
                                 .usage = usage,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                 .queueFamilyIndexCount = 0,
                                 .pQueueFamilyIndices = nullptr,
                                 .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  VmaAllocationCreateInfo ai{};

  ai.flags = allocationFlags;
  ai.usage = vmaUsage;

  VkResult res =
      vmaCreateImage(allocator, &ici, &ai, image, imageMemory, allocationInfo);
  if (vmaUsage == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED && res < 0) {
    // if could not create lazily allocated image try from normal heap
    ai.usage = VMA_MEMORY_USAGE_AUTO;
    res = vmaCreateImage(allocator, &ici, &ai, image, imageMemory,
                         allocationInfo);
  }

  return res;
}

VkResult CreateImageView(VkDevice device, VkImage image, VkFormat format,
                         VkImageAspectFlags aspectFlags,
                         VkImageView *imageView) {
  const VkImageViewCreateInfo viewCI = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_R,
              .g = VK_COMPONENT_SWIZZLE_G,
              .b = VK_COMPONENT_SWIZZLE_B,
              .a = VK_COMPONENT_SWIZZLE_A,
          },
      .subresourceRange = {.aspectMask = aspectFlags,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};

  return vkCreateImageView(device, &viewCI, nullptr, imageView);
}

VkResult CreateTextureSampler(VkDevice device, VkFilter filter,
                              VkSamplerMipmapMode mipMode,
                              VkSamplerAddressMode addressU,
                              VkSamplerAddressMode addressV,
                              VkSamplerAddressMode addressW, float anisotropy,
                              VkSampler *sampler) {
  const VkSamplerCreateInfo sci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = filter,
      .minFilter = filter,
      .mipmapMode = mipMode,
      .addressModeU = addressU,
      .addressModeV = addressV,
      .addressModeW = addressW,
      .mipLodBias = 0.0f,
      .anisotropyEnable = anisotropy != 1.0f ? VK_TRUE : VK_FALSE,
      .maxAnisotropy = anisotropy,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = VK_LOD_CLAMP_NONE,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE};

  return vkCreateSampler(device, &sci, nullptr, sampler);
}

void CopyBufferToImageCmd(VkCommandBuffer cmdBuffer, VkBuffer srcBuffer,
                          VkDeviceSize srcOffset, VkImage dstImage,
                          VkExtent3D size, VkOffset3D offset) {
  const VkBufferImageCopy region = {
      .bufferOffset = srcOffset,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = offset,
      .imageExtent = size};

  vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void TransitionImageLayoutCmd(VkCommandBuffer cmdBuffer, VkImage image,
                              VkImageAspectFlags aspects,
                              VkImageLayout oldLayout, VkImageLayout newLayout,
                              VkPipelineStageFlags srcStageMask,
                              VkAccessFlags srcAccessMask,
                              VkPipelineStageFlags dstStageMask,
                              VkAccessFlags dstAccessMask) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = srcAccessMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = aspects,
                           .baseMipLevel = 0,
                           .levelCount = VK_REMAINING_MIP_LEVELS,
                           .baseArrayLayer = 0,
                           .layerCount = VK_REMAINING_ARRAY_LAYERS}};

  vkCmdPipelineBarrier(cmdBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

VkFormat FindSupportedFormat(VkPhysicalDevice device,
                             const std::vector<VkFormat> &candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features) {
  bool lin = tiling == VK_IMAGE_TILING_LINEAR;
  bool opt = tiling == VK_IMAGE_TILING_OPTIMAL;

  if (!lin && !opt)
    return VK_FORMAT_UNDEFINED;

  for (VkFormat fmt : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(device, fmt, &properties);
    if (((lin   ? properties.linearTilingFeatures
          : opt ? properties.optimalTilingFeatures
                : (assert(0), 0)) &
         features) == features)
      return fmt;
  }

  return VK_FORMAT_UNDEFINED;
}

VkFormat FindDepthFormat(VkPhysicalDevice device) {
  return FindSupportedFormat(device,
                             {
                                 VK_FORMAT_D24_UNORM_S8_UINT,
                                 VK_FORMAT_X8_D24_UNORM_PACK32,
                                 VK_FORMAT_D16_UNORM_S8_UINT,
                                 VK_FORMAT_D16_UNORM,
                                 VK_FORMAT_D32_SFLOAT_S8_UINT,
                                 VK_FORMAT_D32_SFLOAT,
                             },
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkImageAspectFlags FormatGetAspects(VkFormat fmt) {
  const VkFormat depthFormats[] = {
      VK_FORMAT_D32_SFLOAT,          VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM,           VK_FORMAT_D16_UNORM_S8_UINT};

  const VkFormat stencilFormats[] = {
      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_S8_UINT};

  const bool hasDepth =
      find(begin(depthFormats), end(depthFormats), fmt) != end(depthFormats);
  const bool hasStencil = find(begin(stencilFormats), end(stencilFormats),
                               fmt) != end(stencilFormats);

  return (!hasDepth && !hasStencil ? VK_IMAGE_ASPECT_COLOR_BIT : 0) |
         (hasDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
         (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0) | 0;
}

VkResult CreateSemophore(VkDevice device, VkSemaphore *semaphore) {
  const VkSemaphoreCreateInfo sCI = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0};
  return vkCreateSemaphore(device, &sCI, nullptr, semaphore);
}

VkResult CreateTimelineSemaphore(VkDevice dev, uint64_t initialValue,
                                 VkSemaphore *semaphore) {
  const VkSemaphoreTypeCreateInfo stci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = initialValue};
  const VkSemaphoreCreateInfo sCI = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &stci,
      .flags = 0};

  return vkCreateSemaphore(dev, &sCI, nullptr, semaphore);
}

VkResult CreateDescriptorPool(VkDevice device, void *pNext,
                              VkDescriptorPoolCreateFlags flags,
                              uint32_t maxSets, uint32_t poolSizeCount,
                              const VkDescriptorPoolSize *pPoolSizes,
                              VkDescriptorPool *descPool) {
  const VkDescriptorPoolCreateInfo pi = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = pNext,
      .flags = flags,
      .maxSets = maxSets,
      .poolSizeCount = poolSizeCount,
      .pPoolSizes = pPoolSizes};
  return vkCreateDescriptorPool(device, &pi, nullptr, descPool);
}

VkResult CreateDescriptorSetHelper(
    VkDevice device, VkDescriptorPoolCreateFlags flags, uint32_t maxSets,
    uint32_t uniformBuffersPerSet, uint32_t storageBuffersPerSet,
    uint32_t imageSamplersPerSet, VkDescriptorPool *descPool) {
  std::vector<VkDescriptorPoolSize> poolSizes;
  if (uniformBuffersPerSet)
    poolSizes.push_back(
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformBuffersPerSet * maxSets});

  if (storageBuffersPerSet)
    poolSizes.push_back(
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageBuffersPerSet * maxSets});

  if (imageSamplersPerSet)
    poolSizes.push_back(
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, imageSamplersPerSet * maxSets});

  return CreateDescriptorPool(device, nullptr, flags, maxSets,
                              (uint32_t)poolSizes.size(), poolSizes.data(),
                              descPool);
}

VkResult CreateShaderModule(VkDevice device, const char *filename,
                            VkShaderModuleCreateFlags flags, const void *pNext,
                            VkShaderModule *shaderModule) {
  auto stage = stageFromFilename(filename);
  if (stage == EShLangCount)
    return VK_ERROR_INITIALIZATION_FAILED;

  std::vector<uint32_t> SPIRV = getShaderOrGenerate(stage, filename);
  if (SPIRV.empty())
    return VK_ERROR_INITIALIZATION_FAILED;

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = pNext,
      .flags = flags,
      .codeSize = SPIRV.size() * sizeof(uint32_t),
      .pCode = SPIRV.data()};

  return vkCreateShaderModule(device, &ci, nullptr, shaderModule);
}

VkResult CreatePipelineLayout(VkDevice device, uint32_t setLayoutCount,
                              const VkDescriptorSetLayout *pSetLayouts,
                              uint32_t pushConstantRangeCount,
                              const VkPushConstantRange *pPushConstantRanges,
                              VkPipelineLayout *layout) {
  const VkPipelineLayoutCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .setLayoutCount = setLayoutCount,
      .pSetLayouts = pSetLayouts,
      .pushConstantRangeCount = pushConstantRangeCount,
      .pPushConstantRanges = pPushConstantRanges};

  return vkCreatePipelineLayout(device, &ci, nullptr, layout);
}

constexpr VkPipelineVertexInputStateCreateInfo vici = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr};

constexpr VkPipelineInputAssemblyStateCreateInfo iaci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE};

constexpr VkPipelineTessellationStateCreateInfo tsci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .patchControlPoints = 0,
};

constexpr VkPipelineViewportStateCreateInfo vsci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .viewportCount = 1,
    .pViewports = nullptr,
    .scissorCount = 1,
    .pScissors = nullptr,
};

constexpr VkPipelineRasterizationStateCreateInfo rsci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE, // VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f};

constexpr VkPipelineMultisampleStateCreateInfo msci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 1.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE};

constexpr VkPipelineDepthStencilStateCreateInfo dssci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
    .depthBoundsTestEnable = VK_TRUE,
    .stencilTestEnable = VK_FALSE,
    .front = {},
    .back = {},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
};

constexpr VkPipelineColorBlendAttachmentState attachmentBlend = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

constexpr VkPipelineColorBlendStateCreateInfo cbsci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .logicOpEnable = VK_FALSE,
    .logicOp = VK_LOGIC_OP_NO_OP,
    .attachmentCount = 1,
    .pAttachments = &attachmentBlend,
    .blendConstants = {0.0f, 0.0f, 0.0f, 1.0f}};

void FillGraphicsPipelineDefaults(VkGraphicsPipelineCreateInfo *gpci) {
  *gpci = VkGraphicsPipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 0,
      .pStages = nullptr,
      .pVertexInputState = &vici,
      .pInputAssemblyState = &iaci,
      .pTessellationState = &tsci,
      .pViewportState = &vsci,
      .pRasterizationState = &rsci,
      .pMultisampleState = &msci,
      .pDepthStencilState = &dssci,
      .pColorBlendState = &cbsci,
      .pDynamicState = nullptr,
      .layout = VK_NULL_HANDLE,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1};
}

VkPipelineShaderStageCreateInfo FillShaderStage(VkShaderStageFlagBits stage,
                                                VkShaderModule module,
                                                const char *name) {
  return VkPipelineShaderStageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = stage,
      .module = module,
      .pName = name,
      .pSpecializationInfo = nullptr};
}
*/