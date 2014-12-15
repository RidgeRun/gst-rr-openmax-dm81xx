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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstomxaacdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_aac_dec_debug);
#define GST_CAT_DEFAULT gst_omx_aac_dec_debug



static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int," "endianness=1234," "width=16,"
        "depth=16," "rate=[8000,96000]," "signed=true," "channels=[1,8]")
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg,"
        "mpegversion = {2,4}," "rate = [8000,96000]," "channels=  [1,8],"
        "object_type=[1,6]," "parsed=true")
    );



enum
{
  PROP_0,
  PROP_FRAMEMODE,
};


#define GST_OMX_AAC_DEC_FRAMEMODE_DEFAULT FALSE

#define gst_omx_aac_dec_parent_class parent_class
G_DEFINE_TYPE (GstOmxAACDec, gst_omx_aac_dec, GST_TYPE_OMX_BASE);

static gboolean gst_omx_aac_dec_set_caps (GstPad * pad, GstCaps * caps);
static void gst_omx_aac_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_aac_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static OMX_ERRORTYPE gst_omx_aac_dec_init_pads (GstOmxBase * base);

static GstFlowReturn gst_omx_aac_dec_fill_callback (GstOmxBase * base,
    OMX_BUFFERHEADERTYPE * outbuf);
static OMX_ERRORTYPE gst_omx_aac_dec_parameters (GstOmxAACDec * this,
    GstOmxFormat * format);
static OMX_ERRORTYPE gst_omx_aac_dec_init_pads (GstOmxBase * base);

/* initialize the omx's class */
static void
gst_omx_aac_dec_class_init (GstOmxAACDecClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX AAC audio decoder",
      "Codec/Decoder/AAC",
      "RidgeRun's OMX based AAC decoder",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_omx_aac_dec_set_property;
  gobject_class->get_property = gst_omx_aac_dec_get_property;


  g_object_class_install_property (gobject_class, PROP_FRAMEMODE,
      g_param_spec_boolean ("framemode", "Frame Mode",
          "Frame Mode", GST_OMX_AAC_DEC_FRAMEMODE_DEFAULT, G_PARAM_READWRITE));


  // gstomxbase_class->omx_fill_buffer = GST_DEBUG_FUNCPTR (gst_omx_aac_dec_fill_callback);
  //gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_aac_dec_set_caps);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_aac_dec_init_pads);

  gstomxbase_class->handle_name = "OMX.TI.DSP.AUDDEC";

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_aac_dec_debug, "omx_aacdec",
      0, "RidgeRun's OMX based AAC decoder");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_aac_dec_init (GstOmxAACDec * this)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  /* Initialize properties */
  this->framemode = GST_OMX_AAC_DEC_FRAMEMODE_DEFAULT;
  /*Set Audio flag */
  base->output_buffers = 2;
  base->input_buffers = 2;
  /* Add pads */
  this->sinkpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&sink_template), "sink"));
  gst_pad_set_active (this->sinkpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE (this), this->sinkpad);
  gst_element_add_pad (GST_ELEMENT (this), this->sinkpad);

  this->srcpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
  gst_pad_set_active (this->srcpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE (this), this->srcpad);
  gst_element_add_pad (GST_ELEMENT (this), this->srcpad);
}


static void
gst_omx_aac_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxAACDec *this = GST_OMX_AAC_DEC (object);

  switch (prop_id) {
    case PROP_FRAMEMODE:
      this->framemode = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting framemode to %d", this->framemode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_omx_aac_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxAACDec *this = GST_OMX_AAC_DEC (object);

  switch (prop_id) {
    case PROP_FRAMEMODE:
      g_value_set_boolean (value, this->framemode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}



static GstFlowReturn
gst_omx_aac_dec_fill_callback (GstOmxBase * base, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxAACDec *this = GST_OMX_AAC_DEC (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;

  GST_LOG_OBJECT (this, "AAC Decoder Fill buffer callback");

  caps = gst_pad_get_negotiated_caps (this->srcpad);
  if (!caps)
    goto nocaps;

  buffer = gst_buffer_new ();
  if (!buffer)
    goto noalloc;

  GST_BUFFER_SIZE (buffer) = outbuf->nFilledLen;
  GST_BUFFER_CAPS (buffer) = caps;
  GST_BUFFER_DATA (buffer) = outbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) outbuf;
  GST_BUFFER_FREE_FUNC (buffer) = gst_omx_base_release_buffer;

  /* Make buffer fields GStreamer friendly */
  GST_BUFFER_TIMESTAMP (buffer) = outbuf->nTimeStamp;
  GST_BUFFER_DURATION (buffer) = 1e9 * 1 / this->format.rate;
  GST_BUFFER_FLAG_SET (buffer, GST_OMX_BUFFER_FLAG);
  bufdata->buffer = buffer;

  GST_LOG_OBJECT (this,
      "(Fill %s) Buffer %p size %d reffcount %d bufdat %p->%p",
      GST_OBJECT_NAME (this), outbuf->pBuffer, GST_BUFFER_SIZE (buffer),
      GST_OBJECT_REFCOUNT (buffer), bufdata, bufdata->buffer);

  GST_LOG_OBJECT (this, "Pushing buffer %p->%p to %s:%s",
      outbuf, outbuf->pBuffer, GST_DEBUG_PAD_NAME (this->srcpad));

  ret = gst_pad_push (this->srcpad, buffer);
  if (GST_FLOW_OK != ret)
    goto nopush;

  return ret;

noalloc:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Unable to allocate buffer to push"), (NULL));
    return GST_FLOW_ERROR;
  }
nocaps:
  {
    GST_ERROR_OBJECT (this, "Unable to provide the requested caps");
    return GST_FLOW_NOT_NEGOTIATED;
  }
nopush:
  {
    GST_ERROR_OBJECT (this, "Unable to push buffer downstream: %d", ret);
    return ret;
  }
}


static OMX_ERRORTYPE
gst_omx_aac_dec_init_pads (GstOmxBase * base)
{
  GstOmxAACDec *this = GST_OMX_AAC_DEC (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname = NULL;



  GST_DEBUG_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = 0;
  port->nBufferCountActual = base->input_buffers;
  port->nBufferSize = 1024 * 8; /* 1024*8 Recommended buffer size */
  port->format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }
  GST_DEBUG_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));

  port->nPortIndex = 1;
  port->nBufferCountActual = base->output_buffers;
  port->nBufferSize = 1024 * 8; /* 1024*8 Recommended buffer size */
  port->format.audio.eEncoding = OMX_AUDIO_CodingAAC;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);

  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

  //  error = gst_omx_aac_dec_parameters (this, &this->format);

  if (GST_OMX_FAIL (error))
    goto noconfiguration;


  GST_INFO_OBJECT (this, "Enabling input port");
  g_mutex_lock (&_omx_mutex);
  OMX_SendCommand (base->handle, OMX_CommandPortEnable, 0, NULL);
  g_mutex_unlock (&_omx_mutex);


  GST_INFO_OBJECT (this, "Waiting for input port to enable");
  error = gst_omx_base_wait_for_condition (base,
      gst_omx_base_condition_enabled,
      (gpointer) & GST_OMX_PAD (this->sinkpad)->enabled, NULL);
  if (GST_OMX_FAIL (error))
    goto noenable;

  GST_INFO_OBJECT (this, "Enabling output port");
  g_mutex_lock (&_omx_mutex);
  OMX_SendCommand (base->handle, OMX_CommandPortEnable, 1, NULL);
  g_mutex_unlock (&_omx_mutex);

  GST_INFO_OBJECT (this, "Waiting for output port to enable");
  error = gst_omx_base_wait_for_condition (base,
      gst_omx_base_condition_enabled,
      (gpointer) & GST_OMX_PAD (this->srcpad)->enabled, NULL);
  if (GST_OMX_FAIL (error))
    goto noenable;

  return error;

noport:
  {
    GST_ERROR_OBJECT (this, "Failed to set %s port parameters", portname);
    return error;
  }
noconfiguration:
  {
    GST_ERROR_OBJECT (this, "Unable to  change parameters: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
noenable:
  {
    GST_ERROR_OBJECT (this, "Failed to enable AAC decoder");
    return error;
  }
}
