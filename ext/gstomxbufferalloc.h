/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2014 Eugenia Guzman <eugenia.guzman@ridgerun.com>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
//#include <ti/ipc/SharedRegion.h>



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


typedef struct GOmxPort GOmxPort;

typedef struct {
    GOmxPort *port;
	gint refcnt;
	GMutex *mutex;
} GstOmxPortPtr;

static inline GstOmxPortPtr* gst_omxportptr_new(GOmxPort *port) {
	GstOmxPortPtr *p = g_new0(GstOmxPortPtr, 1);
	if (p) {
		p->refcnt = 1;
		p->port = port;
		p->mutex = g_mutex_new();
		if (!p->mutex) { g_free(p); return NULL; }
	}
	return p;
}

static inline GstOmxPortPtr *gst_omxportptr_ref(GstOmxPortPtr *self) {
	g_mutex_lock(self->mutex);
	self->refcnt++;
	g_mutex_unlock(self->mutex);
	return self;
}

enum GOmxPortType
{
    GOMX_PORT_INPUT,
    GOMX_PORT_OUTPUT
};

typedef struct _GstOmxBufferAlloc      GstOmxBufferAlloc;
typedef struct _GstOmxBufferAllocClass GstOmxBufferAllocClass;

typedef struct GOmxImp GOmxImp;
typedef struct GOmxSymbolTable GOmxSymbolTable;
typedef enum GOmxPortType GOmxPortType;

struct GOmxPort
{
	//~ OMX_BUFFERHEADERTYPE **buffers;
	//~ GstBuffer * (*buffer_alloc)(GOmxPort *port, gint len);
	//~ GstOmxPortPtr *portptr;
	//~ GOmxPortType type;
	//~ OMX_HANDLETYPE omx_handle;
	//~ GstCaps *caps;
};

struct GOmxSymbolTable
{
    OMX_ERRORTYPE (*init) (void);
    OMX_ERRORTYPE (*deinit) (void);
    OMX_ERRORTYPE (*get_handle) (OMX_HANDLETYPE *handle,
                                 OMX_STRING name,
                                 OMX_PTR data,
                                 OMX_CALLBACKTYPE *callbacks);
    OMX_ERRORTYPE (*free_handle) (OMX_HANDLETYPE handle);
};

struct GOmxImp
{
    guint client_count;
    void *dl_handle;
    GOmxSymbolTable sym_table;
    GMutex *mutex;
};

GOmxImp * g_omx_request_imp (const gchar *name);

struct _GstOmxBufferAlloc
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean listfull;

  gboolean silent;
  guint num_buffers;
  char *omx_library;
  GOmxImp *imp;
  guint allocSize;
  guint cnt;

  OMX_BUFFERHEADERTYPE **buffers;
  //~ GOmxPort out_port;
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
