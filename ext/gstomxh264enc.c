/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Eugenia Guzman <eugenia.guzman@ridgerun.com>
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

#include "gstomxh264enc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h264_enc_debug);
#define GST_CAT_DEFAULT gst_omx_h264_enc_debug

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
    GST_STATIC_CAPS ("video/x-h264,"
        "width=[16,4096]," "height=[16,4096],"
        "framerate=" GST_VIDEO_FPS_RANGE ","
        "stream-format=(string) byte-stream," "alignment=(string) au")
    );

enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_BYTESTREAM,
  PROP_PROFILE,
  PROP_LEVEL,
  PROP_IPERIOD,
  PROP_IDRPERIOD,
  PROP_NEXT_IDR,
  PROP_PRESET,
  PROP_RATE_CTRL,
  PROP_BFRAMES
};

#define GST_OMX_H264_ENC_BITRATE_DEFAULT	500000
#define GST_OMX_H264_ENC_BYTESTREAM_DEFAULT	FALSE
#define GST_OMX_H264_ENC_PROFILE_DEFAULT	OMX_VIDEO_AVCProfileBaseline
#define GST_OMX_H264_ENC_LEVEL_DEFAULT		OMX_VIDEO_AVCLevel42
#define GST_OMX_H264_ENC_IPERIOD_DEFAULT	90
#define GST_OMX_H264_ENC_IDRPERIOD_DEFAULT	0
#define GST_OMX_H264_ENC_NEXT_IDR_DEFAULT	FALSE
#define GST_OMX_H264_ENC_PRESET_DEFAULT		OMX_Video_Enc_High_Speed_Med_Quality
#define GST_OMX_H264_ENC_RATE_CTRL_DEFAULT	OMX_Video_RC_Low_Delay
#define GST_OMX_H264_ENC_B_FRAMES			0

#define GST_TYPE_OMX_VIDEO_AVCPROFILETYPE (gst_omx_h264_enc_profile_get_type ())
static GType
gst_omx_h264_enc_profile_get_type ()
{
  static GType type = 0;
  if (!type) {
    static const GEnumValue vals[] = {
      {OMX_VIDEO_AVCProfileBaseline, "Base Profile", "base"},
      {OMX_VIDEO_AVCProfileMain, "Main Profile", "main"},
      {OMX_VIDEO_AVCProfileExtended, "Extended Profile", "extended"},
      {OMX_VIDEO_AVCProfileHigh, "High Profile", "high"},
      {OMX_VIDEO_AVCProfileHigh10, "High 10 Profile", "high-10"},
      {OMX_VIDEO_AVCProfileHigh422, "High 4:2:2 Profile", "high-422"},
      {OMX_VIDEO_AVCProfileHigh444, "High 4:4:4 Profile", "high-444"},
      {0, NULL, NULL},
    };

    type = g_enum_register_static ("RRGstOmxVideoAVCProfile", vals);
  }

  return type;
}

#define GST_TYPE_OMX_VIDEO_AVCLEVELTYPE (gst_omx_h264_enc_level_get_type ())
static GType
gst_omx_h264_enc_level_get_type ()
{
  static GType type = 0;

  if (!type) {
    static const GEnumValue vals[] = {
      {OMX_VIDEO_AVCLevel1, "Level 1", "level-1"},
      {OMX_VIDEO_AVCLevel1b, "Level 1b", "level-1b"},
      {OMX_VIDEO_AVCLevel11, "Level 11", "level-11"},
      {OMX_VIDEO_AVCLevel12, "Level 12", "level-12"},
      {OMX_VIDEO_AVCLevel13, "Level 13", "level-13"},
      {OMX_VIDEO_AVCLevel2, "Level 2", "level-2"},
      {OMX_VIDEO_AVCLevel21, "Level 21", "level-21"},
      {OMX_VIDEO_AVCLevel22, "Level 22", "level-22"},
      {OMX_VIDEO_AVCLevel3, "Level 3", "level-3"},
      {OMX_VIDEO_AVCLevel31, "Level 31", "level-31"},
      {OMX_VIDEO_AVCLevel32, "Level 32", "level-32"},
      {OMX_VIDEO_AVCLevel4, "Level 4", "level-4"},
      {OMX_VIDEO_AVCLevel41, "Level 41", "level-41"},
      {OMX_VIDEO_AVCLevel42, "Level 42", "level-42"},
      {OMX_VIDEO_AVCLevel5, "Level 5", "level-5"},
      {OMX_VIDEO_AVCLevel51, "Level 51", "level-51"},
      {0, NULL, NULL},
    };

    type = g_enum_register_static ("RRGstOmxVideoAVCLevel", vals);
  }

  return type;
}

#define GST_TYPE_OMX_VIDEO_ENCODE_PRESETTYPE (gst_omx_h264_enc_preset_get_type ())
static GType
gst_omx_h264_enc_preset_get_type ()
{
  static GType type = 0;

  if (!type) {
    static const GEnumValue vals[] = {
      {OMX_Video_Enc_High_Quality, "High Quality", "hq"},
      {OMX_Video_Enc_User_Defined, "User Defined", "user"},
      {OMX_Video_Enc_High_Speed_Med_Quality, "High Speed Med Qual", "hsmq"},
      {OMX_Video_Enc_Med_Speed_Med_Quality, "Med Speed Med Qaul", "msmq"},
      {OMX_Video_Enc_Med_Speed_High_Quality, "Med Speed High Qaul", "mshq"},
      {OMX_Video_Enc_High_Speed, "High Speed", "hs"},
      {0, NULL, NULL},
    };

    type = g_enum_register_static ("RRGstOmxVideoEncoderPreset", vals);
  }

  return type;
}

#define GST_TYPE_OMX_VIDEO_RATECONTROL_PRESETTYPE (gst_omx_h264_enc_rate_ctrl_get_type ())
static GType
gst_omx_h264_enc_rate_ctrl_get_type ()
{
  static GType type = 0;

  if (!type) {
    static const GEnumValue vals[] = {
      {OMX_Video_RC_Low_Delay, "Low Delay", "low-delay"},
      {OMX_Video_RC_Storage, "Storage", "storage"},
      {OMX_Video_RC_Twopass, "Two Pass", "two-pass"},
      {OMX_Video_RC_None, "none", "none"},
      {0, NULL, NULL},
    };

    type = g_enum_register_static ("RRGstOmxVideoRateControlPreset", vals);
  }

  return type;
}

#define gst_omx_h264_enc_parent_class parent_class
G_DEFINE_TYPE (GstOmxH264Enc, gst_omx_h264_enc, GST_TYPE_OMX_BASE);

static gboolean gst_omx_h264_enc_set_caps (GstPad * pad, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_h264_enc_init_pads (GstOmxBase * this);
static GstFlowReturn gst_omx_h264_enc_fill_callback (GstOmxBase *,
    OMX_BUFFERHEADERTYPE *);

static OMX_ERRORTYPE gst_omx_h264_enc_static_parameters (GstOmxH264Enc * this,
    GstOmxPad *, GstOmxFormat *);
static void gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_h264_enc_class_init (GstOmxH264EncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX H.264 video encoder",
      "Codec/Encoder/Video",
      "RidgeRun's OMX based H264 encoder",
      "Eugenia Guzman <eugenia.guzman@ridgerun.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gobject_class->set_property = gst_omx_h264_enc_set_property;
  gobject_class->get_property = gst_omx_h264_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Encoding bitrate",
          "Sets the encoder bitrate",
          0, 4294967295, GST_OMX_H264_ENC_BITRATE_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_BYTESTREAM,
      g_param_spec_boolean ("bytestream", "Bytestream",
          "Sets the encoder bytestream",
          GST_OMX_H264_ENC_BYTESTREAM_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PROFILE,
      g_param_spec_enum ("profile", "Encoding H264 profile",
          "Sets the H264 profile",
          GST_TYPE_OMX_VIDEO_AVCPROFILETYPE, GST_OMX_H264_ENC_PROFILE_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LEVEL,
      g_param_spec_enum ("level", "Encoding H264 level",
          "Sets the H264 level",
          GST_TYPE_OMX_VIDEO_AVCLEVELTYPE, GST_OMX_H264_ENC_LEVEL_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_IPERIOD,
      g_param_spec_uint ("i_period", "I frames periodicity",
          "Specifies periodicity of I frames",
          0, 2147483647, GST_OMX_H264_ENC_IPERIOD_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_IDRPERIOD,
      g_param_spec_uint ("force_idr_period", "IDR frames periodicity",
          "Specifies periodicity of IDR frames",
          0, 2147483647, GST_OMX_H264_ENC_IDRPERIOD_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_NEXT_IDR,
      g_param_spec_boolean ("force_idr", "Force next frame to be IDR",
          "Force next frame to be IDR",
          GST_OMX_H264_ENC_NEXT_IDR_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PRESET,
      g_param_spec_enum ("encodingPreset", "Encoding preset",
          "Specifies which encoding preset to use",
          GST_TYPE_OMX_VIDEO_ENCODE_PRESETTYPE, GST_OMX_H264_ENC_PRESET_DEFAULT,
          G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_RATE_CTRL,
      g_param_spec_enum ("rateControlPreset", "Encoding rate control preset",
          "Specifies what rate control preset to use",
          GST_TYPE_OMX_VIDEO_RATECONTROL_PRESETTYPE,
          GST_OMX_H264_ENC_RATE_CTRL_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_BFRAMES,
      g_param_spec_uint ("b_frames", "B Frames",
          "Specifies the number of B frames",
          0, 6, GST_OMX_H264_ENC_B_FRAMES, G_PARAM_READWRITE));

  gstomxbase_class->parse_caps = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_set_caps);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_h264_enc_fill_callback);
  gstomxbase_class->init_ports = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_init_pads);

  gstomxbase_class->handle_name = "OMX.TI.DUCATI.VIDENC";

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_omx_h264_enc_debug, "omx_h264enc",
      0, "RidgeRun's OMX based H264 encoder");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_h264_enc_init (GstOmxH264Enc * this)
{
  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  /* Initialize properties */
  this->bitrate = GST_OMX_H264_ENC_BITRATE_DEFAULT;
  this->bytestream = GST_OMX_H264_ENC_BYTESTREAM_DEFAULT;
  this->profile = GST_OMX_H264_ENC_PROFILE_DEFAULT;
  this->level = GST_OMX_H264_ENC_LEVEL_DEFAULT;
  this->i_period = GST_OMX_H264_ENC_IPERIOD_DEFAULT;
  this->force_idr_period = GST_OMX_H264_ENC_IDRPERIOD_DEFAULT;
  this->force_idr = GST_OMX_H264_ENC_NEXT_IDR_DEFAULT;
  this->encodingPreset = GST_OMX_H264_ENC_PRESET_DEFAULT;
  this->rateControlPreset = GST_OMX_H264_ENC_RATE_CTRL_DEFAULT;
  this->cont = 0;
  this->is_interlaced = FALSE;
  this->b_frames = GST_OMX_H264_ENC_B_FRAMES;

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
gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxH264Enc *this = GST_OMX_H264_ENC (object);
  GstOmxBase *base = GST_OMX_BASE (this);
  gboolean reconf = FALSE;

  switch (prop_id) {
    case PROP_BITRATE:
      this->bitrate = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting bitrate to %d", this->bitrate);
      reconf = TRUE;
      break;
    case PROP_BYTESTREAM:
      this->bytestream = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting bytestream to %d", this->bytestream);
      break;
    case PROP_PROFILE:
      this->profile = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting the H264 profile to %d", this->profile);
      break;
    case PROP_LEVEL:
      this->level = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting the H264 level to %d", this->level);
      break;
    case PROP_IPERIOD:
      this->i_period = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting I period to %d", this->i_period);
      reconf = TRUE;
      break;
    case PROP_IDRPERIOD:
      this->force_idr_period = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting IDR period to %d",
          this->force_idr_period);
      break;
    case PROP_NEXT_IDR:
      this->force_idr = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting  the next frame to be IDR to %d",
          this->force_idr);
      break;
    case PROP_PRESET:
      this->encodingPreset = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting the encoding preset to %d",
          this->encodingPreset);
      break;
    case PROP_RATE_CTRL:
      this->rateControlPreset = g_value_get_enum (value);
      GST_INFO_OBJECT (this, "Setting the rate control preset to %d",
          this->rateControlPreset);
      break;
    case PROP_BFRAMES:
      this->b_frames = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting B frames to %d", this->b_frames);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (reconf) {
    OMX_VIDEO_CONFIG_DYNAMICPARAMS tDynParams;
    OMX_ERRORTYPE error_val = OMX_ErrorNone;

    GST_OMX_INIT_STRUCT (&tDynParams, OMX_VIDEO_CONFIG_DYNAMICPARAMS);
    tDynParams.nPortIndex = 1;

    error_val =
        OMX_GetConfig (base->handle, OMX_TI_IndexConfigVideoDynamicParams,
        &tDynParams);
    if (error_val != OMX_ErrorNone) {
      GST_ERROR_OBJECT (this,
          "Unable to retrieve dynamic parameters, error: %x", error_val);
      return;
    }

    tDynParams.videoDynamicParams.h264EncDynamicParams.videnc2DynamicParams.
        targetBitRate = this->bitrate;
    tDynParams.videoDynamicParams.h264EncDynamicParams.videnc2DynamicParams.
        intraFrameInterval = this->i_period;
    error_val =
        OMX_SetConfig (base->handle, OMX_TI_IndexConfigVideoDynamicParams,
        &tDynParams);
    if (error_val != OMX_ErrorNone) {
      GST_ERROR_OBJECT (this, "Unable to set dynamic parameters, error: %x",
          error_val);
      return;
    }
  }
}

static void
gst_omx_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxH264Enc *this = GST_OMX_H264_ENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, this->bitrate);
      break;
    case PROP_BYTESTREAM:
      g_value_set_boolean (value, this->bytestream);
      break;
    case PROP_PROFILE:
      g_value_set_enum (value, this->profile);
      break;
    case PROP_LEVEL:
      g_value_set_enum (value, this->level);
      break;
    case PROP_IPERIOD:
      g_value_set_uint (value, this->i_period);
      break;
    case PROP_IDRPERIOD:
      g_value_set_uint (value, this->force_idr_period);
      break;
    case PROP_NEXT_IDR:
      g_value_set_boolean (value, this->force_idr);
      break;
    case PROP_PRESET:
      g_value_set_enum (value, this->encodingPreset);
      break;
    case PROP_RATE_CTRL:
      g_value_set_enum (value, this->rateControlPreset);
      break;
    case PROP_BFRAMES:
      g_value_set_uint (value, this->b_frames);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_omx_h264_enc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxH264Enc *this = GST_OMX_H264_ENC (GST_OBJECT_PARENT (pad));
  GstOmxBase *base = GST_OMX_BASE (this);
  const GstStructure *structure = gst_caps_get_structure (caps, 0);
  GstStructure *srcstructure = NULL;
  GstCaps *allowedcaps = NULL;
  GstCaps *newcaps = NULL;
  GValue stride = { 0, };

  g_return_val_if_fail (gst_caps_is_fixed (caps), FALSE);

  GST_DEBUG_OBJECT (this, "Reading width");
  if (!gst_structure_get_int (structure, "width", &this->format.width)) {
    this->format.width = -1;
    goto invalidcaps;
  }

  GST_DEBUG_OBJECT (this, "Reading height");
  if (!gst_structure_get_int (structure, "height", &this->format.height)) {
    this->format.height = -1;
    goto invalidcaps;
  }
  if (!gst_structure_get_boolean (structure, "interlaced",
          &this->is_interlaced))
    this->is_interlaced = FALSE;

  GST_DEBUG_OBJECT (this, "Reading framerate");
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

  GST_DEBUG_OBJECT (this, "Fixating output caps");
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
nosetcaps:
  {
    GST_ERROR_OBJECT (this, "Src pad didn't accept new caps");
    return FALSE;
  }
}


static OMX_ERRORTYPE
gst_omx_h264_enc_init_pads (GstOmxBase * base)
{
  GstOmxH264Enc *this = GST_OMX_H264_ENC (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname = NULL;
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;



  GST_OMX_INIT_STRUCT (&param, OMX_VIDEO_PARAM_PROFILELEVELTYPE);

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_IndexParamVideoProfileLevelCurrent, &param);
  g_mutex_unlock (&_omx_mutex);

  param.eProfile = this->profile;
  param.eLevel = this->level;
  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  g_mutex_unlock (&_omx_mutex);

  if (error == OMX_ErrorUnsupportedIndex) {
    GST_WARNING_OBJECT (this,
        "Setting profile/level not supported by component");
  }
  /* TODO: Set here the notification type */

  GST_DEBUG_OBJECT (this, "Initializing sink pad memory");
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

  GST_DEBUG_OBJECT (this, "Initializing src pad memory");
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


  GST_DEBUG_OBJECT (this, "Initializing sink pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->sinkpad));

  port->nPortIndex = 0;         // OMX_VIDENC_INPUT_PORT
  port->eDir = OMX_DirInput;

  port->nBufferCountActual = base->input_buffers;
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height;
  if (this->is_interlaced)
    port->format.video.nFrameHeight = this->format.height * 0.5;

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

  GST_DEBUG_OBJECT (this,
      "Configuring port %lu: width=%lu, height=%lu, stride=%lu, format=%u, buffersize=%lu",
      port->nPortIndex, port->format.video.nFrameWidth,
      port->format.video.nFrameHeight, port->format.video.nStride,
      port->format.video.eColorFormat, port->nBufferSize);

  GST_DEBUG_OBJECT (this, "Initializing src pad port");
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
  port->format.video.nBitrate = this->bitrate;
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (GST_OMX_BASE (this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);

  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

  GST_DEBUG_OBJECT (this,
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

  error = gst_omx_h264_enc_static_parameters (this,
      GST_OMX_PAD (this->srcpad), &this->format);
  if (GST_OMX_FAIL (error))
    goto noconfiguration;



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
    GST_ERROR_OBJECT (this, "Failed to enable h264 encoder");
    return error;
  }
}

static GstFlowReturn
gst_omx_h264_enc_fill_callback (GstOmxBase * base,
    OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxH264Enc *this = GST_OMX_H264_ENC (base);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;

  GST_LOG_OBJECT (this, "H264 Encoder Fill buffer callback");


  /* Currently we use this logic to handle IDR period since the latest
   * EZSDK version doesn't have support for OMX_IndexConfigVideoAVCIntraPeriod
   */
  GST_DEBUG_OBJECT (this, "Setting encoder IDRPeriod");
  if ((this->force_idr_period > 0) || (this->force_idr)) {
    if ((this->cont == this->force_idr_period) || (this->force_idr)) {
      OMX_CONFIG_INTRAREFRESHVOPTYPE confIntraRefreshVOP;

      GST_OMX_INIT_STRUCT (&confIntraRefreshVOP,
          OMX_CONFIG_INTRAREFRESHVOPTYPE);

      confIntraRefreshVOP.nPortIndex = 1;

      OMX_GetConfig (base->handle,
          OMX_IndexConfigVideoIntraVOPRefresh, &confIntraRefreshVOP);
      confIntraRefreshVOP.IntraRefreshVOP = TRUE;

      OMX_SetConfig (base->handle,
          OMX_IndexConfigVideoIntraVOPRefresh, &confIntraRefreshVOP);

      if (this->cont == this->force_idr_period)
        this->cont = 0;

      if (this->force_idr) {
        this->force_idr = FALSE;
        this->cont++;
      }
    } else if (this->cont > this->force_idr_period) {
      this->cont = 0;
    } else {
      this->cont++;
    }
  }

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
gst_omx_h264_enc_static_parameters (GstOmxH264Enc * this,
    GstOmxPad * pad, GstOmxFormat * format)
{
  GstOmxBase *base = GST_OMX_BASE (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_VIDEO_PARAM_AVCTYPE AVCParams;
  OMX_VIDEO_PARAM_ENCODER_PRESETTYPE EncoderPreset;

  port = GST_OMX_PAD_PORT (pad);

  GST_DEBUG_OBJECT (this,
      "Configuring static parameters: bitrate=%d, profile=%d, level=%d, preset=%d, rate=%d",
      this->bitrate, this->profile,
      this->level, this->encodingPreset, this->rateControlPreset);

  GST_DEBUG_OBJECT (this, "Setting ByteStream");
  OMX_INDEXTYPE index;
  if (OMX_GetExtensionIndex (base->handle,
          "OMX.TI.VideoEncode.Config.NALFormat", &index) == OMX_ErrorNone) {
    OMX_U32 nal_format;

    nal_format = this->bytestream ? 0 : 1;
    GST_DEBUG_OBJECT (base->handle,
        "setting 'OMX.TI.VideoEncode.Config.NALFormat' to %ld", nal_format);

    g_mutex_lock (&_omx_mutex);
    error = OMX_SetParameter (base->handle, index, &nal_format);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto noNalFormat;
  } else
    GST_WARNING_OBJECT (this,
        "'OMX.TI.VideoEncode.Config.NALFormat' unsupported");

  GST_DEBUG_OBJECT (this, "Setting encoder AVC Parameters");
  GST_OMX_INIT_STRUCT (&AVCParams, OMX_VIDEO_PARAM_AVCTYPE);
  AVCParams.nPortIndex = OMX_DirOutput;

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle, (OMX_INDEXTYPE) OMX_IndexParamVideoAvc,
      &AVCParams);
  g_mutex_unlock (&_omx_mutex);

  AVCParams.eProfile = this->profile;
  AVCParams.eLevel = this->level;
  AVCParams.nPFrames = this->i_period - 1;
  AVCParams.nBFrames = this->b_frames;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_IndexParamVideoAvc, &AVCParams);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto noAVCParams;

  GST_DEBUG_OBJECT (this, "Setting encoder preset");
  GST_OMX_INIT_STRUCT (&EncoderPreset, OMX_VIDEO_PARAM_ENCODER_PRESETTYPE);
  EncoderPreset.nPortIndex = 1;

  g_mutex_lock (&_omx_mutex);
  OMX_GetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexParamVideoEncoderPreset, &EncoderPreset);
  g_mutex_unlock (&_omx_mutex);

  EncoderPreset.eEncodingModePreset = this->encodingPreset;
  EncoderPreset.eRateControlPreset = this->rateControlPreset;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (base->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexParamVideoEncoderPreset, &EncoderPreset);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto nopreset;


  if (this->is_interlaced) {

    OMX_VIDEO_PARAM_STATICPARAMS tStaticParam;

    GST_OMX_INIT_STRUCT (&tStaticParam, OMX_VIDEO_PARAM_STATICPARAMS);

    tStaticParam.nPortIndex = 1;

    g_mutex_lock (&_omx_mutex);
    OMX_GetParameter (base->handle,
        (OMX_INDEXTYPE) OMX_TI_IndexParamVideoStaticParams, &tStaticParam);
    g_mutex_unlock (&_omx_mutex);

    /* for interlace, base profile can not be used */

    tStaticParam.videoStaticParams.h264EncStaticParams.
        videnc2Params.encodingPreset = XDM_USER_DEFINED;
    tStaticParam.videoStaticParams.h264EncStaticParams.videnc2Params.profile =
        IH264_HIGH_PROFILE;
    tStaticParam.videoStaticParams.h264EncStaticParams.videnc2Params.level =
        IH264_LEVEL_42;

    /* setting Interlace mode */
    tStaticParam.videoStaticParams.h264EncStaticParams.
        videnc2Params.inputContentType = IVIDEO_INTERLACED;
    tStaticParam.videoStaticParams.h264EncStaticParams.bottomFieldIntra = 0;
    tStaticParam.videoStaticParams.h264EncStaticParams.interlaceCodingType =
        IH264_INTERLACE_FIELDONLY_ARF;

    tStaticParam.videoStaticParams.h264EncStaticParams.
        videnc2Params.encodingPreset = XDM_DEFAULT;
    tStaticParam.videoStaticParams.h264EncStaticParams.
        videnc2Params.rateControlPreset = IVIDEO_STORAGE;

    tStaticParam.videoStaticParams.h264EncStaticParams.
        intraCodingParams.lumaIntra4x4Enable = 0x1f;
    tStaticParam.videoStaticParams.h264EncStaticParams.
        intraCodingParams.lumaIntra8x8Enable = 0x1f;

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetParameter (base->handle,
        (OMX_INDEXTYPE) OMX_TI_IndexParamVideoStaticParams, &tStaticParam);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto nointerlaced;

  } else {
    if (this->b_frames) {

      OMX_VIDEO_PARAM_STATICPARAMS tStaticParam;

      GST_OMX_INIT_STRUCT (&tStaticParam, OMX_VIDEO_PARAM_STATICPARAMS);

      tStaticParam.nPortIndex = 1;

      g_mutex_lock (&_omx_mutex);
      OMX_GetParameter (base->handle,
          (OMX_INDEXTYPE) OMX_TI_IndexParamVideoStaticParams, &tStaticParam);
      g_mutex_unlock (&_omx_mutex);

      if (OMX_VIDEO_AVCProfileHigh == this->profile)
        tStaticParam.videoStaticParams.h264EncStaticParams.videnc2Params.
            profile = IH264_HIGH_PROFILE;
      else if (OMX_VIDEO_AVCProfileMain == this->profile)
        tStaticParam.videoStaticParams.h264EncStaticParams.videnc2Params.
            profile = IH264_MAIN_PROFILE;
      else
        GST_ERROR_OBJECT (this,
            "Profile needs to be High or Main to add B frames");

      tStaticParam.videoStaticParams.h264EncStaticParams.
          videnc2Params.rateControlPreset = IVIDEO_STORAGE;

      g_mutex_lock (&_omx_mutex);
      error =
          OMX_SetParameter (base->handle,
          (OMX_INDEXTYPE) OMX_TI_IndexParamVideoStaticParams, &tStaticParam);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error))
        goto nointerlaced;
    }
  }

  return error;

noNalFormat:
  {
    GST_ERROR_OBJECT (this, "Unable to change statically NalFormat: %s",
        gst_omx_error_to_str (error));
    return;
  }
noAVCParams:
  {
    GST_ERROR_OBJECT (this, "Unable to change statically AVCParams: %s",
        gst_omx_error_to_str (error));
    return error;
  }
nopreset:
  {
    GST_ERROR_OBJECT (this,
        "Unable to change statically the encoder preset: %s",
        gst_omx_error_to_str (error));
    return error;
  }
noiperiod:
  {
    GST_ERROR_OBJECT (this, "Unable to change statically i-period: %s",
        gst_omx_error_to_str (error));
    return error;
  }
nointerlaced:
  {
    GST_ERROR_OBJECT (this, "Unable to set interlaced settings: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}
