#pragma once

#include "GameCore.h"
#include "Shader.h"

#include "Collisions.h"
#include <memory>
#include <volk.h>
#include "VulkanHelpers.h"

VkResult fillCommandBuffers( VulkanRenderDevice &vkDev, size_t index );

class GameRenderHandler : public RenderHandler
{
public:
	bool OnCreate(GLFWwindow*) override;
	void OnDraw(const void* FData) override;
	void OnDestroy() override;
	~GameRenderHandler() override;
protected:
	VulkanInstance vk;
	VulkanRenderDevice vkDev;
};

template<typename RH >
class GameRenderHandlerIndirect : public RenderHandler
{
public:
	bool OnCreate(GLFWwindow*wnd) override
	{ 
		renderer = std::make_unique<RH>(); 
		return renderer->OnCreate( wnd ); 
	}
	void OnDraw(const void* FData) { renderer->OnDraw(FData); }
	void OnDestroy() override { renderer->OnDestroy(); }
protected:
	std::unique_ptr<RH> renderer; // freed in destructor
};