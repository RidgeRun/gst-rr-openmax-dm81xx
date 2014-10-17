/* 
 * GStreamer
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
 
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_TI_Common.h>
#include <OMX_TI_Index.h>

#include <gst/gst.h>

typedef struct _GstOmxBufQueue GstOmxBufQueue;

struct _GstOmxBufQueue
{
  GQueue *queue;
  guint queueused;
  GMutex queuemutex;
  GCond queuecond;
};

GstOmxBufQueue *gst_omx_buf_queue_new ();
OMX_BUFFERHEADERTYPE* gst_omx_buf_queue_pop_buffer (GstOmxBufQueue *);
OMX_ERRORTYPE gst_omx_buf_queue_push_buffer (GstOmxBufQueue *, OMX_BUFFERHEADERTYPE *);
