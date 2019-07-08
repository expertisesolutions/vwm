///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_SBO_HPP
#define VWM_WAYLAND_SBO_HPP

#include <vector>
#include <cstddef>

namespace vwm { namespace wayland {

namespace detail {

template <typename T>
struct dynamic_buffer
{
  bool is_dynamic;
  std::vector<T> v;
};

}
    
template <typename T, std::size_t SmallSize = 64>
struct sbo
{
  using dynamic_buffer = detail::dynamic_buffer<T>;
  
  constexpr static const std::size_t dynamic_size = sizeof(dynamic_buffer);
  constexpr static const std::size_t vector_offset = offsetof (dynamic_buffer, v);
  constexpr static const std::size_t static_buffer_size = (SmallSize + vector_offset);
  constexpr static const std::size_t static_size = SmallSize;
  std::array<char, static_buffer_size> static_data;

  typedef sbo<T, SmallSize> self_type;

  sbo ()
  {
    *static_cast<bool*>(static_cast<void*>(static_data.data())) = false;
  }
  
  constexpr bool is_dynamic() const
  {
    return *static_cast<const bool*>(static_cast<const void*>(static_data.data()));
  }

  constexpr const void* data() const
  {
    if (!is_dynamic())
    {
      return &static_data[vector_offset];
    }
    else
    {
      auto dv = get_dynamic_buffer();
      return dv->v.data();
    }
  }
  void* data()
  {
    return const_cast<void*>(static_cast<const self_type*>(this)->data());
  }

  void resize(std::size_t size)
  {
    if (is_dynamic())
    {
      std::cout << "is_dynamic" << std::endl;
      auto db = get_dynamic_buffer();
      db->v.resize(size);
    }
    else if (size > static_size)
    {
      std::vector<char> v(size);
      std::memcpy(v.data(), static_cast<const char*>(data()), static_size);
      auto dst_db = create_dynamic_buffer({true, std::move(v)});
      static_cast<void>(dst_db);
    }
    else
    {
      ///
      // do nothing
    }
  }

  void compact () // compact to static size if possible
  {
    if (is_dynamic() && size() < static_size)
    {
      auto db = get_dynamic_buffer();
      auto v = std::move(db->v);

      db->is_dynamic = false;
      std::memcpy (data(), v.data(), size());
    }
  }

  void grow () // double size
  {
    resize (size() << 2);
  }

  constexpr std::size_t size() const
  {
    return !is_dynamic() ? static_size
      : get_dynamic_buffer() -> v.size();
  }
  
private:
  constexpr const dynamic_buffer* get_dynamic_buffer() const
  {
    assert (is_dynamic());
    return static_cast<const dynamic_buffer*>(static_cast<const void*>(static_data.data()));
  }

  constexpr dynamic_buffer* get_dynamic_buffer()
  {
    return const_cast<dynamic_buffer*>(const_cast<self_type const*>(this)->get_dynamic_buffer());
  }

  constexpr dynamic_buffer* create_dynamic_buffer(dynamic_buffer db = {})
  {
    assert (!is_dynamic());
    return new (static_data.data()) dynamic_buffer(std::move(db));
  }
};
    
} }

#endif
