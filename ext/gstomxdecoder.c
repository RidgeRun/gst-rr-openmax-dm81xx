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

/**
 * SECTION:element-omx_decoder
 *
 * FIXME:Describe omx_decoder here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omx_decoder ! fakesink silent=TRUE
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

#include "gstomxdecoder.h"
#include "gstomxerror.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_decoder_debug);
#define GST_CAT_DEFAULT gst_omx_decoder_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, " "mpegversion=[1,2]," "systemstream=false")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
    );

#define gst_omx_decoder_parent_class parent_class
G_DEFINE_TYPE (GstOmxDecoder, gst_omx_decoder, GST_TYPE_ELEMENT);

static void gst_omx_decoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_omx_decoder_finalize (GObject * object);
static GstFlowReturn gst_omx_decoder_chain (GstPad * pad, GstBuffer * buf);
static OMX_ERRORTYPE gst_omx_decoder_allocate_omx (GstOmxDecoder * this);
static OMX_ERRORTYPE gst_omx_decoder_free_omx (GstOmxDecoder * this);
static void gst_omx_decoder_parse (GstOmxDecoder * this, GstBuffer * buf,
    GstCaps ** caps);
static void gst_omx_decoder_code_to_aspectratio (guint code, guint * num,
    guint * den);
static void gst_omx_decoder_code_to_aspectratio (guint code, guint * num,
    guint * den);
static OMX_ERRORTYPE gst_omx_decoder_init_pads (GstOmxDecoder * this);
static OMX_ERRORTYPE gst_omx_decoder_start (GstOmxDecoder * this);
static OMX_ERRORTYPE gst_omx_decoder_stop (GstOmxDecoder * this);
static OMX_ERRORTYPE gst_omx_decoder_alloc_buffers (GstOmxDecoder * this,
    GstOmxPad * pad);
static OMX_ERRORTYPE gst_omx_decoder_free_buffers (GstOmxDecoder * this,
    GstOmxPad * pad);
static OMX_ERRORTYPE gst_omx_decoder_event_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2,
    gpointer eventdata);
static OMX_ERRORTYPE gst_omx_decoder_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_decoder_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_decoder_wait_for_state (GstOmxDecoder *,
    OMX_STATETYPE);
static OMX_ERRORTYPE gst_omx_decoder_push_buffers (GstOmxDecoder * this,
    GstOmxPad * pad);
static GstStateChangeReturn gst_omx_decoder_change_state (GstElement * element,
    GstStateChange transition);

/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_decoder_class_init (GstOmxDecoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_omx_decoder_set_property;
  gobject_class->get_property = gst_omx_decoder_get_property;
  gobject_class->finalize = gst_omx_decoder_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_decoder_change_state);

  gst_element_class_set_details_simple (gstelement_class,
      "omxdec",
      "Generic/Filter",
      "RidgeRun's OMX based decoder",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_decoder_debug, "omxdec", 0,
      "RidgeRun's OMX based decoder");
}

static OMX_ERRORTYPE
gst_omx_decoder_allocate_omx (GstOmxDecoder * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PORT_PARAM_TYPE init;

  GST_INFO_OBJECT (this, "Allocating OMX resources");

  this->callbacks = TIMM_OSAL_Malloc (sizeof (OMX_CALLBACKTYPE),
      TIMM_OSAL_TRUE, 0, TIMMOSAL_MEM_SEGMENT_EXT);
  if (!this->callbacks) {
    error = OMX_ErrorInsufficientResources;
    goto noresources;
  }

  this->callbacks->EventHandler =
      (GstOmxEventHandler) gst_omx_decoder_event_callback;
  this->callbacks->EmptyBufferDone =
      (GstOmxEmptyBufferDone) gst_omx_decoder_empty_callback;
  this->callbacks->FillBufferDone =
      (GstOmxFillBufferDone) gst_omx_decoder_fill_callback;

  error =
      OMX_GetHandle (&this->handle, "OMX.TI.DUCATI.VIDDEC", this,
      this->callbacks);
  if ((error != OMX_ErrorNone) || (!this->handle))
    goto nohandle;

  this->component = (OMX_COMPONENTTYPE *) this->handle;

  GST_OMX_INIT_STRUCT (&init, OMX_PORT_PARAM_TYPE);
  init.nPorts = 2;
  init.nStartPortNumber = 0;
  error = OMX_SetParameter (this->handle, OMX_IndexParamVideoInit, &init);
  if (error != OMX_ErrorNone)
    goto initport;

  return error;

noresources:
  {
    GST_ERROR_OBJECT (this, "Insufficient OMX memory resources");
    return error;
  }
nohandle:
  {
    GST_ERROR_OBJECT (this, "Unable to grab OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
initport:
  {
    /* TODO: should I free the handle here? */
    GST_ERROR_OBJECT (this, "Unable to init component ports: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_free_omx (GstOmxDecoder * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_INFO_OBJECT (this, "Freeing OMX resources");

  TIMM_OSAL_Free (this->callbacks);

  error = OMX_FreeHandle (this->handle);
  if (error != OMX_ErrorNone)
    goto freehandle;

  return error;

freehandle:
  {
    GST_ERROR_OBJECT (this, "Unable to free OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_decoder_init (GstOmxDecoder * this)
{
  OMX_ERRORTYPE error;

  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->sinkpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&sink_template), "sink"));
  gst_pad_set_active (this->sinkpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (this), this->sinkpad);

  this->srcpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
  gst_pad_set_active (this->srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (this), this->srcpad);

  gst_pad_set_chain_function (this->sinkpad,
      GST_DEBUG_FUNCPTR (gst_omx_decoder_chain));

  this->state = OMX_StateInvalid;
  g_mutex_init (&this->statemutex);
  g_cond_init (&this->statecond);

  error = gst_omx_decoder_allocate_omx (this);
  if (GST_OMX_FAIL (error)) {
    GST_ELEMENT_ERROR (this, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
  }
}

static void
gst_omx_decoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
//  GstOmxDecoder *this = GST_OMX_DECODER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
//  GstOmxDecoder *this = GST_OMX_DECODER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#define MAX_HEADER_RETRIES 300
static GstFlowReturn
gst_omx_decoder_chain (GstPad * pad, GstBuffer * buf)
{
  GstOmxDecoder *this = GST_OMX_DECODER (GST_OBJECT_PARENT (pad));
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxbuf;
  GList *omxlist;
  GstCaps *currentcaps = NULL;
  GstCaps *newcaps = NULL;
  static guint retries = 0;

  currentcaps = gst_pad_get_negotiated_caps (this->srcpad);
  GST_LOG_OBJECT (this, "Current sink pad caps %s",
      gst_caps_to_string (currentcaps));

  gst_omx_decoder_parse (this, buf, &newcaps);

  /* Wait for a sequence header to parse resolution */
  if (!newcaps && !currentcaps) {
    if (MAX_HEADER_RETRIES > retries)
      goto retry;
    else
      goto noformat;
  }

  /* New resolution or resolution change */
  if (newcaps && !gst_caps_is_equal (newcaps, currentcaps)) {
    if (!gst_pad_set_caps (this->srcpad, newcaps))
      goto invalidcaps;
    else {
      /* TODO: do something here */
      error = gst_omx_decoder_start (this);
      if (GST_OMX_FAIL (error))
        goto nostart;

      GST_INFO_OBJECT (this, "Changed decoder resolution");
      retries = 0;
    }
  }

  omxlist = GST_OMX_PAD (pad)->buffers;
  omxbuf = (OMX_BUFFERHEADERTYPE *) omxlist->data;
  memcpy (omxbuf->pBuffer, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
  omxbuf->nFilledLen = GST_BUFFER_SIZE (buf);
  omxbuf->nOffset = 0;

  gst_buffer_unref (buf);

  error = this->component->EmptyThisBuffer (this->handle, omxbuf);
  if (GST_OMX_FAIL (error))
    goto nofill;

  g_mutex_lock (&this->statemutex);
  g_cond_wait (&this->statecond, &this->statemutex);
  g_mutex_unlock (&this->statemutex);

  return GST_FLOW_OK;

retry:
  {
    GST_INFO_OBJECT (this,
        "Skipping frame until a sequence header is received");
    retries++;
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }
noformat:
  {
    GST_ELEMENT_ERROR (this, STREAM, TYPE_NOT_FOUND,
        ("No valid sequence header was found"), (NULL));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
invalidcaps:
  {
    GST_ELEMENT_ERROR (this, STREAM, FORMAT,
        ("The new caps are not supported: %s", gst_caps_to_string (newcaps)),
        (NULL));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
nostart:
  {
    GST_ELEMENT_ERROR (this, LIBRARY, INIT, (gst_omx_error_to_str (error)),
        (NULL));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
nofill:
  {
    GST_ELEMENT_ERROR (this, LIBRARY, ENCODE, (gst_omx_error_to_str (error)),
        (NULL));
    return GST_FLOW_ERROR;
  }
}


static void
gst_omx_decoder_finalize (GObject * object)
{
  GstOmxDecoder *this = GST_OMX_DECODER (object);

  GST_INFO_OBJECT (this, "Finalizing %s", GST_OBJECT_NAME (this));

  gst_omx_decoder_free_omx (this);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* vmethod implementations */
static void
gst_omx_decoder_code_to_framerate (guint code, guint * num, guint * den)
{
  /* Taken from http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html#picture */
  switch (code) {
    case 1:
      *num = 24000;
      *den = 1001;
      break;
    case 2:
      *num = 24;
      *den = 1;
      break;
    case 3:
      *num = 25;
      *den = 1;
      break;
    case 4:
      *num = 30000;
      *den = 1001;
      break;
    case 5:
      *num = 30;
      *den = 1;
      break;
    case 6:
      *num = 50;
      *den = 1;
      break;
    case 7:
      *num = 60000;
      *den = 1001;
      break;
    case 8:
      *num = 60;
      *den = 1;
      break;
    default:
      *num = 0;
      *den = 1;
      break;
  }
}

static void
gst_omx_decoder_code_to_aspectratio (guint code, guint * num, guint * den)
{
  /* Taken from http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html#picture */
  switch (code) {
    case 1:
      *num = 1;
      *den = 1;
      break;
    case 2:
      *num = 4;
      *den = 3;
      break;
    case 3:
      *num = 16;
      *den = 9;
      break;
    case 4:
      *num = 2;                 // It's actually 2.21
      *den = 1;
      break;
    default:
      *num = 1;
      *den = 1;
      break;
  }
}

static void
gst_omx_decoder_parse (GstOmxDecoder * this, GstBuffer * buf, GstCaps ** caps)
{
  const GstCaps *templatecaps = gst_pad_get_pad_template_caps (this->srcpad);
  guint startcode = 0;
  guint32 *data = (guint32 *) GST_BUFFER_DATA (buf);
  GValue width = G_VALUE_INIT;
  GValue height = G_VALUE_INIT;
  GValue framerate = G_VALUE_INIT;
  GValue aspectratio = G_VALUE_INIT;

  /* Initialize gvalues */
  g_value_init (&width, G_TYPE_INT);
  g_value_init (&height, G_TYPE_INT);
  g_value_init (&framerate, GST_TYPE_FRACTION);
  g_value_init (&aspectratio, GST_TYPE_FRACTION);

  /* 32 bits: start code */
  startcode = GST_READ_UINT32_BE (data);
  GST_LOG_OBJECT (this, "Reading start code: 0x%08x", startcode);
  if (0x000001b3 != startcode)
    goto noformat;
  data++;

  /* 12 bits: Horizontal size, 12 bits: Vertical size */
  this->format.width = (GST_READ_UINT32_BE (data) & 0xFFF00000) >> 20;
  g_value_set_int (&width, this->format.width);
  this->format.height = (GST_READ_UINT32_BE (data) & 0x000FFF00) >> 8;
  g_value_set_int (&height, this->format.height);
  data = (guint32 *) ((gchar *) data + 3);

  /* 4 bits: aspect ratio */
  gst_omx_decoder_code_to_aspectratio ((GST_READ_UINT32_BE (data) & 0xF0) >> 4,
      &this->format.aspectratio_num, &this->format.aspectratio_den);
  gst_value_set_fraction (&aspectratio, this->format.aspectratio_num,
      this->format.aspectratio_den);

  /* 4 bits: frame rate code */
  gst_omx_decoder_code_to_framerate (*data & 0x0F, &this->format.framerate_num,
      &this->format.framerate_den);
  gst_value_set_fraction (&framerate, this->format.framerate_num,
      this->format.framerate_den);

  /* This is always fixed */
  this->format.format = GST_VIDEO_FORMAT_NV12;

  GST_LOG_OBJECT (this, "Parsed from stream:\n"
      "\tSize: %ux%u\n"
      "\tFormat NV12\n"
      "\tFramerate: %u/%u\n"
      "\tAspect Ratio: %u/%u",
      this->format.width,
      this->format.height,
      this->format.framerate_num,
      this->format.framerate_den,
      this->format.aspectratio_num, this->format.aspectratio_den);

  *caps = gst_caps_copy (templatecaps);
  gst_caps_set_value (*caps, "width", &width);
  gst_caps_set_value (*caps, "height", &height);
  gst_caps_set_value (*caps, "framerate", &framerate);

  this->format.size = gst_video_format_get_size (this->format.format,
      this->format.width, this->format.height);

  return;

noformat:
  {
    GST_LOG_OBJECT (this, "Skipping non-sequence header");
    *caps = NULL;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_init_pads (GstOmxDecoder * this)
{
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname;

  GST_DEBUG_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = 0;
  port->eDir = OMX_DirInput;
  port->nBufferSize = (this->format.width) * (this->format.height);
  port->format.video.cMIMEType = "MPEG2";
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height;
  port->format.video.xFramerate =
      ((guint) ((gdouble) this->format.framerate_num) /
      this->format.framerate_den) << 16;
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;

  error = OMX_SetParameter (this->handle, OMX_IndexParamPortDefinition, port);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));

#define PADX 8
#define PADY 8

  port->nPortIndex = 1;
  port->eDir = OMX_DirOutput;
  port->nBufferSize = (GST_OMX_ALIGN ((this->format.width + (2 * PADX)), 128) *
      ((((this->format.height + 15) & 0xfffffff0) + (4 * PADY))) * 3) >> 1;
  g_print ("Buffer size %d\n", (guint32) port->nBufferSize);
  port->format.video.cMIMEType = "MPEG2";
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = ((this->format.height + 15) & 0xfffffff0);
  port->format.video.nStride =
      GST_OMX_ALIGN (this->format.width + (2 * PADX), 128);
  port->format.video.xFramerate =
      ((guint) ((gdouble) this->format.framerate_num) /
      this->format.framerate_den) << 16;

  error = OMX_SetParameter (this->handle, OMX_IndexParamPortDefinition, port);
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
gst_omx_decoder_start (GstOmxDecoder * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  error = gst_omx_decoder_init_pads (this);
  if (GST_OMX_FAIL (error))
    goto nopads;

  GST_INFO_OBJECT (this, "Sending handle to Idle");
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Allocating buffers for input port");
  error = gst_omx_decoder_alloc_buffers (this, GST_OMX_PAD (this->srcpad));
  if (GST_OMX_FAIL (error))
    goto noalloc;

  GST_INFO_OBJECT (this, "Allocating buffers for output port");
  error = gst_omx_decoder_alloc_buffers (this, GST_OMX_PAD (this->sinkpad));
  if (GST_OMX_FAIL (error))
    goto noalloc;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_decoder_wait_for_state (this, OMX_StateIdle);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Sending handle to Executing");
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet,
      OMX_StateExecuting, NULL);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Waiting for handle to become Executing");
  error = gst_omx_decoder_wait_for_state (this, OMX_StateExecuting);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Pushing output buffers");
  error = gst_omx_decoder_push_buffers (this, GST_OMX_PAD (this->srcpad));
  if (GST_OMX_FAIL (error))
    goto nopush;

  return error;

nopads:
  {
    GST_ERROR_OBJECT (this, "Unable to initializate ports");
    return error;
  }
starthandle:
  {
    GST_ERROR_OBJECT (this, "Unable to set handle to Idle");
    return error;
  }
noalloc:
  {
    GST_ERROR_OBJECT (this, "Unable to allocate resources for buffers");
    return error;
  }
nopush:
  {
    GST_ERROR_OBJECT (this, "Unable to push buffer into the output port");
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_stop (GstOmxDecoder * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_INFO_OBJECT (this, "Sending handle to Idle");
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_decoder_wait_for_state (this, OMX_StateIdle);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Sending handle to Loaded");
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateLoaded,
      NULL);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Freeing input port buffers");
  error = gst_omx_decoder_free_buffers (this, GST_OMX_PAD (this->srcpad));
  if (GST_OMX_FAIL (error))
    goto nofree;

  GST_INFO_OBJECT (this, "Allocating output port buffers");
  error = gst_omx_decoder_free_buffers (this, GST_OMX_PAD (this->sinkpad));
  if (GST_OMX_FAIL (error))
    goto nofree;

  GST_INFO_OBJECT (this, "Waiting for handle to become Loaded");
  error = gst_omx_decoder_wait_for_state (this, OMX_StateLoaded);
  if (GST_OMX_FAIL (error))
    goto statechange;

  return error;

statechange:
  {
    GST_ERROR_OBJECT (this, "Unable to set component state: %s",
        gst_omx_error_to_str (error));
    return error;
  }

nofree:
  {
    GST_ERROR_OBJECT (this, "Unable to free buffers: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static GstStateChangeReturn
gst_omx_decoder_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxDecoder *this = GST_OMX_DECODER (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_omx_decoder_stop (this);
      break;
    default:
      break;
  }

  return ret;
}

static OMX_ERRORTYPE
gst_omx_decoder_alloc_buffers (GstOmxDecoder * this, GstOmxPad * pad)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  guint i;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {
    GST_INFO_OBJECT (this, "Allocating buffer number %u", i);
    error = OMX_AllocateBuffer (this->handle, &buffer,
        GST_OMX_PAD_PORT (pad)->nPortIndex, this,
        GST_OMX_PAD_PORT (pad)->nBufferSize);
    if (GST_OMX_FAIL (error))
      goto noalloc;
    pad->buffers = g_list_append (pad->buffers, buffer);
  }

  return error;

noalloc:
  {
    GST_ERROR_OBJECT (this, "Failed to allocate buffers");
    /*TODO: should I free buffers? */
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_free_buffers (GstOmxDecoder * this, GstOmxPad * pad)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GList *buffers;
  guint i;

  buffers = pad->buffers;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {
    GST_INFO_OBJECT (this, "Freeing buffer number %u", i);

    if (!buffers)
      goto malformed;

    buffer = (OMX_BUFFERHEADERTYPE *) buffers->data;
    error = OMX_FreeBuffer (this->handle, GST_OMX_PAD_PORT (pad)->nPortIndex,
        buffer);
    if (GST_OMX_FAIL (error))
      goto nofree;

    buffers = g_list_remove (buffers, buffer);
  }

  return error;

malformed:
  {
    GST_ERROR_OBJECT (this, "The buffer list for %s:%s is malformed",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return OMX_ErrorResourcesLost;
  }

nofree:
  {
    GST_ERROR_OBJECT (this, "Error freeing buffers on %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_push_buffers (GstOmxDecoder * this, GstOmxPad * pad)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GList *buffers;
  guint i;

  buffers = pad->buffers;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {
    GST_DEBUG_OBJECT (this, "Pushing buffer number %u", i);

    if (!buffers)
      goto shortread;
    buffer = (OMX_BUFFERHEADERTYPE *) buffers->data;

    error = this->component->FillThisBuffer (this->handle, buffer);
    if (GST_OMX_FAIL (error))
      goto nopush;

    buffers = g_list_next (buffers);
  }

  return error;

nopush:
  {
    GST_ERROR_OBJECT (this, "Failed to push buffers");
    /*TODO: should I free buffers? */
    return error;
  }
shortread:
  {
    GST_ERROR_OBJECT (this, "Malformed output buffer list");
    /*TODO: should I free buffers? */
    return OMX_ErrorResourcesLost;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_event_callback (OMX_HANDLETYPE handle,
    gpointer data,
    OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2, gpointer eventdata)
{
  GstOmxDecoder *this = GST_OMX_DECODER (data);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  switch (event) {
    case OMX_EventCmdComplete:
      GST_INFO_OBJECT (this, "OMX command complete event received: %s (%s)",
          gst_omx_cmd_to_str (nevent1), OMX_CommandStateSet == nevent1 ?
          gst_omx_state_to_str (nevent2) : "No debug");

      if (OMX_CommandStateSet == nevent1) {
        g_mutex_lock (&this->statemutex);
        this->state = nevent2;
        g_cond_signal (&this->statecond);
        g_mutex_unlock (&this->statemutex);
      }
      break;
    case OMX_EventError:
      GST_ERROR_OBJECT (this, "OMX error event received: %s",
          gst_omx_error_to_str (nevent1));

      GST_ELEMENT_ERROR (this, LIBRARY, ENCODE,
          (gst_omx_error_to_str (nevent1)), (NULL));
      break;
    case OMX_EventMark:
      GST_INFO_OBJECT (this, "OMX mark event received");
      break;
    case OMX_EventPortSettingsChanged:
      /* http://maemo.org/api_refs/5.0/alpha/libomxil-bellagio/_o_m_x___index_8h.html */
      GST_INFO_OBJECT (this,
          "OMX port settings changed event received: Port %d: %d", nevent2,
          nevent1);
      break;
    case OMX_EventBufferFlag:
      GST_INFO_OBJECT (this, "OMX buffer flag event received");
      break;
    case OMX_EventResourcesAcquired:
      GST_INFO_OBJECT (this, "OMX resources acquired event received");
      break;
    case OMX_EventComponentResumed:
      GST_INFO_OBJECT (this, "OMX component resumed event received");
      break;
    case OMX_EventDynamicResourcesAvailable:
      GST_INFO_OBJECT (this, "OMX synamic resources available event received");
      break;
    case OMX_EventPortFormatDetected:
      GST_INFO_OBJECT (this, "OMX port format detected event received");
      break;
    default:
      GST_WARNING_OBJECT (this, "Unknown OMX port event");
      break;
  }

  return error;
}

static OMX_ERRORTYPE
gst_omx_decoder_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxDecoder *this = GST_OMX_DECODER (data);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;

  GST_LOG_OBJECT (this, "Fill buffer callback");

  caps = gst_pad_get_negotiated_caps (this->srcpad);

  if (GST_FLOW_OK != gst_pad_alloc_buffer_and_set_caps
      (this->srcpad, 0, this->format.size, caps, &buffer))
    goto noalloc;

  if (!gst_caps_is_equal_fixed (GST_BUFFER_CAPS (buffer), caps))
    goto nocaps;

  memcpy (GST_BUFFER_DATA (buffer), outbuf->pBuffer, GST_BUFFER_SIZE (buffer));

  error = this->component->FillThisBuffer (this->handle, outbuf);
  if (GST_OMX_FAIL (error))
    goto nofill;



//  GST_STATE_LOCK(this);
  /* if (GST_STATE_PAUSED >= GST_STATE (this) && GST_STATE_PAUSED >= GST_STATE_NEXT (this)) { */
  GST_LOG_OBJECT (this, "Pushing buffer to %s:%s",
      GST_DEBUG_PAD_NAME (this->srcpad));
  if (GST_FLOW_OK != gst_pad_push (this->srcpad, buffer))
    goto nopush;
  /* } else { */
  /*   GST_LOG_OBJECT (this,"Element is changing state, discarding buffer"); */
  /*   gst_buffer_unref (buffer); */
  /* } */
//  GST_STATE_UNLOCK(this);

  return error;

noalloc:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Unable to allocate buffer to push"), (NULL));
    return error;
  }

nocaps:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Unable to provide the requested caps"), (NULL));
    gst_buffer_unref (buffer);
    return error;
  }

nofill:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), LIBRARY, ENCODE,
        ("Unable to recycle output buffer"), (NULL));
    gst_buffer_unref (buffer);
    return error;
  }
nopush:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Unable to push buffer downstream"), (NULL));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_decoder_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer)
{
  GstOmxDecoder *this = GST_OMX_DECODER (data);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_LOG_OBJECT (this, "Empty buffer callback");

  g_mutex_lock (&this->statemutex);
  g_cond_signal (&this->statecond);
  g_mutex_unlock (&this->statemutex);

  return error;
}

OMX_ERRORTYPE
gst_omx_decoder_wait_for_state (GstOmxDecoder * this, OMX_STATETYPE state)
{
  guint64 endtime;

  g_mutex_lock (&this->statemutex);

  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  while (this->state != state)
    if (!g_cond_wait_until (&this->statecond, &this->statemutex, endtime))
      goto timeout;

  GST_DEBUG_OBJECT (this, "Wait for state change successful");
  g_mutex_unlock (&this->statemutex);

  return OMX_ErrorNone;

timeout:
  {
    GST_WARNING_OBJECT (this, "Wait for state timed out");
    g_mutex_unlock (&this->statemutex);
    return OMX_ErrorTimeout;
  }
}
