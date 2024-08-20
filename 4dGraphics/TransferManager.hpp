#pragma once

#include "CommandBuffer.hpp"
#include "Context.hpp"
#include "VulkanConstructs.hpp"
#include "VulkanResources.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

/*
TransferManager is a class that is responsible for loading resources
  and transferring them to the GPU. The resources are loaded asynchronously
  and the user must wait for said resources to be loaded before using them.

Available resources:
  - Buffers
  - Textures

Data flow:

1. Request resource load and it is pushed into a transfer queue:
  - Here resources are allocated to allow getting the bindless-resource handles
  - A future is returned to the user to wait for the resource to be loaded
2. Resources from queue are loaded via an async transfer queue
    or if such doesn't exist via normal queue.
    The transfer is made with a speed limit not to have too many in-transit
resources and not to take whole PCI bandwidth.
3. At the start of a frame all new resources are requested:
  a. If resource is currently in-transit / already submitted:
    - Wait for the resource load to finish (push semaphore wait into the CB)
    - Add the queue family ownership transfer/image layout transition barrier
into the CB b. If resource is not in-transit:
    - Transfer the resource on the main CB (no async transfer) to avoid stalls

  The CB to be submitted is the first CB to be executed within the requested
queue. If multiple queues from the family are used additional semaphores must be
injected so all such queues have memory acquired for any needed operation but
those are not handled by the TransferManager (this is a job for the user or (in
the future a work-graph)).
*/

namespace v4dg {
class TransferManager {
private:
  struct QueueItem;

public:
  enum class PriorityClass {
    Low,
    Normal,
    High,
  };

  struct TransferInfo {
    TransferInfo() = delete;
    TransferInfo(std::uint32_t target_family) : target_family(target_family) {}
    TransferInfo(std::uint32_t target_family, std::string_view name)
        : target_family(target_family), name(name) {}
    TransferInfo(std::uint32_t target_family, PriorityClass priority)
        : target_family(target_family), priority(priority) {}
    TransferInfo(std::uint32_t target_family, std::string_view name,
                 PriorityClass priority)
        : target_family(target_family), name(name), priority(priority) {}

    std::uint32_t target_family;
    std::string_view name;
    PriorityClass priority = PriorityClass::Normal;
  };

  struct BufferTransferInfo : TransferInfo {
    vk::BufferUsageFlags2KHR usage =
        vk::BufferUsageFlagBits2KHR::eShaderDeviceAddress;
  };

  struct TextureTransferInfo : TransferInfo {
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal;
  };

  class ResourceTransferHandle {
  public:
    ResourceTransferHandle() = default;
    ResourceTransferHandle(const ResourceTransferHandle &) = delete;
    ResourceTransferHandle &operator=(const ResourceTransferHandle &) = delete;
    ResourceTransferHandle(ResourceTransferHandle &&) = default;
    ResourceTransferHandle &operator=(ResourceTransferHandle &&) = default;

    ~ResourceTransferHandle();

    [[nodiscard]] bool isDone() const noexcept;
    void updatePriority(PriorityClass priority);

  private:
    ResourceTransferHandle(std::list<QueueItem>::iterator queue_item,
                           TransferManager *manager)
        : m_queue_item(queue_item), m_manager(manager) {}

    std::optional<std::list<QueueItem>::iterator> m_queue_item;
    TransferManager *m_manager{nullptr};
    friend class TransferManager;
  };

  struct BufferFuture {
    Buffer buffer;
    ResourceTransferHandle transfer_handle;
  };

  struct TextureFuture {
    ImageView texture;
    ResourceTransferHandle transfer_handle;
  };

  // function that uploads the data to the staging buffer
  // 1st arg is the buffer to upload to (staging or final if mappable)
  using buffer_upload_fn = std::move_only_function<void(Buffer)>;

  TransferManager() = delete;
  TransferManager(Context &ctx);

  Buffer allocateBuffer(std::size_t size, const BufferTransferInfo &ti);
  BufferFuture uploadBuffer(const Buffer &buffer, buffer_upload_fn upload_fn,
                            const BufferTransferInfo &ti);

  // creates a gpu local buffer and copies the data from the staging buffer
  template <typename T = std::byte>
    requires std::is_trivially_copyable_v<T>
  BufferFuture uploadBuffer(std::span<const T> data,
                            const BufferTransferInfo &ti) {
    return uploadBuffer(std::as_bytes(data), ti);
  }

  template <>
  BufferFuture uploadBuffer<std::byte>(std::span<const std::byte> data,
                                       const BufferTransferInfo &ti);

  // load a texture for that will be used only as a sampled image
  TextureFuture uploadTexture(const std::filesystem::path &path,
                              const TextureTransferInfo &ti);

  // CommandBuffer must be in recording state and be from the same family
  //  as the resources' target family
  // If the target family doesn't support transfer operations the
  //  resources must be in the 'Done' state
  // "Immediate mode" is when the resource is not ready when acquiring
  // otherwise the resource is transferred in "Async mode"
  //
  // any exception made by this call is considered unrecoverable (OOM or file
  // corruption)
  void acquireResources(std::span<const ResourceTransferHandle> resources,
                        CommandBuffer &cb);

  // after acquiring all of required frame resources on all queues the user
  // should call this function to begin transfer of non-acquired resources to
  // the GPU
  void doOutstandingTransfers(std::size_t max_transfer_size = 16 << 20,
                              std::size_t max_transfer_count = 8);

private:
  TextureFuture uploadTextureKtx(const std::filesystem::path &path,
                                 const TextureTransferInfo &ti);

  // TODO: other texture formats (probably using stb_image)

  struct uploadTextureHelper_data {
    std::size_t dataSize;
    Buffer staging;
    std::vector<vk::BufferImageCopy> copyRegions;
  };

  using uploadTextureHelper_fn =
      std::move_only_function<uploadTextureHelper_data()>;

  TextureFuture
  uploadTextureHelper(const ImageView &tex, PriorityClass priority,
                      vk::ImageLayout target_layout,
                      std::uint32_t target_family,
                      uploadTextureHelper_fn prepare_transfer_data);

  Buffer stagingBuffer(std::span<const std::byte> data);
  Buffer stagingBuffer(std::size_t size);

  CommandBuffer getCommandBuffer();

  using any_memory_barrier =
      std::variant<std::monostate, vk::MemoryBarrier2, vk::BufferMemoryBarrier2,
                   vk::ImageMemoryBarrier2>;

  struct memory_transfer_info {
    std::size_t transfer_size;

    // there are 3 modes of transfer:
    // Immediate mode: the transfer is done on the main command buffer
    //   -> barrier does layout transition if needed
    //        with appropriate pipeline stages and access flags
    //
    // Async mode: the transfer is done on the transfer command buffer
    //  - on the same family as target family:
    //    - barrier does layout transition and is so semaphores are used to
    //      wait for the transfer
    //      (pipeline stage = none, access flags = none) with semaphore (all)
    //    - acquiring the resource does not submit a barrier
    //  - on different queue family:
    //    - barrier does layout transition and queue family ownership transfer
    //      (pipeline stage = none, access flags = none) with semaphore (all)
    //    - acquiring the resource submits the barrier to finish the possible
    //      layout transition and queue family ownership transfer
    //
    // the barrier member is reused for both release and acquire barriers
    // but the pipeline stages and access flags are changed to match the
    // operation
    any_memory_barrier barrier;
  };

  // transfer function type
  // gets a command buffer and if the transfer is in the "Immediate mode" gets
  // pipeline stages and access flags otherwise as the
  using transfer_fn =
      std::move_only_function<memory_transfer_info(CommandBuffer &)>;

  ResourceTransferHandle enqueueTransfer(PriorityClass priority,
                                         transfer_fn transfer);

  // when the handle is discarded we know that the transfer will not be waited
  // on, so we can just cancel the transfer and free the resources
  void cancelTransfer(std::list<QueueItem>::iterator handle);

  void updatePriority(ResourceTransferHandle &handle, PriorityClass priority);

  friend ResourceTransferHandle;

  Context *m_ctx;

  // for now we will have a single timeline of transfer operations
  //  in the future we can implement a priority based system
  // the stack must also have a way to remove some specific elements from the
  // center
  //  (when a resource is needed for current frame) or in the future promoted
  //  priority

  // the queue item must be able to fully transfer the data to the resource
  //  and inform of required initial and final barriers (needed for batching
  //   and if the current-frame resource is needed)

  struct QueueItem {
    transfer_fn transfer;

    PriorityClass priority;
    bool done = false;
    std::exception_ptr exception;

    // wait semaphore value for the transfer (to make the memory available)
    std::uint64_t semaphore_value = {};

    any_memory_barrier barrier;
  };

  // a queue for every priority & done
  // Those are lists to allow for removal from the middle (changing priority)
  // and the done is a list to keep handles valid until acquisition
  std::mutex queue_mut;

  vk::raii::Semaphore async_transfer_semaphore;
  std::uint64_t async_transfer_semaphore_value{0};

  std::list<QueueItem> &getQueueItemList(bool done, PriorityClass priority);

  std::list<QueueItem> queue_high, queue_normal, queue_low;
  std::list<QueueItem> list_done;
};

}; // namespace v4dg
