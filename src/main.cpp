///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

//#include <libinput.h>
//#include <libudev.h>
#include <uv.h>
#include <xkbcommon/xkbcommon.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cassert>
#include <iostream>
#include <string.h>

#include <fastdraw/output/vulkan/add_image.hpp>

#include <ftk/ui/toplevel_window.hpp>
#include <ftk/ui/backend/xlib_surface.hpp>

#include <ftk/ui/backend/vulkan.hpp>
#include <ftk/ui/backend/uv.hpp>

#include <ftk/ui/backend/vulkan_draw.hpp>
#include <ftk/ui/backend/vulkan.ipp>

//#include <vwm/backend/libinput.hpp>
#include <vwm/backend/xlib_keyboard.hpp>
#include <vwm/uv/detail/poll.hpp>
#include <vwm/wayland/compositor.hpp>

#include <vwm/wayland/client.hpp>
#include <ftk/ui/backend/vulkan_draw.hpp>
#include <vwm/render_thread.hpp>

// #include <wayland-server-core.h>
// #include <wayland-server-protocol.h>

// #include <xdg-shell-server-protocol.h>

//#include "../test.h"

int drm_init();

int main(void) {
  try {
  ::uv_loop_t loop;
  bool dirty = false, exit = false;;
  std::mutex mutex;
  std::condition_variable condvar;

  uv_loop_init (&loop);

  typedef ftk::ui::backend::vulkan<ftk::ui::backend::uv, ftk::ui::backend::xlib_surface<ftk::ui::backend::uv>> backend_type;
  typedef vwm::wayland::client<vwm::backend::xlib::keyboard, ftk::ui::backend::xlib_surface<ftk::ui::backend::uv>> client_type;
  
  vwm::wayland::generated::server_protocol<client_type>* focused = nullptr;

  auto keyboard = vwm::backend::xlib::keyboard{};

  // auto keyboard = vwm::backend::libinput::init
  //   (&loop
  //    ,
  //    [&focused] (std::uint32_t time, std::uint32_t key, std::uint32_t state)
  //    {
  //      if (focused)
  //      {
  //        focused->send_key(time, key, state);
  //      }
  //    }
  //    );
  
  backend_type backend({&loop});

  ftk::ui::toplevel_window<backend_type> w(backend);

  backend.key_signal.connect
    ([&focused, &keyboard] (// std::uint32_t time, std::uint32_t key, std::uint32_t state
                 XKeyEvent ev)
     {
       std::cout << "key signal focused: "  << focused << std::endl;
       if (focused)
       {
         std::cout << " sending key "  << ev.keycode << "  type " << ev.type << std::endl;
         keyboard.update_state (ev.keycode, ev.type == KeyPress);
         focused->send_key(ev.time, ev.keycode - 8, ev.type == KeyPress ? 1 : 0);
       }
     });

  {
    ////////////////////////////////////////////
    auto runtime_dir = getenv("XDG_RUNTIME_DIR");

    const char* name = "wayland-0";
    struct sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    auto name_size = snprintf(addr.sun_path, sizeof(addr.sun_path),
                              "%s/%s", runtime_dir, name) + 1;
    auto socket = ::socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
    assert (socket > 0);

    auto br = bind (socket, static_cast<sockaddr*>(static_cast<void*>(&addr)), sizeof (addr.sun_family) + name_size);
    if (br != 0)
    {
      perror ("error: ");
      return -1;
    }

    auto lr = listen (socket, 128);
    if (lr != 0)
    {
      perror ("error: ");
      return -1;
    }

    vwm::ui::detail::wait (&loop, socket, UV_READABLE
                           , [loop = &loop, socket, backend = &backend, toplevel = &w, keyboard = &keyboard, &focused
                              , &dirty, render_mutex = &mutex, render_condvar = &condvar] (uv_poll_t* handle)
                             {
                               std::cout << "can be accepted?" << std::endl;

                               struct sockaddr_un name;
                               int fd;
                               uv_fileno(static_cast<uv_handle_t*>(static_cast<void*>(handle)), &fd);
                               socklen_t addr_size = sizeof(name);
                               auto new_socket = accept (fd, static_cast<struct sockaddr*>(static_cast<void*>(&name)), &addr_size);
                               assert (new_socket > 0);

                               fcntl(new_socket, F_SETFL, O_NONBLOCK);

                               vwm::wayland::generated::server_protocol<client_type>*
                                 c = new vwm::wayland::generated::server_protocol<client_type>
                                 {new_socket, loop, backend, toplevel, keyboard, vwm::render_dirty (dirty, *render_mutex, *render_condvar)};
                               if (!focused) focused = c;
                               vwm::ui::detail::wait (loop, new_socket, UV_READABLE,
                                                      [loop, c] (uv_poll_t* handle)
                                                      {
                                                        std::cout << "can be read" << std::endl;

                                                        try
                                                        {
                                                          c->read();
                                                        }
                                                        catch (std::exception const& e)
                                                        {
                                                          std::cout << "Error with client: " << e.what() << std::endl;
                                                          uv_poll_stop (handle);
                                                          uv_close (static_cast<uv_handle_t*>(static_cast<void*>(handle)), /*& ::close*/NULL);
                                                        }
                                                      });
                             });

    {
      // char socket[512];
      // sprintf (socket, "%d", fd);
      //std::cout << "socket: " << socket << std::endl;
      setenv("WAYLAND_DISPLAY", /*socket*/"wayland-0", 1);
    }
  }

  // draw (backend, w);
  
  auto thread = vwm::render_thread (&w, dirty, exit, mutex, condvar);

  auto r = uv_run (&loop, UV_RUN_DEFAULT);
  std::cout << "uv_run return " << r << std::endl;

  vwm::render_exit (exit, mutex, condvar)();
  thread.join();
  return 0;
  } catch (std::exception const& e)
  {
    std::cout << "Exception was thrown " << e.what() << std::endl;
    return -1;
  }
}
