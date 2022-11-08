#pragma once
#include <volk.h>
#include "VulkanHelpers.h"

class RenderPass {
public:
    virtual VkResult OnCreate( VulkanInstance &vk, VulkanDevice &vkDev, VkExtent3D screenExtent ) = 0;
    virtual VkResult OnDraw( VulkanInstance &vk, VulkanDevice &vkDev, VkCommandBuffer cmdBuffer ) = 0;
    virtual void OnDestroy( VulkanInstance &vk, VulkanDevice &vkDev ) = 0;
    virtual ~RenderPass() = 0;
};

