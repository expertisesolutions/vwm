///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_VWM_WAYLAND_COMPOSITOR_HPP
#define VWM_VWM_WAYLAND_COMPOSITOR_HPP

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <xdg-shell-server-protocol.h>

namespace vwm { namespace wayland {

static struct wl_surface_interface const surface_impl
  {
      [] (wl_client*, wl_resource*) { std::cout << "destroy" << std::endl; }
    , [] (wl_client*, wl_resource*, wl_resource*, int32_t, int32_t)
      {
        std::cout << "attach" << std::endl;
      }
    , [] (wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t)
      {
        std::cout << "damage" << std::endl;
      }
    , [] (wl_client*, wl_resource*, uint32_t)
      {
        std::cout << "frame" << std::endl;
      }
    , [] (wl_client*, wl_resource*, wl_resource*)
      {
        std::cout << "set_opaque_region" << std::endl;
      }
    , [] (wl_client*, wl_resource*, wl_resource*)
      {
        std::cout << "set_input_region" << std::endl;
      }
    , [] (wl_client*, wl_resource*)
      {
        std::cout << "commit" << std::endl;
      }
    , [] (wl_client*, wl_resource*, int32_t)
      {
        std::cout << "set_buffer_transform" << std::endl;
      }
    , [] (wl_client*, wl_resource*, int32_t scale)
      {
        std::cout << "set_buffer_scale" << std::endl;
      }
    , [] (wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t)
      {
        std::cout << "damage_buffer" << std::endl;
      }      
  };
    
static struct xdg_surface_interface const xdg_surface_impl
  {
     [] (wl_client*, wl_resource*)
     {
       std::cout << "destroy" << std::endl;
     }
   , [] (wl_client*, wl_resource*, uint32_t)
     {
       std::cout << "get_toplevel" << std::endl;
     }
   , [] (wl_client*, wl_resource*, uint32_t, wl_resource*, wl_resource*)
     {
       std::cout << "get_popup" << std::endl;
     }
   , [] (wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t)
     {
       std::cout << "set_window_geometry" << std::endl;
     }
   , [] (wl_client*, wl_resource*, uint32_t)
     {
       std::cout << "ack_configure" << std::endl;
     }
  };

static struct xdg_wm_base_interface const xdg_impl
  {
     [] (wl_client*, wl_resource*)
     {
       std::cout << "destroy" << std::endl;
     }
   , [] (wl_client*, wl_resource*, uint32_t)
     {
       std::cout << "create_positioner" << std::endl;
     }
   , [] (wl_client* client, wl_resource*, uint32_t id, wl_resource*)
     {
       std::cout << "get_xdg_surface" << std::endl;

       uint32_t version = 1;

       auto resource = wl_resource_create(client, &xdg_surface_interface, version, id);
       wl_resource_set_implementation(resource, &xdg_surface_impl, nullptr, nullptr);
     }
   , [] (wl_client*, wl_resource*, uint32_t)
     {
       std::cout << "pong" << std::endl;
     }
   };

} }

#endif
