///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_CLIENT_HPP
#define VWM_WAYLAND_CLIENT_HPP

#include <vwm/wayland/sbo.hpp>
#include <vwm/wayland/types.hpp>

#include "../test.h"

namespace vwm { namespace wayland {

struct client
{
  int fd;
  sbo<char> socket_buffer;
  unsigned int buffer_first;
  unsigned int buffer_last;
  int32_t current_message_size;

  sbo<uint32_t, 2> registry_ids;
  sbo<uint32_t, 2> some_other_ids;

  struct object_type
  {
    vwm::wayland::generated::interface_ interface_ = vwm::wayland::generated::interface_::empty;
  };

  std::vector<object_type> client_objects;

  client (int fd)
    : fd(fd), socket_buffer()
    , buffer_first(0), buffer_last(0)
    , current_message_size (-1)
  {
    client_objects.push_back({vwm::wayland::generated::interface_::wl_display});
  }

  int get_fd() const { return fd; }

  object_type get_object(uint32_t client_id) const
  {
    if (client_id > client_objects.size())
      throw 1.0f;

    auto object = client_objects[client_id - 1];
    
    if (object.interface_ == vwm::wayland::generated::interface_::empty)
      throw -1.0;
    return object;
  }

  void add_object(uint32_t client_id, object_type obj)
  {
    if (client_id == client_objects.size() + 1)
      client_objects.push_back (obj);
    else if (client_id < client_objects.size() + 1)
    {
      if (client_objects[client_id].interface_ != vwm::wayland::generated::interface_::empty)
        throw (char)1;
    }
    else
    {
      std::cout << "adding weird object " << client_id << std::endl;
      throw -1;
    }
  }

  void read()
  {
    static const std::uint32_t header_size = sizeof(uint32_t) * 2;
    
    if (current_message_size < 0) // no known needed data
    {
      std::cout << "no current message " << buffer_last << std::endl;
      if (socket_buffer.size() - buffer_last < socket_buffer.static_size)
      {
        std::cout << "growing" << std::endl;
        socket_buffer.grow();
      }

      auto data = static_cast<char*>(socket_buffer.data());
      
      auto r = recv (fd, &data[buffer_last], socket_buffer.size() - buffer_last, 0);
      if (r < 0)
      {
        throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else
      {
        std::cout << "read " << r << " bytes" << std::endl;
        buffer_last += r;
      }
    }
    else
    {
      std::abort();
    }

    while (buffer_last - buffer_first >= header_size)
    {
      struct header {
        uint32_t from;
        uint16_t opcode;
        uint16_t size;
      } header;

      auto data = static_cast<char*>(socket_buffer.data()) + buffer_first;
      std::memcpy(&header, data, sizeof(header));

      std::cout << "header read: from " << header.from << " opcode " << header.opcode << " size " << header.size << std::endl;
      
      if (header.size < header_size)
      {
        throw (long)1;
      }

      if (buffer_last - buffer_first >= header.size)
      {
        std::cout << "still " << (buffer_last - buffer_first) << " bytes" << std::endl;
        std::string_view payload;
        if (header_size < header.size)
          payload = std::string_view{&data[header_size], header.size - header_size};

        std::cout << "calling message from " << header.from << " opcode " << header.opcode << " size " << header.size << std::endl;
        server_protocol().process_message (header.from, header.opcode, payload);

        buffer_first += header.size;
        std::cout << "buffer last " << buffer_last << " buffer_first " << buffer_first << std::endl;;
      }
      else
      {
        std::cout << "buffer not empty" << std::endl;
        current_message_size = header.size;
        break;
      }

      if (buffer_last == buffer_first)
      {
        std::cout << "buffer now empty" << std::endl;
        buffer_first = buffer_last = 0;
      }
      current_message_size = -1;
    }
      
    
    // // registry_get
    // std::cout << "read " << r << std::endl;
    // std::cout << "this " <<  << std::endl;
    // std::cout << "opcode " << *(uint16_t*)&((const char*)data)[4] << std::endl;
    // std::cout << "size " << *(uint16_t*)&((const char*)data)[6] << std::endl;
    // std::cout << "first arg " << *(uint32_t*)&((const char*)data)[8] << std::endl;

    // auto size = *(uint16_t*)&((const char*)data)[6];

    // // sync
    // std::memmove (data, &data[size], socket_buffer.size() - size);
    // std::cout << "this " << *(uint32_t*)data << std::endl;
    // std::cout << "opcode " << *(uint16_t*)&((const char*)data)[4] << std::endl;
    // std::cout << "size " << *(uint16_t*)&((const char*)data)[6] << std::endl;
    // std::cout << "first arg " << *(uint32_t*)&((const char*)data)[8] << std::endl;

    // // {
    // //   const char 
      
    // //   std::uint32_t buffer []
    // //     = {
    // //        2, (12 << 16) + 1, /*name*/0, /*interface*/2
    // //       };
    // // }

    // {
    //   // std::uint32_t sync_buffer [] =
    //   //   {
    //   //    2, 12 << 16, 1 /* name ? */, "wl_compositor", 3 /*version*/
    //   //   };
    //   // send (fd, sync_buffer, sizeof(sync_buffer), 0);
    // }
    
    // const int flags = fcntl(fd, F_GETFL, 0);
    // fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    
    // r = recv (fd, socket_buffer.data(), socket_buffer.size(), 0);
    // std::cout << "read " << r << std::endl;
    // assert (r >= 8);

    // // registry_get
    // std::cout << "this " << *(uint32_t*)data << std::endl;
    // std::cout << "opcode " << *(uint16_t*)&((const char*)data)[4] << std::endl;
    // std::cout << "size " << *(uint16_t*)&((const char*)data)[6] << std::endl;
    // std::cout << "first arg " << *(uint32_t*)&((const char*)data)[8] << std::endl;

    // size = *(uint16_t*)&((const char*)data)[6];

    // // sync
    // std::memmove (data, &data[size], socket_buffer.size() - size);
    // std::cout << "this " << *(uint32_t*)data << std::endl;
    // std::cout << "opcode " << *(uint16_t*)&((const char*)data)[4] << std::endl;
    // std::cout << "size " << *(uint16_t*)&((const char*)data)[6] << std::endl;
    // std::cout << "first arg " << *(uint32_t*)&((const char*)data)[8] << std::endl;
  }

  constexpr vwm::wayland::generated::server_protocol<client>& server_protocol()
  {
    return static_cast<vwm::wayland::generated::server_protocol<client>&>(*this);
  }
  constexpr vwm::wayland::generated::server_protocol<client> const& server_protocol() const
  {
    return static_cast<vwm::wayland::generated::server_protocol<client> const &>(*this);
  }

  void wl_display_sync (object_type obj, uint32_t new_id)
  {
    std::cout << "wl_display_sync" << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::wl_callback});
    server_protocol().wl_callback_done (new_id, 0);
  }
  
  void wl_display_get_registry (object_type obj, uint32_t new_id)
  {
    std::cout << "wl_display_get_registry" << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::wl_registry});
    server_protocol().wl_registry_global (new_id, 1, "wl_compositor", 4);
    server_protocol().wl_registry_global (new_id, 2, "wl_shm", 1);
    server_protocol().wl_registry_global (new_id, 3, "wl_shell", 1);
    server_protocol().wl_registry_global (new_id, 4, "wl_output", 3);
    server_protocol().wl_registry_global (new_id, 5, "xdg_wm_base", 2);
  }

  void wl_registry_bind (object_type obj, uint32_t global_id, uint32_t new_id)
  {
    std::cout << "called bind?" << std::endl;
  }

  void wl_compositor_create_surface (object_type obj, uint32_t new_id)
  {
  }

  void wl_compositor_create_region (object_type obj, uint32_t new_id)
  {
  }

  void wl_shm_pool_create_buffer (object_type obj, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format, uint32_t new_id)
  {
  }

  void wl_shm_pool_destroy (object_type obj)
  {
  }

  void wl_shm_pool_resize (object_type obj, int32_t size)
  {
  }

  void wl_buffer_destroy (object_type obj)
  {
  }

  void wl_data_offer_destroy (object_type obj)
  {
  }

  void wl_data_offer_finish (object_type obj)
  {
  }

  void wl_data_offer_set_actions (object_type obj, uint32_t, uint32_t) {}
  void wl_data_source_set_actions (object_type obj, uint32_t) {}
  void wl_data_source_destroy (object_type obj) {}
  void wl_data_device_start_drag (object_type obj, uint32_t, uint32_t, uint32_t, uint32_t) {}
  void wl_data_device_set_selection (object_type obj, uint32_t, uint32_t) {}
  void wl_data_device_release (object_type obj) {}
  void wl_data_device_manager_get_data_device (object_type obj, uint32_t, uint32_t) {}
  void wl_data_device_manager_create_data_source (object_type obj, uint32_t) {}
  void wl_shell_get_shell_surface (object_type obj, uint32_t, uint32_t) {}
  void wl_shell_surface_pong (object_type obj, std::uint32_t) {}
  void wl_shell_surface_move (object_type obj, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_resize (object_type obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_set_toplevel (object_type obj) {}
  void wl_shell_surface_set_transient (object_type obj, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_shell_surface_set_fullscreen (object_type obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_set_popup (object_type obj, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_shell_surface_set_maximized (object_type obj, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_surface_destroy (object_type obj) {}
  void wl_surface_attach (object_type obj, std::uint32_t, std::int32_t, std::int32_t) {}
  void wl_surface_damage (object_type obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_surface_frame (object_type obj, std::uint32_t) {}
  void wl_surface_set_opaque_region (object_type obj, std::uint32_t) {}
  void wl_surface_set_input_region (object_type obj, std::uint32_t) {}
  void wl_surface_commit (object_type obj) {}
  void wl_surface_set_buffer_transform (object_type obj, std::int32_t) {}
  void wl_surface_set_buffer_scale (object_type obj, std::int32_t) {}
  void wl_surface_damage_buffer (object_type obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_seat_get_pointer (object_type obj, std::uint32_t) {}
  void wl_seat_get_keyboard (object_type obj, std::uint32_t) {}
  void wl_seat_get_touch (object_type obj, std::uint32_t) {}
  void wl_seat_release (object_type obj) {}
  void wl_pointer_set_cursor (object_type obj, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t) {}
  void wl_pointer_release (object_type obj) {}
  void wl_keyboard_release (object_type obj) {}
  void wl_touch_release (object_type obj) {}
  void wl_output_release (object_type obj) {}
  void wl_region_destroy (object_type obj) {}
  void wl_region_add (object_type obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_region_subtract (object_type obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_subcompositor_destroy (object_type obj) {}
  void wl_subcompositor_get_subsurface (object_type obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_subsurface_destroy (object_type obj) {}
  void wl_subsurface_set_position (object_type obj, std::int32_t, std::int32_t) {}
  void wl_subsurface_place_above (object_type obj, std::uint32_t) {}
  void wl_subsurface_place_below (object_type obj, std::uint32_t) {}
  void wl_subsurface_set_sync (object_type obj) {}
  void wl_subsurface_set_desync (object_type obj) {}
  void wl_shell_surface_set_maximized (object_type obj, std::uint32_t) {}
};
    
} }

#endif
