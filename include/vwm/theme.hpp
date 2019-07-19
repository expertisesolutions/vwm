///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_VWM_THEME_HPP
#define VWM_VWM_THEME_HPP

#include <future>

namespace vwm {

enum class theme_image
{
  pointer
};
  
template <typename ImageLoader, typename OutputImageLoader>
struct theme
{
  ImageLoader image_loader;
  OutputImageLoader output_image_loader;
  std::filesystem::path resource_root;
  
  theme(ImageLoader image_loader, OutputImageLoader output_image_loader
        , std::filesystem::path resource_root)
    : image_loader (image_loader), output_image_loader (output_image_loader)
    , resource_root (resource_root)
  {
  }

  using output_image_type = typename OutputImageLoader::output_image_type;
  
  pc::future<output_image_type> operator[] (theme_image v) const
  {
    switch (v)
    {
    case theme_image::pointer:
      return output_image_loader.load (resource_root / "res/theme/img/pointer.png", image_loader);
    default:
      throw -1;
    }
  }
};
  
}

#endif
