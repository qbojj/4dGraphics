#include "VulkanConstructs.h"
#include <exception>
#include <initializer_list>
#include <filesystem>
#include "Debug.h"

#include "CommonUtility.h"
#include "VulkanHelpers.h"
#include "vulkanTypeTraits.hpp"


using namespace vulkan_helpers;
using namespace cpph;

static std::string FormatUUID(const uint8_t data[VK_UUID_SIZE])
{
    constexpr char valToHex[] = "0123456789abcdef";
    std::string res(VK_UUID_SIZE * 2 + 4, '\0');
    char *out = res.data();

    for (size_t i = 0; i < VK_UUID_SIZE; i++)
    {
        *out++ = valToHex[data[i] >> 4];
        *out++ = valToHex[data[i] & 0x0F];

        if (i == 3 || i == 5 || i == 7 || i == 9)
            *out++ = '-';
    }

    return res;
}

static std::string GetUniqueDeviceName(VkPhysicalDevice physicalDev)
{
    if (!physicalDev)
        return "";

    VkPhysicalDeviceVulkan11Properties v11props{};
    v11props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    v11props.pNext = nullptr;

    VkPhysicalDeviceProperties2 props{};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props.pNext = &v11props;

    vkGetPhysicalDeviceProperties2(physicalDev, &props);
    return FormatUUID(v11props.deviceUUID) + "-" + std::to_string(8 * sizeof(void *));
}

VulkanInstance::VulkanInstance(
    std::vector<const char *> layers,
    std::vector<const char *> extensions,
    const VkApplicationInfo *pAppInfo,
    const void *pCreateInstanceNext)
    : enabledLayers(std::move(layers))
    , enabledExts(std::move(extensions))
{
    const cpph::exception_guard failguard_([this]{clear();});

    const uint32_t maxVer = volkGetInstanceVersion();
    ASSERT_THROW(maxVer != 0, "vulkan not supported");
    ASSERT_THROW(VK_API_VERSION_VARIANT(maxVer) != 0, "only variant-0 vulkan is supported");

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = nullptr,
        .applicationVersion = 0,
        .pEngineName = "4dGraphics",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2};

    if (pAppInfo) // transfer all data that user wants
    {
        appInfo.pNext = pAppInfo->pNext;
        appInfo.pApplicationName = pAppInfo->pApplicationName;
        appInfo.applicationVersion = pAppInfo->applicationVersion;
        if (pAppInfo->apiVersion)
            appInfo.apiVersion = pAppInfo->apiVersion;
    }

    apiVersion = appInfo.apiVersion;

    VkInstanceCreateFlags flags = 0;

    if (enumeratePortability &&
        is_extension_present(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkInstanceCreateInfo InstanceCI{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = pCreateInstanceNext,
        .flags = flags,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint32_t)layers.size(),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = (uint32_t)extensions.size(),
        .ppEnabledExtensionNames = extensions.data()};

    CREATE_THROW(vkCreateInstance(&InstanceCI, nullptr, &tmp_), instance, "could not create instance");

    const VkDebugUtilsMessengerCreateInfoEXT *debUtils =
        (const VkDebugUtilsMessengerCreateInfoEXT *)
            get_vk_structure(pCreateInstanceNext, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);

    if (debUtils)
        CREATE_THROW(vkCreateDebugUtilsMessengerEXT(instance, debUtils, nullptr, &tmp_),
                     messenger, "Could not setup debug callbacks");

    volkLoadInstance(instance);
}

void VulkanInstance::clear()
{
    if (messenger)
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
    
    if (instance)
        vkDestroyInstance(instance, nullptr);
}

VulkanInstance::~VulkanInstance() { clear(); }

size_t VulkanDevice::get_queue_count(std::span<const VkDeviceQueueCreateInfo> vdqci)
{
    size_t cnt = 0;
    for (const VkDeviceQueueCreateInfo &dqci : vdqci)
        cnt += dqci.queueCount;

    return cnt;
}

VulkanDevice::VulkanDevice(
    const VulkanInstance &vk,
    VkPhysicalDevice physicalDevice,
    std::span<const VkDeviceQueueCreateInfo> families,
    std::vector<const char *> extensions,
    const VkPhysicalDeviceFeatures2 *deviceFeatures,
    std::string name )
    : name(std::move(name))
    , physicalDevice(physicalDevice)
    , enabledExts(std::move(extensions))
    , queues(get_queue_count(families))
    , info(physicalDevice)
{
    const cpph::exception_guard failguard_([this]{clear();});

    VkDeviceCreateInfo dCI = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = deviceFeatures,
		.flags = 0,
		.queueCreateInfoCount = (uint32_t)families.size(),
		.pQueueCreateInfos = families.data(),
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (uint32_t)enabledExts.size(),
		.ppEnabledExtensionNames = enabledExts.data(),
		.pEnabledFeatures = nullptr
	};

    CREATE_THROW(vkCreateDevice(physicalDevice, &dCI, nullptr, &tmp_), device, "could not create vulkan device");
    
    const auto allPresent = [this](std::initializer_list<const char *> exts)
    {
        for (const char *s : exts)
            if (!is_extension_present(enabledExts, s))
                return false;
        return true;
    };

    bDebugUtils = allPresent({VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
    set_vk_name(*this, physicalDevice, name + ": physical device");
    set_vk_name(*this, device, name + ": logical device");

    VmaAllocatorCreateInfo aci{};
    if (allPresent({VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME}))
        aci.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

    if (allPresent({VK_KHR_BIND_MEMORY_2_EXTENSION_NAME}))
        aci.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

    if ((allPresent({VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME}) || vk.apiVersion >= VK_API_VERSION_1_1) && 
            allPresent({VK_EXT_MEMORY_BUDGET_EXTENSION_NAME}))
        aci.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    if (allPresent({VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME}))
    {
        const auto *phdcmf = (const VkPhysicalDeviceCoherentMemoryFeaturesAMD *)
            get_vk_structure(deviceFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD);
        
        if (phdcmf && phdcmf->deviceCoherentMemory)
            aci.flags |= VMA_ALLOCATOR_CREATE_AMD_DEVICE_COHERENT_MEMORY_BIT;
    }

    if (vk.apiVersion >= VK_API_VERSION_1_2 || allPresent({VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME}))
    {
        const auto *pdbdaf = (const VkPhysicalDeviceBufferDeviceAddressFeatures *)
            get_vk_structure(deviceFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT);

        const auto *pdv12f = (const VkPhysicalDeviceVulkan12Features *)
            get_vk_structure(deviceFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);

        if ((pdbdaf && pdbdaf->bufferDeviceAddress) || (pdv12f && pdv12f->bufferDeviceAddress))
            aci.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    }

    if (allPresent({VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME}))
    {
        const auto *pdmpf = (const VkPhysicalDeviceMemoryPriorityFeaturesEXT *)
            get_vk_structure(deviceFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT);

        if (pdmpf && pdmpf->memoryPriority)
            aci.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    }

    aci.instance = vk.instance;
    aci.physicalDevice = physicalDevice;
    aci.device = device;
    aci.vulkanApiVersion = vk.apiVersion;

    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    aci.pVulkanFunctions = &functions;

    CREATE_THROW(vmaCreateAllocator(&aci, &tmp_), allocator, "could not create VMA allocator");

    std::string devPathPrefix = 
        (std::filesystem::current_path() / "cache" / GetUniqueDeviceName(physicalDevice)).string();

    {
        std::string cache = GetFileString(devPathPrefix + ".pcache", true);

        VkPipelineCacheCreateInfo pci{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .initialDataSize = cache.size(),
            .pInitialData = cache.data(),
        };

        if (vkCreatePipelineCache(device, &pci, nullptr, &pipelineCache) < 0)
        {
            pipelineCache = VK_NULL_HANDLE;
            TRACE(DebugLevel::Warning, "could not create pipeline cache from file => trying empty cache");

            pci.initialDataSize = 0;
            pci.pInitialData = nullptr;
            CREATE_THROW(vkCreatePipelineCache(device, &pci, nullptr, &tmp_),
                pipelineCache, "Could not create vulkan pipeline cache" );

        }

        set_vk_name(*this, pipelineCache, name + ": pipeline chache" );
    }

    uint32_t queueIndex = 0;
    for( const VkDeviceQueueCreateInfo &f :  families )
    {
        for( uint32_t i = 0; i < f.queueCount; i++ )
        {
            VulkanQueue &queue = queues[queueIndex++];
            queue.family = f.queueFamilyIndex;

            vkGetDeviceQueue( device, queue.family, i, &queue.queue );
            set_vk_name(*this, queue.queue, 
                name + ": queue family " + std::to_string(queue.family) + ": " + std::to_string(i) );
        }
    }
}

void VulkanDevice::serialize()
{
    if( pipelineCache == VK_NULL_HANDLE ) return;

    std::filesystem::path cacheDir = std::filesystem::current_path() / "cache";
    std::filesystem::create_directories(cacheDir);

    std::string cachePath = 
        (cacheDir / (GetUniqueDeviceName(physicalDevice) + ".pcache")).string();

    size_t dataSize = 0;
    std::vector<uint8_t> cacheData;
    VkResult err;

    do
    {
        err = vkGetPipelineCacheData(device, pipelineCache, &dataSize, nullptr);
        if (err != VK_SUCCESS) break;
        
        cacheData.resize(dataSize);
        err = get_data_fn(device, cache, &dataSize, cacheData.data());
        cacheData.resize(dataSize);
    } while (err == VK_INCOMPLETE);

    FILE *fh = nullptr;
    if (err == VK_SUCCESS && (fh = fopen(cachePath.c_str(), "wb") )
    {
        fwrite(cacheData.data(), sizeof(uint8_t), cacheData.size(), fh);
        fclose(fh);
    }
    else TRACE(DebugLevel::Warning, "Could not serialize %s cache", type.c_str() );
}

void VulkanDevice::clear()
{
    if( device )
    {
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
        if(validationCache) vkDestroyValidationCacheEXT(device, validationCache, nullptr);
        vmaDestroyAllocator(allocator);
    }

    vkDestroyDevice(device, nullptr);
}

VulkanDevice::~VulkanDevice() 
{
    serialize();
    clear();
}

VulkanWSI(VulkanContext &ctx, const std::string &name, fn_recreateSwp recreate, VkSurfaceKHR surface )
    : swapchainData(WSISwapchain{ .surface = surface, .swapchain = VK_NULL_HANDLE, .recreate = std::move(recreate) })
    , ctx(ctx)
    , name(std::move(name))
{
    // TODO
}

VulkanPerThread::VulkanPerThread( const VulkanDevice &dev, std::span<const uint32_t> families, const std::string &name )
{
    for( uint32_t f : families )
        cmdBuffManagers.emplace_back( dev, f, name );
}

VulkanPerFrame::VulkanPerFrame( const VulkanDevice &dev_, uint32_t queueCount, uint32_t threadCount, 
    std::span<const uint32_t> families, const std::string &name )
    : readyFences(queueCount, VK_NULL_HANDLE)
    , threadCS(threadCount)
    , lastTID(0)
    , dev(dev_.device)
{
    const cpph::exception_guard failguard_([this]{clear();});

    for( uint32_t i = 0; i < threadCount; i++ )
        threadData.emplace_back(dev, families, name + ": thread " + std::to_string(i));
    
    const VkSemaphoreCreateInfo sci {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
    };

    CREATE_THROW(vkCreateSemaphore(dev, &sci, nullptr, &tmp_), imageReady, "could not create semaphore");
    CREATE_THROW(vkCreateSemaphore(dev, &sci, nullptr, &tmp_), readyForPresent, "could not create semaphore");

    set_vk_name(dev_, imageReady, name + ": image ready semaphore");
    set_vk_name(dev_, readyForPresent, name + ": ready for present semaphore");

    const VkFenceCreateInfo fci {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for( uint32_t i = 0; i < queueCount; i++ )
    {
        CREATE_THROW(vkCreateFence(dev, &fci, nullptr, &tmp_), readyFences[i], "could not create fence");
        set_vk_name(dev_, readyFences[i], name + ": ready: queue " + std::to_string(i) );
    }
}

void VulkanPerFrame::add_destr_fn(std::function<void()> fn)
{
    std::unique_lock lock(mut_);
    destructionQueue.emplace(std::move(fn));
}

void VulkanPerFrame::eval_queue()
{
    std::unique_lock lock(mut_);

    while( !destructionQueue.empty() )
	{
		destructionQueue.front()();
		destructionQueue.pop();
	}
}

uint32_t VulkanPerFrame::get_thread_idx()
{
    return lastTID.load(std::memory_order_relaxed) % (uint32_t)threadData.size();
}

void VulkanPerFrame::release_thread_idx(uint32_t) {}

void VulkanPerFrame::clear()
{
    eval_queue();

    for( VkFence f : readyFences )
        vkDestroyFence(dev, f, nullptr);
    vkDestroySemaphore(dev, imageReady, nullptr);
    vkDestroySemaphore(dev, readyForPresent, nullptr);
}

VulkanPerFrame::~VulkanPerFrame() { clear(); }

VulkanThreadCtx::VulkanThreadCtx(VulkanContext &ctx)
    : ctx(ctx)
    , fCtx(ctx.perFrame[ctx.frameId])
    , threadId(fCtx.get_thread_idx())
    , tCtx(fCtx.threadData[threadId]) {}

VulkanThreadCtx::~VulkanThreadCtx() { fCtx.release_thread_idx(threadId); }

VkResult CreateTextureImage(VulkanRenderDevice &vkDev,
                            const char *filename, VulkanTexture *texture)
{
    *texture = {};
    VkResult ret = VK_SUCCESS;

    stbi_set_flip_vertically_on_load(true);

    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(filename,
                                &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;
    if (!pixels)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkBuffer stagingBuffer;
    VmaAllocation stagingMemory;
    ret = CreateBuffer(vkDev.device.allocator, imageSize, 0,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
                       &stagingBuffer, &stagingMemory, nullptr);

    if (ret >= 0)
    {
        void *pData;
        ret = vmaMapMemory(vkDev.device.allocator, stagingMemory, &pData);
        if (ret >= 0)
        {
            memcpy(pData, pixels, imageSize);
            vmaUnmapMemory(vkDev.device.allocator, stagingMemory);
            ret = vmaFlushAllocation(vkDev.device.allocator, stagingMemory, 0, VK_WHOLE_SIZE);

            if (ret >= 0)
            {
                const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
                const VkExtent3D imageExtent = {
                    .width = (uint32_t)texWidth,
                    .height = (uint32_t)texHeight,
                    .depth = 1};

                ret = CreateImageResource(vkDev.device, imageFormat,
                                          VK_IMAGE_TYPE_2D, imageExtent,
                                          1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
                                          0, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                          0, VMA_MEMORY_USAGE_AUTO,
                                          texture);

                if (ret >= 0)
                {
                    VkCommandBuffer cmdBuffer;
                    ret = BeginSingleTimeCommands(vkDev.device.device, vkDev.commandPool, &cmdBuffer);
                    if (ret >= 0)
                    {
                        TransitionImageLayoutCmd(cmdBuffer, texture->image, VK_IMAGE_ASPECT_COLOR_BIT,
                                                 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
                        CopyBufferToImageCmd(cmdBuffer, stagingBuffer, 0, texture->image,
                                             imageExtent, {0, 0, 0});
                        TransitionImageLayoutCmd(cmdBuffer, texture->image, VK_IMAGE_ASPECT_COLOR_BIT,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

                        ret = EndSingleTimeCommands(vkDev.device.device, vkDev.commandPool,
                                                    vkDev.graphicsQueue.queue, cmdBuffer);
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

VkResult CreateSSBOVertexBuffer(
    VulkanRenderDevice &vkDev,
    const char *filename,
    VkBuffer *storageBuffer, VmaAllocation *storageBufferMemory,
    VulkanBufferSuballocation *vertexBuffer,
    VulkanBufferSuballocation *indexBuffer)
{
    using namespace glm;
    VkResult res = VK_SUCCESS;

    const aiScene *scene = aiImportFile(filename, aiProcess_Triangulate);
    if (!scene || !scene->HasMeshes())
        return VK_ERROR_INITIALIZATION_FAILED;

    const aiMesh *mesh = scene->mMeshes[0];
    struct VertexData
    {
        vec3 pos;
        vec2 uv;
    };
    vertexBuffer->size = sizeof(VertexData) * mesh->mNumVertices;
    indexBuffer->size = sizeof(uint32_t) * mesh->mNumFaces * 3;

    const VkPhysicalDeviceProperties *properties;
    vmaGetPhysicalDeviceProperties(vkDev.device.allocator, &properties);

    vertexBuffer->offset = 0;
    indexBuffer->offset = AlignUp(vertexBuffer->size, properties->limits.minStorageBufferOffsetAlignment);

    VkDeviceSize bufferSize = indexBuffer->offset + indexBuffer->size;

    res = CreateBuffer(vkDev.device.allocator, bufferSize, 0,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       0,
                       VMA_MEMORY_USAGE_AUTO,
                       storageBuffer, storageBufferMemory, nullptr);
    if (res >= 0)
    {
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VmaAllocation stagingBufferMemory = VK_NULL_HANDLE;

        res = CreateBuffer(vkDev.device.allocator, bufferSize, 0,
                           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                           VMA_MEMORY_USAGE_AUTO,
                           &stagingBuffer, &stagingBufferMemory, nullptr);

        if (res >= 0)
        {
            void *pData;
            res = vmaMapMemory(vkDev.device.allocator, stagingBufferMemory, &pData);
            if (res >= 0)
            {
                VertexData *vertexData = (VertexData *)((char *)pData + vertexBuffer->offset);
                for (uint32_t i = 0; i != mesh->mNumVertices; i++)
                {
                    const aiVector3D v = mesh->mVertices[i];
                    const aiVector3D t = mesh->mTextureCoords[0][i];
                    *vertexData++ = VertexData{
                        .pos = vec3(v.x, v.y, v.z),
                        .uv = vec2(t.x, t.y)};
                }

                uint32_t *indexData = (uint32_t *)((char *)pData + indexBuffer->offset);
                for (uint32_t i = 0; i != mesh->mNumFaces; i++)
                {
                    assert(mesh->mFaces[i].mNumIndices == 3);
                    for (uint32_t j = 0; j != 3; j++)
                        *indexData++ = mesh->mFaces[i].mIndices[j];
                }

                vmaUnmapMemory(vkDev.device.allocator, stagingBufferMemory);
                res = vmaFlushAllocation(vkDev.device.allocator, stagingBufferMemory, 0, VK_WHOLE_SIZE);
            }

            if (res >= 0)
            {
                VkCommandBuffer cmdBuffer;
                res = BeginSingleTimeCommands(vkDev.device.device, vkDev.commandPool, &cmdBuffer);
                if (res >= 0)
                {
                    const VkBufferCopy copyParam = {
                        .srcOffset = 0,
                        .dstOffset = 0,
                        .size = bufferSize};

                    vkCmdCopyBuffer(cmdBuffer, stagingBuffer, *storageBuffer, 1, &copyParam);

                    const VkBufferMemoryBarrier bmb{
                        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                        .pNext = nullptr,
                        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDEX_READ_BIT,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer = *storageBuffer,
                        .offset = 0,
                        .size = VK_WHOLE_SIZE};

                    vkCmdPipelineBarrier(cmdBuffer,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                                         0, 0, nullptr, 1, &bmb, 0, nullptr);

                    res = EndSingleTimeCommands(
                        vkDev.device.device, vkDev.commandPool, vkDev.graphicsQueue.queue, cmdBuffer);
                }
            }

            vmaDestroyBuffer(vkDev.device.allocator, stagingBuffer, stagingBufferMemory);
        }

        if (res < 0)
        {
            vmaDestroyBuffer(vkDev.device.allocator, *storageBuffer, *storageBufferMemory);
            *storageBuffer = VK_NULL_HANDLE;
            *storageBufferMemory = VK_NULL_HANDLE;
        }
    }

    return res;
}

VkResult CreateEngineDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout *layout)
{
    constexpr auto DescriptorSetBinding = [](
                                              uint32_t binding,
                                              VkDescriptorType type,
                                              VkPipelineStageFlags stages) -> VkDescriptorSetLayoutBinding
    {
        return {
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
            .stageFlags = stages,
            .pImmutableSamplers = nullptr};
    };

    const VkDescriptorSetLayoutBinding bindings[] = {
        DescriptorSetBinding(0,
                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                             VK_SHADER_STAGE_VERTEX_BIT),
        DescriptorSetBinding(1,
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             VK_SHADER_STAGE_VERTEX_BIT),
        DescriptorSetBinding(2,
                             VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             VK_SHADER_STAGE_VERTEX_BIT),
        DescriptorSetBinding(3,
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    const VkDescriptorBindingFlags bindingFlags[] = {
        0,
        0,
        0,
        0,
    };

    const VkDescriptorSetLayoutBindingFlagsCreateInfo lbfci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = (uint32_t)size(bindingFlags),
        .pBindingFlags = data(bindingFlags)};

    const VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &lbfci,
        .flags = 0,
        .bindingCount = (uint32_t)size(bindings),
        .pBindings = bindings};

    return vkCreateDescriptorSetLayout(device, &ci, nullptr, layout);
}

VkResult CreateEngineDescriptorSets(VulkanRenderDevice &vkDev, VulkanState &vkState)
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
        .descriptorSetCount = 1,                    //(uint32_t)layouts.size(),
        .pSetLayouts = &vkState.descriptorSetLayout // layouts.data()
    };

    // vkState.descriptorSets.resize( layouts.size() );

    res = vkAllocateDescriptorSets(vkDev.device.device, &ai, &vkState.descriptorSet); // descriptorSets.data() );

    if (res >= 0)
    {
        std::vector<VkWriteDescriptorSet> writes;

        // for( size_t i = 0; i < layouts.size(); i++ )
        {
            const VkDescriptorBufferInfo uniformBufferInfo = {
                .buffer = vkState.uniformBufferMemory.buffer,
                .offset = 0,                            // vkState.uniformBuffers[ i ].offset,
                .range = vkState.uniformBuffers[0].size // [ i ].size
            };

            const VkDescriptorBufferInfo vertexBufferInfo = {
                .buffer = vkState.modelBuffer.buffer,
                .offset = vkState.vertexBuffer.offset,
                .range = vkState.vertexBuffer.size};

            const VkDescriptorBufferInfo indexBufferInfo = {
                .buffer = vkState.modelBuffer.buffer,
                .offset = vkState.indexBuffer.offset,
                .range = vkState.indexBuffer.size};

            const VkDescriptorImageInfo textureInfo = {
                .sampler = vkState.textureSampler,
                .imageView = vkState.texture.imageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            auto WriteDescriptorSet = [&](
                                          uint32_t binding,
                                          VkDescriptorType type,
                                          const VkDescriptorImageInfo *ii,
                                          const VkDescriptorBufferInfo *bi,
                                          const VkBufferView *bv)
            {
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = vkState.descriptorSet, // vkState.descriptorSets[ i ],
                    .dstBinding = binding,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = type,
                    .pImageInfo = ii,
                    .pBufferInfo = bi,
                    .pTexelBufferView = bv});
            };

            WriteDescriptorSet(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uniformBufferInfo, nullptr);
            WriteDescriptorSet(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &vertexBufferInfo, nullptr);
            WriteDescriptorSet(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &indexBufferInfo, nullptr);
            WriteDescriptorSet(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &textureInfo, nullptr, nullptr);
        }

        vkUpdateDescriptorSets(vkDev.device.device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }

    return res;
}

void DestroyVulkanState(VulkanDevice &vkDev, VulkanState &vkState)
{
    vkDestroyDescriptorPool(vkDev.device, vkState.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(vkDev.device, vkState.descriptorSetLayout, nullptr);

    vmaDestroyBuffer(vkDev.allocator, vkState.uniformBufferMemory.buffer,
                     vkState.uniformBufferMemory.bufferAllocation);

    vmaDestroyBuffer(vkDev.allocator, vkState.modelBuffer.buffer,
                     vkState.modelBuffer.bufferAllocation);

    vkDestroySampler(vkDev.device, vkState.textureSampler, nullptr);
    DestroyVulkanTexture(vkDev, vkState.texture);

    vkDestroyPipelineLayout(vkDev.device, vkState.layout, nullptr);
    vkDestroyPipeline(vkDev.device, vkState.graphicsPipeline, nullptr);

    DestroyVulkanTexture(vkDev, vkState.depthResource);

    vkState = VulkanState{};
}

void DestroyVulkanContext(VulkanContext &vkCtx)
{
    VkDevice dev = vkCtx->vkDev.device;

    if (dev)
    {
        DestroyVulkanSwapchain(dev, vkCtx.swapchain);

        DestroyVulkanDescriptorAllocatorPool(dev, vkCtx.transientDescPool, nullptr);
        vkCtx.dslc.cleanup();
        vkDestroySampler(dev, vkCtx.defaultSampler, nullptr);

        for (auto &f : vkCtx.perFrame)
        {
            vkDestroySemaphore(dev, f.imageReady, nullptr);
            vkDestroySemaphore(dev, f.readyForPresent, nullptr);

            for (auto &t : f.threadData)
            {
                evalDescQueue(t.destructionQueue);
                for (auto &cbm : t.cmdBuffManagers)
                    cbm.clear(dev);
            }
        }
    }

    vkCtx = VulkanContext{};
}

void DestroyVulkanInstance(VulkanInstance &vk)
{
    if (vk.instance)
    {
        vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
        if (vk.messenger)
            vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.messenger, nullptr);
        vkDestroyInstance(vk.instance, nullptr);
    }

    vk = VulkanInstance{};
}

void DestroyVulkanSwapchain(VkDevice device, VulkanSwapchain &swapchain)
{
    for (VkImageView iv : swapchain.imageViews)
        vkDestroyImageView(device, iv, nullptr);

    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);

    swapchain = VulkanSwapchain{};
}

static constexpr std::vector<VkDescriptorPoolSize> getDescriptorPoolSizes() 
{
    static constexpr uint32_t maxSets = 2000;
    static constexpr std::pair<VkDescriptorType, float> descriptorAllocatorPoolMults[]{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f}
    };

    std::vector<VkDescriptorPoolSize> res;

    for( const std::pair<VkDescriptorType,float> &mult : descriptorAllocatorPoolMults )
        res.emplace_back( (uint32_t)( mult.second * maxSets ), mult.first );
    
    return res;
}


std::vector<VulkanPerFrame> VulkanContext::create_per_frame(uint32_t frames_in_flight, uint32_t thread_count) const
{
    std::vector<VulkanPerFrame> res;
}

VulkanContext::VulkanContext(
    VulkanInstance &vk, VulkanDevice &vkDev, 
    uint32_t framesInFlight, uint32_t threadCount, std::string name )
    : vk(vk)
    , vkDev(vkDev)
    , name(std::move(name))
    , poolSizes(getDescriptorPoolSizes())
    , dpci(VkDescriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .maxSets = maxSets,
        .poolSizeCount = poolSizes.size()
        .pPoolSizes = poolSizes.data()
    })
    , currentFrameId(0)
    , descPool( vkDev, &dpci, framesInFlight, name + ": descriptor pool allocator" )
    , currentFrameId(0)
    , frameIdx(0)
    , perFrame(...)
    , dsl_cache( {vkDev.device, nullptr} )
{

    /*
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
    vkGetPhysicalDeviceQueueFamilyProperties(vkDev.physicalDevice, &queueFamilyCount, nullptr);
    uint32_t uniqueFamilyCount = 0;

    VK_CHECK_GOTO_INIT;

    vkCtx = {};
    vkCtx.vk = &vk;
    vkCtx.vkDev = &vkDev;
    vkCtx.currentFrameId = 0;
    vkCtx.familyToIdx.resize(queueFamilyCount, UINT32_MAX);

    VK_CHECK_GOTO(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDev.physicalDevice, vk.surface, &capabilities));

    swapchain.format = ChooseSwapchainFormat(vkDev.physicalDevice, vk.surface, features);
    swapchain.extent = ChooseSwapchainExtent(wantedExtent, capabilities);
    swapchain.usages = usage;
    presentMode = ChooseSwapPresentMode(vkDev.physicalDevice, vk.surface, true, false);

    // get any supported alpha composition mode
    compositeAlpha = (VkCompositeAlphaFlagBitsKHR)(1 << glm::findLSB((int)capabilities.supportedCompositeAlpha));

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

    VK_CHECK_GOTO(CreateVulkanDescriptorAllocatorPool(vkDev.device, &dapci, nullptr, &vkCtx.transientDescPool));
    AdvanceVulkanDescriptorAllocatorPool(vkCtx.transientDescPool, 0, VK_FALSE, 0);

    vkCtx.perFrame.resize(framesInFlight, VulkanPerFrame{
                                              .resourcesReady = std::vector<uint64_t>(vkDev.queues.size(), 0ull),
                                              .imageReady{VK_NULL_HANDLE},
                                              .readyForPresent{VK_NULL_HANDLE},
                                              .threadCS = tf::CriticalSection{threadCount},
                                              .lastTID{0},
                                              .threadData = std::vector<VulkanPerThread>(threadCount, VulkanPerThread{
                                                                                                          .cmdBuffManagers = std::vector<VulkanCommandBufferManager>(uniqueFamilyCount),
                                                                                                          .destructionQueue = {}})});

    for (uint32_t i = 0; i < framesInFlight; i++)
    {
        auto &pf = vkCtx.perFrame[i];

        VK_CHECK_GOTO(CreateSemophore(vkDev.device, &pf.imageReady));
        VK_CHECK_GOTO(CreateSemophore(vkDev.device, &pf.readyForPresent));

        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, pf.imageReady, VK_OBJECT_TYPE_SEMAPHORE, "vkCtx frameId %d: image ready", i));
        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, pf.readyForPresent, VK_OBJECT_TYPE_SEMAPHORE, "vkCtx frameId %d: ready for present", i));

        for (uint32_t th = 0; th < threadCount; th++)
        {
            for (uint32_t qf = 0; qf < queueFamilyCount; qf++)
                if (vkCtx.familyToIdx[qf] != UINT32_MAX)
                {
                    auto &cbm = pf.threadData[th].cmdBuffManagers[vkCtx.familyToIdx[qf]];
                    VK_CHECK_GOTO(cbm.init(vkDev.device, qf, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT));
                    VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, cbm.cmdPool, VK_OBJECT_TYPE_COMMAND_POOL,
                                              "vkCtx frameId %d: thread %d: family %d: cmd buffer pool", i, th, qf));
                }
        }
    }

    for (uint32_t i = 0; i < (uint32_t)vkDev.queues.size(); i++)
    {
        VulkanQueue &q = vkDev.queues[i];
        VK_CHECK_GOTO(CreateTimelineSemaphore(vkDev.device, 0ull, &q.timeline));
        VK_CHECK_GOTO(SET_VK_NAME(vkDev.device, q.timeline, VK_OBJECT_TYPE_SEMAPHORE, "vkCtx queue %d: (family %d) timeline", i, q.family));
        q.curTimeline = 0;
    }

    return VK_SUCCESS;

    VK_CHECK_GOTO_HANDLE(ret);

    DestroyVulkanContext(vkCtx);
    return ret;
    */
}

void VulkanPDInfo::setup()
{
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    props12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
    props13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
    
    feats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    feats11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    feats12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    feats13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

#define ADD_TO_CHAIN(o, v) do { \
        ((VkBaseOutStructure*)o)->pNext = &v; \
        o = &v; \
    } while (0)

    void *pProp = &props, *pFeat = &feats;
#define ADD_PROP(v) ADD_TO_CHAIN(pProp, v)
#define ADD_FEAT(v) ADD_TO_CHAIN(pFeat, v)

    ADD_PROP(props11);
    ADD_PROP(props12);
    ADD_PROP(props13);

    ADD_FEAT(feats11);
    ADD_FEAT(feats12);
    ADD_FEAT(feats13);

#undef ADD_PROP
#undef ADD_FEAT
#undef ADD_TO_CHAIN
}

void VulkanPDInfo::clear_core_features()
{
    feats = {};
    feats11 = {};
    feats12 = {};
    feats13 = {};

    setup();
}

VulkanPDInfo::VulkanPDInfo() { setup(); }
VulkanPDInfo::VulkanPDInfo(VkPhysicalDevice pd)
{
    setup();
    vkGetPhysicalDeviceMemoryProperties(pd, &memory );
    vkGetPhysicalDeviceProperties2(pd, &props );
    vkGetPhysicalDeviceFeatures2(pd, &feats );
}

VulkanPDInfo::VulkanPDInfo( const VulkanPDInfo &o )
{
    memcpy(this, &o, sizeof(VulkanPDInfo) );
    setup();
}

VulkanPDInfo&VulkanPDInfo::operator=(VulkanPDInfo o)
{
    memcpy(this, &o, sizeof(VulkanPDInfo));
    setup();
}

VulkanCommandBufferManager::VulkanCommandBufferManager(
    const VulkanDevice &dev_, uint32_t family, const std::string &name, VkCommandPoolCreateFlags flags)
    : dev(dev_.device)
{
    const cpph::exception_guard failguard_([this]{clear();});

    const VkCommandPoolCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .queueFamilyIndex = family};

    CREATE_THROW(vkCreateCommandPool(dev, &ci, nullptr, &tmp_), cmdPool, "could not create " + name + ": command pool");
    set_vk_name(dev_, cmdPool, name + ": command pool");
}

void VulkanCommandBufferManager::clear()
{
    vkDestroyCommandPool(dev, cmdPool, nullptr);
}

VulkanCommandBufferManager::~VulkanCommandBufferManager() { clear(); }

VkResult VulkanCommandBufferManager::reset(VkCommandPoolResetFlags flags)
{
    VkResult res = vkResetCommandPool(dev, cmdPool, flags);

    // TODO: implement shrinking
    if (res == VK_SUCCESS)
        usedPrim = usedSec = 0;
    
    return res;
}

VkResult VulkanCommandBufferManager::get(VkCommandBufferLevel level, VkCommandBuffer *cmdBuff)
{
    assert(level == VK_COMMAND_BUFFER_LEVEL_PRIMARY || level == VK_COMMAND_BUFFER_LEVEL_SECONDARY);
    auto &pool = level == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? prim : sec;
    size_t &used = level == VK_COMMAND_BUFFER_LEVEL_PRIMARY ? usedPrim : usedSec;

    size_t cmdBuffCnt = pool.size();
    if (used == cmdBuffCnt)
    {
        constexpr size_t batchSize = 8;

        pool.resize(cmdBuffCnt + batchSize);

        const VkCommandBufferAllocateInfo ai{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = cmdPool,
            .level = level,
            .commandBufferCount = batchSize,
        };

        VkResult res = vkAllocateCommandBuffers(dev, &ai, pool.data() + cmdBuffCnt);
        if (res != VK_SUCCESS)
        {
            pool.resize(cmdBuffCnt);
            return res;
        }
    }

    *cmdBuff = pool[used++];
    return VK_SUCCESS;
}

VkResult CreateImageResource(
    VulkanDevice &vkDev,
    VkFormat format, VkImageType imageType, VkExtent3D size, uint32_t mipLevels, uint32_t arrayLayers, VkSampleCountFlagBits samples,
    VkImageTiling tiling, VkImageCreateFlags flags, VkImageUsageFlags usage,
    VmaAllocationCreateFlags allocationFlags, VmaMemoryUsage vmaUsage,
    VulkanTexture *imageResource )
{
    *imageResource = {};
    VK_CHECK_GOTO_INIT;

    VK_CHECK_GOTO(CreateImage(
        vkDev.allocator, format, imageType, size, mipLevels, arrayLayers, samples,
        tiling, flags, usage, allocationFlags, vmaUsage,
        &imageResource->image, &imageResource->imageMemory, nullptr));

    VK_CHECK_GOTO(CreateImageView(
        vkDev.device, imageResource->image, format, FormatGetAspects(format),
        &imageResource->imageView));

    return VK_SUCCESS;

    VK_CHECK_GOTO_HANDLE(res);
    DestroyVulkanTexture(vkDev, *imageResource);
    return res;
}

void DestroyVulkanTexture(const VulkanDevice &device, VulkanTexture &texture)
{
    vkDestroyImageView(device.device, texture.imageView, nullptr);
    vmaDestroyImage(device.allocator, texture.image, texture.imageMemory);

    texture = VulkanTexture{};
}
