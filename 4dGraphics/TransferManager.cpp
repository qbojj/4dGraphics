#include "TransferManager.hpp"

#include "Context.hpp"
#include "VulkanConstructs.hpp"
#include "cppHelpers.hpp"

#include <ktx.h>
#include <ktxvulkan.h>
#include <vulkan/vulkan.hpp>

#include <filesystem>
#include <ranges>

using namespace v4dg;

TransferManager::TransferManager(Context &ctx) : m_ctx(&ctx) {
  auto features_all = ctx.device()
                          .physicalDevice()
                          .getFeatures2<vk::PhysicalDeviceFeatures2,
                                        vk::PhysicalDeviceVulkan13Features>();

  vk::PhysicalDeviceFeatures &features =
      features_all.get<vk::PhysicalDeviceFeatures2>().features;

  etc2_avaiable = features.textureCompressionETC2;
  bc_avaiable = features.textureCompressionBC;
  astc_ldr_avaiable = features.textureCompressionASTC_LDR;

  astc_hdr_avaiable = features_all.get<vk::PhysicalDeviceVulkan13Features>()
                          .textureCompressionASTC_HDR;
}

template <>
Buffer TransferManager::uploadBuffer<std::byte>(std::span<const std::byte> data,
                                                vk::BufferUsageFlags2KHR usage,
                                                std::string_view name) {
  auto &&dev = m_ctx->device();

  Buffer buffer{
      dev,
      data.size_bytes(),
      usage | vk::BufferUsageFlagBits2KHR::eTransferDst,
      {
          vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
              vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead,
          vma::MemoryUsage::eAuto,
      }};

  if (!name.empty())
    buffer.setName(dev, "{}", name);

  auto flags =
      dev.allocator().getAllocationMemoryProperties(buffer.allocation());

  // check if we need a staging buffer
  if (flags & vk::MemoryPropertyFlagBits::eHostVisible) {
    // direct init
    std::ranges::copy(data, buffer.map<std::byte>().get());
  } else {
    // must use staging transfer
    Buffer staging = stagingBuffer(data);

    if (!name.empty())
      staging.setName(dev, "{} staging", name);

    // transfer staging buffer to our real buffer
    // TODO implement transfer
    throw exception("not implemented");
  }

  return buffer;
}

Texture TransferManager::loadTexture(const std::filesystem::path &path,
                                     std::string_view name) {
  ktxTexture *texture = nullptr;
  detail::destroy_helper _([&] {
    if (texture)
      ktxTexture_Destroy(texture);
    texture = nullptr;
  });

  auto check_error = [&](ktx_error_code_e code) {
    if (code != KTX_SUCCESS)
      throw exception("load texture \"{}\": {}", path.native(),
                      ktxErrorString(code));
  };

  // support ktx2 fromat
  check_error(ktxTexture_CreateFromNamedFile(
      path.string().c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &texture));

  if (ktxTexture_NeedsTranscoding(texture)) {
    assert(texture->classId == ktxTexture2_c);

    ktxTexture2 *texture2 = reinterpret_cast<ktxTexture2 *>(texture);
    ktx_transcode_fmt_e tf;

    // assume ldr (for now)

    // if (colorModel == KHR_DF_MODEL_UASTC && astc_avaiable) {
    //   tf = KTX_TTF_ASTC_4x4_RGBA;
    // } else if (colorModel == KHR_DF_MODEL_ETC1S && etc2_avaiable) {
    //   tf = KTX_TTF_ETC;
    // } else
    if (astc_ldr_avaiable)
      tf = KTX_TTF_ASTC_4x4_RGBA;
    else if (etc2_avaiable)
      tf = KTX_TTF_ETC;
    else if (bc_avaiable)
      tf = KTX_TTF_BC1_OR_3;
    else
      tf = KTX_TTF_RGBA32;

    check_error(ktxTexture2_TranscodeBasis(texture2, tf, 0));
  }

  (void)name;

  // TODO implement texture upload
  throw exception("not implemented");
}

Buffer TransferManager::stagingBuffer(std::span<const std::byte> data) {
  Buffer buffer{m_ctx->device(),
                data.size_bytes(),
                vk::BufferUsageFlagBits2KHR::eTransferSrc,
                {
                    vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
                    vma::MemoryUsage::eAuto,
                }};

  std::ranges::copy(data, buffer.map<std::byte>().get());
  return buffer;
}