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
#ifndef __GST_OMX_PAD_H__
#define __GST_OMX_PAD_H__

#include <gst/gst.h>
//#include "gstomx.h"
#include "gstomxbuftab.h"

G_BEGIN_DECLS
#define TYPE_GST_OMX_PAD (gst_omx_pad_get_type ())
#define GST_OMX_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_GST_OMX_PAD, GstOmxPad))
#define GST_OMX_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_GST_OMX_PAD, GstOmxPadClass))
#define IS_GST_OMX_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_GST_OMX_PAD))
#define IS_GST_OMX_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_GST_OMX_PAD))
#define GST_OMX_PAD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_GST_OMX_PAD, GstOmxPadClass))
typedef struct _GstOmxPad GstOmxPad;
typedef struct _GstOmxPadClass GstOmxPadClass;

struct _GstOmxPad
{
  GstPad parent;

  OMX_PARAM_PORTDEFINITIONTYPE *port;
  GstOmxBufTab *buffers;

  gboolean enabled;
  gboolean flushing;
};

struct _GstOmxPadClass
{
  GstPadClass parent_class;
};

GstOmxPad *gst_omx_pad_new_from_template (GstPadTemplate * templ,
    const gchar * name);

GType gst_omx_pad_get_type (void);

#define GST_OMX_PAD_PORT(pad) ((pad)->port)

G_END_DECLS
#endif //__GST_OMX_PAD_H__
