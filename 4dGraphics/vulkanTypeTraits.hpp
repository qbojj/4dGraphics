#pragma once
#include "VulkanHelpers.h"
#include <type_traits>

namespace vulkan_traits
{
    template<typename T> struct traits {};

    template<typename T> struct traits<const T> : traits<T> {};
    template<typename T> struct traits<T&> : traits<T> {};
    template<typename T> struct traits<T&&> : traits<T> {};

#define DECLARE_OBJTYPE(a,b) template<> struct traits<a> { \
        static constexpr VkObjectType objtype = VK_OBJECT_TYPE_ ## b; \
    };

    DECLARE_OBJTYPE(VkInstance,INSTANCE)
    DECLARE_OBJTYPE(VkDevice,DEVICE)
    DECLARE_OBJTYPE(VkSemaphore,SEMAPHORE)
    DECLARE_OBJTYPE(VkFence,FENCE)
    DECLARE_OBJTYPE(VkEvent,EVENT)
    DECLARE_OBJTYPE(VkPipelineCache,PIPELINE_CACHE)
    DECLARE_OBJTYPE(VkValidationCacheEXT,VALIDATION_CACHE_EXT)
    DECLARE_OBJTYPE(VkBuffer,BUFFER)
    DECLARE_OBJTYPE(VkImage,IMAGE)
    DECLARE_OBJTYPE(VkImageView,IMAGE_VIEW)
    DECLARE_OBJTYPE(VkDescriptorPool,DESCRIPTOR_POOL)
#undef DECLARE_OBJTYPE
}
