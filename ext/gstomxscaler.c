/*
 * Gstreamer
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
 * SECTION:element-omx_scaler
 *
 * FIXME:Describe omx_scaler here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omx_scaler ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#include "gstomxscaler.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_scaler_debug);
#define GST_CAT_DEFAULT gst_omx_scaler_debug

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv,"
        "format=(fourcc)YUY2,"
        "width=[16,1920]," "height=[16,1080]," "interlaced={true,false}")
    );

#define gst_omx_scaler_parent_class parent_class
G_DEFINE_TYPE (GstOmxScaler, gst_omx_scaler, GST_TYPE_OMX_BASE);

static gboolean gst_omx_scaler_set_caps (GstPad * pad, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_scaler_init_pads (GstOmxBase * this);
static GstFlowReturn gst_omx_scaler_fill_callback (GstOmxBase *,
    OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_scaler_dynamic_configuration (GstOmxScaler * this,
    GstOmxPad *, GstOmxFormat *);

/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_scaler_class_init (GstOmxScalerClass * klass)
{
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX video scaler",
      "Filter/Converter/Video/Scaler",
      "RidgeRun's OMX based scaler",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_scaler_set_caps);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_scaler_init_pads);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_scaler_fill_callback);

  gstomxbase_class->handle_name = "OMX.TI.VPSSM3.VFPC.INDTXSCWB";

  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_scaler_debug, "omx_scaler", 0,
      "RidgeRun's OMX based scaler");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_scaler_init (GstOmxScaler * this)
{
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

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

static gboolean
gst_omx_scaler_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxScaler *this = GST_OMX_SCALER (GST_OBJECT_PARENT (pad));
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure;
  GstCaps *allowedcaps;
  GstCaps *newcaps;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG_OBJECT (this, "Reading width");
  if (!gst_structure_get_int (structure, "width", &this->in_format.width)) {
    this->in_format.width = -1;
    goto invalidcaps;
  }

  GST_DEBUG_OBJECT (this, "Reading stride");
  if (!gst_structure_get_int (structure, "stride",
          &this->in_format.width_padded)) {
    this->in_format.width_padded = GST_OMX_ALIGN (this->in_format.width, 16);
  }

  GST_DEBUG_OBJECT (this, "Reading height");
  if (!gst_structure_get_int (structure, "height", &this->in_format.height)) {
    this->in_format.height = -1;
    goto invalidcaps;
  }
  this->in_format.height_padded = GST_OMX_ALIGN (this->in_format.height, 16);

  GST_DEBUG_OBJECT (this, "Reading framerate");
  if (!gst_structure_get_fraction (structure, "framerate",
          &this->in_format.framerate_num, &this->in_format.framerate_den)) {
    this->in_format.framerate_num = -1;
    this->in_format.framerate_den = -1;
    goto invalidcaps;
  }

  GST_DEBUG_OBJECT (this, "Reading interlaced");
  if (!gst_structure_get_int (structure, "interlaced",
          &this->in_format.interlaced)) {
    this->in_format.interlaced = FALSE;
  }

  /* This is always fixed */
  this->in_format.format = GST_VIDEO_FORMAT_NV12;

  this->in_format.size = gst_video_format_get_size (this->in_format.format,
      this->in_format.width, this->in_format.height);
  this->in_format.size_padded =
      this->in_format.width_padded * this->in_format.height_padded * 1.5;

  GST_INFO_OBJECT (this, "Parsed for input caps:\n"
      "\tSize: %ux%u\n"
      "\tFormat NV12\n"
      "\tFramerate: %u/%u",
      this->in_format.width,
      this->in_format.height,
      this->in_format.framerate_num, this->in_format.framerate_den);

  /* Ask for the output caps, if not fixed then try the biggest frame */
  allowedcaps = gst_pad_get_allowed_caps (this->srcpad);
  newcaps = gst_caps_make_writable (gst_caps_copy_nth (allowedcaps, 0));
  srcstructure = gst_caps_get_structure (newcaps, 0);
  gst_caps_unref (allowedcaps);

  GST_DEBUG_OBJECT (this, "Fixating output caps");
  gst_structure_fixate_field_nearest_fraction (srcstructure, "framerate",
      this->in_format.framerate_num, this->in_format.framerate_den);

  gst_structure_fixate_field_nearest_int (srcstructure, "width",
      this->in_format.width);
  gst_structure_fixate_field_nearest_int (srcstructure, "height",
      this->in_format.height);
  gst_structure_fixate_field_boolean (srcstructure, "interlaced",
      this->in_format.interlaced);

  this->out_format.format = GST_VIDEO_FORMAT_YUY2;

  gst_structure_get_int (srcstructure, "width", &this->out_format.width);
  this->out_format.width_padded =
      GST_OMX_ALIGN (this->out_format.width, 16) * 2;

  gst_structure_get_int (srcstructure, "height", &this->out_format.height);
  this->out_format.height_padded = this->out_format.height;

  gst_structure_get_fraction (srcstructure, "framerate",
      &this->out_format.framerate_num, &this->out_format.framerate_den);

  GST_DEBUG_OBJECT (this, "Output caps: %s", gst_caps_to_string (newcaps));

  this->out_format.size = gst_video_format_get_size (this->out_format.format,
      this->out_format.width, this->out_format.height);
  this->out_format.size_padded =
      this->out_format.width_padded * this->out_format.height;

  GST_INFO_OBJECT (this, "Parsed for output caps:\n"
      "\tSize: %ux%u\n"
      "\tFormat YUY2\n"
      "\tFramerate: %u/%u",
      this->out_format.width,
      this->out_format.height,
      this->out_format.framerate_num, this->out_format.framerate_den);

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

/* vmethod implementations */
static OMX_ERRORTYPE
gst_omx_scaler_init_pads (GstOmxBase * base)
{
  GstOmxScaler *this = GST_OMX_SCALER (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_PARAM_VFPC_NUMCHANNELPERHANDLE channels;
  OMX_CONFIG_ALG_ENABLE enable;
  gchar *portname;

  GST_DEBUG_OBJECT (this, "Initializing sink pad memory");
  GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
  memory.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "input";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Initializing src pad memory");
  GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
  memory.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
  memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "output";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  port->eDir = OMX_DirInput;

  port->nBufferCountActual = 6;
  port->format.video.nFrameWidth = this->in_format.width;       //OMX_VFPC_DEFAULT_INPUT_FRAME_WIDTH;
  port->format.video.nFrameHeight = this->in_format.height;     //OMX_VFPC_DEFAULT_INPUT_FRAME_HEIGHT;
  port->format.video.nStride = this->in_format.width_padded;
  port->format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

  port->nBufferSize = this->in_format.size_padded;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (base->handle, OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "input";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));

  port->nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
  port->eDir = OMX_DirOutput;

  port->nBufferCountActual = 6;
  port->format.video.nFrameWidth = this->out_format.width;      //OMX_VFPC_DEFAULT_INPUT_FRAME_WIDTH;
  port->format.video.nFrameHeight = this->out_format.height;    //OMX_VFPC_DEFAULT_INPUT_FRAME_HEIGHT;
  port->format.video.nStride = this->out_format.width_padded;
  port->format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
  port->nBufferSize = this->out_format.size_padded;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (base->handle, OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "output";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Setting channels per handle");
  GST_OMX_INIT_STRUCT (&channels, OMX_PARAM_VFPC_NUMCHANNELPERHANDLE);
  channels.nNumChannelsPerHandle = 1;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &channels);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto nochannels;

  error = gst_omx_scaler_dynamic_configuration (this,
      GST_OMX_PAD (this->sinkpad), &this->in_format);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;

  error = gst_omx_scaler_dynamic_configuration (this,
      GST_OMX_PAD (this->srcpad), &this->out_format);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;

  GST_DEBUG_OBJECT (this, "Deactivating bypass mode");
  GST_OMX_INIT_STRUCT (&enable, OMX_CONFIG_ALG_ENABLE);
  enable.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  enable.nChId = 0;
  enable.bAlgBypass = 0;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetConfig (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &enable);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noenable;

  GST_INFO_OBJECT (this, "Enabling input port");
  g_mutex_lock (&_omx_mutex);
  OMX_SendCommand (base->handle, OMX_CommandPortEnable,
      OMX_VFPC_INPUT_PORT_START_INDEX, NULL);
  g_mutex_unlock (&_omx_mutex);

  GST_INFO_OBJECT (this, "Waiting for input port to enable");
  error = gst_omx_base_wait_for_condition (base,
      gst_omx_base_condition_enabled,
      (gpointer) & GST_OMX_PAD (this->sinkpad)->enabled, NULL);
  if (GST_OMX_FAIL (error))
    goto noenable;

  GST_INFO_OBJECT (this, "Enabling output port");
  g_mutex_lock (&_omx_mutex);
  OMX_SendCommand (base->handle, OMX_CommandPortEnable,
      OMX_VFPC_OUTPUT_PORT_START_INDEX, NULL);
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
nochannels:
  {
    GST_ERROR_OBJECT (this, "Failed to set channels per handle");
    return error;
  }
noconfiguration:
  {
    GST_ERROR_OBJECT (this, "Unable to dynamically change resolutions: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
noenable:
  {
    GST_ERROR_OBJECT (this, "Failed to enable scaler");
    return error;
  }
}

static GstFlowReturn
gst_omx_scaler_fill_callback (GstOmxBase * base, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxScaler *this = GST_OMX_SCALER (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;

  GST_LOG_OBJECT (this, "Scaler fill buffer callback");

  caps = gst_pad_get_negotiated_caps (this->srcpad);
  if (!caps)
    goto nocaps;

  buffer = gst_buffer_new ();
  if (!buffer)
    goto noalloc;

  GST_BUFFER_SIZE (buffer) = this->out_format.size_padded;
  GST_BUFFER_CAPS (buffer) = caps;
  GST_BUFFER_DATA (buffer) = outbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) outbuf;
  GST_BUFFER_FREE_FUNC (buffer) = gst_omx_base_release_buffer;
  GST_BUFFER_TIMESTAMP (buffer) = outbuf->nTimeStamp;
  GST_BUFFER_DURATION (buffer) =
      1e9 * this->out_format.framerate_den / this->out_format.framerate_num;

  GST_LOG_OBJECT (this, "Pushing buffer to %s:%s",
      GST_DEBUG_PAD_NAME (this->srcpad));
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
gst_omx_scaler_dynamic_configuration (GstOmxScaler * this,
    GstOmxPad * pad, GstOmxFormat * format)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_CONFIG_VIDCHANNEL_RESOLUTION resolution;
  OMX_PARAM_PORTDEFINITIONTYPE *port;

  port = GST_OMX_PAD_PORT (pad);

  GST_DEBUG_OBJECT (this, "Dynamically changing resolution");
  GST_OMX_INIT_STRUCT (&resolution, OMX_CONFIG_VIDCHANNEL_RESOLUTION);
  resolution.Frm0Width = format->width;
  resolution.Frm0Height = format->height;
  resolution.Frm0Pitch = port->eDir == OMX_DirInput ?
      GST_OMX_ALIGN (resolution.Frm0Width, 16) :
      GST_OMX_ALIGN (resolution.Frm0Width, 16) * 2;
  resolution.Frm1Width = 0;
  resolution.Frm1Height = 0;
  resolution.Frm1Pitch = 0;
  resolution.FrmStartX = port->eDir == OMX_DirInput ? 0 : 0;
  resolution.FrmStartY = port->eDir == OMX_DirInput ? 0 : 0;
  resolution.FrmCropWidth = port->eDir == OMX_DirInput ? 0 : 0; // Adjust to width
  resolution.FrmCropHeight = port->eDir == OMX_DirInput ? 0 : 0;        // Adjust to height

  resolution.eDir = port->eDir;
  resolution.nChId = 0;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetConfig (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexConfigVidChResolution, &resolution);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noresolution;

  return error;

noresolution:
  {
    GST_ERROR_OBJECT (this, "Unable to change dynamically resolution: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}
