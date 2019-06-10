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

#include <libinput.h>
#include <libudev.h>
#include <uv.h>
#include <xkbcommon/xkbcommon.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cassert>
#include <iostream>
#include <string.h>

#include <ftk/ui/toplevel_window.hpp>
#include <ftk/ui/backend/khr_display.hpp>

#include <ftk/ui/backend/vulkan.hpp>
#include <ftk/ui/backend/uv.hpp>

#include <ftk/ui/backend/vulkan_draw.hpp>
#include <ftk/ui/backend/vulkan.ipp>

#include <vwm/backend/libinput.hpp>
#include <vwm/uv/detail/poll.hpp>
#include <vwm/wayland/compositor.hpp>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <xdg-shell-server-protocol.h>

int drm_init();

int main(void) {
  ::uv_loop_t loop;

  uv_loop_init (&loop);

  vwm::backend::libinput::init (&loop);

  {
    auto display = wl_display_create ();

    // const char *socket = wl_display_add_socket_auto(display);
    wl_display_add_socket (display, NULL);

    wl_event_loop *event_loop = wl_display_get_event_loop(display);

    // auto fd = wl_event_loop_get_fd(event_loop);

    // // uv_poll_t* handle = new uv_poll_t;
    // // ::uv_poll_init (loop, handle, fd);
    // vwm::ui::detail::wait (&loop, fd
    //                        , UV_READABLE
    //                        , []
    //                        {
                             
    //                        });
    
    
    //int fd = drm_init();

  //   uv_poll_t* handle = new uv_poll_t;
  //   ::uv_poll_init (&loop, handle, fd);

  //   handle->data = new lambda_type(lambda);
  
  //   uv_poll_cb cb = [] (uv_poll_t* handle, int status, int event)
  //                   {
  //                     if (status == 0)
  //                     {
  //                       std::cout << "handle drm event" << std::endl;
  //                       int fd = 0;
  //                       uv_fileno(static_cast<uv_handle_t*>(static_cast<void*>(handle)), &fd);

  //                       assert (!!fd);

  //                       auto ret = drmHandleEvent(fd, nullptr);
  //                       if (ret != 0) {
  //                         fprintf(stderr, "drmHandleEvent failed: %s\n", strerror(errno));
  //                       }
  //                     }
  //                   };

  //   uv_poll_start (handle, UV_READABLE, cb);

    {
      // char socket[512];
      // sprintf (socket, "%d", fd);
      //std::cout << "socket: " << socket << std::endl;
      setenv("WAYLAND_DISPLAY", /*socket*/"wayland-0", 1);
    }

    const char *default_seat = "seat0";

    static struct wl_shm_pool_interface shm_pool_implementation = {
          [] (wl_client*, wl_resource*, uint32_t, int32_t, int32_t, int32_t, int32_t, uint32_t)
          {
            std::cout << "create buffer" << std::endl;
          }
        , [] (wl_client*, wl_resource*)
          {
            std::cout << "destroy" << std::endl;
          }
        , [] (wl_client*, wl_resource*, int32_t)
          {
            std::cout << "resize" << std::endl;
          }
    };

    static struct wl_compositor_interface compositor_interface
      {
       [] (wl_client* client, wl_resource*, uint32_t id)
       {
         std::cout << "create_surface" << std::endl;

         uint32_t version = 4;

         auto resource = wl_resource_create(client, &wl_surface_interface, version, id);
         wl_resource_set_implementation(resource, &vwm::wayland::surface_impl, nullptr, nullptr);


       }
       , [] (wl_client*, wl_resource*, uint32_t) { std::cout << "create_region" << std::endl; }
      };

    static struct wl_shm_interface shm_interface
      {
       [] (wl_client* client, wl_resource* resource, uint32_t id, int32_t fd, int32_t size)
       {
         std::cout << "shm interface" << std::endl;
         auto resource_ = wl_resource_create(client, &wl_shm_pool_interface, wl_resource_get_version(resource), id);

         wl_resource_set_implementation(resource_, &shm_pool_implementation, nullptr, /*&destroy_pool_resource*/nullptr);
       }
      };

    auto compositor_global = wl_global_create (display, &wl_compositor_interface, 3, nullptr
                                               , [] (wl_client* client, void*, uint32_t version, uint32_t id)
                                                 {
                                                   std::cout << "compositor bind compositor" << std::endl;
                                                   // create resource and set implementation
                                                   if (version > 3)
                                                     version = 3;
                                                   auto resource = wl_resource_create(client, &wl_compositor_interface, version, id);
                                                   wl_resource_set_implementation(resource, &compositor_interface, NULL, NULL);
                                                 });

    //wl_display_init_shm (display);
    auto shm_global = wl_global_create (display, &wl_shm_interface, 1, nullptr
                                        , [] (wl_client* client, void*, uint32_t version, uint32_t id)
                                          {
                                            std::cout << "shm bind compositor" << std::endl;
                                            // create resource and set implementation
                                            auto resource = wl_resource_create(client, &wl_shm_interface, version, id);
                                            wl_resource_set_implementation(resource, &shm_interface, NULL, NULL);

                                            wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
                                            wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
                                          });

    auto shell_global = wl_global_create (display, &wl_shell_interface, 1, nullptr
                                          , [] (wl_client*, void*, uint32_t version, uint32_t)
                                            {
                                              std::cout << "shell bind compositor" << std::endl;
                                              // create resource and set implementation
                                            });

    auto output_global = wl_global_create (display, &wl_output_interface, 2, nullptr
                                           , [] (wl_client* client, void*, uint32_t version, uint32_t id)
                                             {
                                               std::cout << "output bind compositor" << std::endl;
                                               // create resource and set implementation

                                               auto resource = wl_resource_create(client, &wl_output_interface, version, id);
                                               wl_resource_set_implementation(resource, NULL, NULL, NULL);

                                               	wl_output_send_geometry(resource, 0, 0,
                                                                        100, 200,
                                                                        0, "unknown", "unknown", WL_OUTPUT_TRANSFORM_NORMAL);

                                                wl_output_send_done(resource);
                                             });

    static struct wl_seat_interface seat_implementation = {
        [] (wl_client*, wl_resource*, uint32_t id)
        {
          std::cout << "get_pointer" << std::endl;
        }
      , [] (wl_client*, wl_resource*, uint32_t id)
        {
          std::cout << "get_keyboard" << std::endl;
        }
      , [] (wl_client*, wl_resource*, uint32_t id)
        {
          std::cout << "get_touch" << std::endl;
        }
    };    
    auto seat_global = wl_global_create (display, &wl_seat_interface, 5, nullptr
                                         , [] (wl_client* client, void*, uint32_t version, uint32_t id)
      {
        std::cout << "seat bind compositor" << std::endl;
        // create resource and set implementation

        auto resource = wl_resource_create(client, &wl_seat_interface, version, id);
        wl_resource_set_implementation(resource, &seat_implementation, NULL, nullptr);

        if (version >= 2)
          wl_seat_send_name(resource, "seat0");

        wl_seat_send_capabilities(resource, /*seat.capabilities*/ WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
      });

    auto xdg_shell_global = wl_global_create (display, &xdg_wm_base_interface, 1, nullptr
                                         , [] (wl_client* client, void*, uint32_t version, uint32_t id)
      {
        std::cout << "xdg-shell bind compositor" << std::endl;
        // create resource and set implementation
        auto resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
        wl_resource_set_implementation(resource, &vwm::wayland::xdg_impl, NULL, nullptr);
        
        
      });
    
    // if (!fork ())
    // {
    //   std::cout << "running app" << std::endl;
    //   const char* argv[2] = {"./hello_wayland", nullptr};
    //   unsetenv ("WAYLAND_DEBUG");
    //   execv("./hello_wayland", const_cast<char**>(&argv[0]));
    //   std::cout << "execv failed" << std::endl;
    // }
    // else
    {
      sleep(1);
      std::cout << "compositor" << std::endl;
    }

    

    wl_display_run (display);
    while(1)
      wl_event_loop_dispatch (event_loop, 0);

    
  }

  typedef ftk::ui::backend::vulkan<ftk::ui::backend::uv, ftk::ui::backend::khr_display> backend_type;
  backend_type backend({&loop});

  ftk::ui::toplevel_window<backend_type> w(backend);
  draw (backend, w);

  auto r = uv_run (&loop, UV_RUN_DEFAULT);
  std::cout << "uv_run return " << r << std::endl;
    
  return 0;
}
