/* 
 * GStreamer
 * Copyright (C) 2014 RidgeRun, LLC (http://www.ridgerun.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gstomxutils.h"

/**
 * gst_omx_convert_format_to_omx: 
 * @ format: gstreamer video format
 * 
 * A convenient function to convert a gstreamer video format GstVideoFormat
 * into an OMX video format OMX_COLOR_FORMATTYPE.
 * 
 * Returns: A OMX_COLOR_FORMATTYPE corresponding to @format or
 * OMX_COLOR_FormatUnused if can't find a GstVideoFormat that match it.
 */
OMX_COLOR_FORMATTYPE
gst_omx_convert_format_to_omx (GstVideoFormat format)
{
  OMX_COLOR_FORMATTYPE omx_format;

  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      omx_format = OMX_COLOR_FormatYUV420SemiPlanar;
      break;
    case GST_VIDEO_FORMAT_YUY2:
      omx_format = OMX_COLOR_FormatYCbYCr;
      break;
    default:
      omx_format = OMX_COLOR_FormatUnused;
      break;
  }

  return omx_format;

}
