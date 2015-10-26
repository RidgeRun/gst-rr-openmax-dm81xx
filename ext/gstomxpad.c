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

#include <string.h>
#include "gstomxpad.h"
#include "gstomx.h"
#include "timm_osal_interfaces.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_pad_debug);
#define GST_CAT_DEFAULT gst_omx_pad_debug

G_DEFINE_TYPE (GstOmxPad, gst_omx_pad, GST_TYPE_PAD);
#define parent_class gst_omx_pad_parent_class

/* VTable */
static void gst_omx_pad_finalize (GObject * object);
static GObject *gst_omx_pad_constructor (GType gtype, guint n_properties,
    GObjectConstructParam * properties);
static void gst_omx_init_port_default (OMX_PARAM_PORTDEFINITIONTYPE *,
    GstPadDirection);

GstOmxPad *
gst_omx_pad_new_from_template (GstPadTemplate * templ, const gchar * name)
{

  GstOmxPad *pad;
  pad =  g_object_new (TYPE_GST_OMX_PAD,
      "name", name, "template", templ, "direction", templ->direction, NULL);
  g_object_unref (templ);
  return pad;
  
}

static void
gst_omx_pad_class_init (GstOmxPadClass * klass)
{
  GObjectClass *gobject_class;
  gobject_class = G_OBJECT_CLASS (klass);

  /* Need a custom finalize function to free mapping */
  gobject_class->finalize = gst_omx_pad_finalize;
  gobject_class->constructor = gst_omx_pad_constructor;

  GST_DEBUG_CATEGORY_INIT (gst_omx_pad_debug, "omxpad", 0, "GstOmxPad");
}

/* We just create a dummy constructor in order to chain up with the
   parent one so the creation time properties are properly set */
static GObject *
gst_omx_pad_constructor (GType gtype, guint n_properties,
    GObjectConstructParam * properties)
{
  /* Pass the construction time properties to the parent */
  return G_OBJECT_CLASS (parent_class)->constructor (gtype, n_properties,
      properties);
}

static void
gst_omx_pad_init (GstOmxPad * this)
{
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->buffers = gst_omx_buf_tab_new ();

  this->port = (OMX_PARAM_PORTDEFINITIONTYPE *)
      TIMM_OSAL_Malloc (sizeof (OMX_PARAM_PORTDEFINITIONTYPE), TIMM_OSAL_TRUE,
      0, TIMMOSAL_MEM_SEGMENT_EXT);
  if (!this->port)
    GST_ERROR_OBJECT ("Insufficient resources to allocate %s port definition",
        GST_OBJECT_NAME (this));

  this->enabled = FALSE;
  this->flushing = FALSE;

  gst_omx_init_port_default (this->port,
      gst_pad_get_direction (GST_PAD (this)));
}

/* Object destructor
 */
static void
gst_omx_pad_finalize (GObject * object)
{
  GstOmxPad *this = GST_OMX_PAD (object);

  GST_INFO_OBJECT (this, "Freeing pad %s", GST_OBJECT_NAME (this));

  gst_omx_buf_tab_free (this->buffers);

  TIMM_OSAL_Free (this->port);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_omx_init_port_default (OMX_PARAM_PORTDEFINITIONTYPE * port,
    GstPadDirection direction)
{
  GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);

  if (GST_PAD_SINK == direction) {
    port->nPortIndex = 0;
    port->eDir = OMX_DirInput;
  }

  /* number of buffers are set here */
  port->nBufferCountActual = 20;
  port->bEnabled = OMX_TRUE;
  port->bPopulated = OMX_FALSE;
  port->eDomain = OMX_PortDomainVideo;
  port->bBuffersContiguous = OMX_FALSE;
  port->nBufferAlignment = 0x0;

  /* OMX_VIDEO_PORTDEFINITION values for input port */
  port->format.video.pNativeRender = NULL;
  /* for bitstream buffer stride is not a valid parameter */
  port->format.video.nStride = -1;
  /* component supports only frame based processing */
  port->format.video.nSliceHeight = 0;
  /* bitrate does not matter for decoder */
  port->format.video.nBitrate = 104857600;
  /* as per openmax frame rate is in Q16 format */
  port->format.video.xFramerate = 60 << 16;
  /* this is codec setting, OMX component does not support it */
  port->format.video.bFlagErrorConcealment = OMX_FALSE;
  /* output is raw YUV 420 SP format, It support only this */
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  /* color format is irrelavant */
  port->format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
}
