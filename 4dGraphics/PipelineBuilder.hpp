#pragma once

#include "Context.hpp"
#include "cppHelpers.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <filesystem>
#include <future>
#include <optional>
#include <vector>

namespace v4dg {
std::optional<std::vector<std::uint32_t>>
load_shader_code(const std::filesystem::path &path);

std::optional<vk::raii::ShaderModule>
load_shader_module(const std::filesystem::path &path,
                   const vk::raii::Device &device);

class GraphicsPipelineBuilder;

class ShaderStageData {
public:
  ShaderStageData() = delete;
  ShaderStageData(vk::ShaderStageFlagBits stage, vk::ShaderModule shader_module,
                  std::string entry = "main");
  ShaderStageData(const ShaderStageData &);
  ShaderStageData(ShaderStageData &&);
  ShaderStageData &operator=(ShaderStageData);

  template <typename T>
  ShaderStageData &add_specialization(std::uint32_t id, const T &data) {
    specialization_data.insert(
        specialization_data.end(),
        detail::AlignUpOffset(specialization_data.size(), alignof(T)), {});

    specialization_entries.push_back({
        id,
        static_cast<std::uint32_t>(specialization_data.size()),
        sizeof(T),
    });

    auto *ptr = reinterpret_cast<const std::byte *>(&data);
    specialization_data.insert(specialization_data.end(), ptr, ptr + sizeof(T));

    fixup();

    return *this;
  }

  ShaderStageData &set_flags(vk::PipelineShaderStageCreateFlags flags) {
    info_chain.get<vk::PipelineShaderStageCreateInfo>().flags = flags;
    return *this;
  }
  ShaderStageData &
  set_robustness_info(const vk::PipelineRobustnessCreateInfoEXT &info) {
    info_chain.get<vk::PipelineRobustnessCreateInfoEXT>() = info;
    info_chain.relink<vk::PipelineRobustnessCreateInfoEXT>();
    return *this;
  }
  ShaderStageData &set_subgroup_size_info(
      const vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT &info) {
    info_chain.get<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>() =
        info;
    info_chain
        .relink<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>();
    return *this;
  }

  const vk::PipelineShaderStageCreateInfo &get() const {
    return info_chain.get<vk::PipelineShaderStageCreateInfo>();
  }

private:
  std::string entry{"main"};

  std::vector<vk::SpecializationMapEntry> specialization_entries{};
  std::vector<std::byte> specialization_data{};

  vk::StructureChain<vk::PipelineShaderStageCreateInfo,
                     vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT,
                     vk::PipelineRobustnessCreateInfoEXT>
      info_chain{{}, {}, {}};
  vk::SpecializationInfo specialization_info{};

  void fixup();

  friend GraphicsPipelineBuilder;
};

class GraphicsPipelineBuilder {
public:
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
    color_blend_attachments.push_back({});
    return *this;
  }
  GraphicsPipelineBuilder &add_dynamic_state(vk::DynamicState state) {
    dynamic_states.push_back(state);
    return *this;
  }

  vk::raii::Pipeline build(Context &dev,
                           std::span<const ShaderStageData> shaders);

  vk::PipelineLayout layout;
  vk::PipelineCreateFlags flags{};

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