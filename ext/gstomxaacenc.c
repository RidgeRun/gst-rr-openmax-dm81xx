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

#include "gstomxaacenc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_aac_enc_debug);
#define GST_CAT_DEFAULT gst_omx_aac_enc_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int," "endianness=1234," "width=16,"
        "depth=16," "rate=[8000,48000]," "signed=true," "channels=[1,2]")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg,"
        "mpegversion = {2,4}," "rate = [8000,48000]," "channels=[1,2]")
    );

enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_PROFILE,
  PROP_OUTPUT_FORMAT,
  PROP_ALWAYS_COPY,
};

#define GST_OMX_AAC_ENC_BITRATE_DEFAULT 128000
#define GST_OMX_AAC_ENC_PROFILE_DEFAULT OMX_AUDIO_AACObjectLC
#define GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT OMX_AUDIO_AACStreamFormatRAW
#define GST_OMX_AAC_ENC_ALWAYS_COPY_DEFAULT FALSE

gint gst_omx_aac_rateIdx[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000,
  11025, 8000, 7350
};

static GType
gst_omx_aac_enc_profile_get_type (void)
{
  static GType gst_omx_aac_enc_profile_type = 0;

  if (!gst_omx_aac_enc_profile_type) {
    static GEnumValue gst_omx_aac_enc_profile[] = {
      {OMX_AUDIO_AACObjectMain, "Main", "Main"},
      {OMX_AUDIO_AACObjectLC, "Low Complexity", "LC"},
      {OMX_AUDIO_AACObjectSSR, "Scalable Sample Rate", "SSR"},
      {OMX_AUDIO_AACObjectLTP, "Long Term Prediction", "LTP"},
      {OMX_AUDIO_AACObjectHE, "High Efficiency with SBR (HE-AAC v1)", "HE"},
      {OMX_AUDIO_AACObjectScalable, "Scalable", "Scalable"},
      {OMX_AUDIO_AACObjectERLC,
          "ER AAC Low Complexity object (Error Resilient AAC-LC)", "ERLC"},
      {OMX_AUDIO_AACObjectLD, "AAC Low Delay object (Error Resilient)", "LD"},
      {OMX_AUDIO_AACObjectHE_PS,
            "High Efficiency with Parametric Stereo coding (HE-AAC v2, object type PS)",
          "HE_PS"},
      {0, NULL, NULL},
    };

    gst_omx_aac_enc_profile_type =
        g_enum_register_static ("GstOmxAACencProfile", gst_omx_aac_enc_profile);
  }
  return gst_omx_aac_enc_profile_type;
}


static GType
gst_omx_aac_enc_output_format_get_type (void)
{
  static GType gst_omx_aac_enc_output_format_type = 0;

  if (!gst_omx_aac_enc_output_format_type) {
    static GEnumValue gst_omx_aac_enc_output_format[] = {
      {OMX_AUDIO_AACStreamFormatMP2ADTS, "Audio Data Transport Stream 2 format",
          "MP2ADTS"},
      {OMX_AUDIO_AACStreamFormatMP4ADTS, "Audio Data Transport Stream 4 format",
          "MP4ADTS"},
      {OMX_AUDIO_AACStreamFormatMP4LOAS, "Low Overhead Audio Stream format",
          "MP4LOAS"},
      {OMX_AUDIO_AACStreamFormatMP4LATM,
          "Low overhead Audio Transport Multiplex", "MP4LATM"},
      {OMX_AUDIO_AACStreamFormatADIF, "Audio Data Interchange Format", "ADIF"},
      {OMX_AUDIO_AACStreamFormatMP4FF, "AAC inside MPEG-4/ISO File Format",
          "MP4FF"},
      {OMX_AUDIO_AACStreamFormatRAW, "AAC Raw Format", "RAW"},
      {OMX_AUDIO_AACStreamFormatMax, "AAC Stream format MAX", "MAX"},
      {0, NULL, NULL},
    };

    gst_omx_aac_enc_output_format_type =
        g_enum_register_static ("GstOmxAACencOutputFormat",
        gst_omx_aac_enc_output_format);
  }

  return gst_omx_aac_enc_output_format_type;
}

#define GST_TYPE_OMX_AAC_ENC_PROFILE (gst_omx_aac_enc_profile_get_type ())
#define GST_TYPE_OMX_AAC_ENC_OUTPUT_FORMAT (gst_omx_aac_enc_output_format_get_type ())



#define gst_omx_aac_enc_parent_class parent_class
G_DEFINE_TYPE (GstOmxAACEnc, gst_omx_aac_enc, GST_TYPE_OMX_BASE);

static gboolean gst_omx_aac_enc_set_caps (GstPad * pad, GstCaps * caps);
static void gst_omx_aac_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_aac_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static OMX_ERRORTYPE gst_omx_aac_enc_init_pads (GstOmxBase * base);
static OMX_ERRORTYPE gst_omx_aac_enc_parameters (GstOmxAACEnc * this,
    GstOmxFormat * format);
static GstFlowReturn gst_omx_aac_enc_fill_callback (GstOmxBase * base,
    OMX_BUFFERHEADERTYPE * outbuf);
static gint gst_omx_aac_enc_get_rateIdx (guint rate);
static GstBuffer *gst_omx_aac_enc_generate_codec_data (GstOmxBase * base);

/* initialize the omx's class */
static void
gst_omx_aac_enc_class_init (GstOmxAACEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;
  GstPadTemplate *template;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX AAC audio encoder",
      "Codec/Encoder/AAC",
      "RidgeRun's OMX based AAC encoder",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");

  template = gst_static_pad_template_get (&src_template);
  gst_element_class_add_pad_template (gstelement_class, template);
  gst_object_unref (template);

  template = gst_static_pad_template_get (&sink_template);
  gst_element_class_add_pad_template (gstelement_class, template);
  gst_object_unref (template);

  gobject_class->set_property = gst_omx_aac_enc_set_property;
  gobject_class->get_property = gst_omx_aac_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Encoding bitrate",
          "Sets the encoder bitrate",
          0, 256000, GST_OMX_AAC_ENC_BITRATE_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PROFILE,
      g_param_spec_enum ("profile", "Encoding AAC profile",
          "Sets the AAC profile",
          GST_TYPE_OMX_AAC_ENC_PROFILE, GST_OMX_AAC_ENC_PROFILE_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_FORMAT,
      g_param_spec_enum ("output-format", "Output Format",
          "Sets the AAC output format",
          GST_TYPE_OMX_AAC_ENC_OUTPUT_FORMAT,
          GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ALWAYS_COPY,
      g_param_spec_boolean ("always-copy", "Always Copy",
          "Always copy the output buffer", GST_OMX_AAC_ENC_ALWAYS_COPY_DEFAULT,
          G_PARAM_READWRITE));


  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_aac_enc_set_caps);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_aac_enc_fill_callback);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_aac_enc_init_pads);

  gstomxbase_class->handle_name = "OMX.TI.DSP.AUDENC";

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_aac_enc_debug, "omx_aacenc",
      0, "RidgeRun's OMX based AAC encoder");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_aac_enc_init (GstOmxAACEnc * this)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  /* Initialize properties */
  this->bitrate = GST_OMX_AAC_ENC_BITRATE_DEFAULT;
  this->profile = GST_OMX_AAC_ENC_PROFILE_DEFAULT;
  this->output_format = GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT;
  /*Set Audio flag */
  base->output_buffers = 1;
  base->input_buffers = 1;
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
gst_omx_aac_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxAACEnc *this = GST_OMX_AAC_ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      this->bitrate = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting bitrate to %d", this->bitrate);
      break;
    case PROP_PROFILE:
      this->profile = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting the AAC profile to %d", this->profile);
      break;
    case PROP_OUTPUT_FORMAT:
      this->output_format = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting output format to %d",
          this->output_format);
      break;
    case PROP_ALWAYS_COPY:
      this->always_copy = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting always-copy to %d", this->always_copy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_omx_aac_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxAACEnc *this = GST_OMX_AAC_ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, this->bitrate);
      break;
    case PROP_PROFILE:
      g_value_set_enum (value, this->profile);
      break;
    case PROP_OUTPUT_FORMAT:
      g_value_set_enum (value, this->output_format);
      break;
    case PROP_ALWAYS_COPY:
      g_value_set_boolean (value, this->always_copy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gint
gst_omx_aac_enc_get_rateIdx (guint rate)
{
  gint i;

  for (i = 0; i < 13; i++) {
    if (rate >= gst_omx_aac_rateIdx[i])
      return i;
  }

  return -1;
}


static gboolean
gst_omx_aac_enc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxAACEnc *this = GST_OMX_AAC_ENC (GST_OBJECT_PARENT (pad));
  GstOmxBase *base = GST_OMX_BASE (this);
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure = NULL;
  GstCaps *allowedcaps = NULL;
  GstCaps *newcaps = NULL;

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG_OBJECT (this, "Reading Rate");
  if (!gst_structure_get_int (structure, "rate", &this->format.rate)) {
    this->format.rate = -1;
    goto invalidcaps;
  }

  GST_DEBUG_OBJECT (this, "Reading Channels");
  if (!gst_structure_get_int (structure, "channels", &this->format.channels)) {
    this->format.channels = -1;
    goto invalidcaps;
  }

  GST_INFO_OBJECT (this, "Parsed for input caps:\n"
      "\tRate:  %u\n"
      "\tChannels %u\n", this->format.rate, this->format.channels);

  /* Ask for the output caps, if not fixed then try the biggest frame */
  allowedcaps = gst_pad_get_allowed_caps (this->srcpad);
  newcaps = gst_caps_make_writable (gst_caps_copy_nth (allowedcaps, 0));
  srcstructure = gst_caps_get_structure (newcaps, 0);
  gst_caps_unref (allowedcaps);

  GST_DEBUG_OBJECT (this, "Fixating output caps");
  gst_structure_fixate_field_nearest_int (srcstructure, "rate",
      this->format.rate);
  this->rate = this->format.rate;
  gst_structure_fixate_field_nearest_int (srcstructure, "channels",
      this->format.channels);
  this->channels = this->format.channels;
  gst_structure_fixate_field_nearest_int (srcstructure, "mpegversion",
      this->profile);
  if (gst_structure_has_field (srcstructure, "stream-format")) {
    GST_INFO_OBJECT (this, "Setting format");
    switch (this->output_format) {
      case OMX_AUDIO_AACStreamFormatRAW:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "raw");
        break;
      case OMX_AUDIO_AACStreamFormatMP2ADTS:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "adts");
        break;
      case OMX_AUDIO_AACStreamFormatMP4ADTS:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "adts");
        break;
      case OMX_AUDIO_AACStreamFormatMP4LOAS:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "loas");
        break;
      case OMX_AUDIO_AACStreamFormatMP4LATM:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "latm");
        break;
      case OMX_AUDIO_AACStreamFormatADIF:
        gst_structure_fixate_field_string (srcstructure, "stream-format",
            "adif");
        break;
      default:
        GST_INFO_OBJECT (this, "Invalid format");
        break;
    }
  }

  if (this->output_format == OMX_AUDIO_AACStreamFormatADIF) {
    GstBuffer *codec_data;
    codec_data = gst_omx_aac_enc_generate_codec_data (base);
    gst_caps_set_simple (newcaps, "codec_data",
        GST_TYPE_BUFFER, codec_data, (char *) NULL);
    gst_buffer_unref (codec_data);
  }

  gst_structure_get_int (srcstructure, "rate", &this->format.rate);
  gst_structure_get_int (srcstructure, "channels", &this->format.channels);
  gst_structure_get_int (srcstructure, "mpegversion", &this->profile);
  GST_DEBUG_OBJECT (this, "Output caps: %s", gst_caps_to_string (newcaps));

  if (!gst_pad_set_caps (this->srcpad, newcaps))
    goto nosetcaps;

  gst_caps_unref (newcaps);

  return TRUE;

invalidcaps:
  {
    GST_ERROR_OBJECT (this, "Unable to grab stream format from caps");
    return FALSE;
  }
  /*
     unsupportedcaps:
     {
     GST_ERROR_OBJECT (this, "Encoder don't support input stream caps");
     return FALSE;
     } */
nosetcaps:
  {
    GST_ERROR_OBJECT (this, "Src pad didn't accept new caps");
    return FALSE;
  }
}


static GstBuffer *
gst_omx_aac_enc_generate_codec_data (GstOmxBase * base)
{
  GstBuffer *codec_data = NULL;
  guchar *data;
  guint sr_idx;
  GstOmxAACEnc *this;

  this = GST_OMX_AAC_ENC (base);
  /*
   * Now create the codec data header, it goes like
   * 5 bit: profile
   * 4 bit: sample rate index
   * 4 bit: number of channels
   * 3 bit: unused
   */
  sr_idx = gst_omx_aac_enc_get_rateIdx (this->rate);
  codec_data = gst_buffer_new_and_alloc (2);
  data = GST_BUFFER_DATA (codec_data);
  data[0] = ((this->profile & 0x1F) << 3) | ((sr_idx & 0xE) >> 1);
  data[1] = ((sr_idx & 0x1) << 7) | ((this->channels & 0xF) << 3);

  return codec_data;
}


static OMX_ERRORTYPE
gst_omx_aac_enc_init_pads (GstOmxBase * base)
{
  GstOmxAACEnc *this = GST_OMX_AAC_ENC (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname = NULL;



  GST_DEBUG_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = 0;

  g_mutex_lock (&_omx_mutex);
  error = OMX_GetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }

  port->nBufferCountActual = base->input_buffers;
  port->nBufferSize = 4096;     /*  4096 Recommended buffer size */
  port->format.audio.eEncoding = OMX_AUDIO_CodingPCM;
  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "input";
    goto noport;
  }
  /*
     GST_DEBUG_OBJECT (this,
     "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu",
     port->nPortIndex, port->format.video.nFrameWidth,
     port->format.video.nFrameHeight, port->format.video.nStride,
     port->format.video.eColorFormat, port->nBufferSize);
   */
  GST_DEBUG_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));

  port->nPortIndex = 1;

  g_mutex_lock (&_omx_mutex);
  error = OMX_GetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

  port->nBufferCountActual = base->output_buffers;
  port->nBufferSize = 4608;     /* 4608 Recommended buffer size */
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
  /*
     GST_DEBUG_OBJECT (this,
     "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu bitrate=%d",
     port->nPortIndex, port->format.video.nFrameWidth,
     port->format.video.nFrameHeight, port->format.video.nStride,
     port->format.video.eCompressionFormat, port->nBufferSize,
     port->format.video.nBitrate);
   */

  error = gst_omx_aac_enc_parameters (this, &this->format);
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
    GST_ERROR_OBJECT (this, "Failed to enable h264 encoder");
    return error;
  }
}




static OMX_ERRORTYPE
gst_omx_aac_enc_parameters (GstOmxAACEnc * this, GstOmxFormat * format)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_AUDIO_PARAM_PCMMODETYPE pcm_param;
  OMX_AUDIO_PARAM_AACPROFILETYPE aac_param;


  GST_DEBUG_OBJECT (this, "Setting encoder PCM Parameters");
  GST_DEBUG_OBJECT (this,
      "Configuring static parameters on input port: rate=%d, channels=%d",
      this->rate, this->channels);

  /* PCM configuration. */

  GST_OMX_INIT_STRUCT (&pcm_param, OMX_AUDIO_PARAM_PCMMODETYPE);
  pcm_param.nPortIndex = 0;

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle, (OMX_INDEXTYPE) OMX_IndexParamAudioPcm,
      &pcm_param);
  g_mutex_unlock (&_omx_mutex);


  pcm_param.nSamplingRate = this->rate;
  pcm_param.nChannels = this->channels;

  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (base->handle, OMX_IndexParamAudioPcm, &pcm_param);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noPCMParams;

  /* AAC configuration. */
  GST_DEBUG_OBJECT (this, "Setting encoder AAC Parameters");
  GST_DEBUG_OBJECT (this,
      "Configuring static parameters on output port: rate=%d, channels=%d,"
      "bitrate=%d, profile=%d, output-format=%d ", this->rate, this->channels,
      this->bitrate, this->profile, this->output_format);


  GST_OMX_INIT_STRUCT (&aac_param, OMX_AUDIO_PARAM_AACPROFILETYPE);
  aac_param.nPortIndex = 1;
  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle, OMX_IndexParamAudioAac, &aac_param);
  g_mutex_unlock (&_omx_mutex);

  aac_param.nSampleRate = this->rate;
  aac_param.nChannels = this->channels;
  aac_param.nBitRate = this->bitrate;
  aac_param.eAACProfile = this->profile;
  aac_param.eAACStreamFormat = this->output_format;


  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (base->handle, OMX_IndexParamAudioAac, &aac_param);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noAACParams;


  return error;

noPCMParams:
  {
    GST_ERROR_OBJECT (this, "Unable to change PCMFormat: %s",
        gst_omx_error_to_str (error));
    return error;
  }
noAACParams:
  {
    GST_ERROR_OBJECT (this, "Unable to change AACParams: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static GstFlowReturn
gst_omx_aac_enc_fill_callback (GstOmxBase * base, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxAACEnc *this = GST_OMX_AAC_ENC (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;

  GST_LOG_OBJECT (this, "AAC Encoder Fill buffer callback");

  caps = gst_pad_get_negotiated_caps (this->srcpad);
  if (!caps)
    goto nocaps;


  if (!this->always_copy) {

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
    GST_BUFFER_FLAG_SET (buffer, GST_OMX_BUFFER_FLAG);
    bufdata->buffer = buffer;

    GST_LOG_OBJECT (this,
        "(Fill %s) Buffer %p size %d reffcount %d bufdat %p->%p",
        GST_OBJECT_NAME (this), outbuf->pBuffer, GST_BUFFER_SIZE (buffer),
        GST_OBJECT_REFCOUNT (buffer), bufdata, bufdata->buffer);

    GST_LOG_OBJECT (this, "Pushing buffer %p->%p to %s:%s",
        outbuf, outbuf->pBuffer, GST_DEBUG_PAD_NAME (this->srcpad));
  }

  else {

    buffer = gst_buffer_new_and_alloc (outbuf->nFilledLen);
    if (!buffer)
      goto noalloc;
    gst_buffer_set_caps (buffer, caps);
    memcpy (buffer->data, outbuf->pBuffer, outbuf->nFilledLen);
    GST_BUFFER_TIMESTAMP (buffer) = outbuf->nTimeStamp;
    GST_LOG_OBJECT (this,
        "(Fill %s) Buffer %p size %d reffcount %d bufdat %p->%p",
        GST_OBJECT_NAME (this), outbuf->pBuffer, GST_BUFFER_SIZE (buffer),
        GST_OBJECT_REFCOUNT (buffer), bufdata, bufdata->buffer);

    GST_LOG_OBJECT (this, "Pushing buffer %p->%p to %s:%s",
        outbuf, outbuf->pBuffer, GST_DEBUG_PAD_NAME (this->srcpad));
    gst_omx_base_release_buffer (outbuf);
  }
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
