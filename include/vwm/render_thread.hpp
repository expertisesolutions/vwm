///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_RENDER_THREAD_HPP
#define VWM_RENDER_THREAD_HPP

#include <ftk/ui/toplevel_window.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>

namespace vwm {

template <typename Backend>
std::thread render_thread (ftk::ui::toplevel_window<Backend>* toplevel, bool& dirty, bool& exit, std::mutex& mutex, std::condition_variable& cond)
{
  std::thread thread([toplevel, &dirty, &exit, &mutex, &cond]
                     {
                       while (!exit)
                       {
                         std::unique_lock<std::mutex> l(mutex);
                         while (!dirty && !exit)
                         {
                           cond.wait(l);
                         }
                         if (exit) return;
                         std::cout << "drawing" << std::endl;

                         // for (auto&& buffer : toplevel->buffers)
                         // {
                           
                         // }
                       }
                     });
  return thread;
}

inline std::function<void()> render_dirty (bool& dirty, std::mutex& mutex, std::condition_variable& cond)
{
  return [dirty = &dirty, mutex = &mutex, cond = &cond]
         {
           std::unique_lock<std::mutex> l(*mutex);
           *dirty = true;
           cond->notify_one();
         };
}

inline std::function<void()> render_exit (bool& exit, std::mutex& mutex, std::condition_variable& cond)
{
  return [exit = &exit, mutex = &mutex, cond = &cond]
         {
           std::unique_lock<std::mutex> l(*mutex);
           *exit = true;
           cond->notify_one();
         };
}
  
}

#endif

