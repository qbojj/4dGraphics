#pragma once

#include "Context.hpp"
#include "cppHelpers.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace v4dg {

enum class load_shader_error {
  bad_magic_number,
};

constexpr std::string_view to_string(load_shader_error e) {
  switch (e) {
  case load_shader_error::bad_magic_number:
    return "bad_magic_number";
  }
  return "unknown";
}

std::expected<std::vector<std::uint32_t>,
              std::variant<detail::get_file_error, load_shader_error>>
load_shader_code(const std::filesystem::path &path);

struct GraphicsPipelineBuilder;

class ShaderStageData {
public:
  ShaderStageData() = delete;
  ShaderStageData(vk::ShaderStageFlagBits stage,
                  vk::ArrayProxyNoTemporaries<const uint32_t> shader_data,
                  std::string entry = "main");
  ShaderStageData(const ShaderStageData &);
  ShaderStageData(ShaderStageData &&) noexcept;
  ShaderStageData &operator=(const ShaderStageData &);
  ShaderStageData &operator=(ShaderStageData &&) noexcept;

  template <typename T>
  ShaderStageData &add_specialization(std::uint32_t id, const T &data) {
    specialization_data.insert(
        specialization_data.end(),
        detail::AlignUpOffset(specialization_data.size(), alignof(T)), {});

    specialization_entries.emplace_back(
        id, static_cast<std::uint32_t>(specialization_data.size()), sizeof(T));

    const auto *ptr = reinterpret_cast<const std::byte *>(&data);
    specialization_data.insert(specialization_data.end(), ptr, ptr + sizeof(T));

    specialization_info.setMapEntries(specialization_entries)
        .setData<std::byte>(specialization_data);

    return *this;
  }

  ShaderStageData &set_debug_name(std::string name) {
    debug_name = std::move(name);
    info_chain.assign(vk::DebugUtilsObjectNameInfoEXT{
        vk::ObjectType::eShaderModule, {}, debug_name.c_str()});
    info_chain.relink<vk::DebugUtilsObjectNameInfoEXT>();
    return *this;
  }

  ShaderStageData &set_flags(vk::PipelineShaderStageCreateFlags flags) {
    info_chain.get<vk::PipelineShaderStageCreateInfo>().flags = flags;
    return *this;
  }
  ShaderStageData &
  set_robustness_info(const vk::PipelineRobustnessCreateInfoEXT &info) {
    info_chain.assign(info);
    info_chain.relink<vk::PipelineRobustnessCreateInfoEXT>();
    return *this;
  }
  ShaderStageData &set_subgroup_size_info(
      const vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT &info) {
    info_chain.assign(info);
    info_chain
        .relink<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>();
    return *this;
  }

  [[nodiscard]] const vk::PipelineShaderStageCreateInfo &get() const {
    return info_chain.get<vk::PipelineShaderStageCreateInfo>();
  }

private:
  std::string entry{"main"};

  std::vector<vk::SpecializationMapEntry> specialization_entries;
  std::vector<std::byte> specialization_data;

  std::string debug_name;

  vk::StructureChain<vk::PipelineShaderStageCreateInfo,
                     vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT,
                     vk::PipelineRobustnessCreateInfoEXT,
                     vk::ShaderModuleCreateInfo,
                     vk::DebugUtilsObjectNameInfoEXT>
      info_chain;
  vk::SpecializationInfo specialization_info;

  void fixup();

  friend GraphicsPipelineBuilder;
};

struct GraphicsPipelineBuilder {
  GraphicsPipelineBuilder() = delete;
  GraphicsPipelineBuilder(vk::PipelineLayout layout,
                          vk::PipelineCreateFlags flags = {})
      : layout(layout), flags(flags) {}

  GraphicsPipelineBuilder &set_topology(vk::PrimitiveTopology topology,
                                        bool primitiveRestart = false) {
    this->topology = topology;
    this->primitive_restart = primitiveRestart;
    return *this;
  }

  GraphicsPipelineBuilder &set_depth_test(vk::CompareOp depthCompare,
                                          vk::Bool32 depthWrite = vk::True) {
    depth_stencil_state.setDepthTestEnable(vk::True)
        .setDepthCompareOp(depthCompare)
        .setDepthWriteEnable(depthWrite);
    return *this;
  }
  GraphicsPipelineBuilder &set_depth_format(vk::Format format) {
    depth_format = format;
    return *this;
  }
  GraphicsPipelineBuilder &set_stencil_format(vk::Format format) {
    stencil_format = format;
    return *this;
  }
  GraphicsPipelineBuilder &set_depth_stencil_format(vk::Format format) {
    depth_format = stencil_format = format;
    return *this;
  }
  GraphicsPipelineBuilder &add_additive_blend(vk::Format format) {
    color_formats.push_back(format);
    color_blend_attachments.emplace_back(
        vk::True, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOne,
        vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    return *this;
  }
  GraphicsPipelineBuilder &add_alpha_blend(vk::Format format) {
    color_formats.push_back(format);
    color_blend_attachments.emplace_back(
        vk::True, vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    return *this;
  }
  GraphicsPipelineBuilder &add_null_attachment() {
    color_formats.push_back(vk::Format::eUndefined);
    color_blend_attachments.emplace_back();
    return *this;
  }
  GraphicsPipelineBuilder &add_dynamic_state(vk::DynamicState state) {
    dynamic_states.push_back(state);
    return *this;
  }

  vk::raii::Pipeline build(Context &ctx,
                           std::span<const ShaderStageData> shader_stages);

  vk::PipelineLayout layout;
  vk::PipelineCreateFlags flags;

  vk::PrimitiveTopology topology{vk::PrimitiveTopology::eTriangleList};
  bool primitive_restart{false};

  uint32_t viewports{1};

  vk::PipelineRasterizationStateCreateInfo rasterization_state;
  vk::PipelineMultisampleStateCreateInfo multisample_state;
  std::optional<vk::SampleMask> sample_mask;

  vk::PipelineDepthStencilStateCreateInfo depth_stencil_state;

  vk::PipelineColorBlendStateCreateInfo color_blend_state;
  std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;

  std::uint32_t view_mask{};
  std::vector<vk::Format> color_formats;
  vk::Format depth_format{vk::Format::eUndefined};
  vk::Format stencil_format{vk::Format::eUndefined};

  std::vector<vk::DynamicState> dynamic_states{
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  };
};
} // namespace v4dg
