#pragma once

#include "Device.hpp"
#include "VulkanConstructs.hpp"
#include "VulkanResources.hpp"

#include <vulkan/vulkan.hpp>

#include <concepts>
#include <span>
#include <stack>
#include <string_view>
#include <filesystem>

namespace v4dg {

class Context;

class TransferManager {
public:
  TransferManager() = delete;
  TransferManager(Context &ctx);

  // creates a gpu local buffer and copies the data from the staging buffer
  template <typename T = std::byte>
    requires std::is_trivially_copyable_v<T>
  Buffer uploadBuffer(std::span<const T> data,
                      vk::BufferUsageFlags2KHR usage,
                      std::string_view name) {
    return uploadBuffer(std::as_bytes(data), usage, name);
  }

  // load a texture for that will be used only as a sampled image
  Texture loadTexture(const std::filesystem::path &path, std::string_view name = "");

private:
  Buffer stagingBuffer(std::span<const std::byte> data);

  Context *m_ctx;

  bool etc2_avaiable;
  bool bc_avaiable;
  bool astc_ldr_avaiable;
  bool astc_hdr_avaiable;
};

template <>
Buffer TransferManager::uploadBuffer<std::byte>(std::span<const std::byte> data,
                                                vk::BufferUsageFlags2KHR usage,
                                                std::string_view name);

}; // namespace v4dg