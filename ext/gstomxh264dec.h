/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Carlos Gomez <carlos.gomez@ridgerun.com>
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
 
#ifndef __GST_OMX_H264_DEC_H__
#define __GST_OMX_H264_DEC_H__

#include <gst/gst.h>
#include "gstomxpad.h"
#include "gstomxbase.h"

G_BEGIN_DECLS

#define GST_TYPE_OMX_H264_DEC \
  (gst_omx_h264_dec_get_type())
#define GST_OMX_H264_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_H264_DEC,GstOmxH264Dec))
#define GST_OMX_H264_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_H264_DEC,GstOmxH264DecClass))
#define GST_IS_OMX_H264_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_H264_DEC))
#define GST_IS_OMX_H264_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_H264_DEC))

typedef struct _GstOmxH264Dec      GstOmxH264Dec;
typedef struct _GstOmxH264DecClass GstOmxH264DecClass;

struct _GstOmxH264Dec {
  GstOmxBase element;

  GstPad *srcpad, *sinkpad;
  GstOmxFormat format;
};

struct _GstOmxH264DecClass {
  GstOmxBaseClass parent_class;
};

GType gst_omx_h264_dec_get_type (void);

G_END_DECLS

#endif /* __GST_OMX_H264_DEC_H__ */
