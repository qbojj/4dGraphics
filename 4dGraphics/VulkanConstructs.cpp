#include "VulkanConstructs.hpp"

#include "Debug.hpp"
#include "Device.hpp"
#include "VulkanHelpers.hpp"
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
namespace v4dg {
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

VkResult CreateEngineDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout*
layout)
{
    constexpr auto DescriptorSetBinding = [](uint32_t binding, VkDescriptorType
type, VkPipelineStageFlags stages) -> VkDescriptorSetLayoutBinding { return {
.binding = binding, .descriptorType = type, .descriptorCount = 1, .stageFlags =
stages, .pImmutableSamplers = nullptr }; };

    const VkDescriptorSetLayoutBinding bindings[] = {
        DescriptorSetBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
VK_SHADER_STAGE_VERTEX_BIT), DescriptorSetBinding(1,
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
        DescriptorSetBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
VK_SHADER_STAGE_VERTEX_BIT), DescriptorSetBinding(3,
VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    const VkDescriptorBindingFlags bindingFlags[] = {
        0,
        0,
        0,
        0,
    };

    const VkDescriptorSetLayoutBindingFlagsCreateInfo lbfci { .sType =
VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext =
nullptr, .bindingCount = (uint32_t)size(bindingFlags), .pBindingFlags =
data(bindingFlags) };

    const VkDescriptorSetLayoutCreateInfo ci = { .sType =
VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &lbfci, .flags =
0, .bindingCount = (uint32_t)size(bindings), .pBindings = bindings };

    return vkCreateDescriptorSetLayout(device, &ci, nullptr, layout);
}

VkResult CreateEngineDescriptorSets(VulkanRenderDevice& vkDev, VulkanState&
vkState)
{
    VkResult res = VK_SUCCESS;

    // std::vector<VkDescriptorSetLayout> layouts(
    //	vkDev.swapchainImages.size(),
    //	vkState.descriptorSetLayout
    //);

    const VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = vkState.descriptorPool,
        .descriptorSetCount = 1, //(uint32_t)layouts.size(),
        .pSetLayouts = &vkState.descriptorSetLayout // layouts.data()
    };

    // vkState.descriptorSets.resize( layouts.size() );

    res = vkAllocateDescriptorSets(vkDev.device.device, &ai,
        &vkState.descriptorSet); // descriptorSets.data() );

    if (res >= 0) {
        std::vector<VkWriteDescriptorSet> writes;

        // for( size_t i = 0; i < layouts.size(); i++ )
        {
            const VkDescriptorBufferInfo uniformBufferInfo = {
                .buffer = vkState.uniformBufferMemory.buffer,
                .offset = 0, // vkState.uniformBuffers[ i ].offset,
                .range = vkState.uniformBuffers[0].size // [ i ].size
            };

            const VkDescriptorBufferInfo vertexBufferInfo = { .buffer =
vkState.modelBuffer.buffer, .offset = vkState.vertexBuffer.offset, .range =
vkState.vertexBuffer.size };

            const VkDescriptorBufferInfo indexBufferInfo = { .buffer =
vkState.modelBuffer.buffer, .offset = vkState.indexBuffer.offset, .range =
vkState.indexBuffer.size };

            const VkDescriptorImageInfo textureInfo = { .sampler =
vkState.textureSampler, .imageView = vkState.texture.imageView, .imageLayout =
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            auto WriteDescriptorSet =
                [&](uint32_t binding, VkDescriptorType type, const
VkDescriptorImageInfo* ii, const VkDescriptorBufferInfo* bi, const VkBufferView*
bv) { writes.push_back(VkWriteDescriptorSet { .sType =
VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet =
vkState.descriptorSet, // vkState.descriptorSets[ i ], .dstBinding = binding,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = type,
                        .pImageInfo = ii,
                        .pBufferInfo = bi,
                        .pTexelBufferView = bv });
                };

            WriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
nullptr, &uniformBufferInfo, nullptr); WriteDescriptorSet(1,
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &vertexBufferInfo, nullptr);
            WriteDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
&indexBufferInfo, nullptr); WriteDescriptorSet(3,
VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textureInfo, nullptr, nullptr);
        }

        vkUpdateDescriptorSets(vkDev.device.device, (uint32_t)writes.size(),
writes.data(), 0, nullptr);
    }

    return res;
}
*/
/*
void DestroyVulkanState(VulkanDevice& vkDev, VulkanState& vkState)
{
    vkDestroyDescriptorPool(vkDev.device, vkState.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkDev.device, vkState.descriptorSetLayout,
nullptr);

    vmaDestroyBuffer(vkDev.allocator, vkState.uniformBufferMemory.buffer,
vkState.uniformBufferMemory.bufferAllocation);

    vmaDestroyBuffer(vkDev.allocator, vkState.modelBuffer.buffer,
vkState.modelBuffer.bufferAllocation);

    vkDestroySampler(vkDev.device, vkState.textureSampler, nullptr);
    DestroyVulkanTexture(vkDev, vkState.texture);

    vkDestroyPipelineLayout(vkDev.device, vkState.layout, nullptr);
    vkDestroyPipeline(vkDev.device, vkState.graphicsPipeline, nullptr);

    DestroyVulkanTexture(vkDev, vkState.depthResource);

    vkState = VulkanState {};
}

void DestroyVulkanContext(VulkanContext& vkCtx)
{
    VkDevice dev = vkCtx->vkDev.device;

    if (dev) {
        DestroyVulkanSwapchain(dev, vkCtx.swapchain);

        DestroyVulkanDescriptorAllocatorPool(dev, vkCtx.transientDescPool,
nullptr); vkCtx.dslc.cleanup(); vkDestroySampler(dev, vkCtx.defaultSampler,
nullptr);

        for (auto& f : vkCtx.perFrame) {
            vkDestroySemaphore(dev, f.imageReady, nullptr);
            vkDestroySemaphore(dev, f.readyForPresent, nullptr);

            for (auto& t : f.threadData) {
                evalDescQueue(t.destructionQueue);
                for (auto& cbm : t.cmdBuffManagers)
                    cbm.clear(dev);
            }
        }
    }

    vkCtx = VulkanContext {};
}

void DestroyVulkanInstance(VulkanInstance& vk)
{
    if (vk.instance) {
        vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
        if (vk.messenger)
            vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.messenger, nullptr);
        vkDestroyInstance(vk.instance, nullptr);
    }

    vk = VulkanInstance {};
}

void DestroyVulkanSwapchain(VkDevice device, VulkanSwapchain& swapchain)
{
    for (VkImageView iv : swapchain.imageViews)
        vkDestroyImageView(device, iv, nullptr);

    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);

    swapchain = VulkanSwapchain {};
}

static constexpr std::vector<VkDescriptorPoolSize> getDescriptorPoolSizes()
{
    static constexpr uint32_t maxSets = 2000;
    static constexpr std::pair<VkDescriptorType, float>
descriptorAllocatorPoolMults[] { { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f }, {
VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f }, {
VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f
}, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f }, {
VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f }, {
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f }, {
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f }, {
VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f }, {
VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f }, {
VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f } };

    std::vector<VkDescriptorPoolSize> res;

    for (const std::pair<VkDescriptorType, float>& mult :
descriptorAllocatorPoolMults) res.emplace_back((uint32_t)(mult.second *
maxSets), mult.first);

    return res;
}

std::vector<VulkanPerFrame> VulkanContext::create_per_frame(uint32_t
frames_in_flight, uint32_t thread_count) const
{
    std::vector<VulkanPerFrame> res;
}

VulkanContext::VulkanContext(VulkanInstance& vk, VulkanDevice& vkDev, uint32_t
framesInFlight, uint32_t threadCount, std::string name) : vk(vk) , vkDev(vkDev)
    , name(std::move(name))
    , poolSizes(getDescriptorPoolSizes())
    , dpci(VkDescriptorPoolCreateInfo { .sType =
VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .pNext = nullptr, .maxSets =
maxSets, .poolSizeCount = poolSizes.size().pPoolSizes = poolSizes.data() }) ,
currentFrameId(0) , descPool(vkDev, &dpci, framesInFlight, name + ": descriptor
pool allocator") , currentFrameId(0) , frameIdx(0) , perFrame(...) , dsl_cache({
vkDev.device, nullptr })
{
    uint32_t imageCount;
    VkCommandPoolCreateInfo cpCI;
    VkCommandBufferAllocateInfo cbAI;

    const VkFenceCreateInfo fci{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    VkSurfaceCapabilitiesKHR capabilities;
    VkCompositeAlphaFlagBitsKHR compositeAlpha;
    VulkanSwapchain &swapchain = vkCtx.swapchain;
    VkPresentModeKHR presentMode;
    auto &priv_ = vkCtx.priv_;

    uint32_t framesInFlight;
    uint32_t imageCount;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(vkDev.physicalDevice,
    &queueFamilyCount, nullptr); uint32_t uniqueFamilyCount = 0;

    VK_CHECK_GOTO_INIT;

    vkCtx = {};
    vkCtx.vk = &vk;
    vkCtx.vkDev = &vkDev;
    vkCtx.currentFrameId = 0;
    vkCtx.familyToIdx.resize(queueFamilyCount, UINT32_MAX);

    VK_CHECK_GOTO(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDev.physicalDevice,
    vk.surface, &capabilities));

    swapchain.format = ChooseSwapchainFormat(vkDev.physicalDevice, vk.surface,
    features); swapchain.extent = ChooseSwapchainExtent(wantedExtent,
    capabilities); swapchain.usages = usage; presentMode =
    ChooseSwapPresentMode(vkDev.physicalDevice, vk.surface, true, false);

    // get any supported alpha composition mode
    compositeAlpha = (VkCompositeAlphaFlagBitsKHR)(1 <<
    glm::findLSB((int)capabilities.supportedCompositeAlpha));

    if ((capabilities.supportedUsageFlags & usage) != usage ||
        presentMode == VK_PRESENT_MODE_MAX_ENUM_KHR)
        VK_CHECK_GOTO(VK_ERROR_INITIALIZATION_FAILED);

    imageCount = ChooseSwapImageCount(wantedFramesInFlight, capabilities);
    framesInFlight = min(wantedFramesInFlight, imageCount);

    VK_CHECK_GOTO(CreateSwapchain(
        vkDev.device, vk.surface,
        swapchain.extent, 1, imageCount,
        swapchain.format, swapchain.usages,
        capabilities.currentTransform, compositeAlpha,
        presentMode, VK_TRUE,
        VK_NULL_HANDLE, &swapchain.swapchain));

    VK_CHECK_GOTO(CreateSwapchainImages(
        vkDev.device, swapchain.swapchain,
        &imageCount, swapchain.format.format,
        swapchain.images, swapchain.imageViews));

    for (auto &q : vkDev.queues)
        if (vkCtx.familyToIdx[q.family] == UINT32_MAX)
            vkCtx.familyToIdx[q.family] = uniqueFamilyCount++;

    priv_.poolSizes.reserve(std::size(descriptorAllocatorPoolMults));
    for (auto &m : descriptorAllocatorPoolMults)
        priv_.poolSizes.emplace_back(m.first, (uint32_t)(m.second * maxSets));

    priv_.dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0, // VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
        .maxSets = maxSets,
        .poolSizeCount = (uint32_t)priv_.poolSizes.size(),
        .pPoolSizes = priv_.poolSizes.data()};

    dapci = {
        .pDPci = &priv_.dpci,
        .frameCount = framesInFlight,
        .name = "vkCtx transient ds allocator pool"};

    VK_CHECK_GOTO(CreateVulkanDescriptorAllocatorPool(vkDev.device, &dapci,
    nullptr, &vkCtx.transientDescPool));
    AdvanceVulkanDescriptorAllocatorPool(vkCtx.transientDescPool, 0, VK_FALSE,
    0);

    vkCtx.perFrame.resize(framesInFlight, VulkanPerFrame{
                                              .resourcesReady =
    std::vector<uint64_t>(vkDev.queues.size(), 0ull),
                                              .imageReady{VK_NULL_HANDLE},
                                              .readyForPresent{VK_NULL_HANDLE},
                                              .threadCS =
    tf::CriticalSection{threadCount}, .lastTID{0}, .threadData =
    std::vector<VulkanPerThread>(threadCount, VulkanPerThread{ .cmdBuffManagers
    = std::vector<command_buffer_manager>(uniqueFamilyCount),
                                                                                                          .destructionQueue = {}})});

    for (uint32_t i = 0; i < framesInFlight; i++)
    {
        auto &pf = vkCtx.perFrame[i];

        VK_CHECK_GOTO(CreateSemophore(vkDev.device, &pf.imageReady));
        VK_CHECK_GOTO(CreateSemophore(vkDev.device, &pf.readyForPresent));

        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, pf.imageReady,
    VK_OBJECT_TYPE_SEMAPHORE, "vkCtx frameId %d: image ready", i));
        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, pf.readyForPresent,
    VK_OBJECT_TYPE_SEMAPHORE, "vkCtx frameId %d: ready for present", i));

        for (uint32_t th = 0; th < threadCount; th++)
        {
            for (uint32_t qf = 0; qf < queueFamilyCount; qf++)
                if (vkCtx.familyToIdx[qf] != UINT32_MAX)
                {
                    auto &cbm =
    pf.threadData[th].cmdBuffManagers[vkCtx.familyToIdx[qf]];
                    VK_CHECK_GOTO(cbm.init(vkDev.device, qf,
    VK_COMMAND_POOL_CREATE_TRANSIENT_BIT));
                    VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, cbm.cmdPool,
    VK_OBJECT_TYPE_COMMAND_POOL, "vkCtx frameId %d: thread %d: family %d: cmd
    buffer pool", i, th, qf));
                }
        }
    }

    for (uint32_t i = 0; i < (uint32_t)vkDev.queues.size(); i++)
    {
        VulkanQueue &q = vkDev.queues[i];
        VK_CHECK_GOTO(CreateTimelineSemaphore(vkDev.device, 0ull, &q.timeline));
        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, q.timeline,
    VK_OBJECT_TYPE_SEMAPHORE, "vkCtx queue %d: (family %d) timeline", i,
    q.family)); q.curTimeline = 0;
    }

    return VK_SUCCESS;

    VK_CHECK_GOTO_HANDLE(ret);

    DestroyVulkanContext(vkCtx);
    return ret;
}
*/
Buffer::Buffer(const Device &dev,
               std::pair<vma::UniqueAllocation, vma::UniqueBuffer> p)
    : detail::GpuAllocation(dev.allocator(), std::move(p.first)),
      m_buffer(std::move(p.second)),
      m_deviceAddress(dev.device().getBufferAddress({buffer()})) {}

Buffer::Buffer(const Device &dev, const vk::BufferCreateInfo &bufferCreateInfo,
               const vma::AllocationCreateInfo &allocationCreateInfo)
    : Buffer(dev, [&] -> std::pair<vma::UniqueAllocation, vma::UniqueBuffer> {
        vk::BufferCreateInfo bci{bufferCreateInfo};
        bci.usage |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        auto [buffer, allocation] =
            dev.allocator().createBufferUnique(bci, allocationCreateInfo);

        return {std::move(allocation), std::move(buffer)};
      }()) {}

Image::Image(const Device &device,
             std::pair<vma::UniqueAllocation, vma::UniqueImage> image,
             vk::ImageType imageType, vk::Format format, vk::Extent3D extent,
             uint32_t mipLevels, uint32_t arrayLayers,
             vk::SampleCountFlagBits samples)
    : detail::GpuAllocation(device.allocator(), std::move(image.first)),
      m_image(std::move(image.second)), m_imageType(imageType),
      m_format(format), m_extent(extent), m_mipLevels(mipLevels),
      m_arrayLayers(arrayLayers), m_samples(samples) {}

Image::Image(const Device &device, const ImageCreateInfo &imageCreateInfo,
             const vma::AllocationCreateInfo &allocationCreateInfo)
    : Image(
          device,
          [&] -> std::pair<vma::UniqueAllocation, vma::UniqueImage> {
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

            auto [image, allocation] = device.allocator().createImageUnique(
                ici.get<>(), allocationCreateInfo);

            return {std::move(allocation), std::move(image)};
          }(),
          imageCreateInfo.imageType, imageCreateInfo.format,
          imageCreateInfo.extent, imageCreateInfo.mipLevels,
          imageCreateInfo.arrayLayers, imageCreateInfo.samples) {}

Texture::Texture(const Device &device, const ImageCreateInfo &imageCreateInfo,
                 const vma::AllocationCreateInfo &allocationCreateInfo,
                 vk::ImageViewCreateFlags viewFlags, vk::ImageViewType viewType,
                 vk::ImageAspectFlags aspectFlags,
                 vk::ComponentMapping components)
    : Image(device, imageCreateInfo, allocationCreateInfo),
      m_imageView({device.device(),
                   {viewFlags,
                    image(),
                    viewType,
                    format(),
                    components,
                    {aspectFlags, 0, vk::RemainingMipLevels, 0,
                     vk::RemainingArrayLayers}}}) {}
} // namespace v4dg