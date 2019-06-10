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

#include <cassert>
#include <iostream>
#include <cstring>

namespace vwm { namespace backend { namespace libinput {

namespace {

static int open_restricted(const char *path, int flags, void *user_data)
{
        int fd = open(path, flags);
        return fd < 0 ? -errno : fd;
}
static void close_restricted(int fd, void *user_data)
{
        close(fd);
}
const static struct libinput_interface interface = {
        .open_restricted = open_restricted,
        .close_restricted = close_restricted,
};

}
      
void init ( ::uv_loop_t* loop)
{
  struct udev *udev;
  struct libinput *li;
  xkb_context* xkb_context_;
  xkb_keymap* keymap;
  xkb_rule_names rules = {};

  rules.layout = "us";
  rules.model = "pc105";
  rules.variant = "intl";

  xkb_context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  keymap = xkb_keymap_new_from_names(xkb_context_, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
  assert (!!keymap);

  auto state = xkb_state_new (keymap);

  udev = udev_new();
  assert (udev != nullptr);
  li = libinput_udev_create_context(&interface, NULL, udev);
  assert (li != nullptr);
  libinput_udev_assign_seat(li, "seat0");

  int fd = libinput_get_fd(li);

  uv_poll_t* handle = new uv_poll_t;
  ::uv_poll_init (loop, handle, fd);

  auto lambda = 
    [li, state]
    {
      struct libinput_event *event;
      std::cout << "event on fd" << std::endl;
      libinput_dispatch(li);
      while ((event = libinput_get_event(li)) != NULL)
      {
        // handle the event here
        std::cout << "event is not null" << std::endl;

        auto t = libinput_event_get_type (event);
        switch (t)
        {
        case LIBINPUT_EVENT_DEVICE_ADDED:
          std::cout << "LIBINPUT_EVENT_DEVICE_ADDED" << std::endl;
          break;
        case LIBINPUT_EVENT_KEYBOARD_KEY:
          std::cout << "LIBINPUT_EVENT_KEYBOARD_KEY" << std::endl;
          {
            libinput_event_keyboard* ev = libinput_event_get_keyboard_event (event);
            std::cout << "key " << libinput_event_keyboard_get_key (ev) + 8
                      << " key state " << (int)libinput_event_keyboard_get_key_state (ev) << std::endl;

            auto code = libinput_event_keyboard_get_key (ev) + 8;
            
            xkb_state_update_key (state, code, libinput_event_keyboard_get_key_state(ev) ? XKB_KEY_DOWN : XKB_KEY_UP);

            const xkb_keysym_t* syms;
            auto sym_size = xkb_state_key_get_syms (state, code, &syms);

            std::cout << "keysyms " << sym_size << std::endl;

            if (sym_size)
            {
              xkb_keysym_t sym = syms[0];

              char key[64];
              std::memset(key, 0, sizeof(key));
              xkb_keysym_get_name(sym, key, sizeof(key));

              std::cout << "key is " << key << std::endl;

              char utf8[64];
              std::memset(utf8, 0, sizeof(utf8));
              xkb_keysym_to_utf8(sym, utf8, sizeof(utf8));

              std::cout << "utf8 " << utf8 << std::endl;

              // do something with it
              {
              }
            }
          }
          break;
        default:
          std::cout << "number is " << (int)t << std::endl;
        };
        
    
        libinput_event_destroy(event);
        libinput_dispatch(li);
      }
      std::cout << "event is null" << std::endl;
    };
  using lambda_type = decltype(lambda);

  handle->data = new lambda_type(lambda);
  
  uv_poll_cb cb = [] (uv_poll_t* handle, int status, int event)
                  {
		    std::cout << "event " << event << std::endl;
                    if (status == 0)
                    {
                      (*static_cast<lambda_type*>(handle->data))();
                    }
                  };

  uv_poll_start (handle, UV_READABLE, cb);
}

} } }