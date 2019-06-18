///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_SHM_HPP
#define VWM_WAYLAND_SHM_HPP

#include <vwm/wayland/format.hpp>

namespace vwm { namespace wayland {

struct shm_buffer
{
  void* mmap_buffer_offset;
  std::size_t buffer_size;
  int32_t offset;
  int32_t width, height, stride;
  enum format format;
};

struct shm_pool
{
  int fd;
  void* mmap_buffer;
  std::size_t mmap_size;

  std::vector<shm_buffer> buffers;
};
    
} }

#endif
