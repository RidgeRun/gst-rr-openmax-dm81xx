/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
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

/**
 * SECTION:element-omx_camera
 *
 * omxcamera can be used to capture video from v4l2 devices throught the 
 * OMX capture component
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m omx_camera ! identity ! fakesink silent=TRUE
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

#include "gstomxcamera.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_camera_debug);
#define GST_CAT_DEFAULT gst_omx_camera_debug

enum
{
  PROP_0,
  PROP_ALWAYS_COPY,
  PROP_NUM_OUT_BUFFERS,
  PROP_INTERFACE,
  PROP_CAPT_MODE,
  PROP_VIP_MODE,
  PROP_SCAN_TYPE,
  PROP_SKIP_FRAMES
};


#define gst_omx_camera_parent_class parent_class
static GstOmxBaseSrcClass *parent_class = NULL;

/*
 * Caps:
 */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) {YUY2, NV12}, "
        "width = (int) [ 16, 1920 ], "
        "height = (int) [ 16, 1080 ] , " "framerate = " GST_VIDEO_FPS_RANGE)
    );

#define MAX_SHIFTS	30
/* Properties defaults */
#define PROP_ALWAYS_COPY_DEFAULT          FALSE
#define PROP_NUM_OUT_BUFFERS_DEFAULT      5
#define PROP_INTERFACE_DEFAULT            OMX_VIDEO_CaptureHWPortVIP1_PORTA
#define PROP_CAPT_MODE_DEFAULT            OMX_VIDEO_CaptureModeSC_NON_MUX
#define PROP_VIP_MODE_DEFAULT             OMX_VIDEO_CaptureVifMode_16BIT
#define PROP_SCAN_TYPE_DEFAULT            OMX_VIDEO_CaptureScanTypeProgressive
#define PROP_SKIP_FRAMES_DEFAULT          0


/* Properties enumerates */
#define GST_OMX_CAMERA_INTERFACE_TYPE (gst_omx_camera_interface_get_type())
static GType
gst_omx_camera_interface_get_type (void)
{
  static GType interface_type = 0;

  static const GEnumValue interface_types[] = {
    {OMX_VIDEO_CaptureHWPortVIP1_PORTA, "VIP1 port", "vip1"},
    {OMX_VIDEO_CaptureHWPortVIP2_PORTA, "VIP2 port", "vip2"},
    {0, NULL, NULL}
  };

  if (!interface_type) {
    interface_type =
        g_enum_register_static ("GstOMXCameraInterface", interface_types);
  }
  return interface_type;
}

#define GST_OMX_CAMERA_CAPT_MODE_TYPE (gst_omx_camera_capt_mode_get_type())
static GType
gst_omx_camera_capt_mode_get_type (void)
{
  static GType capt_mode_type = 0;

  static const GEnumValue capt_mode_types[] = {
    {OMX_VIDEO_CaptureModeSC_NON_MUX, "Non multiplexed", "nmux"},
    {OMX_VIDEO_CaptureModeMC_LINE_MUX, "Line multiplexed ", "lmux"},
    {0, NULL, NULL}
  };

  if (!capt_mode_type) {
    capt_mode_type =
        g_enum_register_static ("GstOMXCameraCaptMode", capt_mode_types);
  }
  return capt_mode_type;
}

#define GST_OMX_CAMERA_VIP_MODE_TYPE (gst_omx_camera_vip_mode_get_type())
static GType
gst_omx_camera_vip_mode_get_type (void)
{
  static GType vip_mode_type = 0;

  static const GEnumValue vip_mode_types[] = {
    {OMX_VIDEO_CaptureVifMode_08BIT, "8 bits", "8"},
    {OMX_VIDEO_CaptureVifMode_16BIT, "16 bits ", "16"},
    {OMX_VIDEO_CaptureVifMode_24BIT, "24 bits", "24"},
    {0, NULL, NULL}
  };

  if (!vip_mode_type) {
    vip_mode_type =
        g_enum_register_static ("GstOMXCameraVipMode", vip_mode_types);
  }
  return vip_mode_type;
}

#define GST_OMX_CAMERA_SCAN_TYPE (gst_omx_camera_scan_type_get_type())
static GType
gst_omx_camera_scan_type_get_type (void)
{
  static GType scan_type_type = 0;

  static const GEnumValue scan_type_types[] = {
    {OMX_VIDEO_CaptureScanTypeProgressive, "Progressive", "progressive"},
    {OMX_VIDEO_CaptureScanTypeInterlaced, "Interlaced ", "interlaced"},
    {0, NULL, NULL}
  };

  if (!scan_type_type) {
    scan_type_type =
        g_enum_register_static ("GstOMXCameraScanType", scan_type_types);
  }
  return scan_type_type;
}

/* object methods */
static void gst_omx_camera_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_camera_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_omx_camera_set_caps (GstPad * pad, GstCaps * caps);
static GstCaps *gst_omx_camera_fixate (GstBaseSrc * basesrc, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_camera_init_pads (GstOmxBaseSrc * this);
static GstFlowReturn gst_omx_camera_fill_callback (GstOmxBaseSrc *,
    OMX_BUFFERHEADERTYPE *);


static void
gst_omx_camera_class_init (GstOMXCameraClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstOmxBaseSrcClass *baseomxsrc_class = GST_OMX_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_omx_camera_set_property;
  gobject_class->get_property = gst_omx_camera_get_property;

  g_object_class_install_property (gobject_class, PROP_INTERFACE,
      g_param_spec_enum ("interface", "Interface",
          "The video input interface from where image/video is obtained",
          GST_OMX_CAMERA_INTERFACE_TYPE, PROP_INTERFACE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPT_MODE,
      g_param_spec_enum ("capt-mode", "Capture mode",
          "The video input multiplexed mode",
          GST_OMX_CAMERA_CAPT_MODE_TYPE, PROP_CAPT_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CAPT_MODE,
      g_param_spec_enum ("vip-mode", "VIP mode",
          "VIP port split configuration",
          GST_OMX_CAMERA_VIP_MODE_TYPE, PROP_VIP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SCAN_TYPE,
      g_param_spec_enum ("scan-type", "Scan Type",
          "Video scan type",
          GST_OMX_CAMERA_SCAN_TYPE, PROP_SCAN_TYPE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ALWAYS_COPY,
      g_param_spec_boolean ("always-copy", "Always copy",
          "If the output buffer should be copied or should use the OpenMax buffer",
          PROP_ALWAYS_COPY_DEFAULT, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_SKIP_FRAMES,
      g_param_spec_uint ("skip-frames", "Skip Frames",
          "Skip this amount of frames after a vaild frame",
          0, 30, PROP_SKIP_FRAMES_DEFAULT, G_PARAM_READWRITE));

  element_class->change_state = gst_omx_camera_change_state;


  gst_element_class_set_details_simple (element_class,
      "OpenMAX Video Source",
      "Source/Video",
      "Reads frames from a camera device",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");



  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  baseomxsrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_omx_camera_set_caps);
  //  basesrc_class->event = GST_DEBUG_FUNCPTR (gst_omx_camera_event);
  // baseomxsrc_class->negotiate = GST_DEBUG_FUNCPTR (gst_omx_camera_negotiate);
  // pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_omx_camera_create);
  
  baseomxsrc_class->handle_name = "OMX.TI.VPSSM3.VFCC";
 
  GST_DEBUG_CATEGORY_INIT (gst_omx_camera_debug, "omxcamera", 0,
      "OMX video source element");
}

static void
gst_omx_camera_init (GstOmxCamera *this)
{
  gst_base_src_set_format (GST_BASE_SRC (this), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (this), TRUE);

  //  self->started = FALSE;
  //self->sharing = FALSE;
  this->srcpad = NULL;

  /* Initialize properties */
  this->interface = PROP_INTERFACE_DEFAULT;
  this->capt_mode = PROP_CAPT_MODE_DEFAULT;
  this->vip_mode = PROP_VIP_MODE_DEFAULT;
  this->scan_type = PROP_SCAN_TYPE_DEFAULT;
  this->always_copy = PROP_ALWAYS_COPY_DEFAULT;
  this->num_buffers = PROP_NUM_OUT_BUFFERS_DEFAULT;
  this->skip_frames = PROP_SKIP_FRAMES_DEFAULT;
}


static gboolean
gst_omx_camera_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstOmxCamera *this = GST_OMX_CAMERA (src);
  const GstStructure *structure = gst_caps_get_structure (caps, 0);

  //GstVideoInfo info;
  gchar *caps_str = NULL;


  GstStructure *srcstructure = NULL;
  GstCaps *allowedcaps = NULL;
  GstCaps *newcaps = NULL;
  GValue stride = { 0, };
  GValue interlaced = { 0, };

  gboolean needs_disable = FALSE;

  /*  needs_disable =
      gst_omx_component_get_state (self->comp,
      GST_CLOCK_TIME_NONE) != OMX_StateLoaded;
  */


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
  this->format.height_padded = GST_OMX_ALIGN (this->format.height, 16);

  GST_DEBUG_OBJECT (this, "Reading framerate");
  if (!gst_structure_get_fraction (structure, "framerate",
          &this->format.framerate_num, &this->format.framerate_den)) {
    this->format.framerate_num = -1;
    this->format.framerate_den = -1;
    goto invalidcaps;
  }

  /* This is always fixed */
  this->format.format = GST_VIDEO_FORMAT_NV12;
  /* The right value is set with interlaced flag on output omx buffers */
  this->format.interlaced = FALSE;

  this->format.size_padded =
      this->format.width_padded * (this->format.height_padded + 4 * PADY) * 1.5;
  this->format.size = gst_video_format_get_size (this->format.format,
      this->format.width, this->format.height);

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

  g_value_init (&stride, G_TYPE_INT);
  g_value_set_int (&stride, this->format.width_padded);
  gst_structure_set_value (srcstructure, "stride", &stride);

  g_value_init (&interlaced, G_TYPE_BOOLEAN);
  g_value_set_boolean (&interlaced, this->format.interlaced);
  gst_structure_set_value (srcstructure, "interlaced", &interlaced);

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


/* Following caps negotiation related functions were taken from the 
 * omx_camera element code */

/* this function is a bit of a last resort */
static GstCaps *
gst_omx_camera_fixate (GstBaseSrc * basesrc, GstCaps * caps)
{
  GstStructure *structure;
  gint i;

  GST_DEBUG_OBJECT (basesrc, "fixating caps %" GST_PTR_FORMAT, caps);

  caps = gst_caps_make_writable (caps);

  for (i = 0; i < gst_caps_get_size (caps); ++i) {
    structure = gst_caps_get_structure (caps, i);

    /* We are fixating to a resonable 320x200 resolution
       and the maximum framerate resolution for that size */
    gst_structure_fixate_field_nearest_int (structure, "width", 320);
    gst_structure_fixate_field_nearest_int (structure, "height", 240);
    gst_structure_fixate_field_nearest_fraction (structure, "framerate",
        G_MAXINT, 1);
    gst_structure_fixate_field (structure, "format");
  }

  GST_DEBUG_OBJECT (basesrc, "fixated caps %" GST_PTR_FORMAT, caps);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (basesrc, caps);

  return caps;
}
