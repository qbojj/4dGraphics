#include "VulkanConstructs.hpp"

#include "Debug.hpp"
#include "Device.hpp"
#include "cppHelpers.hpp"

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <ranges>
#include <span>
#include <stdexcept>

using namespace v4dg;

/*
VkResult CreateTextureImage(VulkanRenderDevice& vkDev, const char* filename,
VulkanTexture* texture)
{
    *texture = {};
    VkResult ret = VK_SUCCESS;

    stbi_set_flip_vertically_on_load(true);

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename, &texWidth, &texHeight, &texChannels,
STBI_rgb_alpha); VkDeviceSize imageSize = texWidth * texHeight * 4; if (!pixels)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkBuffer stagingBuffer;
    VmaAllocation stagingMemory;
    ret = CreateBuffer(vkDev.device.allocator,
        imageSize,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VMA_MEMORY_USAGE_AUTO,
        &stagingBuffer,
        &stagingMemory,
        nullptr);

    if (ret >= 0) {
        void* pData;
        ret = vmaMapMemory(vkDev.device.allocator, stagingMemory, &pData);
        if (ret >= 0) {
            memcpy(pData, pixels, imageSize);
            vmaUnmapMemory(vkDev.device.allocator, stagingMemory);
            ret = vmaFlushAllocation(vkDev.device.allocator, stagingMemory, 0,
VK_WHOLE_SIZE);

            if (ret >= 0) {
                const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                const VkExtent3D imageExtent = { .width = (uint32_t)texWidth,
.height = (uint32_t)texHeight, .depth = 1 };

                ret = CreateImageResource(vkDev.device,
                    imageFormat,
                    VK_IMAGE_TYPE_2D,
                    imageExtent,
                    1,
                    1,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_TILING_OPTIMAL,
                    0,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
VK_IMAGE_USAGE_SAMPLED_BIT, 0, VMA_MEMORY_USAGE_AUTO, texture);

                if (ret >= 0) {
                    VkCommandBuffer cmdBuffer;
                    ret = BeginSingleTimeCommands(vkDev.device.device,
vkDev.commandPool, &cmdBuffer); if (ret >= 0) {
                        TransitionImageLayoutCmd(cmdBuffer,
                            texture->image,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            0,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT);
                        CopyBufferToImageCmd(cmdBuffer, stagingBuffer, 0,
texture->image, imageExtent, { 0, 0, 0 }); TransitionImageLayoutCmd(cmdBuffer,
                            texture->image,
                            VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_ACCESS_TRANSFER_WRITE_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT);

                        ret = EndSingleTimeCommands(vkDev.device.device,
vkDev.commandPool, vkDev.graphicsQueue.queue, cmdBuffer);
                    }
                }
            }
        }

        vmaDestroyBuffer(vkDev.device.allocator, stagingBuffer, stagingMemory);
    }

    stbi_image_free(pixels);
    if (ret < 0)
        DestroyVulkanTexture(vkDev.device, *texture);

    return ret;
}

VkResult CreateSSBOVertexBuffer(VulkanRenderDevice& vkDev,
    const char* filename,
    VkBuffer* storageBuffer,
    VmaAllocation* storageBufferMemory,
    VulkanBufferSuballocation* vertexBuffer,
    VulkanBufferSuballocation* indexBuffer)
{
    using namespace glm;
    VkResult res = VK_SUCCESS;

    const aiScene* scene = aiImportFile(filename, aiProcess_Triangulate);
    if (!scene || !scene->HasMeshes())
        return VK_ERROR_INITIALIZATION_FAILED;

    const aiMesh* mesh = scene->mMeshes[0];

    struct VertexData {
        vec3 pos;
        vec2 uv;
    };

    vertexBuffer->size = sizeof(VertexData) * mesh->mNumVertices;
    indexBuffer->size = sizeof(uint32_t) * mesh->mNumFaces * 3;

    const VkPhysicalDeviceProperties* properties;
    vmaGetPhysicalDeviceProperties(vkDev.device.allocator, &properties);

    vertexBuffer->offset = 0;
    indexBuffer->offset = AlignUp(vertexBuffer->size,
properties->limits.minStorageBufferOffsetAlignment);

    VkDeviceSize bufferSize = indexBuffer->offset + indexBuffer->size;

    res = CreateBuffer(vkDev.device.allocator,
        bufferSize,
        0,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 0, VMA_MEMORY_USAGE_AUTO, storageBuffer,
        storageBufferMemory,
        nullptr);
    if (res >= 0) {
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingBufferMemory = VK_NULL_HANDLE;

        res = CreateBuffer(vkDev.device.allocator,
            bufferSize,
            0,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO,
            &stagingBuffer,
            &stagingBufferMemory,
            nullptr);

        if (res >= 0) {
            void* pData;
            res = vmaMapMemory(vkDev.device.allocator, stagingBufferMemory,
&pData); if (res >= 0) { VertexData* vertexData = (VertexData*)((char*)pData +
vertexBuffer->offset); for (uint32_t i = 0; i != mesh->mNumVertices; i++) {
                    const aiVector3D v = mesh->mVertices[i];
                    const aiVector3D t = mesh->mTextureCoords[0][i];
                    *vertexData++ = VertexData { .pos = vec3(v.x, v.y, v.z), .uv
= vec2(t.x, t.y) };
                }

                uint32_t* indexData = (uint32_t*)((char*)pData +
indexBuffer->offset); for (uint32_t i = 0; i != mesh->mNumFaces; i++) {
                    assert(mesh->mFaces[i].mNumIndices == 3);
                    for (uint32_t j = 0; j != 3; j++)
                        *indexData++ = mesh->mFaces[i].mIndices[j];
                }

                vmaUnmapMemory(vkDev.device.allocator, stagingBufferMemory);
                res = vmaFlushAllocation(vkDev.device.allocator,
stagingBufferMemory, 0, VK_WHOLE_SIZE);
            }

            if (res >= 0) {
                VkCommandBuffer cmdBuffer;
                res = BeginSingleTimeCommands(vkDev.device.device,
vkDev.commandPool, &cmdBuffer); if (res >= 0) { const VkBufferCopy copyParam = {
.srcOffset = 0, .dstOffset = 0, .size = bufferSize };

                    vkCmdCopyBuffer(cmdBuffer, stagingBuffer, *storageBuffer, 1,
&copyParam);

                    const VkBufferMemoryBarrier bmb { .sType =
VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask =
VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
VK_ACCESS_INDEX_READ_BIT, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer = *storageBuffer,
                        .offset = 0,
                        .size = VK_WHOLE_SIZE };

                    vkCmdPipelineBarrier(cmdBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bmb, 0, nullptr);

                    res = EndSingleTimeCommands(vkDev.device.device,
vkDev.commandPool, vkDev.graphicsQueue.queue, cmdBuffer);
                }
            }

            vmaDestroyBuffer(vkDev.device.allocator, stagingBuffer,
stagingBufferMemory);
        }

        if (res < 0) {
            vmaDestroyBuffer(vkDev.device.allocator, *storageBuffer,
*storageBufferMemory); *storageBuffer = VK_NULL_HANDLE; *storageBufferMemory =
VK_NULL_HANDLE;
        }
    }

    return res;
}
*/

Buffer::Buffer(const Device &device,
               std::pair<vma::Allocation, vk::raii::Buffer> p,
               bool hasDeviceAddress, const char *name)
    : detail::GpuAllocation(device.allocator(), p.first),
      m_buffer(std::move(p.second)),
      m_deviceAddress(hasDeviceAddress
                          ? device.device().getBufferAddress({buffer()})
                          : vk::DeviceAddress{}) {
  if (name)
    setName(device, name);
}

Buffer::Buffer(const Device &device,
               const vk::BufferCreateInfo &bufferCreateInfo,
               const vma::AllocationCreateInfo &allocationCreateInfo,
               const char *name)
    : Buffer(
          device,
          [&] {
            auto [buffer, allocation] = device.allocator().createBuffer(
                bufferCreateInfo, allocationCreateInfo);

            return std::pair<vma::Allocation, vk::raii::Buffer>(
                allocation, vk::raii::Buffer(device.device(), buffer));
          }(),
          static_cast<bool>(getBufferUsage(bufferCreateInfo) &
                            vk::BufferUsageFlagBits2KHR::eShaderDeviceAddress),
          name) {}

Buffer::Buffer(const Device &device, vk::DeviceSize size,
               vk::BufferUsageFlags2KHR usage,
               const vma::AllocationCreateInfo &allocationCreateInfo,
               const char *name, vk::BufferCreateFlags flags,
               vk::ArrayProxy<const uint32_t> queueFamilyIndices)
    : Buffer(device,
             vk::StructureChain<vk::BufferCreateInfo,
                                vk::BufferUsageFlags2CreateInfoKHR>{
                 {
                     flags,
                     size,
                     {},
                     queueFamilyIndices.empty() ? vk::SharingMode::eExclusive
                                                : vk::SharingMode::eConcurrent,
                     queueFamilyIndices,
                 },
                 {usage},
             }
                 .get<>(),
             allocationCreateInfo, name) {}

Image::Image(const Device &device,
             std::pair<vma::Allocation, vk::raii::Image> image,
             vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
             uint32_t mipLevels, uint32_t arrayLayers,
             vk::SampleCountFlagBits samples, const char *name)
    : detail::GpuAllocation(device.allocator(), image.first),
      m_image(std::move(image.second)), m_imageType(imageType),
      m_format(format), m_extent(extent), m_mipLevels(mipLevels),
      m_arrayLayers(arrayLayers), m_samples(samples) {
  if (name)
    setName(device, name);
}

Image::Image(const Device &device, const ImageCreateInfo &imageCreateInfo,
             const vma::AllocationCreateInfo &allocationCreateInfo,
             const char *name)
    : Image(
          device,
          [&] {
            vk::StructureChain<vk::ImageCreateInfo,
                               vk::ImageFormatListCreateInfo,
                               vk::ImageStencilUsageCreateInfo>
                ici{{
                        imageCreateInfo.flags,
                        imageCreateInfo.imageType,
                        imageCreateInfo.format,
                        imageCreateInfo.extent,
                        imageCreateInfo.mipLevels,
                        imageCreateInfo.arrayLayers,
                        imageCreateInfo.samples,
                        imageCreateInfo.tiling,
                        imageCreateInfo.usage,
                        imageCreateInfo.sharingMode,
                        imageCreateInfo.queueFamilyIndices,
                        imageCreateInfo.initialLayout,
                    },
                    {},
                    {}};

            if (imageCreateInfo.formats)
              ici.get<vk::ImageFormatListCreateInfo>().setViewFormats(
                  *imageCreateInfo.formats);
            else
              ici.unlink<vk::ImageFormatListCreateInfo>();

            if (imageCreateInfo.stencilUsage)
              ici.get<vk::ImageStencilUsageCreateInfo>().setStencilUsage(
                  *imageCreateInfo.stencilUsage);
            else
              ici.unlink<vk::ImageStencilUsageCreateInfo>();

            auto [image, allocation] = device.allocator().createImage(
                ici.get<vk::ImageCreateInfo>(), allocationCreateInfo);

            return std::pair<vma::Allocation, vk::raii::Image>(
                allocation, vk::raii::Image(device.device(), image));
          }(),
          imageCreateInfo.imageType, imageCreateInfo.format,
          imageCreateInfo.extent, imageCreateInfo.mipLevels,
          imageCreateInfo.arrayLayers, imageCreateInfo.samples, name) {}