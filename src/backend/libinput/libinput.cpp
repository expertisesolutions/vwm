///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#include <vwm/uv/detail/poll.hpp>

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
#include <functional>

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
      
vwm::keyboard init ( ::uv_loop_t* loop, std::function<void(uint32_t, uint32_t, uint32_t)> function)
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

  vwm::keyboard keyboard{xkb_context_, keymap, state};
  
  int fd = libinput_get_fd(li);

  auto lambda = 
    [li, state, function, keyboard] (uv_poll_t*)
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
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
          std::cout << "LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE" << std::endl;
          break;
        case LIBINPUT_EVENT_POINTER_MOTION:
          std::cout << "LIBINPUT_EVENT_POINTER_MOTION" << std::endl;
          break;
        case LIBINPUT_EVENT_POINTER_BUTTON:
          std::cout << "LIBINPUT_EVENT_POINTER_BUTTON" << std::endl;
          break;
        case LIBINPUT_EVENT_TOUCH_DOWN:
          std::cout << "LIBINPUT_EVENT_TOUCH_DOWN" << std::endl;
          break;
        case LIBINPUT_EVENT_DEVICE_ADDED:{
          std::cout << "LIBINPUT_EVENT_DEVICE_ADDED" << std::endl;

          libinput_event_device_notify* ev = libinput_event_get_device_notify_event (event);

          assert (ev != NULL);

          libinput_device* dev = libinput_event_get_device (event);
          
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_KEYBOARD))
          {
            std::cout << "a keyboard" << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_POINTER))
          {
            std::cout << "a mouse" << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_TOUCH))
          {
            std::cout << "a touchscreen" << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_GESTURE))
          {
            std::cout << "a gesture input" << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_SWITCH))
          {
            std::cout << "a switch input" << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
          {
            std::cout << "a tablet tool " << libinput_device_get_name (dev) << std::endl;
          }
          if (libinput_device_has_capability (dev, LIBINPUT_DEVICE_CAP_TABLET_PAD))
          {
            std::cout << "a tablet pad " << libinput_device_get_name (dev) << std::endl;
          }
          
          break;}
        case LIBINPUT_EVENT_KEYBOARD_KEY:
          std::cout << "LIBINPUT_EVENT_KEYBOARD_KEY" << std::endl;
          {
            libinput_event_keyboard* ev = libinput_event_get_keyboard_event (event);
            std::cout << "key " << libinput_event_keyboard_get_key (ev) + 8
                      << " key state " << (int)libinput_event_keyboard_get_key_state (ev) << std::endl;

            auto code = libinput_event_keyboard_get_key (ev) + 8;

            xkb_state_update_mask(keyboard.state, keyboard.mods_depressed(), keyboard.mods_latched(), keyboard.mods_locked()
                                  , 0, 0, keyboard.group());
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
              if (function)
              {
                function (libinput_event_keyboard_get_time (ev), libinput_event_keyboard_get_key (ev)
                          , libinput_event_keyboard_get_key_state (ev));
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

  vwm::ui::detail::wait (loop, fd, UV_READABLE, lambda);

  //xkb_state_update_mask(keyboard.state, keyboard.mods_depressed(), keyboard.mods_latched(), keyboard.mods_locked(), 0, 0, keyboard.group());

  return keyboard;
}

} } }
