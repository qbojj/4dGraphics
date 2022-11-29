#pragma once
#include <volk.h>
#include "VulkanHelpers.h"

class RednerBase {
public:
    virtual VkResult OnCreate( VulkanInstance &vk, VulkanDevice &vkDev, void *usrContext ) = 0;
    virtual VkResult OnDraw( VulkanInstance &vk, VulkanDevice &vkDev, void *usrContext, VkCommandBuffer cmdBuffer, void *FData_ ) = 0;
    virtual void OnDestroy( VulkanInstance &vk, VulkanDevice &vkDev ) = 0;
    virtual ~RednerBase() = 0;
};

class RenderPass : public RednerBase {
public:
    
};

class RenderLayer : public RednerBase {
public: 
    
};