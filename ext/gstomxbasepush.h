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

#ifndef __GST_OMX_BASE_PUSH_H__
#define __GST_OMX_BASE_PUSH_H__

#include "gstomx.h"
#include "gstomxbase.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_BASE_PUSH			\
  (gst_omx_base_push_get_type())
#define GST_OMX_BASE_PUSH(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_BASE_PUSH,GstOmxBasePush))
#define GST_OMX_BASE_PUSH_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_BASE_PUSH,GstOmxBasePushClass))
#define GST_OMX_BASE_PUSH_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMX_BASE_PUSH, GstOmxBasePushClass))
#define GST_IS_OMX_BASE_PUSH(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_BASE_PUSH))
#define GST_IS_OMX_BASE_PUSH_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_BASE_PUSH))
typedef struct _GstOmxBasePush GstOmxBasePush;
typedef struct _GstOmxBasePushClass GstOmxBasePushClass;

struct _GstOmxBasePush
{
  GstOmxBase element;

  /* Queue of buffers pushed on the output */
  GstOmxBufQueue *queue_buffers;
  GstTask *pushtask;
  GStaticRecMutex taskmutex;

  GstFlowReturn fill_ret;
};

struct _GstOmxBasePushClass
{
  GstOmxBaseClass parent_class;
  GstFlowReturn (*push_buffer) (GstOmxBasePush *, OMX_BUFFERHEADERTYPE *);
};

GType gst_omx_base_push_get_type (void);

gboolean gst_omx_base_push_add_pad (GstOmxBasePush *, GstPad *);

typedef gboolean (*GstOmxBasePushCondition) (gpointer, gpointer);
gboolean gst_omx_base_push_condition_state (gpointer targetstate,
    gpointer currentstate);
gboolean gst_omx_base_push_condition_enabled (gpointer enabled, gpointer dummy);
gboolean gst_omx_base_push_condition_disabled (gpointer enabled,
    gpointer dummy);

OMX_ERRORTYPE gst_omx_base_push_wait_for_condition (GstOmxBasePush *,
    GstOmxBasePushCondition, gpointer, gpointer);

void gst_omx_base_push_release_buffer (gpointer data);

G_END_DECLS
#endif /* __GST_OMX_BASE_PUSH_H__ */
