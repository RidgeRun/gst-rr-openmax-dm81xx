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

/**
 * SECTION:element-omx_h264_dec
 *
 * FIXME:Describe omx_h264_dec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omx_h264_dec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>
#include <gst/video/video.h>

#include "timm_osal_interfaces.h"

#include "gstomxh264dec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h264_dec_debug);
#define GST_CAT_DEFAULT gst_omx_h264_dec_debug

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS (
    "video/h264"
    )
);

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(GST_VIDEO_CAPS_YUV("NV12"))
);

#define gst_omx_h264_dec_parent_class parent_class
G_DEFINE_TYPE (GstOmxH264Dec, gst_omx_h264_dec, GST_TYPE_OMX_BASE);

static gboolean gst_omx_h264_dec_set_caps (GstPad * pad, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_h264_dec_init_pads (GstOmxBase *this);
static OMX_ERRORTYPE gst_omx_h264_dec_fill_callback (GstOmxBase *, OMX_BUFFERHEADERTYPE *);
/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_h264_dec_class_init (GstOmxH264DecClass * klass)
{
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS(klass);

  gst_element_class_set_details_simple (gstelement_class,
    "omx_h264dec",
    "Codec/Decoder/Video",
    "RidgeRun's OMX based H264 decoder",
    "Carlos Gomez <carlos.gomez@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstomxbase_class->parse_caps = 
    GST_DEBUG_FUNCPTR(gst_omx_h264_dec_set_caps);
  gstomxbase_class->omx_fill_buffer = 
    GST_DEBUG_FUNCPTR(gst_omx_h264_dec_fill_callback);
  gstomxbase_class->init_ports = 
    GST_DEBUG_FUNCPTR(gst_omx_h264_dec_init_pads);

  gstomxbase_class->handle_name = "OMX.TI.DUCATI.VIDDEC";

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_h264_dec_debug, "omx_h264dec", 
     0, "RidgeRun's OMX based H264 decoder");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_h264_dec_init (GstOmxH264Dec *this)
{
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME(this));

  this->sinkpad = GST_PAD(
    gst_omx_pad_new_from_template (
        gst_static_pad_template_get (&sink_template), "sink"));
  gst_pad_set_active (this->sinkpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE(this), this->sinkpad);
  
  this->srcpad = GST_PAD(
    gst_omx_pad_new_from_template (
     gst_static_pad_template_get (&src_template), "src"));
  gst_pad_set_active (this->srcpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE(this), this->srcpad);
}

#define PADX 32
#define PADY 24
static gboolean
gst_omx_h264_dec_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxH264Dec *this = GST_OMX_H264_DEC(GST_OBJECT_PARENT(pad));
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure;
  GstCaps *allowedcaps;
  GstCaps *newcaps;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG_OBJECT (this, "Reading width");
  if (!gst_structure_get_int (structure, "width", &this->format.width)) {
    this->format.width = -1;
    goto invalidcaps;
  }
  this->format.width_padded = 
    GST_OMX_ALIGN (this->format.width + (2 * PADX), 128);

  GST_DEBUG_OBJECT (this, "Reading height");
  if (!gst_structure_get_int (structure, "height", &this->format.height)) {
    this->format.height = -1;
    goto invalidcaps;
  }
  this->format.height_padded = 
    GST_OMX_ALIGN (this->format.height, 16);

  GST_DEBUG_OBJECT (this, "Reading framerate");
  if (!gst_structure_get_fraction (structure, "framerate", 
      &this->format.framerate_num, &this->format.framerate_den)) {
    this->format.framerate_num = -1;
    this->format.framerate_den = -1;
    goto invalidcaps;
  }

  /* This is always fixed */
  this->format.format = GST_VIDEO_FORMAT_NV12;

  this->format.size_padded = 
    this->format.width_padded*(this->format.height_padded + 4*PADY)*1.5;
  this->format.size = gst_video_format_get_size (this->format.format, 
      this->format.width, this->format.height);

  GST_INFO_OBJECT (this, "Parsed for input caps:\n"
		   "\tSize: %ux%u\n"
		   "\tFormat NV12\n"
		   "\tFramerate: %u/%u",
		   this->format.width, 
		   this->format.height, 
		   this->format.framerate_num,
		   this->format.framerate_den);

  /* Ask for the output caps, if not fixed then try the biggest frame */
  allowedcaps = gst_pad_get_allowed_caps (this->srcpad);
  newcaps = gst_caps_make_writable(gst_caps_copy_nth (allowedcaps, 0));
  srcstructure = gst_caps_get_structure (newcaps, 0);
  gst_caps_unref (allowedcaps);

  GST_DEBUG_OBJECT (this, "Fixating output caps");
  gst_structure_fixate_field_nearest_fraction (srcstructure, "framerate",
      this->format.framerate_num, this->format.framerate_den);
  gst_structure_fixate_field_nearest_int (srcstructure, "width", this->format.width);
  gst_structure_fixate_field_nearest_int (srcstructure, "height", this->format.height);

  gst_structure_get_int (srcstructure, "width", &this->format.width);
  gst_structure_get_int (srcstructure, "height", &this->format.height);
  gst_structure_get_fraction (srcstructure, "framerate", &this->format.framerate_num,
      &this->format.framerate_den);
  
  GST_DEBUG_OBJECT (this, "Output caps: %s", gst_caps_to_string (newcaps));

  if (!gst_pad_set_caps (this->srcpad, newcaps))
    goto nosetcaps;

  return TRUE;

invalidcaps:
  {
    GST_ERROR_OBJECT (this, "Unable to grab stream format from caps");
    return FALSE;
  }
nosetcaps:
  {
    GST_ERROR_OBJECT (this, "Src pad didn't accept new caps");
    return FALSE;
  }
}


static OMX_ERRORTYPE
gst_omx_h264_dec_init_pads (GstOmxBase *base)
{
  GstOmxH264Dec *this = GST_OMX_H264_DEC(base);
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname;

  GST_DEBUG_OBJECT(this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT(GST_OMX_PAD(this->sinkpad));

  port->nPortIndex = 0;
  port->eDir = OMX_DirInput;
  port->nBufferCountActual = 4;
  port->nBufferSize = this->format.size_padded;
  port->format.video.cMIMEType = "H264";
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height;
  port->format.video.xFramerate = 
    ((guint)((gdouble)this->format.framerate_num)/this->format.framerate_den) << 16;
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (GST_OMX_BASE(this)->handle, 
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }

  GST_DEBUG_OBJECT(this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT(GST_OMX_PAD(this->srcpad));

  port->nPortIndex = 1;
  port->eDir = OMX_DirOutput;
  port->nBufferCountActual = 6;
  port->nBufferSize = this->format.size_padded;
  port->format.video.cMIMEType = "H264";
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height_padded;
  port->format.video.nStride = this->format.width_padded;
  port->format.video.xFramerate = 
    ((guint)((gdouble)this->format.framerate_num)/this->format.framerate_den) << 16;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (GST_OMX_BASE(this)->handle, OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

  /* TODO: Set here the notification type */

  return error;

noport:
  {
    GST_ERROR_OBJECT (this, "Failed to set %s port parameters", portname);
    return error;
  }
}

static OMX_ERRORTYPE 
gst_omx_h264_dec_fill_callback (GstOmxBase *base, OMX_BUFFERHEADERTYPE *outbuf)
{
  GstOmxH264Dec *this = GST_OMX_H264_DEC(base);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxBufferData *bufdata = (GstOmxBufferData *)outbuf->pAppPrivate;

  GST_LOG_OBJECT (this,"H264 Fill buffer callback");

  caps = gst_pad_get_negotiated_caps (this->srcpad);
  if (!caps)
    goto nocaps;

  buffer = gst_buffer_new ();
  if (!buffer)
    goto noalloc;

  GST_BUFFER_SIZE(buffer) = this->format.size_padded;
  GST_BUFFER_CAPS(buffer) = caps;
  GST_BUFFER_DATA(buffer) = outbuf->pBuffer;
  GST_BUFFER_MALLOCDATA(buffer) = (guint8 *)outbuf;
  GST_BUFFER_FREE_FUNC(buffer) = gst_omx_base_release_buffer;

  /* Make buffer fields GStreamer friendly */
  GST_BUFFER_SIZE(buffer) = this->format.size;
  GST_BUFFER_TIMESTAMP(buffer) = outbuf->nTimeStamp;
  GST_BUFFER_DURATION(buffer) = 
    1e9*this->format.framerate_den/this->format.framerate_num;
  GST_BUFFER_FLAG_SET(buffer,GST_OMX_BUFFER_FLAG);
  bufdata->buffer = buffer;

  GST_LOG_OBJECT(this, "(Fill %s) Buffer %p size %d reffcount %d bufdat %p->%p", GST_OBJECT_NAME(this), outbuf->pBuffer, GST_BUFFER_SIZE(buffer), GST_OBJECT_REFCOUNT(buffer), bufdata, bufdata->buffer);

  GST_LOG_OBJECT (this,"Pushing buffer %p->%p to %s:%s", 
		  outbuf, outbuf->pBuffer, GST_DEBUG_PAD_NAME(this->srcpad));
  ret = gst_pad_push (this->srcpad, buffer);
  if (GST_FLOW_OK != ret)
    goto nopush;
  
  return error;

noalloc:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT(this), CORE, PAD,
        ("Unable to allocate buffer to push"), (NULL));
    error = OMX_ErrorInsufficientResources;
    return error;
  }
nocaps:
  {
    GST_ERROR_OBJECT (this,"Caps must be set at this point");
    error = OMX_ErrorNotReady;
    return error;
  }
nopush:
  {
    GST_DEBUG_OBJECT(this, "Unable to push buffer downstream: %d", ret);
    return error;
  }
}
