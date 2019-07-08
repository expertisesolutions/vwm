///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_BACKEND_KEYBOARD_HPP
#define VWM_BACKEND_KEYBOARD_HPP

#include <cassert>
#include <xkbcommon/xkbcommon.h>

namespace vwm { namespace backend { namespace xlib {

struct keyboard
{
  static const constexpr bool has_keymap = true;
  
  xkb_context* context;
  xkb_keymap* keymap;
  xkb_state* state;

  keyboard ()
  {
    xkb_rule_names rules = {};
    rules.layout = "us";
    rules.model = "pc105";
    rules.variant = "intl";

    context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    assert (context != nullptr);
    keymap = xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    assert (!!keymap);
    state = xkb_state_new (keymap);
    assert (state != nullptr);
  }

  void update_state (uint32_t keycode, bool is_pressed)
  {
    xkb_state_update_mask (this->state, mods_depressed(), mods_latched(), mods_locked(), 0, 0, group());
    xkb_state_update_key (this->state, keycode, is_pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
    std::cout << "updated" << std::endl;
  }
  
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

  keyboard (keyboard const&) = default;
};

} } }

#endif
