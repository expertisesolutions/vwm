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

#include <vwm/backend/keyboard.hpp>

namespace vwm { namespace backend { namespace libinput {

vwm::keyboard init ( ::uv_loop_t* loop, std::function<void(uint32_t, uint32_t, uint32_t)> function);

} } }

#endif
