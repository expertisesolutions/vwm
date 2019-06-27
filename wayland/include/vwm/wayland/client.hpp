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

//#include <sys/syslimits.h>
#include <fcntl.h>

#include <png.h>
#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>

namespace vwm { namespace wayland {

struct client
{
  int fd;
  sbo<char> socket_buffer;
  unsigned int buffer_first;
  unsigned int buffer_last;
  int32_t current_message_size;

  typedef ftk::ui::backend::vulkan<ftk::ui::backend::uv, ftk::ui::backend::khr_display> backend_type;
  backend_type* backend;
  ftk::ui::toplevel_window<backend_type>* toplevel;
  std::uint32_t serial;
  std::uint32_t output_id, keyboard_id, focused_surface_id;
  xkb_keymap* keymap;

  client (int fd, backend_type* backend, ftk::ui::toplevel_window<backend_type>* toplevel
          , xkb_keymap* keymap)
    : fd(fd), backend(backend), toplevel(toplevel)
    , buffer_first(0), buffer_last(0)
    , current_message_size (-1), serial (0u), output_id(0u), keyboard_id (0u)
    , keymap (keymap)
  {
    client_objects.push_back({vwm::wayland::generated::interface_::wl_display});
  }

  struct empty {};
  
  struct object
  {
    vwm::wayland::generated::interface_ interface_ = vwm::wayland::generated::interface_::empty;

    std::variant<empty, shm_pool, shm_buffer*, surface> data;
  };

  typedef std::reference_wrapper<object> object_type;

  std::vector<object> client_objects;
  std::deque<int> fds;

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

  std::uint32_t get_object_id (object* ptr)
  {
    assert (!client_objects.empty());
    return (ptr - &client_objects[0]) + 1;
  }

  void add_object(uint32_t client_id, object obj)
  {
    std::cout << "adding object with client_id " << client_id << std::endl;

    auto id = client_id - 1;
    
    if (id == client_objects.size())
      client_objects.push_back (obj);
    else if (id < client_objects.size())
    {
      if (client_objects[id].interface_ != vwm::wayland::generated::interface_::empty)
      {
        std::cout << "client_id " << client_id << " already used "
                  << (int)client_objects[client_id].interface_ << std::endl;
        throw (char)1;
      }
      else
      {
        client_objects[id] = std::move(obj);
      }
    }
    else
    {
      std::cout << "adding weird object " << client_id << " client_objects.size() " << client_objects.size() << std::endl;
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

    do
    {
      size = recvmsg(socket, &message, 0);
    }
    while (size < 0 && errno == EINTR);

    if (size < 0)
      return size;

    cmsg = CMSG_FIRSTHDR(&message);

    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(fd)))
    {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {

        std::cout << "pushing new fd" << std::endl;

        int fd;
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));

        assert (fds.empty());
        
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
      //std::cout << "no current message " << buffer_last
      //<< " dyn " << socket_buffer.is_dynamic() << std::endl;
      if (socket_buffer.size() - buffer_last < socket_buffer.static_size)
      {
        //std::cout << "growing" << std::endl;
        socket_buffer.grow();
        //std::cout << "growed" << std::endl;
      }

      auto data = static_cast<char*>(socket_buffer.data());
      assert (socket_buffer.size() - buffer_last != 0);
      //std::cout << "asking to read " << socket_buffer.size() - buffer_last << " bytes first " << buffer_first << " last " << buffer_last << std::endl;
      auto r = read_msg (fd, &data[buffer_last], socket_buffer.size() - buffer_last, 0);
      if (r < 0)
      {
        throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else if (r == 0)
      {
        //std::cout << "read 0 errno " << errno << std::endl;
        perror("");
        if (errno != EINTR)
          exit(0);
        else
          return;
      }
      else
      {
        //std::cout << "read " << r << " bytes" << std::endl;
        buffer_last += r;
      }
    }
    else
    {
      //std::cout << "current_message_size == " << current_message_size << std::endl;
      std::memmove (socket_buffer.data(), &static_cast<char*>(socket_buffer.data())[buffer_first], (buffer_last - buffer_first));
      buffer_last -= buffer_first;
      buffer_first = 0;
      //std::abort();
      socket_buffer.resize (current_message_size);

      //std::cout << "buffer_first " << buffer_first << " buffer_last " << buffer_last << std::endl;
      
      auto data = static_cast<char*>(socket_buffer.data());
      assert (socket_buffer.size() - buffer_last != 0);
      //std::cout << "asking to read " << socket_buffer.size() - buffer_last << " bytes first " << buffer_first
      //<< " last " << buffer_last << std::endl;
      auto r = read_msg (fd, &data[buffer_last], socket_buffer.size() - buffer_last, 0);
      if (r < 0)
      {
        throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else if (r == 0)
      {
        //std::cout << "read 0 errno " << errno << std::endl;
        perror("");
        if (errno != EINTR)
          exit(0);
        else
          return;
      }
      else
      {
        //std::cout << "read " << r << " bytes" << std::endl;
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

      //std::cout << "header read: from " << header.from << " opcode " << header.opcode << " size " << header.size << std::endl;
      
      if (header.size < header_size)
      {
        throw (long)1;
      }

      if (buffer_last - buffer_first >= header.size)
      {
        //std::cout << "still " << (buffer_last - buffer_first) << " bytes" << std::endl;
        std::string_view payload;
        if (header_size < header.size)
          payload = std::string_view{&data[header_size], header.size - header_size};

        //std::cout << "calling message from " << header.from << " opcode " << header.opcode << " size " << header.size << std::endl;
        server_protocol().process_message (header.from, header.opcode, payload);

        buffer_first += header.size;
        //std::cout << "buffer last " << buffer_last << " buffer_first " << buffer_first << std::endl;;
      }
      else
      {
        //std::cout << "buffer not empty" << std::endl;
        current_message_size = header.size;
        break;
      }

      if (buffer_last == buffer_first)
      {
        //std::cout << "buffer now empty" << std::endl;
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
    std::cout << "wl_display_sync " << new_id << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::empty});
    server_protocol().wl_callback_done (new_id, serial++);
    server_protocol().wl_display_delete_id (1, new_id);
  }
  
  void wl_display_get_registry (object& obj, uint32_t new_id)
  {
    std::cout << "wl_display_get_registry with new id " << new_id << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::wl_registry});
    server_protocol().wl_registry_global (new_id, 1, "wl_compositor", 4);
    server_protocol().wl_registry_global (new_id, 2, "wl_subcompositor", 1);
    server_protocol().wl_registry_global (new_id, 2, "wl_data_device_manager", 3);
    server_protocol().wl_registry_global (new_id, 3, "wl_shm", 1);
    server_protocol().wl_registry_global (new_id, 4, "wl_drm", 2);
    server_protocol().wl_registry_global (new_id, 5, "wl_seat", 5);
    server_protocol().wl_registry_global (new_id, 6, "wl_output", 3);
    server_protocol().wl_registry_global (new_id, 7, "xdg_wm_base", 2);
    server_protocol().wl_registry_global (new_id, 8, "wl_shell", 1);
  }

  void wl_registry_bind (object& obj, uint32_t global_id, std::string_view interface, uint32_t version, uint32_t new_id)
  {
    std::cout << "called bind for " << new_id << " interface |" << interface << "| " << interface.size()
              << " last byte " << (int)interface[interface.size() -1] << " version " << version  << std::endl;

    if (interface == "wl_compositor")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_compositor});
      }
    else if (interface == "wl_subcompositor")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_subcompositor});
      }
    else if (interface == "wl_data_device_manager")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_data_device_manager});
      }
    else if (interface == "wl_shm")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_shm});
        server_protocol().wl_shm_format (new_id, 0u);
        server_protocol().wl_shm_format (new_id, 1u);
        server_protocol().wl_shm_format (new_id, 909199186);
        server_protocol().wl_shm_format (new_id, 842093913);
        server_protocol().wl_shm_format (new_id, 842094158);
        server_protocol().wl_shm_format (new_id, 1448695129);
      }
    else if (interface == "wl_shell")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::wl_shell});
      }
    else if (interface == "wl_output")
      {
        output_id = new_id;
        
        add_object (new_id, {vwm::wayland::generated::interface_::wl_output});
        server_protocol().wl_output_geometry (new_id, 0u, 0u, 151, 95, 0, "unknow", "unknow", 0);
        server_protocol().wl_output_scale (new_id, 1u);
        server_protocol().wl_output_mode (new_id, 3u, 1920, 1200, 60022);
        server_protocol().wl_output_mode (new_id, 3u, 1920, 1200, 60022);
        server_protocol().wl_output_done (new_id/*, serial++*/);
      }
    else if (interface == "xdg_wm_base")
      {
        add_object (new_id, {vwm::wayland::generated::interface_::xdg_wm_base});
      }
    else if (interface == "wl_seat")
      {
        std::cout << "registering wl_seat" << std::endl;
        add_object (new_id, {vwm::wayland::generated::interface_::wl_seat});
        server_protocol().wl_seat_capabilities (new_id, 7);
        server_protocol().wl_seat_name (new_id, "default");
      }
    else
      {
        std::cout << "none of the above?" << std::endl;
      }
    
  }

  void wl_compositor_create_surface (object& obj, uint32_t new_id)
  { 
    old_focused_surface_id = focused_surface_id;
    focused_surface_id = new_id;
    add_object (new_id, {vwm::wayland::generated::interface_::wl_surface, {surface{}}});
  }

  void wl_compositor_create_region (object& obj, uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_region, {}});
  }

  void wl_shm_create_pool (object& obj, uint32_t new_id, int fd, uint32_t size)
  {
    std::cout << "create pool with new_id " << new_id << " fd " << fd << " size " << size << std::endl;

    struct stat s;
    
    if (fstat(fd, &s) < 0) {
      //close(fd);
      std::cout << "Failed to stat" << std::endl;
    }

    void* buffer = ::mmap (NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    assert (buffer != MAP_FAILED);

    assert (buffer != nullptr);
    
    add_object (new_id, {vwm::wayland::generated::interface_::wl_shm_pool, {shm_pool{fd, buffer, size}}});
  }

  void wl_shm_pool_create_buffer (object& obj, std::uint32_t new_id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format)
  {
    assert (obj.interface_ == vwm::wayland::generated::interface_::wl_shm_pool);
    if (shm_pool* pool = std::get_if<shm_pool>(&obj.data))
    {
      pool->buffers.reserve(100);
      std::cout << "offset " << offset << " width " << width << " height " << height << " stride " << stride << " format "
                << "buffers.pointer " << &pool->buffers[0]
                << " pool mmap size " << pool->mmap_size << std::endl;
      
      pool->buffers.push_back ({static_cast<char*>(pool->mmap_buffer) + offset, height*stride, offset, width, height
                                , stride, static_cast<enum format>(format)});

      assert (offset + height*stride <= pool->mmap_size);
      
      add_object (new_id, {vwm::wayland::generated::interface_::wl_buffer, {&pool->buffers.back()}});
      std::cout << "create buffer format " << format_description (static_cast<enum format>(format)) << " & " << &pool->buffers[0] << std::endl;
    }
    else
      throw -1;
  }

  void wl_shm_pool_destroy (object& obj)
  {
  }

  void wl_shm_pool_resize (object& obj, int32_t size)
  {
    if (shm_pool* pool = std::get_if<shm_pool>(&obj.data))
    {
      ::munmap (pool->mmap_buffer, pool->mmap_size);

      auto buffer = pool->mmap_buffer;
      
      pool->mmap_buffer = ::mmap (NULL, pool->mmap_size = size, PROT_READ, MAP_SHARED, pool->fd, 0);
      assert (pool->mmap_buffer != MAP_FAILED);
      std::cout << "old  " << buffer << " new " << pool->mmap_buffer << std::endl;
      
      for (auto&& buffer : pool->buffers)
      {
        buffer.mmap_buffer_offset = pool->mmap_buffer + buffer.offset;
      }
    }
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
  void wl_data_device_manager_get_data_device (object& obj, uint32_t new_id, uint32_t seat_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_data_device});
  }
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
    std::cout << "wl_surface_attach " << buffer_id << std::endl;
    if (surface* s = std::get_if<surface>(&obj.data))
    {
      object_type buffer_obj = get_object(buffer_id);
      shm_buffer** buffer = std::get_if<shm_buffer*>(&buffer_obj.get().data);

      if (buffer)
        s->attach (*buffer, buffer_id, x, y);
    }
  }
  
  void wl_surface_damage (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_surface_frame (object& obj, std::uint32_t new_id)
  {
    std::cout << "frame throttling yet" << std::endl;

    add_object (new_id, {vwm::wayland::generated::interface_::empty});
    server_protocol().wl_callback_done (new_id, serial++);
    server_protocol().wl_display_delete_id (1, new_id);
  }
  void wl_surface_set_opaque_region (object& obj, std::uint32_t) {}
  void wl_surface_set_input_region (object& obj, std::uint32_t) {}
  void wl_surface_commit (object& obj)
  {
    if (surface* s = std::get_if<surface>(&obj.data))
    {
      std::cout << "calling draw buffer " << s->buffer << std::endl;

      if (s->buffer)
      {
        // std::ofstream ofs ("abc.raw");
        // ofs.write (static_cast<const char*>(s->buffer->mmap_buffer_offset), s->buffer->buffer_size);
        FILE* fp = fopen ("abc.png", "wb");
        
        png_bytep* rows = new png_bytep[s->buffer->height];
        for (int i = 0; i != s->buffer->height; i++)
          rows[i] = static_cast<unsigned char*>(s->buffer->mmap_buffer_offset) + i*s->buffer->stride;

        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) abort();

        png_infop info = png_create_info_struct(png);
        if (!info) abort();

        png_init_io(png, fp);

        // Output is 8bit depth, RGBA format.
        png_set_IHDR(
                     png,
                     info,
                     s->buffer->width, s->buffer->height,
                     8,
                     PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT
                     );
        png_write_info(png, info);

        // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
        // Use png_set_filler().
        //png_set_filler(png, 0, PNG_FILLER_AFTER);

        //if (!row_pointers) abort();

        png_write_image(png, rows);
        png_write_end(png, NULL);

        fclose(fp);

        ftk::ui::backend::draw_buffer (*backend, *toplevel, s->buffer->mmap_buffer_offset, s->buffer->buffer_size
                                       , s->buffer->width, s->buffer->height, s->buffer->stride);

        server_protocol().wl_buffer_release (s->buffer_id);
        if (output_id)
          server_protocol().wl_surface_enter (get_object_id(&obj), output_id);
        if (focused_surface_id && keyboard_id)
        {
          array<uint32_t> keys;
          server_protocol().wl_keyboard_enter (keyboard_id, serial++, focused_surface_id, keys);
        }
      }
    }
    else
    {
      std::cout << "no surface?" << std::endl;
    }
  }
  void wl_surface_set_buffer_transform (object& obj, std::int32_t) {}
  void wl_surface_set_buffer_scale (object& obj, std::int32_t) {}
  void wl_surface_damage_buffer (object& obj, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
  void wl_seat_get_pointer (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_pointer});
  }
  void wl_seat_get_keyboard (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_pointer});

    keyboard_id = new_id;
    char* keymap_string = xkb_keymap_get_as_string (keymap, XKB_KEYMAP_FORMAT_TEXT_V1);

    int fd = ::memfd_create ("keymap_shared", MFD_CLOEXEC);
    ::ftruncate (fd, strlen(keymap_string));
    void* p = ::mmap (NULL, strlen(keymap_string), PROT_WRITE, MAP_SHARED, fd, 0);
    memcpy (p, keymap_string, strlen(keymap_string));

    std::cout << "sending keymap" << std::endl;
    server_protocol().wl_keyboard_keymap (new_id, 1 /* XKB_V1 */, fd, strlen(keymap_string));
    std::cout << "sent keymap" << std::endl;
  }
  void send_key (std::uint32_t time, std::uint32_t key, std::uint32_t state)
  {
    if (keyboard_id)
      server_protocol().wl_keyboard_key (keyboard_id, serial++, time, key, state);
  }
  void wl_seat_get_touch (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_pointer});
  }
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
  void xdg_wm_base_get_xdg_surface (object& obj, std::uint32_t new_id, std::uint32_t surface)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::xdg_surface});
    server_protocol().xdg_surface_configure (new_id, serial++);
  }
  void xdg_wm_base_pong (object& obj, std::uint32_t serial) {}
  void xdg_positioner_destroy (object& obj) {}
  void xdg_positioner_set_size (object& obj, std::int32_t width, std::int32_t height) {}
  void xdg_positioner_set_anchor_rect (object& obj, std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h) {}
  void xdg_positioner_set_anchor (object& obj, std::uint32_t anchor) {}
  void xdg_positioner_set_gravity (object& obj, std::uint32_t gravity) {}
  void xdg_positioner_set_constraint_adjustment (object& obj, std::uint32_t arg0) {}
  void xdg_positioner_set_offset (object& obj, std::int32_t arg0, std::int32_t arg1) {}
  void xdg_surface_destroy (object& obj) {}
  void xdg_surface_get_toplevel (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::xdg_toplevel});
    //server_protocol().xdg_toplevel_configure (0, 0, );
  }
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
