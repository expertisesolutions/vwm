///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_PROTOCOL_OVERLOAD_HPP

namespace vwm { namespace protocol {

template <typename...Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};

template <typename...Ts>
overloaded<std::remove_reference_t<Ts>...> overload (Ts&&... ts)
{
  return overloaded<std::remove_reference_t<Ts>...>{std::move(ts)...};
}

} }
    
#endif
