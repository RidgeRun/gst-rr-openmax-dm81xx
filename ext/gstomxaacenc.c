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
    PROP_OUTPUT_FORMAT
};

#define GST_OMX_AAC_ENC_BITRATE_DEFAULT 128000
#define GST_OMX_AAC_ENC_PROFILE_DEFAULT OMX_AUDIO_AACObjectLC
#define GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT OMX_AUDIO_AACStreamFormatRAW


gint gst_omx_aac_rateIdx[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,
    11025,8000,7350};

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
            {OMX_AUDIO_AACObjectERLC, "ER AAC Low Complexity object (Error Resilient AAC-LC)", "ERLC"},
            {OMX_AUDIO_AACObjectLD, "AAC Low Delay object (Error Resilient)", "LD"},
            {OMX_AUDIO_AACObjectHE_PS, "High Efficiency with Parametric Stereo coding (HE-AAC v2, object type PS)", "HE_PS"},
            {0, NULL, NULL},
        };

        gst_omx_aac_enc_profile_type = g_enum_register_static ("GstOmxAacencProfile",
                                                              gst_omx_aac_enc_profile);
    }
    return gst_omx_aac_enc_profile_type;
}


static GType
gst_omx_aac_enc_output_format_get_type (void)
{
    static GType gst_omx_aac_enc_output_format_type = 0;

    if (!gst_omx_aac_enc_output_format_type) {
        static GEnumValue gst_omx_aac_enc_output_format[] = {
            {OMX_AUDIO_AACStreamFormatMP2ADTS, "Audio Data Transport Stream 2 format", "MP2ADTS"},
            {OMX_AUDIO_AACStreamFormatMP4ADTS, "Audio Data Transport Stream 4 format", "MP4ADTS"},
            {OMX_AUDIO_AACStreamFormatMP4LOAS, "Low Overhead Audio Stream format", "MP4LOAS"},
            {OMX_AUDIO_AACStreamFormatMP4LATM, "Low overhead Audio Transport Multiplex", "MP4LATM"},
            {OMX_AUDIO_AACStreamFormatADIF, "Audio Data Interchange Format", "ADIF"},
            {OMX_AUDIO_AACStreamFormatMP4FF, "AAC inside MPEG-4/ISO File Format", "MP4FF"},
            {OMX_AUDIO_AACStreamFormatRAW, "AAC Raw Format", "RAW"},
            {OMX_AUDIO_AACStreamFormatMax, "AAC Stream format MAX", "MAX"},
            {0, NULL, NULL},
        };

        gst_omx_aac_enc_output_format_type = g_enum_register_static ("GstOmxAacencOutputFormat",
                                                                    gst_omx_aac_enc_output_format);
    }

    return gst_omx_aac_enc_output_format_type;
}

#define GST_TYPE_OMX_AAC_ENC_PROFILE (gst_omx_aac_enc_profile_get_type ())
#define GST_TYPE_OMX_AAC_ENC_OUTPUT_FORMAT (gst_omx_aac_enc_output_format_get_type ())



#define gst_omx_aac_enc_parent_class parent_class
G_DEFINE_TYPE (GstOmxAACEnc, gst_omx_aac_enc, GST_TYPE_OMX_BASE);


static void gst_omx_aac_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_aac_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_omx_aac_enc_finalize (GObject * object);



/* initialize the omx's class */
static void
gst_omx_aac_enc_class_init (GstOmxAACEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX AAC audio encoder",
      "Codec/Encoder/AAC",
      "RidgeRun's OMX based AAC encoder",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

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
           GST_TYPE_OMX_AAC_ENC_OUTPUT_FORMAT, GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT,
          G_PARAM_READWRITE));


  /*  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_aac_enc_set_caps);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_aac_enc_fill_callback);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_aac_enc_init_pads);
  */
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
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  /* Initialize properties */
  this->bitrate = GST_OMX_AAC_ENC_BITRATE_DEFAULT;
  this->profile = GST_OMX_AAC_ENC_PROFILE_DEFAULT;
  this->output_format = GST_OMX_AAC_ENC_OUTPUT_FORMAT_DEFAULT;
 
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
  GstOmxBase *base = GST_OMX_BASE (this);

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
    this->output_format = g_value_get_boolean (value);
    GST_INFO_OBJECT (this, "Setting output format to %d", this->output_format);
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
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
gst_omx_aac_enc_finalize (GObject * object)
{
}
