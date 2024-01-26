#pragma once

#include "Device.hpp"
#include "HandleCache.hpp"
#include "v4dgCore.hpp"
#include "v4dgVulkan.hpp"

#include <ankerl/unordered_dense.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

namespace v4dg {
namespace detail {
inline void hash_combine(::std::size_t &seed, ::std::size_t hash) noexcept {
  seed = ::ankerl::unordered_dense::detail::wyhash::mix(seed + hash, UINT64_C(0x5ed1dbb3b4bc2e98));
}

template <typename T = void> struct omni_hash {
  using is_avalanching = void;

  auto operator()(const T &t) const noexcept {
    if constexpr (::std::has_unique_object_representations_v<T>) {
      return ::ankerl::unordered_dense::detail::wyhash::hash(std::addressof(t),
                                                             sizeof(T));
    } else {
      return ::ankerl::unordered_dense::hash<T>{}(t);
    }
  }
};

template <::std::ranges::range R> class omni_hash<R> {
private:
  using T = ::std::remove_cv_t<::std::ranges::range_value_t<R>>;

public:
  using is_avalanching = void;

  auto operator()(const R &r) const noexcept {    
    if constexpr (::std::ranges::contiguous_range<R> &&
                  ::std::has_unique_object_representations_v<T>) {
      return ::ankerl::unordered_dense::detail::wyhash::hash(
          ::std::ranges::data(r), ::std::ranges::size(r) * sizeof(T));
    } else {
      ::std::size_t seed = ::std::ranges::distance(r);
      for (const auto &e : r)
        hash_combine(seed, omni_hash<T>{}(e));
      return seed;
    }
  }
};

template <> struct omni_hash<void> {
  template <typename T> auto operator()(T &&t) const noexcept {
    return omni_hash<::std::remove_cvref_t<T>>{}(std::forward<T>(t));
  }
};

inline void add_hash(::std::size_t &seed, auto &&...args) noexcept {
  (hash_combine(seed, omni_hash{}(std::forward<decltype(args)>(args))), ...);
}

} // namespace detail

class SamplerInfo {
public:
  using handle_type = vk::raii::Sampler;
  using handle_data = const Device *;
  struct hash {
    size_t operator()(const SamplerInfo &) const noexcept;
  };

  SamplerInfo(vk::SamplerCreateFlags flags = {}) noexcept;

  SamplerInfo &setFilters(
      vk::Filter minFilter = vk::Filter::eLinear,
      vk::Filter magFilter = vk::Filter::eLinear,
      vk::SamplerMipmapMode mipMode = vk::SamplerMipmapMode::eLinear) noexcept {
    sci.setMinFilter(minFilter).setMagFilter(magFilter).setMipmapMode(mipMode);
    return *this;
  }

  SamplerInfo &setAddressMode(
      vk::SamplerAddressMode addressModeU = vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode addressModeV = vk::SamplerAddressMode::eRepeat,
      vk::SamplerAddressMode addressModeW =
          vk::SamplerAddressMode::eRepeat) noexcept {
    sci.setAddressModeU(addressModeU)
        .setAddressModeV(addressModeV)
        .setAddressModeW(addressModeW);
    return *this;
  }

  SamplerInfo &setMipLodBias(float bias = 0.f) noexcept {
    sci.setMipLodBias(bias);
    return *this;
  }

  SamplerInfo &setAnisotropy(float ani = 0.f) noexcept {
    sci.setAnisotropyEnable(ani != 0.f).setMaxAnisotropy(ani);
    return *this;
  }

  SamplerInfo &
  setCompareOp(vk::Bool32 enable = vk::False,
               vk::CompareOp op = vk::CompareOp::eLessOrEqual) noexcept {
    sci.setCompareEnable(enable).setCompareOp(op);
    return *this;
  }

  SamplerInfo &setLodBounds(float minLod = 0.f,
                            float maxLod = vk::LodClampNone) noexcept {
    sci.setMinLod(minLod).setMaxLod(maxLod);
    return *this;
  }

  SamplerInfo &
  setBorderColor(vk::BorderColor color =
                     vk::BorderColor::eFloatTransparentBlack) noexcept {
    sci.setBorderColor(color);
    return *this;
  }

  SamplerInfo &
  setReductionMode(vk::SamplerReductionMode mode =
                       vk::SamplerReductionMode::eWeightedAverage) noexcept {
    srmci.setReductionMode(mode);
    return *this;
  }

  SamplerInfo &setYcbcrConversion(
      vk::SamplerYcbcrConversion conversion = VK_NULL_HANDLE) noexcept {
    syci.setConversion(conversion);
    return *this;
  }

  auto operator<=>(const SamplerInfo &) const = default;

  vk::raii::Sampler create(const Device *) const;

private:
  vk::SamplerCreateInfo sci;
  vk::SamplerReductionModeCreateInfo srmci;
  vk::SamplerYcbcrConversionInfo syci;
};

class DescriptorSetLayoutInfo {
public:
  using handle_type = vk::raii::DescriptorSetLayout;
  using handle_data = const Device *;
  struct hash {
    size_t operator()(const DescriptorSetLayoutInfo &) const noexcept;
  };

  DescriptorSetLayoutInfo(
      vk::DescriptorSetLayoutCreateFlags flags = {}) noexcept
      : flags(flags) {}

  DescriptorSetLayoutInfo &
  add_binding(uint32_t binding, vk::DescriptorType type,
              vk::ShaderStageFlags stages, uint32_t count = 1,
              std::span<const vk::Sampler> immutableSamplers = {},
              vk::DescriptorBindingFlags flags = {});

  auto operator<=>(const DescriptorSetLayoutInfo &) const = default;

  vk::raii::DescriptorSetLayout create(const Device *) const;

private:
  vk::DescriptorSetLayoutCreateFlags flags;
  std::vector<vk::DescriptorSetLayoutBinding> bindings;
  std::vector<std::vector<vk::Sampler>> bindSamplers;
  std::vector<vk::DescriptorBindingFlags> bindFlags;
};

class PipelineLayoutInfo {
public:
  using handle_type = vk::raii::PipelineLayout;
  using handle_data = const Device *;
  struct hash {
    size_t operator()(const PipelineLayoutInfo &) const noexcept;
  };

  PipelineLayoutInfo(vk::PipelineLayoutCreateFlags flags = {}) noexcept
      : flags(flags) {}

  PipelineLayoutInfo &add_set(vk::DescriptorSetLayout set) {
    setLayouts.push_back(set);
    return *this;
  }

  PipelineLayoutInfo &add_push(vk::PushConstantRange range) {
    pushRanges.push_back(range);
    return *this;
  }

  PipelineLayoutInfo &normalize() {
    std::ranges::sort(pushRanges, {}, &vk::PushConstantRange::offset);
    return *this;
  }

  auto operator<=>(const PipelineLayoutInfo &) const = default;

  vk::raii::PipelineLayout create(const Device *) const;

private:
  vk::PipelineLayoutCreateFlags flags;
  std::vector<vk::DescriptorSetLayout> setLayouts;
  std::vector<vk::PushConstantRange> pushRanges;
};

// TODO
class RenderPassInfo {
public:
  using handle_type = vk::raii::RenderPass;
  using handle_data = const Device *;

  RenderPassInfo(vk::RenderPassCreateFlags flags = {}) : flags(flags) {}

  void normalize();
  vk::raii::RenderPass create(const Device *) const;

private:
  vk::RenderPassCreateFlags flags;
};

/*
// TODO
class GraphicsPipelineInfo {
public:
  using handle_type = vk::raii::Pipeline;
  using handle_data = const Device *;
};

// TODO
class ComputePipelineInfo {
public:
  using handle_type = vk::raii::Pipeline;
  using handle_data = const Device *;
};

// TODO
class RayTracingPipelineInfo {
public:
  using handle_type = vk::raii::Pipeline;
  using handle_data = const Device *;
};

// TODO
class SamplerYcbcrConversionInfo {
public:
  using handle_type = vk::raii::SamplerYcbcrConversion;
  using handle_data = const Device *;
};
*/
// typedef permament_handle_cache<SamplerYcbcrConversionInfo>
// sampler_ycbcr_conversion_cache;
typedef permament_handle_cache<SamplerInfo> sampler_cache;

// typedef permament_handle_cache<DescriptorSetLayoutInfo> descriptor_set_layout_cache;
// typedef permament_handle_cache<PipelineLayoutInfo> pipeline_layout_cache;
// typedef permament_handle_cache<RenderPassInfo> render_pass_cache;

// typedef handle_cache<GraphicsPipelineInfo> GraphicsPipelineCache;
// typedef handle_cache<ComputePipelineInfo> ComputePipelineCache;
// typedef handle_cache<RayTracingPipelineInfo> RayTracingPipelineCache;

} // namespace v4dg