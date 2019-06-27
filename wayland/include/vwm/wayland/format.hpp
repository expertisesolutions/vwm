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
 argb8888 = 0
 , xrgb8888 = 1
 , c8 = 0x20203843
 , rgb332 = 0x38424752
 , bgr233 = 0x38524742
 , xrgb4444 = 0x32315258
 , xbgr4444 = 0x32314258
 , rgbx4444 = 0x32315852
 , bgrx4444 = 0x32315842
 , argb4444 = 0x32315241
 , abgr4444 = 0x32314241
 , rgba4444 = 0x32314152
 , bgra4444 = 0x32314142
 , xrgb1555 = 0x35315258
 , xbgr1555 = 0x35314258
 , rgbx5551 = 0x35315852
 , bgrx5551 = 0x35315842
 , argb1555 = 0x35315241
 , abgr1555 = 0x35314241
 , rgba5551 = 0x35314152
 , bgra5551 = 0x35314142
 , rgb565 = 0x36314752
 , bgr565 = 0x36314742
 , rgb888 = 0x34324752
 , bgr888 = 0x34324742
 , xbgr8888 = 0x34324258
 , rgbx8888 = 0x34325852
 , bgrx8888 = 0x34325842
 , abgr8888 = 0x34324241
 , rgba8888 = 0x34324152
 , bgra8888 = 0x34324142
 , xrgb2101010 = 0x30335258
 , xbgr2101010 = 0x30334258
 , rgbx1010102 = 0x30335852
 , bgrx1010102 = 0x30335842
 , argb2101010 = 0x30335241
 , abgr2101010 = 0x30334241
 , rgba1010102 = 0x30334152
 , bgra1010102 = 0x30334142
 , yuyv = 0x56595559
 , yvyu = 0x55595659
 , uyvy = 0x59565955
 , vyuy = 0x59555956
 , ayuv = 0x56555941
 , nv12 = 0x3231564e
 , nv21 = 0x3132564e
 , nv16 = 0x3631564e
 , nv61 = 0x3136564e
 , yuv410 = 0x39565559
 , yvu410 = 0x39555659
 , yuv411 = 0x31315559
 , yvu411 = 0x31315659
 , yuv420 = 0x32315559
 , yvu420 = 0x32315659
 , yuv422 = 0x36315559
 , yvu422 = 0x36315659
 , yuv444 = 0x34325559
 , yvu444 = 0x34325659
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
