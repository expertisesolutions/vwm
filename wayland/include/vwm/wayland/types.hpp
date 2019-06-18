///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_TYPES_HPP
#define VWM_WAYLAND_TYPES_HPP

#include <vwm/wayland/sbo.hpp>

namespace vwm { namespace wayland {

struct fixed
{
  uint32_t value;
};

struct array
{
  //uint32_t value;

  std::size_t size () const { return 0; }
};

std::size_t marshall_size (std::string_view v)
{
  std::size_t rest = (v.size() + 1) % sizeof(std::uint32_t);
  return sizeof (std::uint32_t) + v.size() + 1 + (rest == 0 ? 0 : sizeof(std::uint32_t) - rest);
}
std::size_t marshall_size (array v)
{
  return 0;
}
std::size_t marshall_size (std::uint32_t)
{
  return sizeof(std::uint32_t);
}
void marshall_send (int fd, std::string_view v)
{
  sbo<char> buffer;
  buffer.resize (marshall_size(v));
  auto data = static_cast<char*>(buffer.data());
  std::memset (data, 0, marshall_size(v));
  std::uint32_t size = v.size() + 1;
  std::memcpy (data, &size, sizeof(size));
  std::memcpy (&data[sizeof(std::uint32_t)], v.data(), v.size());
  std::cout << "sending marshalling variadic marshal size " << marshall_size(v) << " and normal size " << size << std::endl;
  send (fd, data, marshall_size(v), 0);
}
void marshall_send (int fd, array v)
{
  std::abort();
}

template <typename Client>
void unmarshall (std::string_view& string, std::string_view payload, Client&)
{
  std::uint32_t size;
  if (payload.size() >= sizeof(size))
  {
    std::memcpy (&size, payload.data(), sizeof(size));
    if (payload.size() >= sizeof(size) + size)
    {
      string = std::string_view {payload.data() + sizeof(size), size-1 /* NUL-terminated */};
    }
  }
}

template <typename Client>
void unmarshall (int& fd, std::string_view payload, Client& c)
{
  if (c.fds.empty())
    throw std::runtime_error ("no fds avaialable in ancillary data");

  fd = c.fds.front();
  c.fds.pop_back();
}
    
unsigned unmarshall_size (std::string_view payload, std::string_view string)
{
  std::abort();
  return 0;
}
    
} }

#endif
