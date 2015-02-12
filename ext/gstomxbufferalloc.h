/*
 * GStreamer
 * Copyright (C) 2015 Eugenia Guzman <eugenia.guzman@ridgerun.com>
 * Copyright (C) 2014 Diego Solano <diego.solano@ridgerun.com>
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

#ifndef __GST_OMXBUFFERALLOC_H__
#define __GST_OMXBUFFERALLOC_H__

#include "gstomx.h"
#include <stdio.h>
#include <glib.h>

#include <xdc/std.h>
#include <ti/syslink/utils/IHeap.h>

G_BEGIN_DECLS

#define GST_TYPE_OMXBUFFERALLOC \
  (gst_omx_buffer_alloc_get_type())
#define GST_OMXBUFFERALLOC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMXBUFFERALLOC,GstOmxBufferAlloc))
#define GST_OMXBUFFERALLOC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMXBUFFERALLOC,GstOmxBufferAllocClass))
#define GST_IS_OMXBUFFERALLOC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMXBUFFERALLOC))
#define GST_IS_OMXBUFFERALLOC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMXBUFFERALLOC))

typedef struct _GstOmxBufferAlloc      GstOmxBufferAlloc;
typedef struct _GstOmxBufferAllocClass GstOmxBufferAllocClass;

struct _GstOmxBufferAlloc
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean listfull;

  gboolean silent;
  guint num_buffers;
  char *omx_library;
  guint allocSize;
  guint cnt;

  OMX_BUFFERHEADERTYPE **buffers;
  IHeap_Handle heap;
};

struct _GstOmxBufferAllocClass
{
  GstElementClass parent_class;
};

GType gst_omx_buffer_alloc_get_type (void);

#define GST_OMX_BUFFER_FLAG (GST_BUFFER_FLAG_LAST << 0)

G_END_DECLS

#endif /* __GST_OMXBUFFERALLOC_H__ */
