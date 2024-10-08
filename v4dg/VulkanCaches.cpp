#include "VulkanCaches.hpp"

#include "BindlessManager.hpp"
#include "Context.hpp"
#include "Device.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace v4dg {
SamplerInfo::SamplerInfo(vk::SamplerCreateFlags flags) noexcept {
  sci.flags = flags;

  setFilters();
  setAddressMode();
  setMipLodBias();
  setAnisotropy();
  setCompareOp();
  setLodBounds();
  setBorderColor();
  setReductionMode();
}

std::shared_ptr<const Sampler> SamplerInfo::create(Context &ctx) const {
  vk::StructureChain<vk::SamplerCreateInfo, vk::SamplerReductionModeCreateInfo>
      chain(sci, srmci);

  auto res = std::make_shared<Sampler>(
      vk::raii::Sampler{ctx.vkDevice(), chain.get<>()},
      ctx.bindlessManager().allocate(BindlessType::eSampler));

  vk::DescriptorImageInfo dii{*res->sampler};

  ctx.vkDevice().updateDescriptorSets(
      ctx.bindlessManager().write_for(*res->handle, dii), {});

  return res;
}

std::size_t
SamplerInfo::hash::operator()(const SamplerInfo &si) const noexcept {
  std::size_t seed{};
  detail::add_hash(seed, si.sci, si.srmci);
  return seed;
}

size_t DescriptorSetLayoutInfo::hash::operator()(
    const DescriptorSetLayoutInfo &dsli) const noexcept {
  size_t seed = dsli.bindings.size();

  auto add_hash = [&seed](auto &&...args) {
    detail::add_hash(seed, std::forward<decltype(args)>(args)...);
  };

  add_hash(dsli.flags);

  for (const auto &binding : dsli.bindings) {
    add_hash(binding.binding, binding.descriptorType, binding.descriptorCount,
             binding.stageFlags);
    // do not add to hash pImmutable samplers => pointer (hash samplers
    // themselves)
  }

  add_hash(dsli.bindFlags);
  add_hash(dsli.bindSamplers);

  return seed;
}

DescriptorSetLayoutInfo &DescriptorSetLayoutInfo::add_binding(
    uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stages,
    uint32_t count, std::span<const vk::Sampler> immutableSamplers,
    vk::DescriptorBindingFlags flags) {
  bindSamplers.emplace_back(immutableSamplers.begin(), immutableSamplers.end());
  bindFlags.emplace_back(flags);
  bindings.emplace_back(binding, type, count, stages,
                        bindSamplers.back().data());

  return *this;
}

vk::raii::DescriptorSetLayout
DescriptorSetLayoutInfo::create(const Device &dev) const {
  vk::StructureChain<vk::DescriptorSetLayoutCreateInfo,
                     vk::DescriptorSetLayoutBindingFlagsCreateInfo>
      chain{
          {flags, bindings},
          {bindFlags},
      };

  return {dev.device(), chain.get<>()};
}

vk::raii::PipelineLayout PipelineLayoutInfo::create(const Device &dev) const {
  return {dev.device(), {flags, setLayouts, pushRanges}};
}

/*
file_watcher::file_data::file_data(const fs::path &file,
                                   vector<callback_handle> callbacks)
    : subscribed_callbacks(std::move(callbacks)) {
  error_code ec;
  ftime = last_write_time(file, ec);
  if (ec)
    ftime = fs::file_time_type::min();
}

auto file_watcher::add_callback(
    callback_type callback,
    const vector<pair<fs::path, fs::file_time_type>> &files)
    -> callback_handle {
  // TODO add strong exception safety
  lock_guard lock(mut);

  callback_handle h = ++lastHandle;
  vector<fs::path> paths;
  paths.reserve(files.size());

  bool update = false;

  for (const auto &[file, time] : files) {
    fs::path file_abs = absolute(weakly_canonical(file));
    auto it = this->files.find(file_abs);

    if (it == this->files.end())
      it = this->files.emplace(file_abs, file_abs).first;

    assert(it != this->files.end());
    if (it->second.ftime > time)
      update = true;

    it->second.subscribed_callbacks.push_back(h);
  }

  auto cb = callbacks.emplace(h, std::move(callback), std::move(paths));

  if (update)
    cb.first->second.first(*this, h);

  return h;
}

void file_watcher::remove_callback(callback_handle h) {
  lock_guard lock(mut);

  auto it = callbacks.find(h);
  if (it == callbacks.end())
    return;

  vector<fs::path> paths = std::move(it->second.second);
  callbacks.erase(it);

  for (const fs::path &p : paths) {
    auto fit = files.find(p);
    if (fit == files.end())
      continue;

    vector<callback_handle> &sub = fit->second.subscribed_callbacks;
    ptrdiff_t idx = find(sub.begin(), sub.end(), h) - sub.begin();
    if ((size_t)idx + 1 != sub.size())
      swap(sub[idx], sub.back());

    sub.resize(sub.size() - 1);

    if (sub.size() == 0)
      files.erase(fit);
  }
}

void file_watcher::update() {
  lock_guard lock(mut);
  set<callback_handle> to_update; // flat_set

  for (auto &[file, fdata] : files) {
    error_code ec;
    fs::file_time_type cur_ftime = last_write_time(file, ec);
    if (ec || cur_ftime <= fdata.ftime)
      continue; // file removed or not changed

    // file changed
    fdata.ftime = cur_ftime;

    for (callback_handle h : fdata.subscribed_callbacks)
      to_update.insert(h);
  }

  for (callback_handle h : to_update)
    if (auto it = callbacks.find(h); it != callbacks.end())
      it->second.first(*this, h);
}
*/
} // namespace v4dg
