#pragma once

#include "cppHelpers.hpp"

#include <ankerl/unordered_dense.h>
#include <concepts>
#include <shared_mutex>
#include <mutex>
#include <cassert>

namespace v4dg {
template <typename T>
concept handle_descriptor_base = requires(const T &cref, typename T::handle_data &d, typename T::handle_type h) {
  typename T::handle_data;
  typename T::hash;

  typename T::handle_type;

  requires std::movable<typename T::handle_type>;
  requires std::copyable<T> && std::equality_comparable<T>;
  
  { typename T::hash{}(cref) } -> std::convertible_to<std::uint64_t>;
};

template <typename T>
concept permament_handle_descriptor = handle_descriptor_base<T> && requires(const T &cref, typename T::handle_data &d) {
  { cref.create(d) } -> std::same_as<typename T::handle_type>;
};

template <permament_handle_descriptor handle_desc>
class permament_handle_cache {
public:
  using handle_type = typename handle_desc::handle_type;
  using handle_data = typename handle_desc::handle_data;

  permament_handle_cache(handle_data data) : m_data(data) {}

private:
  using hash = typename handle_desc::hash;
  using map_type = ankerl::unordered_dense::map<handle_desc, handle_type, hash>;

public:
  const handle_type &get(const handle_desc &desc) {
    if (auto phandle = get_old(desc); phandle)
      return *phandle;
    
    return get_locked(desc);
  }

  const handle_type &get_locked(const handle_desc &desc) {
    {
      std::shared_lock lock(m_mut);
      if (auto phandle = get_lockless(desc); phandle)
        return *phandle;
    }

    {
      // lock for write
      std::unique_lock lock(m_mut);
      if (auto phandle = get_lockless(desc); phandle)
        return *phandle;
      
      return m_new.insert(desc, desc.create(m_data)).first->second;
    }
  }

  void flush() {
    std::unique_lock lock(m_mut);

    auto storage = m_new.extract();
    m_old.insert(std::move_iterator(storage.begin()), std::move_iterator(storage.end()));
  }

private:
  const handle_type *get_old(const handle_desc &desc) const {
    if (auto it = m_old.find(desc); it != m_old.end())
      return &it->second;

    return nullptr;
  }

  const handle_type *get_new(const handle_desc &desc) const {
    if (auto it = m_new.find(desc); it != m_new.end())
      return &it->second;

    return nullptr;
  }

  const handle_type *get_lockless(const handle_desc &desc) const {
    if (auto phandle = get_old(desc); phandle) return phandle;
    return get_new(desc);
  }

  // lockless, already created at least frame before
  map_type m_old;

  // stores handle and atomic bool telling if the object is already constucted
  mutable std::shared_mutex m_mut;
  map_type m_new;

  handle_data m_data;
};

template <typename T>
concept handle_descriptor = handle_descriptor_base<T> && requires(const T &cref, typename T::handle_data &d) {
  { cref.create(d) } -> std::same_as<std::shared_ptr<const typename T::handle_type>>;
};

template <handle_descriptor handle_desc>
class handle_cache {
public:
  using handle_type = typename handle_desc::handle_type;
  using handle_data = typename handle_desc::handle_data;

  handle_cache(handle_data data) : m_data(data) {}

private:
  using hash = typename handle_desc::hash;
  using map_type = ankerl::unordered_dense::map<handle_desc, std::weak_ptr<const handle_type>, hash>;

public:
  std::shared_ptr<const handle_type> get(const handle_desc &desc) {
    if (auto phandle = get_old(desc); phandle)
      return phandle;
    
    return get_locked(desc);
  }

  std::shared_ptr<const handle_type> get_locked(const handle_desc &desc) {
    {
      std::shared_lock lock(m_mut);
      if (auto phandle = get_lockless(desc); phandle)
        return phandle;
    }

    {
      // lock for write
      std::unique_lock lock(m_mut);
      if (auto phandle = get_lockless(desc); phandle)
        return phandle;
      
      auto handle = desc.create(m_data);
      m_new.insert(desc, handle);
      return handle;
    }
  }

  void flush(bool cleanup = true) {
    std::unique_lock lock(m_mut);

    auto storage = m_new.extract();
    for (auto &&[desc, handle] : storage)
      m_old.insert_or_assign(std::move(desc), std::move(handle));

    if (cleanup)
      std::erase_if(m_old, [](const auto &pair) { return pair.second.expired(); });
  }

private:
  std::shared_ptr<const handle_type> get_old(const handle_desc &desc) const {
    if (auto it = m_old.find(desc); it != m_old.end())
      return it->second.lock();

    return nullptr;
  }

  std::shared_ptr<const handle_type> get_new(const handle_desc &desc) const {
    if (auto it = m_new.find(desc); it != m_new.end())
      return it->second.lock();

    return nullptr;
  }

  std::shared_ptr<const handle_type> get_lockless(const handle_desc &desc) const {
    if (auto phandle = get_old(desc); phandle) return phandle;
    return get_new(desc);
  }

  // lockless, already created at least frame before
  map_type m_old;

  // stores handle and atomic bool telling if the object is already constucted
  mutable std::shared_mutex m_mut;
  map_type m_new;

  handle_data m_data;
};

} // namespace v4dg
