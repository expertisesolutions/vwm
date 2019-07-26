///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_DMABUF_HPP
#define VWM_WAYLAND_DMABUF_HPP

namespace vwm { namespace wayland {

struct dma_buffer_params
{
  int fd;
  std::uint32_t plane_index;
  std::uint32_t offset, stride, modifier_hi, modifier_lo;
};
    
struct dma_buffer
{
  std::int32_t width, height;
  std::uint32_t format, flags;

  std::vector<dma_buffer_params> params;
};

inline int32_t width (dma_buffer const& buffer)
{
  return buffer.width;
}

inline int32_t height (dma_buffer const& buffer)
{
  return buffer.height;
}
    
struct dma_params
{
  
  std::vector<dma_buffer_params> params;
};
    
} }

#endif
