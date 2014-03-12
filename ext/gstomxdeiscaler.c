/*
 * Gstreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Melissa Montero <melissa.montero@ridgerun.com>
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
 * SECTION:element-omx_deiscaler
 *
 * omx_deiscaler takes interlaced/progressive input and provides up 
 * to two scaled progressive version of the video. The omx_deiscaler
 * deinterlaces the video frames when required. 
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m videotestsrc peer-alloc=false ! omx_deiscaler peer-alloc=false ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#include "gstomxdeiscaler.h"
#include "gstomxutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_deiscaler_debug);
#define GST_CAT_DEFAULT gst_omx_deiscaler_debug

#define NUM_OUTPUTS 2

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
    );

static GstStaticPadTemplate src0_template = GST_STATIC_PAD_TEMPLATE ("src_00",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv,"
        "format=(fourcc)YUY2,"
        "width=[16,1920]," "height=[16,1920]," 
        "framerate=" GST_VIDEO_FPS_RANGE ","
        "interlaced={true,false}")
    );

static GstStaticPadTemplate src1_template = GST_STATIC_PAD_TEMPLATE ("src_01 ",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv,"
        "format=(fourcc)NV12,"
        "width=[16,1920]," "height=[16,1920]," 
        "framerate=" GST_VIDEO_FPS_RANGE ","
        "interlaced={true,false}")
    );

#define gst_omx_deiscaler_parent_class parent_class
G_DEFINE_TYPE (GstOmxDeiscaler, gst_omx_deiscaler, GST_TYPE_OMX_BASE);

static gboolean gst_omx_deiscaler_set_caps (GstPad * pad, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_deiscaler_init_pads (GstOmxBase * this);
static GstFlowReturn gst_omx_deiscaler_fill_callback (GstOmxBase *,
    OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_deiscaler_sink_dynamic_configuration (GstOmxDeiscaler * this,
    GstOmxPad *, GstOmxFormat *);
static OMX_ERRORTYPE gst_omx_deiscaler_srcs_dynamic_configuration (GstOmxDeiscaler * this,
    GList * pads, GList * formats);

/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_deiscaler_class_init (GstOmxDeiscalerClass * klass)
{
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX video deiscaler",
      "Filter/Converter/Video/Deiscaler",
      "RidgeRun's OMX based deiscaler",
      "Michael Gruner <michael.gruner@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src0_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src1_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_deiscaler_set_caps);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_deiscaler_init_pads);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_deiscaler_fill_callback);

  //~ gstomxbase_class->handle_name = "OMX.TI.VPSSM3.VFPC.DEIHDUALOUT";
  gstomxbase_class->handle_name = "OMX.TI.VPSSM3.VFPC.DEIMDUALOUT";
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_deiscaler_debug, "omx_deiscaler", 0,
      "RidgeRun's OMX based deiscaler");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_deiscaler_init (GstOmxDeiscaler * this)
{
  GstPad *srcpad;
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->sinkpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&sink_template), "sink"));
  gst_pad_set_active (this->sinkpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE (this), this->sinkpad);

  this->srcpads = NULL;
  srcpad = GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src0_template), "src_00"));
  gst_pad_set_active (srcpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE (this), srcpad);
  this->srcpads = g_list_prepend (this->srcpads, srcpad);
  
  GST_OMX_PAD_PORT (GST_OMX_PAD (srcpad))->nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX;
  
  srcpad = GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src1_template), "src_01"));
  gst_pad_set_active (srcpad, TRUE);
  gst_omx_base_add_pad (GST_OMX_BASE (this), srcpad);
  this->srcpads = g_list_append (this->srcpads, srcpad);
  GST_OMX_PAD_PORT (GST_OMX_PAD (srcpad))->nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + 1;
}

static gboolean
gst_omx_deiscaler_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxDeiscaler *this = GST_OMX_DEISCALER (GST_OBJECT_PARENT (pad));
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure;
  GstCaps *allowedcaps;
  GstCaps *newcaps;
  GstPad *srcpad;
  GList *l;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_LOG_OBJECT (this, "Reading width");
  if (!gst_structure_get_int (structure, "width", &this->in_format.width)) {
    this->in_format.width = -1;
    goto invalidcaps;
  }
  this->in_format.width_padded = GST_OMX_ALIGN (this->in_format.width, 16);

  GST_LOG_OBJECT (this, "Reading height");
  if (!gst_structure_get_int (structure, "height", &this->in_format.height)) {
    this->in_format.height = -1;
    goto invalidcaps;
  }
  this->in_format.height_padded = this->in_format.height;

  GST_LOG_OBJECT (this, "Reading framerate");
  if (!gst_structure_get_fraction (structure, "framerate",
          &this->in_format.framerate_num, &this->in_format.framerate_den)) {
    this->in_format.framerate_num = -1;
    this->in_format.framerate_den = -1;
    goto invalidcaps;
  }

  /* This is always fixed */
  this->in_format.format = GST_VIDEO_FORMAT_NV12;

  this->in_format.size = gst_video_format_get_size (this->in_format.format,
      this->in_format.width, this->in_format.height);
  this->in_format.size_padded =
      this->in_format.width_padded * this->in_format.height * 1.5;

  GST_INFO_OBJECT (this, "Parsed for input caps:\n"
      "\tSize: %ux%u\n"
      "\tFormat NV12\n"
      "\tFramerate: %u/%u",
      this->in_format.width,
      this->in_format.height,
      this->in_format.framerate_num, this->in_format.framerate_den);

  for (l=this->srcpads; l; l = l->next) {
    srcpad = l->data;
    GstOmxFormat *out_format;
    out_format = g_slice_new0 (GstOmxFormat);

    /* Ask for the output caps, if not fixed then try the biggest frame */
    allowedcaps = gst_pad_get_allowed_caps (srcpad);
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
    gst_structure_fixate_field_boolean (srcstructure, "interlaced", FALSE);

    if (!gst_caps_is_fixed (newcaps))
      GST_DEBUG_OBJECT (this, "not fixed caps");

    GST_DEBUG_OBJECT (this, "new caps %s", gst_caps_to_string (newcaps));
    if (!gst_video_format_parse_caps (newcaps, &out_format->format, &out_format->width, &out_format->height))
      goto invalidcaps;
    
    out_format->height_padded = out_format->height;

    gst_structure_get_fraction (srcstructure, "framerate",
        &out_format->framerate_num, &out_format->framerate_den);

    GST_DEBUG_OBJECT (this, "Output caps: %s", gst_caps_to_string (newcaps));

    out_format->size = gst_video_format_get_size (out_format->format,
        out_format->width, out_format->height);

    if (out_format->format == GST_VIDEO_FORMAT_YUY2) {
      out_format->width_padded = GST_OMX_ALIGN (out_format->width, 16) * 2;
      out_format->size_padded =
          out_format->width_padded * out_format->height;
    } else {
      out_format->width_padded = GST_OMX_ALIGN (out_format->width, 16);
      out_format->size_padded =
          out_format->width_padded * out_format->height * 1.5;
    }
    
    GST_INFO_OBJECT (this, "Parsed for output caps:\n"
        "\tSize: %ux%u\n"
        "\tFormat %s\n"
        "\tFramerate: %u/%u",
        out_format->width,
        out_format->height,
        out_format->format == GST_VIDEO_FORMAT_YUY2? "YUY2":"NV12",
        out_format->framerate_num, out_format->framerate_den);

    if (!gst_pad_set_caps (srcpad, newcaps))
      goto nosetcaps;
      
    this->out_formats = g_list_append (this->out_formats, out_format);
  }
  return TRUE;

invalidcaps:
  {
    GST_ERROR_OBJECT (this, "Unable to grab stream format from caps!");
    return FALSE;
  }
nosetcaps:
  {
    GST_ERROR_OBJECT (this, "%s:%s didn't accept new caps", GST_DEBUG_PAD_NAME (srcpad));
    return FALSE;
  }
}

/* vmethod implementations */
static OMX_ERRORTYPE
gst_omx_deiscaler_init_pads (GstOmxBase * base)
{
  GstOmxDeiscaler *this = GST_OMX_DEISCALER (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_PARAM_VFPC_NUMCHANNELPERHANDLE channels;
  OMX_CONFIG_ALG_ENABLE enable;
  OMX_CONFIG_SUBSAMPLING_FACTOR subsampling_factor = {0};
  gchar *portname;
  GList *l, *f;
  gint i;

  GST_OMX_INIT_STRUCT (&subsampling_factor, OMX_CONFIG_SUBSAMPLING_FACTOR);
  subsampling_factor.nSubSamplingFactor = 1;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetConfig (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexConfigSubSamplingFactor, &subsampling_factor);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;
    
  GST_DEBUG_OBJECT (this, "Initializing input port memory");
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

  GST_DEBUG_OBJECT (this, "Initializing output ports memory");
  for (i=0 ; i< NUM_OUTPUTS; i++) {
    
    GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
    memory.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + i;
    memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
    g_mutex_lock (&_omx_mutex);
    error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      portname = "output";
      goto noport;
    }
    GST_LOG_OBJECT (this, "Setting memory to raw for port %lu", memory.nPortIndex);
  }

  GST_DEBUG_OBJECT (this, "Setting input port definition");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));
  
  GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);
  port->nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  
  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle, OMX_IndexParamPortDefinition,
                    &port);
  g_mutex_unlock (&_omx_mutex);

  port->nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  port->format.video.nFrameWidth = this->in_format.width;       //OMX_VFPC_DEFAULT_INPUT_FRAME_WIDTH;
  port->format.video.nFrameHeight = this->in_format.height;     //OMX_VFPC_DEFAULT_INPUT_FRAME_HEIGHT;
  port->format.video.nStride = this->in_format.width_padded;
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  port->format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
  port->nBufferSize = this->in_format.size_padded;
  port->nBufferAlignment = 0;
  port->bBuffersContiguous = 0;
  port->nBufferCountActual = 8;
  
  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (base->handle, OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "input";
    goto noport;
  }
  GST_DEBUG_OBJECT (this, "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu", 
      port->nPortIndex,
      port->format.video.nFrameWidth,
      port->format.video.nFrameHeight,
      port->format.video.nStride,
      port->format.video.eColorFormat,
      port->nBufferSize);
      

  GST_DEBUG_OBJECT (this, "Setting output port definition");
  f = this->out_formats;
  for (l = this->srcpads, i=0; l; l=l->next,i++) {
    GstPad * srcpad = l->data;
    GstOmxFormat *out_format = f->data;
    guint32 index;
    
    port = GST_OMX_PAD_PORT (GST_OMX_PAD (srcpad));
    index = port->nPortIndex;

    GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);
    port->nPortIndex = index;
    
    g_mutex_lock (&_omx_mutex);
    OMX_GetParameter (base->handle, OMX_IndexParamPortDefinition,
                    &port);
    g_mutex_unlock (&_omx_mutex);

    port->nPortIndex = index;
    port->format.video.nFrameWidth = out_format->width;      //OMX_VFPC_DEFAULT_INPUT_FRAME_WIDTH;
    port->format.video.nFrameHeight = out_format->height;    //OMX_VFPC_DEFAULT_INPUT_FRAME_HEIGHT;
    port->format.video.nStride = out_format->width_padded;
    port->format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    if (port->nPortIndex == 16) {
      port->format.video.eColorFormat = gst_omx_convert_format_to_omx (out_format->format);
    } else {
      GST_DEBUG_OBJECT (this, "Skipping eColorFormat");
    }
    port->nBufferSize = out_format->size_padded;
    port->nBufferAlignment = 0;
    port->nBufferCountActual = 8;

    GST_DEBUG_OBJECT (this, "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu", 
      port->nPortIndex,
      port->format.video.nFrameWidth,
      port->format.video.nFrameHeight,
      port->format.video.nStride,
      port->format.video.eColorFormat,
      port->nBufferSize);

    g_mutex_lock (&_omx_mutex);
    error = OMX_SetParameter (base->handle, OMX_IndexParamPortDefinition, port);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      portname = "output";
      goto noport;
    }
    f = g_list_next (f);
  }

  GST_DEBUG_OBJECT (this, "Setting number of channels per handle");
  GST_OMX_INIT_STRUCT (&channels, OMX_PARAM_VFPC_NUMCHANNELPERHANDLE);
  channels.nNumChannelsPerHandle = 1;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &channels);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto nochannels;

  error = gst_omx_deiscaler_sink_dynamic_configuration (this,
      GST_OMX_PAD (this->sinkpad), &this->in_format);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;
    
  error = gst_omx_deiscaler_srcs_dynamic_configuration (this,
      this->srcpads, this->out_formats);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;

  GST_DEBUG_OBJECT (this, "Activating bypass mode");
  GST_OMX_INIT_STRUCT (&enable, OMX_CONFIG_ALG_ENABLE);
  enable.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX;
  enable.nChId = 0;
  enable.bAlgBypass = 1;

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

  for (l = this->srcpads, i=0; l; l=l->next,i++) {
    GstPad * srcpad = l->data;
    GST_INFO_OBJECT (this, "Enabling output port %lu", (GST_OMX_PAD_PORT ( GST_OMX_PAD(srcpad)))->nPortIndex);
    g_mutex_lock (&_omx_mutex);
    OMX_SendCommand (base->handle, OMX_CommandPortEnable,
        (GST_OMX_PAD_PORT ( GST_OMX_PAD(srcpad)))->nPortIndex, NULL);
    g_mutex_unlock (&_omx_mutex);

    GST_INFO_OBJECT (this, "Waiting for output port to enable");
    error = gst_omx_base_wait_for_condition (base,
        gst_omx_base_condition_enabled,
        (gpointer) & GST_OMX_PAD (srcpad)->enabled, NULL);
    if (GST_OMX_FAIL (error))
      goto noenable;
  }

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
    GST_ERROR_OBJECT (this, "Failed to enable deiscaler");
    return error;
  }
}

static GstFlowReturn
gst_omx_deiscaler_fill_callback (GstOmxBase * base, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxDeiscaler *this = GST_OMX_DEISCALER (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxFormat *out_format;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;
  GstPad *srcpad = GST_PAD(bufdata->pad);

  GST_LOG_OBJECT (this, "Deiscaler fill buffer callback");

  caps = gst_pad_get_negotiated_caps (srcpad);
  if (!caps)
    goto nocaps;

  buffer = gst_buffer_new ();
  if (!buffer)
    goto noalloc;

  out_format = this->out_formats->data;

  GST_BUFFER_SIZE (buffer) = GST_OMX_PAD_PORT(bufdata->pad)->nBufferSize;
  GST_BUFFER_CAPS (buffer) = caps;
  GST_BUFFER_DATA (buffer) = outbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) outbuf;
  GST_BUFFER_FREE_FUNC (buffer) = gst_omx_base_release_buffer;
  GST_BUFFER_TIMESTAMP (buffer) = outbuf->nTimeStamp;
  GST_BUFFER_DURATION (buffer) =
      1e9 * out_format->framerate_den / out_format->framerate_num;

  GST_LOG_OBJECT (this, "Pushing buffer to %s:%s",
      GST_DEBUG_PAD_NAME (srcpad));
  ret = gst_pad_push (srcpad, buffer);
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
gst_omx_deiscaler_sink_dynamic_configuration (GstOmxDeiscaler * this,
    GstOmxPad * pad, GstOmxFormat * format)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_CONFIG_VIDCHANNEL_RESOLUTION resolution;
  OMX_PARAM_PORTDEFINITIONTYPE *port;

  port = GST_OMX_PAD_PORT (pad);

  GST_DEBUG_OBJECT (this, "Setting input channel resolution");
  GST_OMX_INIT_STRUCT (&resolution, OMX_CONFIG_VIDCHANNEL_RESOLUTION);
  resolution.Frm0Width = format->width;
  resolution.Frm0Height = format->height;
  resolution.Frm0Pitch = format->width_padded;
  resolution.Frm1Width = 0;
  resolution.Frm1Height = 0;
  resolution.Frm1Pitch = 0;
  resolution.FrmStartX = 0;
  resolution.FrmStartY = 0;
  resolution.FrmCropWidth = format->width;  // Adjust to width
  resolution.FrmCropHeight = format->height; // Adjust to height

  resolution.eDir = OMX_DirInput;
  resolution.nChId = 0;
  
  GST_DEBUG_OBJECT (this, "Resolution settings:\n"
      "\tPort 0:  Width=%lu\n" 
      "\t\t Height=%lu\n"
      "\t\t Stride=%lu\n"
      "\tCrop: (%lu,%lu) %lux%lu", 
      resolution.Frm0Width,
      resolution.Frm0Height,
      resolution.Frm0Pitch,
      resolution.FrmStartX,
      resolution.FrmStartY,
      resolution.FrmCropWidth,
      resolution.FrmCropHeight);
      
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
    GST_ERROR_OBJECT (this, "Unable to change resolution: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_deiscaler_srcs_dynamic_configuration (GstOmxDeiscaler * this,
    GList * pads, GList * formats)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_CONFIG_VIDCHANNEL_RESOLUTION resolution;
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  GList *l, *f;

  GST_DEBUG_OBJECT (this, "Setting output channel resolution");
  GST_OMX_INIT_STRUCT (&resolution, OMX_CONFIG_VIDCHANNEL_RESOLUTION);
  
  resolution.FrmStartX = 0;
  resolution.FrmStartY = 0;
  resolution.FrmCropWidth = 0;
  resolution.FrmCropHeight = 0;

  resolution.eDir = OMX_DirOutput;
  resolution.nChId = 0;
    
  
  f = formats;

  for (l = pads; l; l=l->next) {
    GstPad *pad = l->data;
    GstOmxFormat *format = f->data;

    port = GST_OMX_PAD_PORT (GST_OMX_PAD (pad));
    if (port->nPortIndex == OMX_VFPC_OUTPUT_PORT_START_INDEX) {
      resolution.Frm0Width = format->width;
      resolution.Frm0Height = format->height;
      resolution.Frm0Pitch = format->width_padded;
    } else {
      resolution.Frm1Width = format->width;
      resolution.Frm1Height = format->height;
      resolution.Frm1Pitch = format->width_padded;
    }
    f = g_list_next (f);
  }

    GST_DEBUG_OBJECT (this, "Resolution settings:\n"
      "\tPort 0:  Width=%lu\n" 
      "\t\t Height=%lu\n"
      "\t\t Stride=%lu\n"
      "\tPort 1:  Width=%lu\n" 
      "\t\t Height=%lu\n"
      "\t\t Stride=%lu\n" 
      "\tCrop: (%lu,%lu) %lux%lu", 
      resolution.Frm0Width,
      resolution.Frm0Height,
      resolution.Frm0Pitch,
      resolution.Frm1Width,
      resolution.Frm1Height,
      resolution.Frm1Pitch,
      resolution.FrmStartX,
      resolution.FrmStartY,
      resolution.FrmCropWidth,
      resolution.FrmCropHeight);
      
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
    GST_ERROR_OBJECT (this, "Unable to change output resolution: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}
