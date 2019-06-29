///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_TYPES_HPP
#define VWM_WAYLAND_TYPES_HPP

#include <vwm/wayland/sbo.hpp>

namespace vwm { namespace wayland {

struct fixed
{
  uint32_t value;
};

struct array_base
{
  array_base (int sizeT)
    : data_(nullptr)
    , size_ (0u)
  {}
  void* data() const
  {
    return data_;
  }
  std::uint32_t bytes_size () const
  {
    return size_;
  }
  std::size_t size () const { return size_ / sizeT_; }
protected:
  bool needs_allocation (std::size_t new_size)
  {
    return new_size * sizeT_ > allocated_;
  }  

  void* data_;
  int sizeT_;
  unsigned int size_;
  unsigned int allocated_;
};
    
template <typename T, typename Allocator = std::allocator<T>>
struct array : array_base
{
  array () : array_base (sizeof(T)) {}
  array (std::initializer_list<T> l)
    : array_base (sizeof(T))
  {
    for(auto&& i : l)
    {
      push_back (i);
    }
  }
  T* data()
  {
    return static_cast<T*>(data_);
  }
  void push_back (T object)
  {
    if (needs_allocation (size_ + 1))
      _grow();
    nonchecked_push_back (object);
  }
  std::size_t size () const { return bytes_size/sizeof(T); }
  void reserve (std::size_t new_size)
  {
    if (needs_allocation (new_size))
    {
      auto new_p = allocator.allocate (new_size);
      auto p = new_p;
      for (auto data = this->data();data != this->data() + size_; ++data)
        allocator.construct (p, std::move(*data));
      //size_ = new_size;
      std::swap (new_p, data_);
    }
  }
private:
  void _grow()
  {
    reserve (size_ << 1);
  }
  void nonchecked_push_back (T object)
  {
    new (static_cast<T*>(data()) + size()) T(std::move(object));
    size_++;
  }
  Allocator allocator;
};

template <>
struct array<std::string_view> : array_base
{
  // std::size_t size () const { assert (bytes_size%sizeof(T) == 0); return bytes_size/sizeof(T); }
};

std::size_t marshall_size (std::string_view v)
{
  std::size_t rest = (v.size() + 1) % sizeof(std::uint32_t);
  auto r = sizeof (std::uint32_t) + v.size() + 1 + (rest == 0 ? 0 : sizeof(std::uint32_t) - rest);
  std::cout << "marshall size for string " << v << " is " << r << std::endl;
  return r;
}
std::size_t marshall_size (array_base const& v)
{
  return sizeof(uint32_t) + v.bytes_size();
}
std::size_t marshall_size (std::uint32_t)
{
  return sizeof(std::uint32_t);
}
std::size_t marshall_size (int)
{
  return 0;
}
void marshall_send (int fd, std::string_view v)
{
  sbo<char> buffer;
  buffer.resize (marshall_size(v));
  auto data = static_cast<char*>(buffer.data());
  std::memset (data, 0, marshall_size(v));
  std::uint32_t size = v.size() + 1;
  std::memcpy (data, &size, sizeof(size));
  std::memcpy (&data[sizeof(std::uint32_t)], v.data(), v.size());
  //std::cout << "sending marshalling variadic marshal size " << marshall_size(v) << " and normal size " << size << std::endl;
  send (fd, data, marshall_size(v), 0);
}
void marshall_send (int fd, array_base const& array)
{
  std::uint32_t size = array.bytes_size();
  send (fd, &size, sizeof(size), 0);
  send (fd, array.data(), size, 0);
}
void marshall_send (int sock, int fd)
{
}
void send_with_fds (int sock, void* buffer, std::size_t length)
{
  send (sock, buffer, length, 0);
}
template <typename F, typename...T>
void send_with_fds (int sock, void* buffer, std::size_t length
                    , F fd, T...fds)
{
  std::cout << "sending file descriptor " << fd << " plus " << sizeof...(fds) << " file descriptors" << std::endl;
  
  //ssize_t size;
  char control[CMSG_SPACE(sizeof(int32_t) + 1 + sizeof...(fds))];
  struct iovec iov = {
		.iov_base = buffer,
		.iov_len = length,
  };
  struct msghdr message = {
		.msg_name = NULL,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = &control,
		.msg_controllen = sizeof(control),
                .msg_flags = 0,
  };
  struct cmsghdr* cmsg = static_cast<cmsghdr*>(static_cast<void*>(&control));
  cmsg->cmsg_len = CMSG_LEN(sizeof(int32_t) + 1 + sizeof...(fds));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  auto fd_buffer = static_cast<int*>(static_cast<void*>(CMSG_DATA(cmsg)));
  *fd_buffer++ = fd;
  ((*fd_buffer++ = fds), ...);
  
  
  int r = sendmsg (sock, &message, 0);

  std::cout << "sent " << r << std::endl;

  if (r < 0)
  {
    perror ("error: ");
    throw std::system_error( std::error_code (errno, std::system_category()));
  }
}

template <typename Client>
void unmarshall (std::string_view& string, std::string_view payload, Client&)
{
  std::uint32_t size;
  if (payload.size() >= sizeof(size))
  {
    std::memcpy (&size, payload.data(), sizeof(size));
    if (payload.size() >= sizeof(size) + size)
    {
      string = std::string_view {payload.data() + sizeof(size), size-1 /* NUL-terminated */};
    }
  }
}

template <typename Client>
void unmarshall (int& fd, std::string_view payload, Client& c)
{
  if (c.fds.empty())
    throw std::runtime_error ("no fds avaialable in ancillary data");

  fd = c.fds.front();
  c.fds.pop_back();
}
    
unsigned unmarshall_size (std::string_view payload, std::string_view string)
{
  std::abort();
  return 0;
}
    
} }

#endif
