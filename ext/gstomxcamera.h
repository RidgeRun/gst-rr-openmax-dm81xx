/* 
 * GStreamer
 * Copyright (C) 2014 Melissa Montero <melissa.montero@ridgerun.com>
 * Copyright (C) 2014 Jose Jimenez <jose.jimenez@ridgerun.com> 
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

#ifndef __GST_OMX_CAMERA_H__
#define __GST_OMX_CAMERA_H__

#include <gst/gst.h>
#include "gstomxpad.h"
#include "gstomxbasesrc.h"


G_BEGIN_DECLS
#define GST_TYPE_OMX_CAMERA \
  (gst_omx_camera_get_type())
#define GST_OMX_CAMERA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_CAMERA,GstOmxCamera))
#define GST_OMX_CAMERA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_CAMERA,GstOmxCameraClass))
#define GST_IS_OMX_CAMERA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_CAMERA))
#define GST_IS_OMX_CAMERA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_CAMERA))
typedef struct _GstOmxCamera GstOmxCamera;
typedef struct _GstOmxCameraClass GstOmxCameraClass;

struct _GstOmxCamera
{
  GstOmxBaseSrc element;

  GstPad *srcpad;
  GstOmxFormat format;

  /* properties */
  gint interface;
  gint capt_mode;
  gint vip_mode;
  gint scan_type;
  guint skip_frames;
  gboolean field_merged;
};


struct _GstOmxCameraClass
{
  GstOmxBaseSrcClass parent_class;
};

GType gst_omx_camera_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_CAMERA_H__ */
