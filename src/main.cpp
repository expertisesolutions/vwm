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
#include <fastdraw/image_loader/extension_loader.hpp>

#include <ftk/ui/toplevel_window.hpp>
#include <ftk/ui/backend/vulkan_vertex_buffer.hpp>
#include <ftk/ui/backend/vulkan_renderpass.hpp>
#include <ftk/ui/backend/xlib_surface.hpp>

#include <ftk/ui/backend/vulkan_image.hpp>

#include <ftk/ui/backend/vulkan.hpp>
#include <ftk/ui/backend/uv.hpp>

#include <ftk/ui/backend/vulkan_draw.hpp>
#include <ftk/ui/backend/vulkan.ipp>
#include <ftk/ui/backend/vulkan_submission_pool.hpp>

//#include <vwm/backend/libinput.hpp>
#include <vwm/backend/xlib_keyboard.hpp>
#include <vwm/backend/xlib_mouse.hpp>
#include <vwm/uv/detail/poll.hpp>
#include <vwm/uv/detail/timer.hpp>
#include <vwm/wayland/compositor.hpp>
#include <vwm/theme.hpp>

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
  std::mutex render_mutex;
  std::condition_variable condvar;

  uv_loop_init (&loop);

  typedef ftk::ui::backend::vulkan<ftk::ui::backend::uv, ftk::ui::backend::xlib_surface<ftk::ui::backend::uv>> backend_type;
  typedef vwm::wayland::client<vwm::backend::xlib::keyboard, ftk::ui::backend::xlib_surface<ftk::ui::backend::uv>> client_type;
  
  vwm::wayland::generated::server_protocol<client_type>* focused = nullptr;

  auto keyboard = vwm::backend::xlib::keyboard{};

  backend_type backend({&loop});
  ftk::ui::toplevel_window<backend_type> w(backend);

  ftk::ui::backend::vulkan_submission_pool vulkan_thread_pool (w.window.voutput.device
                                                               , &w.window.queues
                                                               , 4 /* thread count */);
  
  vwm::theme<fastdraw::image_loader::extension_loader, ftk::ui::backend::vulkan_image_loader>
    theme {{},
           {w.window.voutput.device, w.window.voutput.physical_device
            , &vulkan_thread_pool}
           , std::filesystem::current_path()};

  // pc::future<ftk::ui::backend::vulkan_image> mouse_cursor = theme[vwm::theme_image::pointer];
  // pc::future<ftk::ui::backend::vulkan_image> background = theme[vwm::theme_image::pointer];
  pc::future<ftk::ui::backend::vulkan_image> mouse_cursor = theme[vwm::theme_image::pointer];
  pc::future<ftk::ui::backend::vulkan_image> background = theme[vwm::theme_image::background];

  auto background_img = background.get();

  w.load_background ({background_img.image_view, 0, 0
                      , w.window.voutput.swapChainExtent.width
                      , w.window.voutput.swapChainExtent.height});

  w.framebuffers_damaged_regions[0].push_back
    ({0, 0, w.window.voutput.swapChainExtent.width, w.window.voutput.swapChainExtent.height});
  w.framebuffers_damaged_regions[1].push_back
    ({0, 0, w.window.voutput.swapChainExtent.width, w.window.voutput.swapChainExtent.height});
  w.append_image ({background_img.image_view, 100, 100, 160, 90});
  vwm::render_dirty (dirty, render_mutex, condvar)();

  bool is_moving_window = false;
  
  backend.key_signal.connect
    ([&focused, &keyboard, &is_moving_window] (// std::uint32_t time, std::uint32_t key, std::uint32_t state
                 XKeyEvent ev)
     {
       std::cout << "key signal focused: "  << focused << std::endl;
       if (is_moving_window && ev.keycode == XKB_KEY_Shift_L && ev.type == KeyRelease)
       {
         is_moving_window = false;
       }
       
       if (focused)
       {
         std::cout << " sending key "  << ev.keycode << "  type " << ev.type << std::endl;
         keyboard.update_state (ev.keycode, ev.type == KeyPress);
         focused->send_key(ev.time, ev.keycode - 8, ev.type == KeyPress ? 1 : 0);
       }
     });

  auto mouse_iterator = w.images.end();

#if 1
  vwm::backend::xlib::mouse mouse;
  backend.button_signal.connect
    ([&mouse, &keyboard, &is_moving_window] (XButtonEvent ev)
     {
       if (xkb_state_mod_indices_are_active (keyboard.state, XKB_STATE_MODS_EFFECTIVE, XKB_STATE_MATCH_ALL
                                             , XKB_KEY_Shift_L, XKB_MOD_INVALID))
       {
         std::cout << "Shift is pressed" << std::endl;
         is_moving_window = true;
       }
     });
     
  backend.motion_signal.connect
    ([&mouse, &mouse_iterator, &w, &mouse_cursor, &dirty, &render_mutex, &condvar
      , &keyboard, &is_moving_window] (XMotionEvent ev)
     {
       static auto mouse_cursor_ = mouse_cursor.get();

       if (is_moving_window)
       {
         // which window ?
       }

       std::unique_lock<std::mutex> l (render_mutex);
       if (mouse_iterator == w.images.end())
       {
         std::cout << "mouse adding new mouse image" << std::endl;
         mouse_iterator = w.append_image ({mouse_cursor_.image_view, ev.x, ev.y, 32, 32});
         //vwm::render_dirty (dirty, mutex, condvar)();
       }
       else
       {
         std::cout << "replacing coordinates" << std::endl;
         //*mouse_iterator = {mouse_cursor_.image_view, ev.x, ev.y, 32, 32};
         // change vertex buffer ?
         // mouse_iterator->x = ev.x;
         // mouse_iterator->y = ev.y;
         w.move_image (mouse_iterator, ev.x, ev.y);
       }
       std::cout << "please render mouse (" << w.images.size() << ") at " << ev.x << "x" << ev.y << std::endl;
       vwm::render_dirty (dirty, l, condvar)();
     });
#endif
#if 0
  unsigned int timer_iteration = 0;
  int32_t positions[][2] =
    {
     {1000, 800}
     , {950, 800}
     , {900, 800}
     , {850, 800}
     , {800, 800}
     , {750, 800}
     , {700, 800}
     , {650, 800}
     , {600, 800}
     , {0, 0}
    };

  vwm::ui::detail::timer_wait
    (&loop
     , 1*1000
     , 10
     , [&] (uv_timer_t* timer)
       {
         static auto mouse_cursor_ = mouse_cursor.get();
         std::unique_lock<std::mutex> l (render_mutex);
         if (mouse_iterator == w.images.end())
         {
           std::cout << "mouse adding new mouse image" << std::endl;
           mouse_iterator = w.append_image
             ({mouse_cursor_.image_view, positions[timer_iteration][0]
               , positions[timer_iteration][1], 32, 32});
         }
         else if (positions[timer_iteration][0])
         {
           std::cout << "replacing coordinates" << std::endl;
           w.move_image (mouse_iterator, positions[timer_iteration][0], positions[timer_iteration][1]);
         }
         else
         {
           static bool one_time = false;

           if (!one_time)
           {
             one_time = true;
             w.append_image ({background_img.image_view, 500, 100, 160, 90});
           }
           uv_timer_set_repeat (timer, 1000);
           vwm::render_dirty (dirty, l, condvar)();
           return;
         }
         //std::cout << "please render mouse (" << w.images.size() << ") at " << ev.x << "x" << ev.y << std::endl;
         timer_iteration++;
         vwm::render_dirty (dirty, l, condvar)();
       });
#endif  
  
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
                              , &dirty, &render_mutex, render_condvar = &condvar
                              , &theme] (uv_poll_t* handle)
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
                                 {new_socket, loop, backend, toplevel, keyboard, vwm::render_dirty (dirty, render_mutex, *render_condvar), &theme.output_image_loader, &render_mutex};
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
  
  auto thread = vwm::render_thread (&w, dirty, exit, render_mutex, condvar);

  auto r = uv_run (&loop, UV_RUN_DEFAULT);
  std::cout << "uv_run return " << r << std::endl;

  vwm::render_exit (exit, render_mutex, condvar)();
  thread.join();
  return 0;
  } catch (std::exception const& e)
  {
    std::cout << "Exception was thrown " << e.what() << std::endl;
    return -1;
  }
}
