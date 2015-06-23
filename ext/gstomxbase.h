/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
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

#ifndef __GST_OMX_BASE_H__
#define __GST_OMX_BASE_H__

#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_BASE			\
  (gst_omx_base_get_type())
#define GST_OMX_BASE(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_BASE,GstOmxBase))
#define GST_OMX_BASE_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_BASE,GstOmxBaseClass))
#define GST_OMX_BASE_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMX_BASE, GstOmxBaseClass))
#define GST_IS_OMX_BASE(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_BASE))
#define GST_IS_OMX_BASE_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_BASE))
typedef struct _GstOmxBase GstOmxBase;
typedef struct _GstOmxBaseClass GstOmxBaseClass;

typedef OMX_ERRORTYPE (*GstOmxBasePadFunc) (GstOmxBase *, GstOmxPad *,
    gpointer);

struct _GstOmxBase
{
  GstElement element;

  OMX_HANDLETYPE handle;
  OMX_COMPONENTTYPE *component;
  OMX_CALLBACKTYPE *callbacks;

  guint32 requested_size;
  guint32 field_offset;

  guint input_buffers;
  guint output_buffers;

  guint num_buffers;
  guint cont;
  GCond *num_buffers_cond;
  GMutex *num_buffers_mutex;

  gboolean peer_alloc;
  gboolean flushing;
  gboolean started;
  gboolean first_buffer;
  gboolean interlaced;
  gboolean joined_fields;
  gboolean audio_component;

  OMX_STATETYPE state;
  GMutex waitmutex;
  GCond waitcond;
  
  /*Conditions for Paused State*/
  GMutex pausedwaitmutex;
  GCond pausedwaitcond;
  gboolean block_buffers;
  gint prerolled_buffers;

  GstFlowReturn fill_ret;

  GList *pads;
};

struct _GstOmxBaseClass
{
  GstElementClass parent_class;

  gchar *handle_name;

    OMX_ERRORTYPE (*omx_event) (GstOmxBase *, OMX_EVENTTYPE, guint32,
      guint32, gpointer);
    GstFlowReturn (*omx_fill_buffer) (GstOmxBase *, OMX_BUFFERHEADERTYPE *);
    GstFlowReturn (*omx_empty_buffer) (GstOmxBase *, OMX_BUFFERHEADERTYPE *);
    OMX_ERRORTYPE (*init_ports) (GstOmxBase *);
    gboolean (*parse_caps) (GstPad *, GstCaps *);
  GstCaps *(*parse_buffer) (GstOmxBase *, GstBuffer *);

};

GType gst_omx_base_get_type (void);
gboolean gst_omx_base_add_pad (GstOmxBase *, GstPad *);

typedef gboolean (*GstOmxBaseCondition) (gpointer, gpointer);
gboolean gst_omx_base_condition_state (gpointer targetstate,
    gpointer currentstate);
gboolean gst_omx_base_condition_enabled (gpointer enabled, gpointer dummy);
gboolean gst_omx_base_condition_disabled (gpointer enabled, gpointer dummy);

OMX_ERRORTYPE gst_omx_base_wait_for_condition (GstOmxBase *,
    GstOmxBaseCondition, gpointer, gpointer);

void gst_omx_base_release_buffer (gpointer data);

G_END_DECLS
#endif /* __GST_OMX_BASE_H__ */
