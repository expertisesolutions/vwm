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

#include <uv.h>

#include <utility>

namespace vwm { namespace ui { namespace detail {

template <typename F>
void wait (uv_loop_t* loop, int fd, int events, F function)
{
  uv_poll_t* handle = new uv_poll_t;
  ::uv_poll_init (loop, handle, fd);

  handle->data = new F(std::move(function));
  
  uv_poll_cb cb = [] (uv_poll_t* handle, int status, int event)
                  {
		    //std::cout << "event " << event << std::endl;
                    if (status == 0)
                    {
                      (*static_cast<F*>(handle->data))(handle);
                    }
                  };
  uv_poll_start (handle, UV_READABLE, cb);
}

template <typename F>
void uv_async_cb (uv_async_t* handle)
{
  F* f = static_cast<F*>(handle->data);
  (*f)();
  delete f;
  uv_close (static_cast<uv_handle_t*>(static_cast<void*>(handle)), nullptr);
}
      
template <typename F>
void async (uv_loop_t* loop, F function)
{
  uv_async_t* handle = new uv_async_t;
  ::uv_async_init (loop, handle, static_cast< ::uv_async_cb>(&uv_async_cb<F>));

  handle->data = new F(std::move(function));
  
  uv_async_send (handle);
}
      
} } }

#endif
