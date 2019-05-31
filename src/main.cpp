#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <libinput.h>
#include <libudev.h>
#include <uv.h>

#include <cassert>
#include <iostream>

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
int main(void) {
  struct udev *udev;
  struct libinput *li;
  struct libinput_event *event;
  ::uv_loop_t loop;

  uv_loop_init (&loop);

  udev = udev_new();
  assert (udev != nullptr);
  li = libinput_udev_create_context(&interface, NULL, udev);
  assert (li != nullptr);
  libinput_udev_assign_seat(li, "seat0");

  int fd = libinput_get_fd(li);

  uv_poll_t* handle = new uv_poll_t;
  ::uv_poll_init (&loop, handle, fd);

  auto lambda = 
    [&]
    {
      std::cout << "event on fd" << std::endl;
      libinput_dispatch(li);
      while ((event = libinput_get_event(li)) != NULL) {
        // handle the event here
        std::cout << "event is not null" << std::endl;
    
        libinput_event_destroy(event);
        libinput_dispatch(li);
      }
      std::cout << "event is null" << std::endl;
    };
  using lambda_type = decltype(lambda);

  handle->data = new lambda_type(lambda);
  
  uv_poll_cb cb = [] (uv_poll_t* handle, int status, int event)
                  {
                    if (status == 0)
                    {
                      (*static_cast<lambda_type*>(handle->data))();
                    }
                  };

  uv_poll_start (handle, UV_READABLE, cb);
  
  uv_run (&loop, UV_RUN_DEFAULT);
  
  
  libinput_unref(li);
  return 0;
}
