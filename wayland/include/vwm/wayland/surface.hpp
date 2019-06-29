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
#include <vwm/wayland/dmabuf.hpp>

namespace vwm { namespace wayland {

template <typename Token>
struct surface
{
  std::size_t buffer_id;
  std::variant<shm_buffer*, dma_buffer> buffer = nullptr;
  Token token;
  bool loaded = false;
  bool failed = false;
  
  void set_attachment (shm_buffer* buffer, std::size_t buffer_id, Token token, std::int32_t x, std::int32_t y)
  {
    this->buffer_id = buffer_id;
    this->buffer = buffer;
    this->token = token;
  }

  void set_attachment (dma_buffer buffer, std::size_t buffer_id, Token token, std::int32_t x, std::int32_t y)
  {
    this->buffer_id = buffer_id;
    this->buffer = std::move(buffer);
  }
};
    
} }

#endif
