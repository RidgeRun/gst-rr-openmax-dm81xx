/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Eugenia Guzman <eugenia.guzman@ridgerun.com>
 * Copyright (C) 2015 Diego Solano <diego.solano@ridgerun.com>
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
#include <gst/controller/gstcontroller.h>
#include <gst/video/video.h>

#include "timm_osal_interfaces.h"

#include "gstomxjpegenc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_jpeg_enc_debug);
#define GST_CAT_DEFAULT gst_omx_jpeg_enc_debug

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED ("NV12", "[ 0, max ]") ";")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg,"
        "width=[16,4096]," "height=[16,4096]," "framerate=" GST_VIDEO_FPS_RANGE)
    );

enum
{
  PROP_0,
  PROP_QUALITY,

};

#define GST_OMX_JPEG_ENC_QUALITY_DEFAULT	90

#define gst_omx_jpeg_enc_parent_class parent_class
G_DEFINE_TYPE (GstOmxJpegEnc, gst_omx_jpeg_enc, GST_TYPE_OMX_BASE);

static gboolean gst_omx_jpeg_enc_set_caps (GstPad * pad, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_jpeg_enc_init_pads (GstOmxBase * this);
static GstFlowReturn gst_omx_jpeg_enc_fill_callback (GstOmxBase *,
    OMX_BUFFERHEADERTYPE *);

static void gst_omx_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_jpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_jpeg_enc_class_init (GstOmxJpegEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;
  GstPadTemplate *template;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX JPEG video encoder",
      "Codec/Encoder/Video",
      "RidgeRun's OMX based JPEG encoder",
      "Diego Solano <diego.solano@ridgerun.com>");

  template = gst_static_pad_template_get (&src_template);
  gst_element_class_add_pad_template (gstelement_class,
      template);
  gst_object_unref (template);

  template = gst_static_pad_template_get (&sink_template);
  gst_element_class_add_pad_template (gstelement_class,
      template);
  gst_object_unref (template);

  gobject_class->set_property = gst_omx_jpeg_enc_set_property;
  gobject_class->get_property = gst_omx_jpeg_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_uint ("quality", "MJPEG/JPEG quality",
          "MJPEG/JPEG quality (integer 0:min 100:max)",
          0, 100, GST_OMX_JPEG_ENC_QUALITY_DEFAULT, G_PARAM_READWRITE));

  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_jpeg_enc_set_caps);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_jpeg_enc_fill_callback);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_jpeg_enc_init_pads);

  gstomxbase_class->handle_name = "OMX.TI.DUCATI.VIDENC";       //TODO: change here the omx component

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_jpeg_enc_debug, "omx_jpegenc",
      0, "RidgeRun's OMX based JPEG encoder");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_jpeg_enc_init (GstOmxJpegEnc * this)
{
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  /* Initialize properties */
  this->quality = GST_OMX_JPEG_ENC_QUALITY_DEFAULT;
  this->is_interlaced = FALSE;

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
gst_omx_jpeg_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxJpegEnc *this = GST_OMX_JPEG_ENC (object);
  GstOmxBase *base = GST_OMX_BASE (this);
  gboolean reconf = FALSE;

  switch (prop_id) {

    case PROP_QUALITY:
      this->quality = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting quality to %d", this->quality);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_jpeg_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxJpegEnc *this = GST_OMX_JPEG_ENC (object);

  switch (prop_id) {

    case PROP_QUALITY:
      g_value_set_uint (value, this->quality);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_omx_jpeg_enc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxJpegEnc *this = GST_OMX_JPEG_ENC (GST_OBJECT_PARENT (pad));
  GstOmxBase *base = GST_OMX_BASE (this);
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure = NULL;
  GstCaps *allowedcaps = NULL;
  GstCaps *newcaps = NULL;
  GValue stride = { 0, };

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_INFO_OBJECT (this, "Reading width");
  if (!gst_structure_get_int (structure, "width", &this->format.width)) {
    this->format.width = -1;
    goto invalidcaps;
  }

  GST_INFO_OBJECT (this, "Reading height");
  if (!gst_structure_get_int (structure, "height", &this->format.height)) {
    this->format.height = -1;
    goto invalidcaps;
  }

  if (!gst_structure_get_boolean (structure, "interlaced",
          &this->is_interlaced))
    this->is_interlaced = FALSE;

  GST_INFO_OBJECT (this, "Reading framerate");
  if (!gst_structure_get_fraction (structure, "framerate",
          &this->format.framerate_num, &this->format.framerate_den)) {
    this->format.framerate_num = -1;
    this->format.framerate_den = -1;
    goto invalidcaps;
  }

  GST_INFO_OBJECT (this, "Parsed for input caps:\n"
      "\tSize: %ux%u\n"
      "\tFormat NV12\n"
      "\tFramerate: %u/%u",
      this->format.width,
      this->format.height,
      this->format.framerate_num, this->format.framerate_den);

  /* Ask for the output caps, if not fixed then try the biggest frame */
  allowedcaps = gst_pad_get_allowed_caps (this->srcpad);
  newcaps = gst_caps_make_writable (gst_caps_copy_nth (allowedcaps, 0));
  srcstructure = gst_caps_get_structure (newcaps, 0);
  gst_caps_unref (allowedcaps);

  GST_INFO_OBJECT (this, "Fixating output caps");
  gst_structure_fixate_field_nearest_fraction (srcstructure, "framerate",
      this->format.framerate_num, this->format.framerate_den);
  gst_structure_fixate_field_nearest_int (srcstructure, "width",
      this->format.width);
  gst_structure_fixate_field_nearest_int (srcstructure, "height",
      this->format.height);

  gst_structure_get_int (srcstructure, "width", &this->format.width);
  gst_structure_get_int (srcstructure, "height", &this->format.height);
  gst_structure_get_fraction (srcstructure, "framerate",
      &this->format.framerate_num, &this->format.framerate_den);

  GST_INFO_OBJECT (this, "Output caps: %s", gst_caps_to_string (newcaps));

  if (!gst_pad_set_caps (this->srcpad, newcaps))
    goto nosetcaps;

  gst_caps_unref (newcaps);

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
gst_omx_jpeg_enc_init_pads (GstOmxBase * base)
{
  GstOmxJpegEnc *this = GST_OMX_JPEG_ENC (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname = NULL;
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  OMX_IMAGE_PARAM_QFACTORTYPE tQualityFactor;


  /* PADS and PORTS initialization for OMX_COMPONENT  */


  GST_INFO_OBJECT (this, "Initializing sink pad memory");
  GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
  memory.nPortIndex = 0;
  memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "input";
    goto noport;
  }

  GST_INFO_OBJECT (this, "Initializing src pad memory");
  GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
  memory.nPortIndex = 1;
  memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "output";
    goto noport;
  }


  GST_INFO_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = 0;         // OMX_VIDENC_INPUT_PORT
  port->eDir = OMX_DirInput;

  port->nBufferCountActual = base->input_buffers;
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height;
  if (this->is_interlaced) {
    port->format.video.nFrameHeight = this->format.height * 0.5;
  }
  port->format.video.nStride = this->format.width;
  port->format.video.xFramerate =
      ((guint) ((gdouble) this->format.framerate_num) /
      this->format.framerate_den) << 16;
  port->format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
  port->nBufferSize =           //this->format.size;
      (port->format.video.nStride * port->format.video.nFrameHeight) * 1.5;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }

  GST_INFO_OBJECT (this,
      "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu",
      port->nPortIndex, port->format.video.nFrameWidth,
      port->format.video.nFrameHeight, port->format.video.nStride,
      port->format.video.eColorFormat, port->nBufferSize);

  GST_INFO_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));

  port->nPortIndex = 1;         // OMX_VIDENC_OUTPUT_PORT
  port->eDir = OMX_DirOutput;

  port->nBufferCountActual = base->output_buffers;
  port->nBufferSize = this->format.width * this->format.height;
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height;

  if (this->is_interlaced) {
    port->format.video.nFrameHeight = this->format.height / 2;
    port->nBufferSize = this->format.width * this->format.height / 2;
  }
  port->format.video.nStride = 0;
  port->format.video.xFramerate =
      ((guint) ((gdouble) this->format.framerate_num) /
      this->format.framerate_den) << 16;
  port->format.video.nBitrate = 500000; //TODO: testing value only
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);

  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

  GST_INFO_OBJECT (this,
      "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu bitrate=%d",
      port->nPortIndex, port->format.video.nFrameWidth,
      port->format.video.nFrameHeight, port->format.video.nStride,
      port->format.video.eCompressionFormat, port->nBufferSize,
      port->format.video.nBitrate);



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

  GST_INFO_OBJECT (this, "Ports enabled ok");


  /* SETTING QUALITY PROPERTY TO OMX_COMPONENT */

  GST_OMX_INIT_STRUCT (&tQualityFactor, OMX_IMAGE_PARAM_QFACTORTYPE);

  GST_INFO_OBJECT (this, "Initial QFactor %d", (gint) tQualityFactor.nQFactor);

  tQualityFactor.nPortIndex = 1;

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_IndexParamQFactor, &tQualityFactor);
  g_mutex_unlock (&_omx_mutex);

  GST_INFO_OBJECT (this, "Got QFactor %d", (gint) tQualityFactor.nQFactor);

  tQualityFactor.nQFactor = this->quality;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamQFactor, &tQualityFactor);
  g_mutex_unlock (&_omx_mutex);

  /* Testing correct quality setting */

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_IndexParamQFactor, &tQualityFactor);
  g_mutex_unlock (&_omx_mutex);

  GST_INFO_OBJECT (this, "Exit setup QFactor %d",
      (gint) tQualityFactor.nQFactor);

  if (error == OMX_ErrorUnsupportedIndex) {
    GST_WARNING_OBJECT (this, "Setting quality not supported by component");
  }

  return error;

noport:
  {
    GST_ERROR_OBJECT (this, "Failed to set %s port parameters", portname);
    return error;
  }
noconfiguration:
  {
    GST_ERROR_OBJECT (this, "Unable to dynamically change parameters: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
noenable:
  {
    GST_ERROR_OBJECT (this, "Failed to enable jpeg encoder");
    return error;
  }
}

static GstFlowReturn
gst_omx_jpeg_enc_fill_callback (GstOmxBase * base,
    OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxJpegEnc *this = GST_OMX_JPEG_ENC (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;

  GST_INFO_OBJECT (this, "JPEG Encoder Fill buffer callback");

  GST_INFO_OBJECT (this,
      "______________GOT OMX BUFFER FROM COMPONENT omx_buffer=%p size=%lu, len=%lu, flags=%lu, offset=%lu, timestamp=%lld",
      outbuf, outbuf->nAllocLen, outbuf->nFilledLen, outbuf->nFlags,
      outbuf->nOffset, outbuf->nTimeStamp);

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
  GST_BUFFER_DURATION (buffer) =
      1e9 * this->format.framerate_den / this->format.framerate_num;
  GST_BUFFER_FLAG_SET (buffer, GST_OMX_BUFFER_FLAG);
  bufdata->buffer = buffer;

  GST_INFO_OBJECT (this,
      "(Fill %s) Buffer %p size %d reffcount %d bufdat %p->%p",
      GST_OBJECT_NAME (this), outbuf->pBuffer, GST_BUFFER_SIZE (buffer),
      GST_OBJECT_REFCOUNT (buffer), bufdata, bufdata->buffer);

  GST_INFO_OBJECT (this, "Pushing buffer %p->%p to %s:%s",
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
