#include "PipelineBuilder.hpp"

#include "Context.hpp"
#include "cppHelpers.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace v4dg {
namespace {
bool fix_spirv_endianess(std::span<uint32_t> code) {
  if (code.empty()) {
    return false;
  }

  static constexpr uint32_t magic = 0x07230203;
  static constexpr uint32_t rev_magic = std::byteswap(magic);

  if (code[0] == magic) { // file is already in correct endianess
    return true;
  }

  if (code[0] == rev_magic) // file is in wrong endianess
  {
    for (auto &word : code) {
      word = std::byteswap(word);
    }
    return true;
  }

  // invalid magic number
  return false;
}
} // namespace

std::expected<std::vector<std::uint32_t>,
              std::variant<detail::get_file_error, load_shader_error>>
load_shader_code(const std::filesystem::path &path) {
  using res_t =
      std::expected<std::vector<std::uint32_t>,
                    std::variant<detail::get_file_error, load_shader_error>>;

  return static_cast<res_t>(detail::GetFileBinary<uint32_t>(path))
      .and_then([](auto &&code) -> res_t {
        if (!fix_spirv_endianess(code)) {
          return std::unexpected(load_shader_error::bad_magic_number);
        }
        return code;
      });
}

void ShaderStageData::fixup() {
  info_chain.get<vk::PipelineShaderStageCreateInfo>()
      .setPName(entry.c_str())
      .setPSpecializationInfo(&specialization_info);

  specialization_info.setMapEntries(specialization_entries)
      .setData<std::byte>(specialization_data);

  info_chain.get<vk::DebugUtilsObjectNameInfoEXT>()
      .setObjectType(vk::ObjectType::eShaderModule)
      .setPObjectName(debug_name.data());
}

ShaderStageData::ShaderStageData(
    vk::ShaderStageFlagBits stage,
    vk::ArrayProxyNoTemporaries<const uint32_t> shader_module,
    std::string entry)
    : entry(std::move(entry)) {
  info_chain.get<vk::PipelineShaderStageCreateInfo>().setStage(stage);

  info_chain.get<vk::ShaderModuleCreateInfo>().setCode(shader_module);

  info_chain.unlink<vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT>();
  info_chain.unlink<vk::PipelineRobustnessCreateInfoEXT>();
  info_chain.unlink<vk::DebugUtilsObjectNameInfoEXT>();

  fixup();
}

ShaderStageData::ShaderStageData(const ShaderStageData &o)
    : entry(o.entry), specialization_entries(o.specialization_entries),
      specialization_data(o.specialization_data), info_chain(o.info_chain),
      specialization_info(o.specialization_info) {
  fixup();
}

ShaderStageData::ShaderStageData(ShaderStageData &&o) noexcept
    : entry(std::move(o.entry)),
      specialization_entries(std::move(o.specialization_entries)),
      specialization_data(std::move(o.specialization_data)),
      info_chain(std::move(o.info_chain)),
      specialization_info(o.specialization_info) {
  fixup();
}

ShaderStageData &ShaderStageData::operator=(ShaderStageData o) {
  std::swap(entry, o.entry);
  std::swap(specialization_entries, o.specialization_entries);
  std::swap(specialization_data, o.specialization_data);
  std::swap(debug_name, o.debug_name);
  std::swap(info_chain, o.info_chain);
  std::swap(specialization_info, o.specialization_info);

  fixup();
  o.fixup();

  return *this;
}

vk::raii::Pipeline
GraphicsPipelineBuilder::build(Context &ctx,
                               std::span<const ShaderStageData> shader_stages) {
  std::vector<vk::PipelineShaderStageCreateInfo> psscis;
  psscis.reserve(shader_stages.size());

  for (const auto &stage_data : shader_stages) {
    psscis.push_back(stage_data.get());
  }

  vk::PipelineVertexInputStateCreateInfo const vertex_input_state;
  vk::PipelineInputAssemblyStateCreateInfo const input_assembly_state{
      {},
      topology,
      static_cast<vk::Bool32>(primitive_restart),
  };
  vk::PipelineViewportStateCreateInfo const viewport_state{
      {}, viewports, nullptr, viewports, nullptr,
  };
  vk::PipelineMultisampleStateCreateInfo multisample_state_my =
      multisample_state;
  multisample_state_my.setPSampleMask(sample_mask ? &*sample_mask : nullptr);

  vk::PipelineColorBlendStateCreateInfo color_blend_state_my =
      color_blend_state;
  color_blend_state_my.setAttachments(color_blend_attachments);

  vk::PipelineDynamicStateCreateInfo const dynamic_state{{}, dynamic_states};

  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      chain{
          vk::GraphicsPipelineCreateInfo{
              flags,
              psscis,
              &vertex_input_state,
              &input_assembly_state,
              nullptr,
              &viewport_state,
              &rasterization_state,
              &multisample_state_my,
              &depth_stencil_state,
              &color_blend_state_my,
              &dynamic_state,
              layout,
          },
          vk::PipelineRenderingCreateInfo{
              view_mask,
              color_formats,
              depth_format,
              stencil_format,
          },
      };

  return {ctx.vkDevice(), ctx.pipeline_cache(), chain.get<>()};
}
} // namespace v4dg