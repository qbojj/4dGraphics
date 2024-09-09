#include "TransferManager.hpp"

#include "CommandBuffer.hpp"
#include "Constants.hpp"
#include "Context.hpp"
#include "VulkanConstructs.hpp"
#include "VulkanResources.hpp"
#include "cppHelpers.hpp"

#include <ktx.h>
#include <tracy/Tracy.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <expected>
#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

using namespace v4dg;

namespace {
struct KtxException : std::runtime_error {
  KtxException(ktx_error_code_e error_code)
      : std::runtime_error(ktxErrorString(error_code)) {}
};

class UniqueKtxTexture {
public:
  UniqueKtxTexture() = delete;
  UniqueKtxTexture(const UniqueKtxTexture &) = delete;
  UniqueKtxTexture(UniqueKtxTexture &&other) noexcept : tex(other.tex) {
    other.tex = nullptr;
  }
  UniqueKtxTexture &operator=(const UniqueKtxTexture &) = delete;
  UniqueKtxTexture &operator=(UniqueKtxTexture &&other) noexcept {
    if (tex != nullptr) {
      ktxTexture_Destroy(get());
    }
    tex = other.tex;
    other.tex = nullptr;
    return *this;
  }

  UniqueKtxTexture(std::nullptr_t) : tex{nullptr} {}
  explicit UniqueKtxTexture(ktxTexture2 *tex) : tex(tex) {
    if (tex && tex->classId != ktxTexture2_c) {
      throw exception("only ktx2 is supported");
    }
  }

  ~UniqueKtxTexture() {
    if (tex != nullptr) {
      ktxTexture_Destroy(get());
    }
  }

  [[nodiscard]] ktxTexture *get() const {
    return reinterpret_cast<ktxTexture *>(tex);
  }

  [[nodiscard]] operator ktxTexture *() const { return get(); }
  [[nodiscard]] operator ktxTexture2 *() const { return tex; }

  ktxTexture2 *operator->() const { return tex; }

  [[nodiscard]] explicit operator bool() const { return tex != nullptr; }

  [[nodiscard]] ktx_transcode_fmt_e
  get_transcode_format(const Device &device) const {
    if (!ktxTexture2_NeedsTranscoding(tex)) {
      return KTX_TTF_NOSELECTION;
    }

    ktx_transcode_fmt_e tf = KTX_TTF_NOSELECTION;

    const auto *features2 =
        device.stats().features.get<vk::PhysicalDeviceFeatures2>();
    const auto *features =
        (features2 != nullptr) ? &features2->features : nullptr;

    bool const astc_ldr_avaiable =
        (features != nullptr) && (features->textureCompressionASTC_LDR != 0U);
    bool const etc2_avaiable =
        (features != nullptr) && (features->textureCompressionETC2 != 0U);
    bool const bc_avaiable =
        (features != nullptr) && (features->textureCompressionBC != 0U);

    // TODO: different preferred formats for UASTC and ETC1S
    // TODO: add quality modifier to enable BC1-3 / lower quality uncompressed
    // formats

    if (astc_ldr_avaiable) {
      tf = KTX_TTF_ASTC_4x4_RGBA;
    } else if (bc_avaiable) {
      tf = KTX_TTF_BC7_RGBA;
    } else if (etc2_avaiable) {
      tf = KTX_TTF_ETC;
    } else {
      tf = KTX_TTF_RGBA32;
    }

    return tf;
  }

  [[nodiscard]] std::expected<void, ktx_error_code_e>
  transcode(const Device &device) {
    if ((tex == nullptr) || !ktxTexture2_NeedsTranscoding(tex)) {
      return {};
    }

    assert(tex->classId == ktxTexture2_c);

    auto format = get_transcode_format(device);
    return to_expected(ktxTexture2_TranscodeBasis(tex, format, 0));
  }

  [[nodiscard]] std::expected<vk::ImageType, std::string>
  get_image_type() const {
    switch (tex->numDimensions) {
    case 1:
      return vk::ImageType::e1D;
    case 2:
      return vk::ImageType::e2D;
    case 3:
      return vk::ImageType::e3D;
    default:
      return std::unexpected(std::format("unknown texture dimmentionality {}",
                                         tex->numDimensions));
    }
  }

  [[nodiscard]] std::expected<vk::ImageViewType, std::string>
  get_image_view_type() const {
    const auto is_array = tex->isArray;
    const auto is_cube = tex->isCubemap;

    switch (tex->numDimensions) {
    case 1:
      return is_array ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
    case 2:
      if (is_cube) {
        return is_array ? vk::ImageViewType::eCubeArray
                        : vk::ImageViewType::eCube;
      } else {
        return is_array ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
      }
    case 3:
      if (is_array) {
        return std::unexpected("unsupported configuration - 3D array texture");
      }

      return vk::ImageViewType::e3D;
    default:
      return std::unexpected(std::format("unknown texture dimmentionality {}",
                                         tex->numDimensions));
    }
  }

  [[nodiscard]] static auto to_expected(ktx_error_code_e err_code)
      -> std::expected<void, ktx_error_code_e> {
    return err_code == KTX_SUCCESS ? std::expected<void, ktx_error_code_e>{}
                                   : std::unexpected(err_code);
  }

  [[nodiscard]] static auto to_expected(ktx_error_code_e err_code, auto &&val) {
    using T = typename std::remove_cvref_t<decltype(val)>;
    return err_code == KTX_SUCCESS
               ? std::expected<T, ktx_error_code_e>{std::forward<decltype(val)>(
                     val)}
               : std::unexpected(err_code);
  }

  [[nodiscard]] static std::expected<UniqueKtxTexture, ktx_error_code_e>
  try_create_from_file(zstring_view path, ktxTextureCreateFlags flags) {
    ktxTexture2 *tex{};
    auto error_code = ktxTexture2_CreateFromNamedFile(path.data(), flags, &tex);

    return to_expected(error_code, UniqueKtxTexture{tex});
  }

private:
  ktxTexture2 *tex;
};
} // namespace

TransferManager::ResourceTransferHandle::~ResourceTransferHandle() {
  if (m_queue_item) {
    m_manager->cancelTransfer(*m_queue_item);
  }
}

bool TransferManager::ResourceTransferHandle::isDone() const noexcept {
  if (!m_queue_item) {
    return true;
  }

  std::scoped_lock const _(m_manager->queue_mut);
  return (*m_queue_item)->done;
}

void TransferManager::ResourceTransferHandle::updatePriority(
    PriorityClass priority) {
  if (m_queue_item) {
    m_manager->updatePriority(*this, priority);
  }
}

TransferManager::TransferManager(Context &ctx)
    : m_ctx(&ctx),
      async_transfer_semaphore(
          m_ctx->vkDevice().createSemaphore(vk::StructureChain{
              vk::SemaphoreCreateInfo{},
              vk::SemaphoreTypeCreateInfo{vk::SemaphoreType::eTimeline,
                                          0}}.get<>())) {
  m_ctx->device().setDebugName(async_transfer_semaphore,
                               "async transfer semaphore");
}

Buffer TransferManager::allocateBuffer(std::size_t size,
                                       const BufferTransferInfo &ti) {
  const auto &dev = m_ctx->device();

  Buffer buffer{
      dev,
      size,
      ti.usage | vk::BufferUsageFlagBits2KHR::eTransferDst,
      vma::AllocationCreateInfo{
          vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
              vma::AllocationCreateFlagBits::eHostAccessAllowTransferInstead,
          vma::MemoryUsage::eAuto,
      },
  };

  if (!ti.name.empty()) {
    buffer->setName(dev, "{}", ti.name);
  }

  return buffer;
}

auto TransferManager::uploadBuffer(const Buffer &buffer, buffer_upload_fn fn,
                                   const BufferTransferInfo &ti)
    -> BufferFuture {
  const auto &dev = m_ctx->device();

  auto flags =
      dev.allocator().getAllocationMemoryProperties(buffer->allocation());

  if (flags & vk::MemoryPropertyFlagBits::eHostVisible) {
    // direct init
    fn(buffer);

    // direct init is done (no need to transfer)
    return {.buffer = buffer, .transfer_handle = {}};
  }

  auto transfer = [this, fn = std::move(fn), buffer, family = ti.target_family](
                      CommandBuffer &cmd) mutable -> memory_transfer_info {
    auto staging = stagingBuffer(buffer->size());
    fn(staging);

    cmd.add_resource(buffer);
    cmd.add_resource(staging);

    cmd->copyBuffer(staging->vk(), buffer->vk(),
                    vk::BufferCopy{0, 0, buffer->size()});

    return {
        .transfer_size = buffer->size(),
        .barrier =
            vk::BufferMemoryBarrier2{
                vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferWrite,
                {},
                {},
                {},
                family,
                buffer->vk(),
                0,
                buffer->size(),
            },
    };
  };

  // transfer staging buffer to our real buffer
  return {
      .buffer = buffer,
      .transfer_handle = enqueueTransfer(ti.priority, std::move(transfer)),
  };
}

auto TransferManager::uploadTexture(const std::filesystem::path &path,
                                    const TextureTransferInfo &ti)
    -> TextureFuture {
  auto ext = path.extension();

  if (ext == ".ktx" || ext == ".ktx2") {
    return uploadTextureKtx(path, ti);
  }

  throw exception("unsupported texture format: {}", ext.string());
}

auto TransferManager::uploadTextureKtx(const std::filesystem::path &path,
                                       const TextureTransferInfo &ti)
    -> TextureFuture {
  auto error = [path_str = path.string()]<typename... Args> [[noreturn]] (
                   std::format_string<Args...> fmt, Args &&...args) {
    throw exception("loading texture \"{}\": {}", path_str,
                    std::format(fmt, std::forward<Args>(args)...));
  };

  auto resolve_expected = [error]<typename T, typename E> [[nodiscard]] (
                              std::expected<T, E> &&expected,
                              std::string_view msg) {
    if (!expected) {
      if constexpr (std::is_same_v<E, ktx_error_code_e>) {
        error("while {}: {}", msg,
              std::string_view{ktxErrorString(std::move(expected).error())});
      } else if constexpr (std::formattable<E, char>) {
        error("while {}: {}", msg, std::move(expected).error());
      } else {
        static_assert(false);
      }
    }

    if constexpr (std::is_void_v<T>) {
      return;
    } else {
      return std::move(expected).value();
    }
  };

  auto check_error = [resolve_expected](ktx_error_code_e code,
                                        std::string_view msg) {
    resolve_expected(UniqueKtxTexture::to_expected(code), msg);
  };

  auto texture =
      resolve_expected(UniqueKtxTexture::try_create_from_file(
                           path.string(), KTX_TEXTURE_CREATE_NO_FLAGS),
                       "creation");

  if (texture->classId != ktxTexture2_c) {
    error("only ktx2 is supported");
  }

  if (texture->generateMipmaps) {
    error("mipmaps generation is not supported");
  }

  resolve_expected(texture.transcode(m_ctx->device()), "transcoding");

  bool const is_cube = texture->isCubemap;

  assert(is_cube ? texture->numFaces == 6 : texture->numFaces == 1);

  std::uint32_t const layers = texture->numFaces * texture->numLayers;
  std::uint32_t const levels = texture->numLevels;

  vk::ImageCreateFlags const flags =
      is_cube ? vk::ImageCreateFlagBits::eCubeCompatible
              : vk::ImageCreateFlags{};

  vk::ImageType const imageType =
      resolve_expected(texture.get_image_type(), "getting image type");
  vk::ImageViewType const viewType = resolve_expected(
      texture.get_image_view_type(), "getting image view type");

  vk::Extent3D const extent{texture->baseWidth, texture->baseHeight,
                            texture->baseDepth};

  auto format = static_cast<vk::Format>(texture->vkFormat);

  // check if format is supproted (and has appropriate features)
  [[maybe_unused]] auto [_, format_properties] =
      m_ctx->vkPhysicalDevice()
          .getFormatProperties2<vk::FormatProperties2, vk::FormatProperties3>(
              format);

  [[maybe_unused]] auto features = format_properties.optimalTilingFeatures;

  if (!features) {
    // the format is not supported
    error("format {} is not supproted by the driver", format);
  }

  // TODO: check if the required features are present (probably in a helper )

  auto tex = ImageView::createTexture(
      *m_ctx,
      Image::ImageCreateInfo{
          .flags = flags,
          .imageType = imageType,
          .format = format,
          .extent = extent,
          .mipLevels = levels,
          .arrayLayers = layers,
          .usage = ti.usage | vk::ImageUsageFlagBits::eTransferDst,
      },
      vma::AllocationCreateInfo{}
          .setUsage(vma::MemoryUsage::eAuto)
          .setPriority(0.0F),
      vk::ImageViewCreateFlags{}, viewType);

  std::string name =
      ti.name.empty() ? path.filename().string() : std::string{ti.name};

  tex->setName(m_ctx->device(), "image view {}", name);
  tex->image()->setName(m_ctx->device(), "image {}", name);

  return uploadTextureHelper(
      tex, ti.priority, ti.layout, ti.target_family,
      [this, texture = std::move(texture),
       check_error] -> uploadTextureHelper_data {
        std::vector<vk::BufferImageCopy> regions;
        regions.reserve(texture->numLevels);

        std::size_t const textureSize =
            ktxTexture_GetDataSizeUncompressed(texture);
        Buffer staging = stagingBuffer(textureSize);

        if (texture->pData) {
          std::memcpy(staging->map<ktx_uint8_t>().get(), texture->pData,
                      textureSize);
        } else {
          /* Load the image data directly into the staging buffer. */
          check_error(
              ktxTexture_LoadImageData(
                  texture, staging->map<ktx_uint8_t>().get(), textureSize),
              "loading");
        }

        struct iter_info {
          vk::DeviceSize offset;
          std::uint32_t layers;
          std::span<vk::BufferImageCopy> regions;
          std::uint32_t region_idx;
        };

        iter_info ii{
            .offset = 0,
            .layers = texture->numFaces * texture->numLayers,
            .regions = regions,
            .region_idx = 0,
        };

        // NOLINTBEGIN(bugprone-easily-swappable-parameters): lambdas type is
        // used externally
        auto callback = [](int miplevel, int face, int width, int height,
                           int depth, ktx_uint64_t faceLodSize, void *,
                           void *userdata) -> ktxResult {
          iter_info &ii = *static_cast<iter_info *>(userdata);

          ii.regions[ii.region_idx++] = vk::BufferImageCopy{
              ii.offset,
              0,
              0,
              {
                  vk::ImageAspectFlagBits::eColor,
                  static_cast<std::uint32_t>(miplevel),
                  static_cast<std::uint32_t>(face),
                  ii.layers,
              },
              {0, 0, 0},
              {
                  static_cast<std::uint32_t>(width),
                  static_cast<std::uint32_t>(height),
                  static_cast<std::uint32_t>(depth),
              },
          };

          ii.offset += faceLodSize;

          return KTX_SUCCESS;
        };
        // NOLINTEND(bugprone-easily-swappable-parameters)

        check_error(ktxTexture_IterateLevels(texture, callback, &ii),
                    "iterating");

        return {
            .dataSize = textureSize,
            .staging = std::move(staging),
            .copyRegions = std::move(regions),
        };
      });
}

auto TransferManager::uploadTextureHelper(
    const ImageView &tex, PriorityClass priority, vk::ImageLayout target_layout,
    std::uint32_t target_family,
    uploadTextureHelper_fn prepare_transfer_data) -> TextureFuture {
  return {
      .texture = tex,
      .transfer_handle = enqueueTransfer(
          priority,
          [tex, prepare_transfer_data = std::move(prepare_transfer_data),
           target_layout,
           target_family](CommandBuffer &cmd) mutable -> memory_transfer_info {
            uploadTextureHelper_data const data = prepare_transfer_data();

            cmd.add_resource(data.staging);
            cmd.add_resource(tex);

            cmd.barrier({}, {}, {},
                        vk::ImageMemoryBarrier2{
                            vk::PipelineStageFlagBits2::eNone,
                            vk::AccessFlagBits2::eNone,
                            vk::PipelineStageFlagBits2::eTransfer,
                            vk::AccessFlagBits2::eTransferWrite,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eTransferDstOptimal,
                            vk::QueueFamilyIgnored,
                            vk::QueueFamilyIgnored,
                            tex->vkImage(),
                            vk::ImageSubresourceRange{
                                vk::ImageAspectFlagBits::eColor,
                                0,
                                vk::RemainingMipLevels,
                                0,
                                vk::RemainingArrayLayers,
                            },
                        });

            cmd->copyBufferToImage(data.staging->vk(), tex->vkImage(),
                                   vk::ImageLayout::eTransferDstOptimal,
                                   data.copyRegions);

            return {
                .transfer_size = data.dataSize,
                .barrier =
                    vk::ImageMemoryBarrier2{
                        vk::PipelineStageFlagBits2::eTransfer,
                        vk::AccessFlagBits2::eTransferWrite,
                        {},
                        {},
                        vk::ImageLayout::eTransferDstOptimal,
                        target_layout,
                        {},
                        target_family,
                        tex->vkImage(),
                        vk::ImageSubresourceRange{
                            vk::ImageAspectFlagBits::eColor,
                            0,
                            vk::RemainingMipLevels,
                            0,
                            vk::RemainingArrayLayers,
                        },
                    },
            };
          }),
  };
}

Buffer TransferManager::stagingBuffer(std::size_t size) {
  return Buffer{
      m_ctx->device(),
      size,
      vk::BufferUsageFlagBits2KHR::eTransferSrc,
      {
          vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
          vma::MemoryUsage::eAuto,
      },
  };
}

Buffer TransferManager::stagingBuffer(std::span<const std::byte> data) {
  auto buffer = stagingBuffer(data.size_bytes());
  std::ranges::copy(data, buffer->map<std::byte>().get());
  return buffer;
}

auto TransferManager::getQueueItemList(bool done, PriorityClass priority)
    -> std::list<QueueItem> & {
  if (done) {
    return list_done;
  }

  switch (priority) {
    using enum PriorityClass;
  case Low:
    return queue_low;
  case Normal:
    return queue_normal;
  case High:
    return queue_high;
  default:
    throw exception("invalid priority class");
  }
}

auto TransferManager::enqueueTransfer(
    PriorityClass priority, transfer_fn transfer) -> ResourceTransferHandle {
  std::scoped_lock const _(queue_mut);

  auto &queue = getQueueItemList(false, priority);

  queue.emplace_front(std::move(transfer), priority, false);
  return {queue.begin(), this};
}

void TransferManager::cancelTransfer(std::list<QueueItem>::iterator handle) {
  std::scoped_lock const _(queue_mut);

  // the erasure destroys the function object so all resources are released
  getQueueItemList(handle->done, handle->priority).erase(handle);
}

void TransferManager::updatePriority(ResourceTransferHandle &handle,
                                     PriorityClass priority) {
  if (!handle.m_queue_item) {
    return;
  }

  auto it = *handle.m_queue_item;

  std::scoped_lock const _(queue_mut);

  if (priority == it->priority || it->done) {
    return;
  }

  auto &src_queue = getQueueItemList(it->done, it->priority);
  auto &dst_queue = getQueueItemList(it->done, priority);

  dst_queue.splice(dst_queue.begin(), src_queue, it);
  it->priority = priority;
}

/*
TODO: what to do with failed resources?

Currently we are throwing (forwarding) the exception, but we are leaving
effectively all requested resources in an undeterminate state.

Current behaviour during an exception:
- Resources before the exception were erased but the resources after the
exception are still intact.
- The final memory barrier is not issued for any resources - even those before
the exception which potentially have issued their own commands/barriers.

Additional problems arise when the command buffer is not submitted to the queue
as this structure expects those.

---------

Currently there are 3 possible types of exception that may happen:
  -> out of CPU memory
  -> out of GPU memory / error during mapping the memory
  -> the structure of a file is invalid (the initial checks were OK but e.g the
compressed payload is invalid)

All of which I deem unrecoverable so any broken postconditions are irrelevant
(effectivelly abort with a way to log this problem out).
*/
void TransferManager::acquireResources(
    std::span<const ResourceTransferHandle> resources, CommandBuffer &cb) {
  ZoneScoped;

  std::scoped_lock const _(queue_mut);

  auto label_scope_{
      cb.debugLabelScope("async transfer - acquire", constants::vDarkYellow)};

  std::vector<vk::MemoryBarrier2> mem_barriers;
  std::vector<vk::BufferMemoryBarrier2> buf_barriers;
  std::vector<vk::ImageMemoryBarrier2> img_barriers;

  for (const auto &rth : resources) {
    if (!rth.m_queue_item) {
      continue;
    }

    auto it = *rth.m_queue_item;

    // if done, we need to do a queue family transfer
    // if not done, we need to transfer the data and insert a barrier

    if (!it->done) {
      memory_transfer_info ti = it->transfer(cb);

      auto set_dst_flags = [&](auto barrier) {
        barrier.setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands)
            .setDstAccessMask(vk::AccessFlagBits2::eMemoryRead |
                              vk::AccessFlagBits2::eMemoryWrite);
        return barrier;
      };

      std::visit(detail::overload_set{
                     [&](std::monostate) {},
                     [&](vk::MemoryBarrier2 &barrier) {
                       mem_barriers.push_back(set_dst_flags(barrier));
                     },
                     [&](vk::BufferMemoryBarrier2 &barrier) {
                       buf_barriers.push_back(set_dst_flags(barrier));
                     },
                     [&](vk::ImageMemoryBarrier2 &barrier) {
                       img_barriers.push_back(set_dst_flags(barrier));
                     },
                 },
                 ti.barrier);

      getQueueItemList(false, it->priority).erase(it);
    } else {
      // done by async transfer
      if (it->exception) {
        std::rethrow_exception(it->exception);
      }

      std::visit(
          detail::overload_set{
              [&](std::monostate) {},
              [&](vk::MemoryBarrier2) {
                assert(false &&
                       "VkMemoryBarrier2 as async transfer finalize barrier");
              },
              [&](vk::BufferMemoryBarrier2 &barrier) {
                buf_barriers.push_back(barrier);
              },
              [&](vk::ImageMemoryBarrier2 &barrier) {
                img_barriers.push_back(barrier);
              }},
          it->barrier);

      cb.add_wait(*async_transfer_semaphore, it->semaphore_value,
                  vk::PipelineStageFlagBits2::eAllCommands);

      list_done.erase(it);
    }
  }

  // check that all resources target queue family is the same as
  // cb.queueFamily()
  for (auto &barrier : buf_barriers) {
    if (barrier.dstQueueFamilyIndex != cb.queueFamily() &&
        barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex) {
      throw exception("buffer barrier with different queue family");
    }

    barrier.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
  }

  for (auto &barrier : img_barriers) {
    if (barrier.dstQueueFamilyIndex != cb.queueFamily() &&
        barrier.srcQueueFamilyIndex != barrier.dstQueueFamilyIndex) {
      throw exception("image barrier with different queue family");
    }

    barrier.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
  }

  cb.barrier({}, mem_barriers, buf_barriers, img_barriers);
}

void TransferManager::doOutstandingTransfers(std::size_t max_transfer_size,
                                             std::size_t max_transfer_count) {
  ZoneScoped;
  std::scoped_lock const _(queue_mut);

  auto &&queues = {std::ref(queue_high), std::ref(queue_normal),
                   std::ref(queue_low)};

  if (std::ranges::all_of(queues, &std::list<QueueItem>::empty)) {
    return;
  }

  std::size_t transfer_size = 0;
  std::size_t transfer_count = 0;

  // use a dedicated queue for transfers if we have a one
  //  otherwise we use a general queue

  auto &pqi = [&] -> PerQueueFamily & {
    if (auto &atr_pqi = m_ctx->get_queue(Context::QueueType::AsyncTransfer)) {
      return *atr_pqi;
    }

    if (auto &g_pqi = m_ctx->get_queue(Context::QueueType::Graphics)) {
      return *g_pqi;
    }

    throw exception(
        "neither asyc transfer queue nor graphics queue is available");
  }();

  auto cb = pqi.getCommandBuffer();

  cb.add_wait(*async_transfer_semaphore, async_transfer_semaphore_value,
              vk::PipelineStageFlagBits2::eBottomOfPipe);
  auto semaphore_value = ++async_transfer_semaphore_value;
  cb.add_signal(*async_transfer_semaphore, semaphore_value,
                vk::PipelineStageFlagBits2::eBottomOfPipe);

  std::vector<vk::BufferMemoryBarrier2> buf_barriers;
  std::vector<vk::ImageMemoryBarrier2> img_barriers;

  buf_barriers.reserve(max_transfer_count);
  img_barriers.reserve(max_transfer_count);

  {
    auto _{cb.debugLabelScope("async transfer - send", constants::vDarkCyan)};

    // going from high to low priority
    for (std::list<QueueItem> &queue : queues) {
      // try to transfer as much as possible until we reach the limit
      while (transfer_size < max_transfer_size &&
             transfer_count < max_transfer_count && !queue.empty()) {

        auto &item = queue.front();

        try {
          memory_transfer_info ti = item.transfer(cb);

          any_memory_barrier finalize_barrier = {};

          auto reset_src_flags = [](auto barrier) {
            barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                .setSrcAccessMask(vk::AccessFlagBits2::eNone);
            return barrier;
          };

          std::visit(detail::overload_set{
                         [&](std::monostate) {},
                         [&](vk::MemoryBarrier2) {
                           assert(false &&
                                  "VkMemoryBarrier2 got during async transfer "
                                  "(cannot do a queue ownership transfer)");
                         },
                         [&](vk::BufferMemoryBarrier2 barrier) {
                           buf_barriers.push_back(barrier);
                           if (barrier.srcQueueFamilyIndex !=
                               barrier.dstQueueFamilyIndex) {
                             finalize_barrier = reset_src_flags(barrier);
                           }
                         },
                         [&](vk::ImageMemoryBarrier2 barrier) {
                           img_barriers.push_back(barrier);
                           if (barrier.srcQueueFamilyIndex !=
                               barrier.dstQueueFamilyIndex) {
                             finalize_barrier = reset_src_flags(barrier);
                           }
                         },
                     },
                     ti.barrier);

          transfer_size += ti.transfer_size;
          transfer_count++;

          item.semaphore_value = semaphore_value;

          item.barrier = finalize_barrier;
        } catch (...) {
          item.exception = std::current_exception();
        }

        item.done = true;
        list_done.splice(list_done.begin(), queue, queue.begin());
      }
    }

    cb.barrier({}, {}, buf_barriers, img_barriers);
  }
  cb.end();

  pqi.submit(SubmitionInfo::gather(std::move(cb)));
}
