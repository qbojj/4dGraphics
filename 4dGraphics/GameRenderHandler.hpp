#pragma once

#include "Debug.hpp"
#include "VulkanConstructs.hpp"
#include "VulkanHelpers.hpp"

#include "Context.hpp"

#include <exception>
#include <taskflow/taskflow.hpp>

struct SDL_Window;

class GameRenderHandler {
public:
/*
  GameRenderHandler(tf::Subflow &, SDL_Window *);

  void OnDraw(const void *FData);
  ~GameRenderHandler();
*/
private:
/*
  v4dg::Context context;

  struct computePushConstants {
    glm::dvec2 start;
    glm::dvec2 increment;
  };

  const computePushConstants *pPC;

  VkDescriptorSetLayout computeSetLayout;
  VkPipelineLayout computeLayout;
  VkDescriptorPool computePool;
  std::vector<VkDescriptorSet> computeDescriptors;
  VkPipeline compute;

  double lt;
  VkResult FillCommandBuffers(uint32_t index);
  VkResult AdvanceFrame(uint32_t *imageIdx);
  VkResult EndFrame(uint32_t imageIdx);
  VkResult RecreateSwapchain();
  void ClearDestructionQueue(uint64_t until);
  uint64_t getDeviceViability(VkPhysicalDevice);
  */
};