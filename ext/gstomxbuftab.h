/* 
 * GStreamer
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

#ifndef __GST_OMX_BUF_TAB_H__
#define __GST_OMX_BUF_TAB_H__

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_TI_Common.h>
#include <OMX_TI_Index.h>

#include <gst/gst.h>

G_BEGIN_DECLS typedef struct _GstOmxBufTab GstOmxBufTab;
typedef struct _GstOmxBufTabNode GstOmxBufTabNode;

struct _GstOmxBufTab
{
  GList *table;
  guint tabused;
  GMutex tabmutex;
  GCond tabcond;
};

struct _GstOmxBufTabNode
{
  OMX_BUFFERHEADERTYPE *buffer;
  gpointer phys_addr;
  gboolean busy;
};

GstOmxBufTab *gst_omx_buf_tab_new ();
OMX_ERRORTYPE gst_omx_buf_tab_add_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE *);
OMX_ERRORTYPE gst_omx_buf_tab_get_free_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE **);
OMX_ERRORTYPE gst_omx_buf_tab_find_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE *, OMX_BUFFERHEADERTYPE **, gboolean * busy);
OMX_ERRORTYPE gst_omx_buf_tab_use_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE *);
OMX_ERRORTYPE gst_omx_buf_tab_return_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE *);
OMX_ERRORTYPE gst_omx_buf_tab_remove_buffer (GstOmxBufTab *,
    OMX_BUFFERHEADERTYPE *);
OMX_ERRORTYPE gst_omx_buf_tab_wait_free (GstOmxBufTab *);
OMX_ERRORTYPE gst_omx_buf_tab_free (GstOmxBufTab *);

G_END_DECLS
#endif //__GST_OMX_BUF_TAB_H__
