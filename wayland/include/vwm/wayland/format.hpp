///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef VWM_WAYLAND_FORMAT_HPP
#define VWM_WAYLAND_FORMAT_HPP

namespace vwm { namespace wayland {

enum class format
{
 argb8888
 , xrgb8888
 , c8
 , rgb332
 , bgr233
 , xrgb4444
 , xbgr4444
 , rgbx4444
 , bgrx4444
 , argb4444
 , abgr4444
 , rgba4444
 , bgra4444
 , xrgb1555
 , xbgr1555
 , rgbx5551
 , bgrx5551
 , argb1555
 , abgr1555
 , rgba5551
 , bgra5551
 , rgb565
 , bgr565
 , rgb888
 , bgr888
 , xbgr8888
 , rgbx8888
 , bgrx8888
 , abgr8888
 , rgba8888
 , bgra8888
 , xrgb2101010
 , xbgr2101010
 , rgbx1010102
 , bgrx1010102
 , argb2101010
 , abgr2101010
 , rgba1010102
 , bgra1010102
 , yuyv
 , yvyu
 , uyvy
 , vyuy
 , ayuv
 , nv12
 , nv21
 , nv16
 , nv61
 , yuv410
 , yvu410
 , yuv411
 , yvu411
 , yuv420
 , yvu420
 , yuv422
 , yvu422
 , yuv444
 , yvu444
};

const char* format_description (format f)
{
  switch (f)
  {
  case format::argb8888: return "32-bit ARGB format, [31:0] A:R:G:B 8:8:8:8 little endian";
  case format::xrgb8888: return "32-bit RGB format, [31:0] x:R:G:B 8:8:8:8 little endian";
  case format::c8: return "8-bit color index format, [7:0] C";
  case format::rgb332: return "8-bit RGB format, [7:0] R:G:B 3:3:2";
  case format::bgr233: return "8-bit BGR format, [7:0] B:G:R 2:3:3";
  case format::xrgb4444: return "16-bit xRGB format, [15:0] x:R:G:B 4:4:4:4 little endian";
  case format::xbgr4444: return "16-bit xBGR format, [15:0] x:B:G:R 4:4:4:4 little endian";
  case format::rgbx4444: return "16-bit RGBx format, [15:0] R:G:B:x 4:4:4:4 little endian";
  case format::bgrx4444: return "16-bit BGRx format, [15:0] B:G:R:x 4:4:4:4 little endian";
  case format::argb4444: return "16-bit ARGB format, [15:0] A:R:G:B 4:4:4:4 little endian";
  case format::abgr4444: return "16-bit ABGR format, [15:0] A:B:G:R 4:4:4:4 little endian";
  case format::rgba4444: return "16-bit RBGA format, [15:0] R:G:B:A 4:4:4:4 little endian";
  case format::bgra4444: return "16-bit BGRA format, [15:0] B:G:R:A 4:4:4:4 little endian";
  case format::xrgb1555: return "16-bit xRGB format, [15:0] x:R:G:B 1:5:5:5 little endian";
  case format::xbgr1555: return "16-bit xBGR 1555 format, [15:0] x:B:G:R 1:5:5:5 little endian";
  case format::rgbx5551: return "16-bit RGBx 5551 format, [15:0] R:G:B:x 5:5:5:1 little endian";
  case format::bgrx5551: return "16-bit BGRx 5551 format, [15:0] B:G:R:x 5:5:5:1 little endian";
  case format::argb1555: return "16-bit ARGB 1555 format, [15:0] A:R:G:B 1:5:5:5 little endian";
  case format::abgr1555: return "16-bit ABGR 1555 format, [15:0] A:B:G:R 1:5:5:5 little endian";
  case format::rgba5551: return "16-bit RGBA 5551 format, [15:0] R:G:B:A 5:5:5:1 little endian";
  case format::bgra5551: return "16-bit BGRA 5551 format, [15:0] B:G:R:A 5:5:5:1 little endian";
  case format::rgb565: return "16-bit RGB 565 format, [15:0] R:G:B 5:6:5 little endian";
  case format::bgr565: return "16-bit BGR 565 format, [15:0] B:G:R 5:6:5 little endian";
  case format::rgb888: return "24-bit RGB format, [23:0] R:G:B little endian";
  case format::bgr888: return "24-bit BGR format, [23:0] B:G:R little endian";
  case format::xbgr8888: return "32-bit xBGR format, [31:0] x:B:G:R 8:8:8:8 little endian";
  case format::rgbx8888: return "32-bit RGBx format, [31:0] R:G:B:x 8:8:8:8 little endian";
  case format::bgrx8888: return "32-bit BGRx format, [31:0] B:G:R:x 8:8:8:8 little endian";
  case format::abgr8888: return "32-bit ABGR format, [31:0] A:B:G:R 8:8:8:8 little endian";
  case format::rgba8888: return "32-bit RGBA format, [31:0] R:G:B:A 8:8:8:8 little endian";
  case format::bgra8888: return "32-bit BGRA format, [31:0] B:G:R:A 8:8:8:8 little endian";
  case format::xrgb2101010: return "32-bit xRGB format, [31:0] x:R:G:B 2:10:10:10 little endian";
  case format::xbgr2101010: return "32-bit xBGR format, [31:0] x:B:G:R 2:10:10:10 little endian";
  case format::rgbx1010102: return "32-bit RGBx format, [31:0] R:G:B:x 10:10:10:2 little endian";
  case format::bgrx1010102: return "32-bit BGRx format, [31:0] B:G:R:x 10:10:10:2 little endian";
  case format::argb2101010: return "32-bit ARGB format, [31:0] A:R:G:B 2:10:10:10 little endian";
  case format::abgr2101010: return "32-bit ABGR format, [31:0] A:B:G:R 2:10:10:10 little endian";
  case format::rgba1010102: return "32-bit RGBA format, [31:0] R:G:B:A 10:10:10:2 little endian";
  case format::bgra1010102: return "32-bit BGRA format, [31:0] B:G:R:A 10:10:10:2 little endian";
  case format::yuyv: return "packed YCbCr format, [31:0] Cr0:Y1:Cb0:Y0 8:8:8:8 little endian";
  case format::yvyu: return "packed YCbCr format, [31:0] Cb0:Y1:Cr0:Y0 8:8:8:8 little endian";
  case format::uyvy: return "packed YCbCr format, [31:0] Y1:Cr0:Y0:Cb0 8:8:8:8 little endian";
  case format::vyuy: return "packed YCbCr format, [31:0] Y1:Cb0:Y0:Cr0 8:8:8:8 little endian";
  case format::ayuv: return "packed AYCbCr format, [31:0] A:Y:Cb:Cr 8:8:8:8 little endian";
  case format::nv12: return "2 plane YCbCr Cr:Cb format, 2x2 subsampled Cr:Cb plane";
  case format::nv21: return "2 plane YCbCr Cb:Cr format, 2x2 subsampled Cb:Cr plane";
  case format::nv16: return "2 plane YCbCr Cr:Cb format, 2x1 subsampled Cr:Cb plane";
  case format::nv61: return "2 plane YCbCr Cb:Cr format, 2x1 subsampled Cb:Cr plane";
  case format::yuv410: return "3 plane YCbCr format, 4x4 subsampled Cb (1) and Cr (2) planes";
  case format::yvu410: return "3 plane YCbCr format, 4x4 subsampled Cr (1) and Cb (2) planes";
  case format::yuv411: return "3 plane YCbCr format, 4x1 subsampled Cb (1) and Cr (2) planes";
  case format::yvu411: return "3 plane YCbCr format, 4x1 subsampled Cr (1) and Cb (2) planes";
  case format::yuv420: return "3 plane YCbCr format, 2x2 subsampled Cb (1) and Cr (2) planes";
  case format::yvu420: return "3 plane YCbCr format, 2x2 subsampled Cr (1) and Cb (2) planes";
  case format::yuv422: return "3 plane YCbCr format, 2x1 subsampled Cb (1) and Cr (2) planes";
  case format::yvu422: return "3 plane YCbCr format, 2x1 subsampled Cr (1) and Cb (2) planes";
  case format::yuv444: return "3 plane YCbCr format, non-subsampled Cb (1) and Cr (2) planes";
  case format::yvu444: return "3 plane YCbCr format, non-subsampled Cr (1) and Cb (2) planes";
  default: return "unknown";
  };
}

} }

#endif
