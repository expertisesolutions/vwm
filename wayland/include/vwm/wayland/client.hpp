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
#include <vwm/wayland/shm.hpp>
#include <vwm/wayland/surface.hpp>

#include "../test.h"

#include <deque>
#include <vector>

#include <sys/mman.h>

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

  struct empty {};
  
  struct object
  {
    vwm::wayland::generated::interface_ interface_ = vwm::wayland::generated::interface_::empty;

    std::variant<empty, shm_pool, shm_buffer*, surface> data;
  };

  typedef std::reference_wrapper<object> object_type;

  std::vector<object> client_objects;
  std::deque<int> fds;

  client (int fd)
    : fd(fd), socket_buffer()
    , buffer_first(0), buffer_last(0)
    , current_message_size (-1)
  {
    client_objects.push_back({vwm::wayland::generated::interface_::wl_display});
  }

  int get_fd() const { return fd; }

  static vwm::wayland::generated::interface_ get_interface(object_type obj)
  {
    return obj.get().interface_;
  }
  
  object_type get_object(uint32_t client_id)
  {
    if (client_id > client_objects.size())
    {
      std::cout << "object " << client_id << " not found" << std::endl;
      throw 1.0f;
    }

    object_type object = client_objects[client_id - 1];
    
    if (object.get().interface_ == vwm::wayland::generated::interface_::empty)
    {
      std::cout << "object is empty" << std::endl;
      throw -1.0;
    }
    return object;
  }

  void add_object(uint32_t client_id, object obj)
  {
    std::cout << "adding object with client_id " << client_id << std::endl;
    
    if (client_id == client_objects.size() + 1)
      client_objects.push_back (obj);
    else if (client_id < client_objects.size() + 1)
    {
      if (client_objects[client_id].interface_ != vwm::wayland::generated::interface_::empty)
      {
        std::cout << "client_id " << client_id << " already used" << std::endl;
        throw (char)1;
      }
    }
    else
    {
      std::cout << "adding weird object " << client_id << " client_objects.size() + 1 " << client_objects.size() + 1 << std::endl;
      throw -1;
    }
  }

  ssize_t read_msg (int socket, void* buffer, size_t len, int flags)
  {
    ssize_t size;
    char control[CMSG_SPACE(sizeof(fd))];
    struct iovec iov = {
		.iov_base = buffer,
		.iov_len = len,
    };
    struct msghdr message = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = &control,
		.msg_controllen = sizeof(control),
    };
    struct cmsghdr *cmsg;

    size = recvmsg(socket, &message, 0);
    if (size < 0)
      return size;

    cmsg = CMSG_FIRSTHDR(&message);

    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(fd)))
    {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {

        int fd;
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
        fds.push_back(fd);
      }
    }
    return size;
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
        std::cout << "growed" << std::endl;
      }

      auto data = static_cast<char*>(socket_buffer.data());
      assert (socket_buffer.size() - buffer_last != 0);
      std::cout << "asking to read " << socket_buffer.size() - buffer_last << " bytes first " << buffer_first
                << " last " << buffer_last << std::endl;
      auto r = read_msg (fd, &data[buffer_last], socket_buffer.size() - buffer_last, 0);
      if (r < 0)
      {
        throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else if (r == 0)
      {
        std::cout << "read 0 errno " << errno << std::endl;
        perror("");
        if (errno != EINTR)
          exit(0);
        else
          return;
      }
      else
      {
        std::cout << "read " << r << " bytes" << std::endl;
        buffer_last += r;
      }
    }
    else
    {
      std::cout << "current_message_size == " << current_message_size << std::endl;
      std::memmove (socket_buffer.data(), &static_cast<char*>(socket_buffer.data())[buffer_first], (buffer_last - buffer_first));
      buffer_last -= buffer_first;
      buffer_first = 0;
      //std::abort();
      socket_buffer.resize (current_message_size);

      std::cout << "buffer_first " << buffer_first << " buffer_last " << buffer_last << std::endl;
      
      auto data = static_cast<char*>(socket_buffer.data());
      assert (socket_buffer.size() - buffer_last != 0);
      std::cout << "asking to read " << socket_buffer.size() - buffer_last << " bytes first " << buffer_first
                << " last " << buffer_last << std::endl;
      auto r = read_msg (fd, &data[buffer_last], socket_buffer.size() - buffer_last, 0);
      if (r < 0)
      {
        throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else if (r == 0)
      {
        std::cout << "read 0 errno " << errno << std::endl;
        perror("");
        if (errno != EINTR)
          exit(0);
        else
          return;
      }
      else
      {
        std::cout << "read " << r << " bytes" << std::endl;
        buffer_last += r;
      }
      
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
        socket_buffer.compact();
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

  void wl_display_sync (object& obj, uint32_t new_id)
  {
    std::cout << "wl_display_sync" << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::wl_callback});
    server_protocol().wl_callback_done (new_id, 0);
  }
  
  void wl_display_get_registry (object& obj, uint32_t new_id)
  {
    std::cout << "wl_display_get_registry with new id " << new_id << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::wl_registry});
    server_protocol().wl_registry_global (new_id, 1, "wl_compositor", 4);
    server_protocol().wl_registry_global (new_id, 2, "wl_shm", 1);
    server_protocol().wl_registry_global (new_id, 3, "wl_seat", 6);
    server_protocol().wl_registry_global (new_id, 4, "wl_shell", 1);
    server_protocol().wl_registry_global (new_id, 5, "wl_output", 3);
    server_protocol().wl_registry_global (new_id, 6, "xdg_wm_base", 2);
  }

  void wl_registry_bind (object& obj, uint32_t global_id, std::string_view interface, uint32_t version, uint32_t new_id)
  {
    std::cout << "called bind for " << new_id << " interface |" << interface << "| " << interface.size()
              << " last byte " << (int)interface[interface.size() -1] << " version " << version  << std::endl;

    if (interface == "wl_compositor")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_compositor});
      }
    else if (interface == "wl_shm")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_shm});
      }
    else if (interface == "wl_shell")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_shell});
      }
    else if (interface == "wl_output")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_output});
      }
    else if (interface == "xdg_wm_base")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_callback});
      }
    else if (interface == "wl_seat")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_seat});
      }
    else
      {
        std::cout << "none of the above?" << std::endl;
      }
    
  }

  void wl_compositor_create_surface (object& obj, uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_surface});
  }

  void wl_compositor_create_region (object& obj, uint32_t new_id)
  {
  }

  void wl_shm_create_pool (object& obj, uint32_t new_id, int fd, uint32_t size)
  {
    std::cout << "create pool with new_id " << new_id << " fd " << fd << std::endl;
    void* buffer = ::mmap (NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    assert (buffer != nullptr);
    
    add_object (new_id, {vwm::wayland::generated::interface_::wl_shm_pool, {shm_pool{fd, buffer, size}}});
  }

  void wl_shm_pool_create_buffer (object& obj, std::uint32_t new_id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
  {
    if (shm_pool* pool = std::get_if<shm_pool>(&obj.data))
    {
      pool->buffers.push_back ({static_cast<char*>(pool->mmap_buffer) + offset, 0u, offset, width, height
                                , stride, static_cast<enum format>(format)});
      add_object (new_id, {vwm::wayland::generated::interface_::wl_buffer, {&pool->buffers.back()}});
    }
  }

  void wl_shm_pool_destroy (object& obj)
  {
  }

  void wl_shm_pool_resize (object& obj, int32_t size)
  {
  }

  void wl_buffer_destroy (object& obj)
  {
  }

  void wl_data_offer_accept (object& obj, std::uint32_t arg0, std::string_view arg1)
  {
  }

  void wl_data_offer_receive (object& obj, std::string_view arg0, int arg1)
  {
  }

  void wl_data_offer_destroy (object& obj)
  {
  }

  void wl_data_offer_finish (object& obj)
  {
  }

  void wl_data_offer_set_actions (object& obj, uint32_t, uint32_t) {}
  void wl_data_source_set_actions (object& obj, uint32_t) {}
  void wl_data_source_offer (object& obj, std::string_view) {}
  void wl_data_source_destroy (object& obj) {}
  void wl_data_device_start_drag (object& obj, uint32_t, uint32_t, uint32_t, uint32_t) {}
  void wl_data_device_set_selection (object& obj, uint32_t, uint32_t) {}
  void wl_data_device_release (object& obj) {}
  void wl_data_device_manager_get_data_device (object& obj, uint32_t, uint32_t) {}
  void wl_data_device_manager_create_data_source (object& obj, uint32_t) {}
  void wl_shell_get_shell_surface (object& obj, uint32_t new_id, uint32_t surface)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_shell_surface});
  }
  void wl_shell_surface_pong (object& obj, std::uint32_t) {}
  void wl_shell_surface_move (object& obj, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_resize (object& obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_set_toplevel (object& obj) {}
  void wl_shell_surface_set_transient (object& obj, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_shell_surface_set_fullscreen (object& obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_shell_surface_set_popup (object& obj, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_shell_surface_set_maximized (object& obj, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::uint32_t) {}
  void wl_surface_destroy (object& obj) {}
  
  void wl_surface_attach (object& obj, std::uint32_t buffer_id, std::int32_t x, std::int32_t y)
  {
    if (surface* s = std::get_if<surface>(&obj.data))
    {
      object_type buffer_obj = get_object(buffer_id);
      shm_buffer** buffer = std::get_if<shm_buffer*>(&buffer_obj.get().data);

      if (buffer)
        s->attach (*buffer, x, y);
    }
  }
  
  void wl_surface_damage (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_surface_frame (object& obj, std::uint32_t) {}
  void wl_surface_set_opaque_region (object& obj, std::uint32_t) {}
  void wl_surface_set_input_region (object& obj, std::uint32_t) {}
  void wl_surface_commit (object& obj) {}
  void wl_surface_set_buffer_transform (object& obj, std::int32_t) {}
  void wl_surface_set_buffer_scale (object& obj, std::int32_t) {}
  void wl_surface_damage_buffer (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_seat_get_pointer (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_pointer});
  }
  void wl_seat_get_keyboard (object& obj, std::uint32_t) {}
  void wl_seat_get_touch (object& obj, std::uint32_t) {}
  void wl_seat_release (object& obj) {}
  void wl_pointer_set_cursor (object& obj, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t) {}
  void wl_pointer_release (object& obj) {}
  void wl_keyboard_release (object& obj) {}
  void wl_touch_release (object& obj) {}
  void wl_output_release (object& obj) {}
  void wl_region_destroy (object& obj) {}
  void wl_region_add (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_region_subtract (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_subcompositor_destroy (object& obj) {}
  void wl_subcompositor_get_subsurface (object& obj, std::uint32_t, std::uint32_t, std::uint32_t) {}
  void wl_subsurface_destroy (object& obj) {}
  void wl_subsurface_set_position (object& obj, std::int32_t, std::int32_t) {}
  void wl_subsurface_place_above (object& obj, std::uint32_t) {}
  void wl_subsurface_place_below (object& obj, std::uint32_t) {}
  void wl_subsurface_set_sync (object& obj) {}
  void wl_subsurface_set_desync (object& obj) {}
  void wl_shell_surface_set_maximized (object& obj, std::uint32_t) {}
  void wl_shell_surface_set_title (object& obj, std::string_view title) {}
  void wl_shell_surface_set_class (object& obj, std::string_view arg0) {}
  void xdg_wm_base_destroy (object& obj) {}
  void xdg_wm_base_create_positioner (object& obj, std::uint32_t new_id) {}
  void xdg_wm_base_get_xdg_surface (object& obj, std::uint32_t new_id, std::uint32_t surface) {}
  void xdg_wm_base_pong (object& obj, std::uint32_t serial) {}
  void xdg_positioner_destroy (object& obj) {}
  void xdg_positioner_set_size (object& obj, std::int32_t width, std::int32_t height) {}
  void xdg_positioner_set_anchor_rect (object& obj, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) {}
  void xdg_positioner_set_anchor (object& obj, std::uint32_t anchor) {}
  void xdg_positioner_set_gravity (object& obj, std::uint32_t gravity) {}
  void xdg_positioner_set_constraint_adjustment (object& obj, std::uint32_t arg0) {}
  void xdg_positioner_set_offset (object& obj, std::int32_t arg0, std::int32_t arg1) {}
  void xdg_surface_destroy (object& obj) {}
  void xdg_surface_get_toplevel (object& obj, std::uint32_t arg0) {}
  void xdg_surface_get_popup(object& obj, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {}
  void xdg_surface_set_window_geometry(object& obj, std::int32_t arg0, std::int32_t arg1, std::int32_t arg2, std::int32_t) {}
  void xdg_surface_ack_configure(object& obj, std::uint32_t arg0) {}
  void xdg_toplevel_destroy(object& obj) {}
  void xdg_toplevel_set_parent(object& obj, std::uint32_t arg0) {}
  void xdg_toplevel_set_title(object& obj, std::string_view arg0) {}
  void xdg_toplevel_set_app_id(object& obj, std::string_view arg0) {}
  void xdg_toplevel_show_window_menu(object& obj, std::uint32_t arg0, std::uint32_t arg1, std::int32_t arg2, std::int32_t arg3) {}
  void xdg_toplevel_move(object& obj, std::uint32_t arg0, std::uint32_t arg1) {}
  void xdg_toplevel_resize(object& obj, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t arg2) {}
  void xdg_toplevel_set_max_size(object& obj, std::int32_t arg0, std::int32_t arg1) {}
  void xdg_toplevel_set_min_size(object& obj, std::int32_t arg0, std::int32_t arg1) {}
  void xdg_toplevel_set_maximized(object& obj) {}
  void xdg_toplevel_unset_maximized (object& obj) {}
  void xdg_toplevel_set_fullscreen(object& obj, std::uint32_t arg0) {}
  void xdg_toplevel_unset_fullscreen(object& obj) {}
  void xdg_toplevel_set_minimized(object& obj) {}
  void xdg_popup_destroy(object& obj) {}
  void xdg_popup_grab(object& obj, std::uint32_t arg0, std::uint32_t arg1) {}
};
    
} }

#endif
