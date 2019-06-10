///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_VWM_UV_DETAIL_POLL_HPP
#define VWM_VWM_UV_DETAIL_POLL_HPP

namespace vwm { namespace ui { namespace detail {

template <typename F>
void wait (uv_loop_t* loop, int fd, int events, F function)
{
  uv_poll_t* handle = new uv_poll_t;
  ::uv_poll_init (loop, handle, fd);

  handle->data = new F(std::move(function));
  
  uv_poll_cb cb = [] (uv_poll_t* handle, int status, int event)
                  {
		    std::cout << "event " << event << std::endl;
                    if (status == 0)
                    {
                      (*static_cast<F*>(handle->data))();
                    }
                  };
  uv_poll_start (handle, UV_READABLE, cb);
}
  
} } }

#endif
