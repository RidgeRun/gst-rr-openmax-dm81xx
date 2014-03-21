/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Melissa Montero <melissa.montero@ridgerun.com>
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

#ifndef __GST_OMX_DEISCALER_H__
#define __GST_OMX_DEISCALER_H__

#include "gstomxbase.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_DEISCALER \
  (gst_omx_deiscaler_get_type())
#define GST_TYPE_OMX_HDEISCALER \
  (gst_omx_hdeiscaler_get_type())
#define GST_TYPE_OMX_MDEISCALER \
  (gst_omx_mdeiscaler_get_type())

#define GST_OMX_DEISCALER(obj) \
  ((GstOmxDeiscaler*) obj)
#define GST_OMX_DEISCALER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_DEISCALER,GstOmxDeiscalerClass))
#define GST_IS_OMX_DEISCALER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_DEISCALER))
#define GST_IS_OMX_DEISCALER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_DEISCALER))
  
typedef struct _GstOmxDeiscaler GstOmxDeiscaler;
typedef struct _GstOmxDeiscalerClass GstOmxDeiscalerClass;

typedef struct _GstOmxDeiscaler GstOmxHDeiscaler;
typedef struct _GstOmxDeiscalerClass GstOmxHDeiscalerClass;
typedef struct _GstOmxDeiscaler GstOmxMDeiscaler;
typedef struct _GstOmxDeiscalerClass GstOmxMDeiscalerClass;

typedef struct _GstCropArea GstCropArea;
struct _GstCropArea
{
  guint x;
  guint y;
  guint width;
  guint height;
};

struct _GstOmxDeiscaler
{
  GstOmxBase base;

  GstPad *sinkpad;
  GList *srcpads;

  GstOmxFormat in_format;
  GList *out_formats;
  
  /* Properties */
  guint framerate_divisor;
  gchar *crop_str;
  GstCropArea crop_area;
};

struct _GstOmxDeiscalerClass
{
  GstOmxBaseClass parent_class;
};

GType gst_omx_deiscaler_get_type (void);
GType gst_omx_hdeiscaler_get_type (void);
GType gst_omx_mdeiscaler_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_DEISCALER_H__ */
