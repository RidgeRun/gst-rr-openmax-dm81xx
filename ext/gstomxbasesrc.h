/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Jose Jimenez <jose.jimenez@ridgerun.com>
 * Copyright (C) 2014 Ronny Jimenez <ronny.jimenez@ridgerun.com>
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

#ifndef __GST_OMX_BASE_SRC_H__
#define __GST_OMX_BASE_SRC_H__

#include "gstomx.h"
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS
#define GST_TYPE_OMX_BASE_SRC			\
  (gst_omx_base_src_get_type())
#define GST_OMX_BASE_SRC(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_BASE_SRC,GstOmxBaseSrc))
#define GST_OMX_BASE_SRC_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_BASE_SRC,GstOmxBaseSrcClass))
#define GST_OMX_BASE_SRC_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMX_BASE_SRC, GstOmxBaseSrcClass))
#define GST_IS_OMX_BASE_SRC(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_BASE_SRC))
#define GST_IS_OMX_BASE_SRC_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_BASE_SRC))

typedef struct _GstOmxBaseSrc GstOmxBaseSrc;
typedef struct _GstOmxBaseSrcClass GstOmxBaseSrcClass;

typedef OMX_ERRORTYPE (*GstOmxBaseSrcPadFunc) (GstOmxBaseSrc *, GstOmxPad *, gpointer);

struct _GstOmxBaseSrc
{
  GstPushSrc element;

  OMX_HANDLETYPE handle;
  OMX_COMPONENTTYPE *component;
  OMX_CALLBACKTYPE *callbacks;

  guint32 requested_size;
  guint32 field_offset;

  guint output_buffers;

  gboolean peer_alloc;
  gboolean flushing;
  gboolean started;
  gboolean first_buffer;
  gboolean interlaced;

  OMX_STATETYPE state;
  GMutex waitmutex;
  GCond waitcond;

  GstFlowReturn fill_ret;

  GList *pads;
};

struct _GstOmxBaseSrcClass
{
  GstPushSrcClass parent_class;

  gchar *handle_name;
  
  OMX_ERRORTYPE (*omx_event) (GstOmxBaseSrc *, OMX_EVENTTYPE, guint32,
				guint32, gpointer);
  GstFlowReturn (*omx_fill_buffer) (GstOmxBaseSrc *, OMX_BUFFERHEADERTYPE *);
  GstFlowReturn (*omx_empty_buffer) (GstOmxBaseSrc *, OMX_BUFFERHEADERTYPE *);

};

GType gst_omx_base_src_get_type (void);
gboolean gst_omx_base_src_add_pad (GstOmxBaseSrc *, GstPad *);

typedef gboolean (*GstOmxBaseSrcCondition) (gpointer, gpointer);
gboolean gst_omx_base_src_condition_state (gpointer targetstate,
    gpointer currentstate);
gboolean gst_omx_base_src_condition_enabled (gpointer enabled, gpointer dummy);
gboolean gst_omx_base_src_condition_disabled (gpointer enabled, gpointer dummy);

OMX_ERRORTYPE gst_omx_base_src_wait_for_condition (GstOmxBaseSrc *,
    GstOmxBaseSrcCondition, gpointer, gpointer);

void gst_omx_base_src_release_buffer (gpointer data);

G_END_DECLS
#endif /* __GST_OMX_BASE_H__ */

