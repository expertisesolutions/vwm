///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_BACKEND_MOUSE_HPP
#define VWM_BACKEND_MOUSE_HPP

#include <cassert>

namespace vwm { namespace backend { namespace xlib {

struct mouse
{
  mouse ()
  {
  }

  mouse (mouse const&) = default;
};

} } }

#endif
