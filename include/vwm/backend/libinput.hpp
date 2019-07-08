///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_VWM_BACKEND_LIBINPUT_HPP
#define VWM_VWM_BACKEND_LIBINPUT_HPP

#include <xkbcommon/xkbcommon.h>

namespace vwm { namespace backend { namespace libinput {

struct libinput_keyboard
{
  xkb_context* context;
  xkb_keymap* keymap;
  xkb_state* state;

  libinput_keyboard (xkb_context* context, xkb_keymap* keymap
            , xkb_state* state)
    : context(context), keymap(keymap), state (state)
  {
    assert (context != nullptr);
    assert (keymap != nullptr);
    assert (state != nullptr);
  }
  libinput_keyboard () : context(nullptr), keymap(nullptr), state(nullptr) {}

  uint32_t mods_depressed () const
  {
    return xkb_state_serialize_mods(state, static_cast<xkb_state_component>(XKB_STATE_DEPRESSED));
  }

  uint32_t mods_latched () const
  {
    return xkb_state_serialize_mods(state, static_cast<xkb_state_component>(XKB_STATE_LATCHED));
  }

  uint32_t mods_locked () const
  {
    return xkb_state_serialize_mods(state, static_cast<xkb_state_component>(XKB_STATE_LOCKED));
  }

  uint32_t group () const
  {
    return xkb_state_serialize_mods(state, static_cast<xkb_state_component>(XKB_STATE_LAYOUT_EFFECTIVE));
  }

  libinput_keyboard (libinput_keyboard const&) = default;
};
      
libinput::libinput_keyboard init ( ::uv_loop_t* loop, std::function<void(uint32_t, uint32_t, uint32_t)> function);

} } }

#endif
