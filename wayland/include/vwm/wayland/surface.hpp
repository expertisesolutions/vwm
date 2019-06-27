///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_SURFACE_HPP
#define VWM_WAYLAND_SURFACE_HPP

#include <vwm/wayland/shm.hpp>

namespace vwm { namespace wayland {

struct surface
{
  std::size_t buffer_id;
  shm_buffer* buffer = nullptr;
  
  void attach (shm_buffer* buffer, std::size_t buffer_id, std::int32_t x, std::int32_t y)
  {
    this->buffer_id = buffer_id;
    this->buffer = buffer;
  }
};
    
} }

#endif
