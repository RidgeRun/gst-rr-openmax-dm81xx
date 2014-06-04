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

#ifndef __GST_OMX_UTILS_H__
#define __GST_OMX_UTILS_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <OMX_Core.h>
#include <OMX_IVCommon.h>

G_BEGIN_DECLS
    OMX_COLOR_FORMATTYPE gst_omx_convert_format_to_omx (GstVideoFormat format);
G_END_DECLS
#endif // __GST_OMX_UTILS_H__
