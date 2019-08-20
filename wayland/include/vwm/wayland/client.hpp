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
#include <vwm/wayland/drm.hpp>
#include <vwm/wayland/dmabuf.hpp>

#include <ftk/ui/backend/vulkan_load.hpp>

#include "../test.h"

#include <deque>
#include <vector>

#include <sys/mman.h>

//#include <sys/syslimits.h>
#include <fcntl.h>

#include <png.h>
#include <xkbcommon/xkbcommon.h>

#include <sys/mman.h>
#include <stropts.h>
#include <drm_fourcc.h>

namespace vwm { namespace wayland {

template <typename Keyboard, typename Executor, typename WindowingBase>
struct client
{
  int fd;
  sbo<char> socket_buffer;
  unsigned int buffer_first;
  unsigned int buffer_last;
  int32_t current_message_size;

  typedef ftk::ui::backend::vulkan<ftk::ui::backend::uv, WindowingBase> backend_type;
  uv_loop_t* loop;
  backend_type* backend;
  ftk::ui::toplevel_window<backend_type>* toplevel;
  std::uint32_t serial;
  std::uint32_t output_id, keyboard_id, focused_surface_id, old_focused_surface_id;
  std::uint32_t last_surface_entered_id;
  Keyboard* keyboard;
  std::function<void()> render_dirty;
  ftk::ui::backend::vulkan_image_loader<Executor>* image_loader;
  std::mutex* render_mutex;
  std::int32_t surface_start_x = 0, surface_start_y = 0;
  std::int32_t surface_start_x_offset = 30, surface_start_y_offset = 30;

  using token_type = pc::future<ftk::ui::backend::vulkan_image>;
  using surface_type = surface<token_type, typename ftk::ui::toplevel_window<backend_type>::image_iterator>;

  client (int fd, uv_loop_t* loop, backend_type* backend, ftk::ui::toplevel_window<backend_type>* toplevel
          , Keyboard* keyboard, std::function<void()> render_dirty
          , ftk::ui::backend::vulkan_image_loader<Executor>* image_loader
          , std::mutex* render_mutex
          , std::int32_t surface_start_x = 0, std::int32_t surface_start_y = 0)
    : fd(fd), buffer_first(0), buffer_last(0)
    , current_message_size (-1), loop(loop), backend(backend), toplevel(toplevel), serial (0u), output_id(0u), keyboard_id (0u)
    , old_focused_surface_id (0u), last_surface_entered_id (0u)
    , keyboard (keyboard), render_dirty (render_dirty), image_loader (image_loader)
    , render_mutex (render_mutex), surface_start_x (surface_start_x)
    , surface_start_y (surface_start_y)
  {
    std::cout << "keyboard " << keyboard << std::endl;
    client_objects.push_back({vwm::wayland::generated::interface_::wl_display});
  }

  struct empty {};
  
  struct object
  {
    vwm::wayland::generated::interface_ interface_ = vwm::wayland::generated::interface_::empty;

    std::variant<empty, shm_pool, shm_buffer*, surface_type, drm, dma_buffer, dma_params> data;
  };

  typedef std::reference_wrapper<object> object_type;

  std::vector<object> client_objects;
  std::deque<int> fds;
  std::vector<fastdraw::output::vulkan::vulkan_draw_info> vulkan_draws;

  void pin()
  {
  }

  void unpin()
  {
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
      client_objects.push_back (std::move(obj));
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

    if (cmsg)
      std::cout << "Has msg and len is " << cmsg->cmsg_len << " should be " << CMSG_LEN(sizeof(fd))
                << " has level " << cmsg->cmsg_level << " type " << cmsg->cmsg_type
                << " should have level " << SOL_SOCKET << " type " << SCM_RIGHTS << std::endl;

    if (cmsg && cmsg->cmsg_len >= CMSG_LEN(sizeof(fd)))
    {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {

        unsigned int rest = cmsg->cmsg_len - CMSG_LEN(0);
        unsigned int off = 0;

        std::cout << "rest " << rest << std::endl;

        while (rest >= sizeof(fd))
        {
          std::cout << "rest " << rest << std::endl;
          std::cout << "pushing new fd" << std::endl;

          int fd;
          memcpy(&fd, &CMSG_DATA(cmsg)[off], sizeof(fd));
          
          fds.push_back(fd);
          rest -= sizeof(fd);
          off += sizeof(fd);
        }
      }
    }
    return size;
  }

  void connection_drop (std::error_code ec)
  {
    std::cout << "connection_drop " << ec.message() << std::endl;
    for (auto&& object : client_objects)
    {
      if (auto* s = std::get_if<surface_type>(&object.data))
      {
        std::cout << "removing surface" << std::endl;
        if (s->render_token)
        {
          std::unique_lock<std::mutex> l(*render_mutex);
          //toplevel->images.erase (*s->render_token);
          toplevel->remove_image (*s->render_token);
          std::int32_t w, h;
          std::visit ([&w, &h] (auto&& buffer)
                      {
                        w = width (buffer);
                        h = height (buffer);
                      }, s->buffer);
          std::cout << "pushed damage region " << s->pos_x << 'x' << s->pos_y
                    << "-" << w << "x" << h << std::endl;
          toplevel->framebuffers_damaged_regions[0].push_back({s->pos_x, s->pos_y, w, h});
          toplevel->framebuffers_damaged_regions[1].push_back({s->pos_x, s->pos_y, w, h});
          l.unlock();
        }
      }
    }
    render_dirty();
    //close (fd);
    throw std::system_error (ec);
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
        connection_drop (std::error_code(errno, std::system_category()));
        //throw std::system_error( std::error_code (errno, std::system_category()));
      }
      else if (r == 0)
      {
        std::cout << "read 0 errno " << errno << std::endl;
        //perror("");
        if (errno != EINTR)
          //exit(0);
          connection_drop (std::error_code(errno, std::system_category()));
        //throw std::system_error( std::error_code (errno, std::system_category()));
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
        connection_drop (std::error_code(errno, std::system_category()));
        //throw std::system_error( std::error_code (errno, std::system_category()));
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
    server_protocol().wl_registry_global (new_id, 1,  "wl_compositor", 4);
    server_protocol().wl_registry_global (new_id, 2,  "wl_subcompositor", 1);
    server_protocol().wl_registry_global (new_id, 2,  "wl_data_device_manager", 3);
    server_protocol().wl_registry_global (new_id, 3,  "wl_shm", 1);
    server_protocol().wl_registry_global (new_id, 4,  "wl_drm", 2);
    server_protocol().wl_registry_global (new_id, 5,  "wl_seat", 5);
    server_protocol().wl_registry_global (new_id, 6,  "wl_output", 3);
    server_protocol().wl_registry_global (new_id, 7,  "xdg_wm_base", 2);
    server_protocol().wl_registry_global (new_id, 8,  "wl_shell", 1);
    server_protocol().wl_registry_global (new_id, 9,  "zwp_linux_dmabuf_v1", 3);
    server_protocol().wl_registry_global (new_id, 10, "zwp_linux_explicit_synchronization_v1", 2);
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
        // server_protocol().wl_shm_format (new_id, 909199186);
        // server_protocol().wl_shm_format (new_id, 842093913);
        // server_protocol().wl_shm_format (new_id, 842094158);
        // server_protocol().wl_shm_format (new_id, 1448695129);
      }
    else if (interface == "wl_drm")
      {
        int fd = open("/dev/dri/renderD128", O_RDWR);
        
        add_object (new_id, {vwm::wayland::generated::interface_::wl_drm, {wayland::drm{fd}}});

        server_protocol().wl_drm_device (new_id, "/dev/dri/renderD128");
        // server_protocol().wl_drm_format (new_id, 0x34325241);
        server_protocol().wl_drm_format (new_id,  DRM_FORMAT_BGRX8888);
        // server_protocol().wl_drm_format (new_id, 875713089);
        // server_protocol().wl_drm_format (new_id, 875713112);
        // server_protocol().wl_drm_format (new_id, 909199186);
        // server_protocol().wl_drm_format (new_id, 961959257);
        // server_protocol().wl_drm_format (new_id, 825316697);
        // server_protocol().wl_drm_format (new_id, 842093913);
        // server_protocol().wl_drm_format (new_id, 909202777);
        // server_protocol().wl_drm_format (new_id, 875713881);
        // server_protocol().wl_drm_format (new_id, 842094158);
        // server_protocol().wl_drm_format (new_id, 909203022);
        // server_protocol().wl_drm_format (new_id, 1448695129);
        server_protocol().wl_drm_capabilities (new_id, 1);
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
    else if (interface == "zwp_linux_dmabuf_v1")
    {
      add_object (new_id, {vwm::wayland::generated::interface_::zwp_linux_dmabuf_v1});

      // formats
      //      server_protocol().zwp_linux_dmabuf_v1_format (new_id, 0x34325241);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 875713089);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 875713112);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 909199186);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 961959257);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 825316697);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 842093913);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 909202777);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 875713881);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 842094158);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 909203022);
      // server_protocol().zwp_linux_dmabuf_v1_format (new_id, 1448695129);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRX8888, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRX8888, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRX8888, 16777216, 2);

      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRA8888, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRA8888, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_BGRA8888, 16777216, 2);
      
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_XRGB8888, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_XRGB8888, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_XRGB8888, 16777216, 2);

      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_ARGB8888, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_ARGB8888, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, DRM_FORMAT_ARGB8888, 16777216, 2);

      /*      
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669761, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669761, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669761, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669784, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669784, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808669784, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665665, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665665, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665665, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665688, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665688, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808665688, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713089, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713089, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713089, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713089, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875708993, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875708993, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875708993, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875708993, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713112, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713112, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713112, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713112, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875709016, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875709016, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875709016, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875709016, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 892424769, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 892424769, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 892424769, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909199186, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909199186, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909199186, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 538982482, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 538982482, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 538982482, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 540422482, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 540422482, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 540422482, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 943215175, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 943215175, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 943215175, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842224199, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842224199, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842224199, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842224199, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961959257, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961959257, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961959257, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316697, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316697, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316697, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842093913, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842093913, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842093913, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909202777, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909202777, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909202777, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713881, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713881, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875713881, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961893977, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961893977, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 961893977, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316953, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316953, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 825316953, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094169, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094169, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094169, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203033, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203033, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203033, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875714137, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875714137, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 875714137, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094158, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094158, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842094158, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808530000, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808530000, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 808530000, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842084432, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842084432, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 842084432, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909193296, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909193296, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909193296, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203022, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203022, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 909203022, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448433985, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448433985, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448433985, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448433985, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448434008, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448434008, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448434008, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448434008, 16777216, 4);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448695129, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448695129, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1448695129, 16777216, 2);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1498831189, 0, 0);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1498831189, 16777216, 1);
      server_protocol().zwp_linux_dmabuf_v1_modifier (new_id, 1498831189, 16777216, 2);
      */
      /*
[982457.013] wl_drm@21.format(875713089)
[982457.020] wl_drm@21.format(875713112)
[982457.026] wl_drm@21.format(909199186)
[982457.031] wl_drm@21.format(961959257)
[982457.037] wl_drm@21.format(825316697)
[982457.042] wl_drm@21.format(842093913)
[982457.048] wl_drm@21.format(909202777)
[982457.054] wl_drm@21.format(875713881)
[982457.059] wl_drm@21.format(842094158)
[982457.064] wl_drm@21.format(909203022)
[982457.070] wl_drm@21.format(1448695129)
[982457.075] wl_drm@21.capabilities(1)
[982457.082] zwp_linux_dmabuf_v1@22.modifier(808669761, 0, 0)
[982457.093] zwp_linux_dmabuf_v1@22.modifier(808669761, 16777216, 1)
[982457.103] zwp_linux_dmabuf_v1@22.modifier(808669761, 16777216, 2)
[982457.114] zwp_linux_dmabuf_v1@22.modifier(808669784, 0, 0)
[982457.122] zwp_linux_dmabuf_v1@22.modifier(808669784, 16777216, 1)
[982457.133] zwp_linux_dmabuf_v1@22.modifier(808669784, 16777216, 2)
[982457.143] zwp_linux_dmabuf_v1@22.modifier(808665665, 0, 0)
[982457.153] zwp_linux_dmabuf_v1@22.modifier(808665665, 16777216, 1)
[982457.163] zwp_linux_dmabuf_v1@22.modifier(808665665, 16777216, 2)
[982457.173] zwp_linux_dmabuf_v1@22.modifier(808665688, 0, 0)
[982457.183] zwp_linux_dmabuf_v1@22.modifier(808665688, 16777216, 1)
[982457.194] zwp_linux_dmabuf_v1@22.modifier(808665688, 16777216, 2)
[982457.204] zwp_linux_dmabuf_v1@22.modifier(875713089, 0, 0)
[982457.214] zwp_linux_dmabuf_v1@22.modifier(875713089, 16777216, 1)
[982457.224] zwp_linux_dmabuf_v1@22.modifier(875713089, 16777216, 2)
[982457.235] zwp_linux_dmabuf_v1@22.modifier(875713089, 16777216, 4)
[982457.245] zwp_linux_dmabuf_v1@22.modifier(875708993, 0, 0)
[982457.255] zwp_linux_dmabuf_v1@22.modifier(875708993, 16777216, 1)
[982457.266] zwp_linux_dmabuf_v1@22.modifier(875708993, 16777216, 2)
[982457.276] zwp_linux_dmabuf_v1@22.modifier(875708993, 16777216, 4)
[982457.286] zwp_linux_dmabuf_v1@22.modifier(875713112, 0, 0)
[982457.297] zwp_linux_dmabuf_v1@22.modifier(875713112, 16777216, 1)
[982457.307] zwp_linux_dmabuf_v1@22.modifier(875713112, 16777216, 2)
[982457.317] zwp_linux_dmabuf_v1@22.modifier(875713112, 16777216, 4)
[982457.328] zwp_linux_dmabuf_v1@22.modifier(875709016, 0, 0)
[982457.338] zwp_linux_dmabuf_v1@22.modifier(875709016, 16777216, 1)
[982457.348] zwp_linux_dmabuf_v1@22.modifier(875709016, 16777216, 2)
[982457.359] zwp_linux_dmabuf_v1@22.modifier(875709016, 16777216, 4)
[982457.369] zwp_linux_dmabuf_v1@22.modifier(892424769, 0, 0)
[982457.379] zwp_linux_dmabuf_v1@22.modifier(892424769, 16777216, 1)
[982457.390] zwp_linux_dmabuf_v1@22.modifier(892424769, 16777216, 2)
[982457.400] zwp_linux_dmabuf_v1@22.modifier(909199186, 0, 0)
[982457.410] zwp_linux_dmabuf_v1@22.modifier(909199186, 16777216, 1)
[982457.419] zwp_linux_dmabuf_v1@22.modifier(909199186, 16777216, 2)
[982457.429] zwp_linux_dmabuf_v1@22.modifier(538982482, 0, 0)
[982457.439] zwp_linux_dmabuf_v1@22.modifier(538982482, 16777216, 1)
[982457.449] zwp_linux_dmabuf_v1@22.modifier(538982482, 16777216, 2)
[982457.459] zwp_linux_dmabuf_v1@22.modifier(540422482, 0, 0)
[982457.470] zwp_linux_dmabuf_v1@22.modifier(540422482, 16777216, 1)
[982457.480] zwp_linux_dmabuf_v1@22.modifier(540422482, 16777216, 2)
[982457.490] zwp_linux_dmabuf_v1@22.modifier(943215175, 0, 0)
[982457.500] zwp_linux_dmabuf_v1@22.modifier(943215175, 16777216, 1)
[982457.510] zwp_linux_dmabuf_v1@22.modifier(943215175, 16777216, 2)
[982457.521] zwp_linux_dmabuf_v1@22.modifier(842224199, 0, 0)
[982457.531] zwp_linux_dmabuf_v1@22.modifier(842224199, 16777216, 1)
[982457.542] zwp_linux_dmabuf_v1@22.modifier(842224199, 16777216, 2)
[982457.552] zwp_linux_dmabuf_v1@22.modifier(842224199, 16777216, 4)
[982457.562] zwp_linux_dmabuf_v1@22.modifier(961959257, 0, 0)
[982457.572] zwp_linux_dmabuf_v1@22.modifier(961959257, 16777216, 1)
[982457.583] zwp_linux_dmabuf_v1@22.modifier(961959257, 16777216, 2)
[982457.593] zwp_linux_dmabuf_v1@22.modifier(825316697, 0, 0)
[982457.603] zwp_linux_dmabuf_v1@22.modifier(825316697, 16777216, 1)
[982457.614] zwp_linux_dmabuf_v1@22.modifier(825316697, 16777216, 2)
[982457.624] zwp_linux_dmabuf_v1@22.modifier(842093913, 0, 0)
[982457.634] zwp_linux_dmabuf_v1@22.modifier(842093913, 16777216, 1)
[982457.645] zwp_linux_dmabuf_v1@22.modifier(842093913, 16777216, 2)
[982457.655] zwp_linux_dmabuf_v1@22.modifier(909202777, 0, 0)
[982457.665] zwp_linux_dmabuf_v1@22.modifier(909202777, 16777216, 1)
[982457.676] zwp_linux_dmabuf_v1@22.modifier(909202777, 16777216, 2)
[982457.686] zwp_linux_dmabuf_v1@22.modifier(875713881, 0, 0)
[982457.695] zwp_linux_dmabuf_v1@22.modifier(875713881, 16777216, 1)
[982457.705] zwp_linux_dmabuf_v1@22.modifier(875713881, 16777216, 2)
[982457.716] zwp_linux_dmabuf_v1@22.modifier(961893977, 0, 0)
[982457.726] zwp_linux_dmabuf_v1@22.modifier(961893977, 16777216, 1)
[982457.736] zwp_linux_dmabuf_v1@22.modifier(961893977, 16777216, 2)
[982457.747] zwp_linux_dmabuf_v1@22.modifier(825316953, 0, 0)
[982457.757] zwp_linux_dmabuf_v1@22.modifier(825316953, 16777216, 1)
[982457.767] zwp_linux_dmabuf_v1@22.modifier(825316953, 16777216, 2)
[982457.778] zwp_linux_dmabuf_v1@22.modifier(842094169, 0, 0)
[982457.787] zwp_linux_dmabuf_v1@22.modifier(842094169, 16777216, 1)
[982457.798] zwp_linux_dmabuf_v1@22.modifier(842094169, 16777216, 2)
[982457.808] zwp_linux_dmabuf_v1@22.modifier(909203033, 0, 0)
[982457.819] zwp_linux_dmabuf_v1@22.modifier(909203033, 16777216, 1)
[982457.829] zwp_linux_dmabuf_v1@22.modifier(909203033, 16777216, 2)
[982457.839] zwp_linux_dmabuf_v1@22.modifier(875714137, 0, 0)
[982457.849] zwp_linux_dmabuf_v1@22.modifier(875714137, 16777216, 1)
[982457.859] zwp_linux_dmabuf_v1@22.modifier(875714137, 16777216, 2)
[982457.870] zwp_linux_dmabuf_v1@22.modifier(842094158, 0, 0)
[982457.880] zwp_linux_dmabuf_v1@22.modifier(842094158, 16777216, 1)
[982457.890] zwp_linux_dmabuf_v1@22.modifier(842094158, 16777216, 2)
[982457.901] zwp_linux_dmabuf_v1@22.modifier(808530000, 0, 0)
[982457.911] zwp_linux_dmabuf_v1@22.modifier(808530000, 16777216, 1)
[982457.921] zwp_linux_dmabuf_v1@22.modifier(808530000, 16777216, 2)
[982457.932] zwp_linux_dmabuf_v1@22.modifier(842084432, 0, 0)
[982457.942] zwp_linux_dmabuf_v1@22.modifier(842084432, 16777216, 1)
[982457.952] zwp_linux_dmabuf_v1@22.modifier(842084432, 16777216, 2)
[982457.962] zwp_linux_dmabuf_v1@22.modifier(909193296, 0, 0)
[982457.972] zwp_linux_dmabuf_v1@22.modifier(909193296, 16777216, 1)
[982457.983] zwp_linux_dmabuf_v1@22.modifier(909193296, 16777216, 2)
[982457.993] zwp_linux_dmabuf_v1@22.modifier(909203022, 0, 0)
[982458.004] zwp_linux_dmabuf_v1@22.modifier(909203022, 16777216, 1)
[982458.014] zwp_linux_dmabuf_v1@22.modifier(909203022, 16777216, 2)
[982458.024] zwp_linux_dmabuf_v1@22.modifier(1448433985, 0, 0)
[982458.034] zwp_linux_dmabuf_v1@22.modifier(1448433985, 16777216, 1)
[982458.044] zwp_linux_dmabuf_v1@22.modifier(1448433985, 16777216, 2)
[982458.055] zwp_linux_dmabuf_v1@22.modifier(1448433985, 16777216, 4)
[982458.065] zwp_linux_dmabuf_v1@22.modifier(1448434008, 0, 0)
[982458.075] zwp_linux_dmabuf_v1@22.modifier(1448434008, 16777216, 1)
[982458.086] zwp_linux_dmabuf_v1@22.modifier(1448434008, 16777216, 2)
[982458.096] zwp_linux_dmabuf_v1@22.modifier(1448434008, 16777216, 4)
[982458.106] zwp_linux_dmabuf_v1@22.modifier(1448695129, 0, 0)
[982458.116] zwp_linux_dmabuf_v1@22.modifier(1448695129, 16777216, 1)
[982458.127] zwp_linux_dmabuf_v1@22.modifier(1448695129, 16777216, 2)
[982458.137] zwp_linux_dmabuf_v1@22.modifier(1498831189, 0, 0)
[982458.147] zwp_linux_dmabuf_v1@22.modifier(1498831189, 16777216, 1)
[982458.158] zwp_linux_dmabuf_v1@22.modifier(1498831189, 16777216, 2)
      */      
    }
    else if (interface == "zwp_linux_explicit_synchronization_v1")
    {
      add_object (new_id, {vwm::wayland::generated::interface_::zwp_linux_explicit_synchronization_v1});
    }
    else
      {
        std::cout << "none of the above?" << std::endl;
      }
    
  }

  void wl_compositor_create_surface (object& obj, uint32_t new_id)
  { 
    focused_surface_id = new_id;
    add_object (new_id, {vwm::wayland::generated::interface_::wl_surface
                         , {surface_type{surface_start_x, surface_start_y}}});
    surface_start_x += surface_start_x_offset;
    surface_start_y += surface_start_y_offset;
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

    std::cout << "pool created with mmap starting at " << buffer << std::endl;
    
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
                << " pool mmap size " << pool->mmap_size << " mmap buffer " << (void*)(static_cast<char*>(pool->mmap_buffer) + offset) << std::endl;

      pool->buffers.push_back ({static_cast<char*>(pool->mmap_buffer) + offset, static_cast<uint32_t>(height)*stride, offset, width, height
                                , stride, static_cast<enum format>(format)});

      assert (height >= 0);
      assert (offset + static_cast<std::uint32_t>(height)*stride <= pool->mmap_size);
      
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

      int i = 0;
      for (auto&& buffer : pool->buffers)
      {
        std::cout << "old from buffer[" << i << "] " << buffer.mmap_buffer_offset << " new "
                  << static_cast<void*>(static_cast<char*>(pool->mmap_buffer) + buffer.offset) << std::endl;
        buffer.mmap_buffer_offset = static_cast<char*>(pool->mmap_buffer) + buffer.offset;
        ++i;
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
    if (surface_type* s = std::get_if<surface_type>(&obj.data))
    {
      object_type buffer_obj = get_object(buffer_id);
      if (shm_buffer** buffer = std::get_if<shm_buffer*>(&buffer_obj.get().data))
      {
        if (*buffer)
        {
          pin();
          // auto token = ftk::ui::backend::load_buffer
          //   (*backend, *toplevel, (*buffer)->mmap_buffer_offset, (*buffer)->width
          //    , (*buffer)->height, (*buffer)->stride, static_cast<void*>(s)
          //    , [this] (std::error_code const& ec, auto token)
          //      {
          //        vwm::ui::detail::async
          //          (loop
          //           , [this, ec, token]
          //             {
          //               surface_type* s = static_cast<surface_type*>(token.user_value());
          //               if (!ec)
          //               {
          //                 std::cout << "No error loading buffer" << std::endl;
          //                 s->loaded = true;
          //               }
          //               else
          //               {
          //                 std::cout << "Error loading buffer " << ec.message() << std::endl;
          //                 s->failed = true;
          //               }
          //               /// can I release ?
          //               unpin();
          //             });
          //      });
          auto future = image_loader->load ((*buffer)->mmap_buffer_offset
                                            , (*buffer)->width, (*buffer)->height
                                            , (*buffer)->stride);
          s->set_attachment (*buffer, buffer_id, std::move(future), x, y);
        }
      }
      else if (dma_buffer* buffer = std::get_if<dma_buffer>(&buffer_obj.get().data))
      {
        std::cout << "dma buffer" << std::endl;
        static_cast<void>(buffer);
        // s->attach (std::move(*buffer), buffer_id, x, y);
      }
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
    if (surface_type* s = std::get_if<surface_type>(&obj.data))
    {
      if (shm_buffer** buffer = std::get_if <shm_buffer*>(&s->buffer))
      {
        std::cout << "calling draw buffer " << *buffer << std::endl;

        if ((*buffer))
        {
          if (!s->render_token)
          {
            std::unique_lock <std::mutex> l(*render_mutex);
            std::cout << "adding to image draw list" << std::endl;
            auto value = s->load_token.get().image_view;
            s->loaded = true;
            std::cout << "adding image from client to render" << std::endl;
            auto iterator = toplevel->append_image
              ({value, s->pos_x, s->pos_y, (*buffer)->width, (*buffer)->height});
            s->render_token = iterator;
          }
          else
          {
            std::unique_lock <std::mutex> l(*render_mutex);
            auto value = s->load_token.get().image_view;
            s->loaded = true;
            toplevel->replace_image_view (*s->render_token, value);
          }
          render_dirty ();

          
          if (s->failed)
          {
            std::cout << "failed" << std::endl;
          }
          else if (s->loaded)
          {
            auto surface_id = get_object_id(&obj);
        
            server_protocol().wl_buffer_release (s->buffer_id);
            if (output_id && last_surface_entered_id != surface_id)
            {
              last_surface_entered_id = surface_id;
              server_protocol().wl_surface_enter (surface_id, output_id);
            }
            if (focused_surface_id && keyboard_id && old_focused_surface_id != focused_surface_id)
            {
              old_focused_surface_id = focused_surface_id;
              array<uint32_t> keys;
              server_protocol().wl_keyboard_enter (keyboard_id, serial++, focused_surface_id, keys);
            }
          }
          else
          {
            std::cout << "not loaded yet" << std::endl;
          }
        }
      }
      else if (dma_buffer* buffer = std::get_if<dma_buffer>(&s->buffer))
      {
        std::cout << "dma buffer commit" << std::endl;

        // ftk::ui::backend::draw_buffer (*backend, *toplevel, buffer->params[0].fd, buffer->width
        //                                , buffer->height, buffer->format, buffer->params[0].offset, buffer->params[0].stride
        //                                , buffer->params[0].modifier_hi, buffer->params[0].modifier_lo);
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
    std::cout << "wl_seat_get_keyboard new_id " << new_id << std::endl;
    
    add_object (new_id, {vwm::wayland::generated::interface_::wl_pointer});
    keyboard_id = new_id;

    //if constexpr (true/*keyboard->has_keymap*/)
    {
      char* keymap_string = xkb_keymap_get_as_string (keyboard->keymap, XKB_KEYMAP_FORMAT_TEXT_V1);

      int fd = ::memfd_create ("keymap_shared", MFD_CLOEXEC);
      ::ftruncate (fd, strlen(keymap_string));
      void* p = ::mmap (NULL, strlen(keymap_string), PROT_WRITE, MAP_SHARED, fd, 0);
      memcpy (p, keymap_string, strlen(keymap_string));

      std::cout << "sending keymap" << std::endl;
      server_protocol().wl_keyboard_keymap (new_id, 1 /* XKB_V1 */, fd, strlen(keymap_string));
      std::cout << "sent keymap" << std::endl;
    }
  }
  void send_key (std::uint32_t time, std::uint32_t key, std::uint32_t state)
  {
    std::cout << " keyboard id "  << keyboard_id << std::endl;
    if (keyboard_id)
    {
      server_protocol().wl_keyboard_key (keyboard_id, serial, time, key, state);
      server_protocol().wl_keyboard_modifiers
        (keyboard_id, serial++, keyboard->mods_depressed(), keyboard->mods_latched(), keyboard->mods_locked()
         , keyboard->group());
    }
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
  void wl_subcompositor_get_subsurface (object& obj, std::uint32_t new_id, std::uint32_t surface, std::uint32_t parent)
  {
    add_object (new_id, {vwm::wayland::generated::interface_::wl_subsurface});
  }
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

  void zwp_linux_dmabuf_v1_destroy (object& obj) {}
  void zwp_linux_dmabuf_v1_create_params (object& obj, std::uint32_t new_id)
  {
    add_object (new_id, {wayland::generated::interface_::zwp_linux_buffer_params_v1, {dma_params{}}});
  }
  void zwp_linux_buffer_params_v1_destroy (object& obj) {}
  void zwp_linux_buffer_params_v1_add(object& obj, int fd, std::uint32_t arg1, std::uint32_t arg2
                                      , std::uint32_t arg3, std::uint32_t arg4, std::uint32_t arg5)
  {
    if (wayland::dma_params* params = std::get_if<wayland::dma_params>(&obj.data))
    {
      params->params.push_back({fd, arg1, arg2, arg3, arg4, arg5});
    }
  }
  void zwp_linux_buffer_params_v1_create(object& obj, std::int32_t arg0, std::int32_t arg1, std::uint32_t arg2, std::uint32_t arg3)
  {
  }
  void zwp_linux_buffer_params_v1_create_immed(object& obj, std::uint32_t new_id, std::int32_t width, std::int32_t height
                                               , std::uint32_t format, std::uint32_t flags)
  {
    std::cout << "zwp_linux_buffer_params_v1_create_immed " << new_id << " " << width << " " << height << " format " << format << " " << flags <<std::endl;
    if (wayland::dma_params* params = std::get_if<wayland::dma_params>(&obj.data))
    {
      add_object (new_id, {wayland::generated::interface_::wl_buffer, {wayland::dma_buffer{width, height, format, flags, params->params}}});
    }
  }

  void zwp_linux_explicit_synchronization_v1_destroy(object& obj) {}
  void zwp_linux_explicit_synchronization_v1_get_synchronization(object& obj, std::uint32_t arg0, std::uint32_t arg1) {}
  void zwp_linux_surface_synchronization_v1_destroy(object& obj) {}
  void zwp_linux_surface_synchronization_v1_set_acquire_fence(object& obj, int arg0) {}
  void zwp_linux_surface_synchronization_v1_get_release(object& obj, std::uint32_t arg0) {}

  void wl_drm_authenticate(object& obj, std::uint32_t magic)
  {
    std::cout << "wl_drm_authenticate with magic " << magic << std::endl;
    if (struct drm* drm = std::get_if<struct drm>(&obj.data))
    {
      ::ioctl(drm->fd, DRM_IOCTL_AUTH_MAGIC, &magic);
    }
    
    server_protocol().wl_drm_authenticated(get_object_id(&obj));
  }
  void wl_drm_create_buffer(object& obj, std::uint32_t arg0, std::uint32_t arg1, std::int32_t arg2, std::int32_t arg3
                            , std::uint32_t arg4, std::uint32_t arg5) {}
  void wl_drm_create_planar_buffer(object& obj, std::uint32_t arg0, std::uint32_t arg1, std::int32_t arg2
                                   , std::int32_t arg3, std::uint32_t arg4, std::int32_t arg5
                                   , std::int32_t arg6, std::int32_t arg7, std::int32_t arg8
                                   , std::int32_t arg9, std::int32_t arg10) {}
  void wl_drm_create_prime_buffer(object& obj, std::int32_t arg0, std::int32_t arg1, std::uint32_t arg2
                                  , std::int32_t arg3, std::int32_t arg4, std::int32_t arg5, std::int32_t arg6
                                  , std::int32_t arg7, std::int32_t arg8, std::int32_t arg9, std::int32_t arg10) {}
};
    
} }

#endif
