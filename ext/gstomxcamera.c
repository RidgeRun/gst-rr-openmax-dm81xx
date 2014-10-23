/*
 * GStreamer
 * Copyright (C) 2014 Melissa Montero <melissa.montero@ridgerun.com>
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

#include "OMX_TI_Common.h"
#include <omx_vfcc.h>
#include <OMX_TI_Index.h>
#include "timm_osal_interfaces.h"

#include "gstomxutils.h"
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

#define PADX 32
#define PADY 24

#define GSTOMX_ALL_FORMATS  "{ NV12, I420, YUY2, UYVY }"


#define gst_omx_camera_parent_class parent_class
G_DEFINE_TYPE (GstOmxCamera, gst_omx_camera, GST_TYPE_OMX_BASE_SRC);

/*
 * Caps:
 */
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV_STRIDED (GSTOMX_ALL_FORMATS,
            "[ 0, max ]"))
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
static gboolean gst_omx_camera_set_caps (GstBaseSrc * src, GstCaps * caps);
static void gst_omx_camera_fixate (GstBaseSrc * basesrc, GstCaps * caps);
static OMX_ERRORTYPE gst_omx_camera_init_pads (GstOmxBaseSrc * this);
static GstFlowReturn gst_omx_camera_create (GstPushSrc * src, GstBuffer ** buf);
static void gst_omx_camera_set_skip_frames (GstOmxCamera * this);
//static GstFlowReturn gst_omx_camera_fill_callback (GstOmxBaseSrc *, OMX_BUFFERHEADERTYPE *);


static void
gst_omx_camera_class_init (GstOmxCameraClass * klass)
{
 GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
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

  //element_class->change_state = gst_omx_camera_change_state;


  gst_element_class_set_details_simple (element_class,
      "OpenMAX Video Source",
      "Source/Video",
      "Reads frames from a camera device",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");



  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_omx_camera_set_caps);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_omx_camera_fixate);
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

  /* WARNING: Ugly hack follows 
   * Since the element pad needs to be a OmxPad, we need to overwrite 
   * the basesrc default pad with our new one to handle memory managment
   * in the same way is handled on the other omx elements.
   */

  GstPad *basesrcpad= (GstPad *) g_object_ref(GST_BASE_SRC(this)->srcpad);
  
  GST_DEBUG_OBJECT (this, "creating src pad");
  gst_pad_set_active (basesrcpad, FALSE);
  this->srcpad = GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
 
  GST_DEBUG_OBJECT (this, "setting functions on src pad");

  gst_pad_set_activatepush_function (this->srcpad, GST_PAD_ACTIVATEPUSHFUNC(basesrcpad));
  gst_pad_set_activatepull_function (this->srcpad, GST_PAD_ACTIVATEPULLFUNC(basesrcpad));
  gst_pad_set_event_function (this->srcpad, GST_PAD_EVENTFUNC(basesrcpad));
  gst_pad_set_query_function(this->srcpad, GST_PAD_QUERYFUNC(basesrcpad));
  gst_pad_set_checkgetrange_function (this->srcpad, GST_PAD_CHECKGETRANGEFUNC(basesrcpad) );
  gst_pad_set_getrange_function (this->srcpad, GST_PAD_GETRANGEFUNC(basesrcpad));
  gst_pad_set_getcaps_function (this->srcpad, GST_PAD_GETCAPSFUNC(basesrcpad));
  gst_pad_set_setcaps_function (this->srcpad,  GST_PAD_SETCAPSFUNC(basesrcpad));
  gst_pad_set_fixatecaps_function (this->srcpad, GST_PAD_FIXATECAPSFUNC(basesrcpad));
  gst_element_remove_pad (GST_ELEMENT (this), basesrcpad); 

  g_object_unref ( basesrcpad );
     GST_BASE_SRC_PAD(this) = this->srcpad ;
  gst_element_add_pad (GST_ELEMENT (this),  this->srcpad);
  gst_omx_base_src_add_pad (GST_OMX_BASE_SRC (this), this->srcpad);
  
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
  gchar *caps_str = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GstStructure *srcstructure = NULL;
  GstCaps *allowedcaps = NULL;
  GstCaps *newcaps = NULL;
  GValue stride = { 0, };
  GValue interlaced = { 0, };

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

  /* This is fixed for testing with NV12*/
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

  GST_DEBUG_OBJECT (this, "allowed caps: %s", gst_caps_to_string (allowedcaps));

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
  gst_structure_set (srcstructure, "format", GST_TYPE_FOURCC, 
      gst_video_format_to_fourcc (this->format.format), NULL);

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

  error = gst_omx_camera_init_pads (GST_OMX_BASE_SRC(this));
  if (GST_OMX_FAIL (error))
    goto nopads;
  
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
nopads:
  {
    GST_ERROR_OBJECT (this, "Unable to initializate ports: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
}

/* Following caps negotiation related functions were taken from the 
 * omx_camera element code */

/* this function is a bit of a last resort */
static void
gst_omx_camera_fixate (GstBaseSrc * basesrc, GstCaps * caps)
{
  GstStructure *structure;
  guint32 format;
  gint i;
  guint32 fourcc;

  GST_DEBUG_OBJECT (basesrc, "fixating caps %" GST_PTR_FORMAT, caps);
  caps = gst_caps_make_writable (caps);
  for (i = 0; i < gst_caps_get_size (caps); ++i) {
    const GValue *v;
    
    structure = gst_caps_get_structure (caps, i);

    /* We are fixating to 1920x1080 resolution
       and the maximum framerate resolution for that size */
    gst_structure_fixate_field_nearest_int (structure, "width", 1920);
    gst_structure_fixate_field_nearest_int (structure, "height", 1080);
    gst_structure_fixate_field_nearest_fraction (structure, "framerate",
        G_MAXINT, 1);

    v = gst_structure_get_value (structure, "format");
    if (v && G_VALUE_TYPE (v) != GST_TYPE_FOURCC) {
      guint32 fourcc;
      
      g_return_if_fail (G_VALUE_TYPE (v) == GST_TYPE_LIST);
      
      fourcc = gst_value_get_fourcc (gst_value_list_get_value (v, 0));
      gst_structure_set (structure, "format", GST_TYPE_FOURCC, fourcc, NULL);
    }
  }

  GST_DEBUG_OBJECT (basesrc, "fixated caps %" GST_PTR_FORMAT, caps);

}


static void
gst_omx_camera_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxCamera *this = GST_OMX_CAMERA (object);

  switch (prop_id) {
    case PROP_INTERFACE:
      this->interface = g_value_get_enum (value);
      break;
    case PROP_CAPT_MODE:
      this->capt_mode = g_value_get_enum (value);
      break;
    case PROP_VIP_MODE:
      this->vip_mode = g_value_get_enum (value);
      break;
    case PROP_SCAN_TYPE:
      this->scan_type = g_value_get_enum (value);
      break;
    case PROP_ALWAYS_COPY:
      this->always_copy = g_value_get_boolean (value);
      break;
    case PROP_NUM_OUT_BUFFERS:
      this->num_buffers = g_value_get_uint (value);
      break;
    case PROP_SKIP_FRAMES:
      this->skip_frames = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static void
gst_omx_camera_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec){

  GstOmxCamera *this = GST_OMX_CAMERA (object);
  //  OMX_ERRORTYPE err;

  switch (prop_id) {
    case PROP_INTERFACE:
      g_value_set_enum (value, this->interface);
      break;
    case PROP_CAPT_MODE:
      g_value_set_enum (value, this->capt_mode);
      break;
    case PROP_VIP_MODE:
      g_value_set_enum (value, this->vip_mode);
      break;
    case PROP_SCAN_TYPE:
      g_value_set_enum (value, this->scan_type);
      break;
    case PROP_ALWAYS_COPY:
      g_value_set_boolean (value, this->always_copy);
      break;
    case PROP_NUM_OUT_BUFFERS:
      g_value_set_uint (value, this->num_buffers);
      break;
    case PROP_SKIP_FRAMES:
    {
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static OMX_ERRORTYPE gst_omx_camera_init_pads (GstOmxBaseSrc * base)
{
  GstOmxCamera *this = GST_OMX_CAMERA (base);
  OMX_PARAM_PORTDEFINITIONTYPE *port = NULL;
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_PARAM_VFCC_HWPORT_PROPERTIES hw_port_param;
  OMX_PARAM_VFCC_HWPORT_ID hw_port;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gchar *portname = NULL;


  GST_DEBUG_OBJECT (this, "Initializing  output port memory");
  GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
  memory.nPortIndex = OMX_VFCC_OUTPUT_PORT_START_INDEX;
  memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;
  g_mutex_lock (&_omx_mutex);

  GST_DEBUG_OBJECT (this, "Memory type: %d", memory.eBufMemoryType);

  error =
      OMX_SetParameter (base->handle, OMX_TI_IndexParamBuffMemType, &memory);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    portname = "output";
    goto noport;
  }

  GST_DEBUG_OBJECT (this, "Initializing src pad port");
  port = GST_OMX_PAD_PORT (GST_OMX_PAD (this->srcpad));
  GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);

  port->nPortIndex = OMX_VFCC_OUTPUT_PORT_START_INDEX;;
  port->nBufferCountActual = base->output_buffers;
  port->format.video.nFrameWidth = this->format.width;
  port->format.video.nFrameHeight = this->format.height_padded;
  port->format.video.nStride = this->format.width_padded;
  port->format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  port->format.video.eColorFormat = gst_omx_convert_format_to_omx (this->format.format);

 if (port->format.video.eColorFormat == OMX_COLOR_FormatUnused) {
   //GST_ERROR_OBJECT (this, "Unsupported format %s",
   //		      gst_video_format_to_string (this->format.format));
   GST_ERROR_OBJECT (this, "Unsupported format ");
  }
 
 port->nBufferSize = gst_omx_camera_get_buffer_size(this->format.format,this->format.width_padded,this->format.height_padded);

  GST_DEBUG_OBJECT (this,
      "width= %li, height=%li, stride=%li, format %d, buffersize %li",
      port->format.video.nFrameWidth, port->format.video.nFrameHeight,
      port->format.video.nStride,  port->format.video.eColorFormat,
      port->nBufferSize);


  g_mutex_lock (&_omx_mutex);
  error =
    OMX_SetParameter (GST_OMX_BASE_SRC(this)->handle,
      OMX_IndexParamPortDefinition, port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }

 GST_DEBUG_OBJECT (this, "Initializing  capture interface");

  GST_OMX_INIT_STRUCT (&hw_port, OMX_PARAM_VFCC_HWPORT_ID);
 /* Set capture interface */
  hw_port.eHwPortId = this->interface;  
  g_mutex_lock (&_omx_mutex);
  error =
    OMX_SetParameter (GST_OMX_BASE_SRC(this)->handle,
			OMX_TI_IndexParamVFCCHwPortID, &hw_port);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone) {
    portname = "output";
    goto noport;
  }


  GST_DEBUG_OBJECT (this, "Hardware port id: %d", hw_port.eHwPortId);

  GST_OMX_INIT_STRUCT (&hw_port_param, OMX_PARAM_VFCC_HWPORT_PROPERTIES);
  hw_port_param.eCaptMode = this->capt_mode;
  hw_port_param.eVifMode = this->vip_mode;
  hw_port_param.eInColorFormat = OMX_COLOR_FormatYCbYCr;
  hw_port_param.eScanType = this->scan_type;
  hw_port_param.nMaxHeight =  this->format.height_padded;
  hw_port_param.nMaxWidth = this->format.width;
  hw_port_param.nMaxChnlsPerHwPort = 1;
  if (this->scan_type == OMX_VIDEO_CaptureScanTypeInterlaced)
    hw_port_param.nMaxHeight = hw_port_param.nMaxHeight >> 1;


GST_DEBUG_OBJECT (this,
      "Hw port properties: capture mode %d, vif mode %d, max height %li, max width %li, max channel %li, scan type %d, format %d",
      hw_port_param.eCaptMode, hw_port_param.eVifMode, hw_port_param.nMaxHeight,
      hw_port_param.nMaxWidth, hw_port_param.nMaxChnlsPerHwPort,
      hw_port_param.eScanType, hw_port_param.eInColorFormat);

  g_mutex_lock (&_omx_mutex);
  error =
    OMX_SetParameter (base->handle, OMX_TI_IndexParamVFCCHwPortProperties, &hw_port_param);
  g_mutex_unlock (&_omx_mutex);

  if (GST_OMX_FAIL (error)) {
    portname = "output";
    goto noport;
  }


   gst_omx_camera_set_skip_frames (this);
   /*
  this->duration = gst_util_uint64_scale_int (GST_SECOND,
      GST_VIDEO_INFO_FPS_D (info), GST_VIDEO_INFO_FPS_N (info));
      */
  return error;

noport:
  {
    GST_ERROR_OBJECT (this, "Failed to set %s port parameters", portname);
    return error;
  }

noenable:
  {
    GST_ERROR_OBJECT (this, "Failed to enable omx_camera");
    return error;
  }
}


gint
gst_omx_camera_get_buffer_size (GstVideoFormat format, gint stride, gint height)
{
  gint buffer_size;

  switch (format) {
    case GST_VIDEO_FORMAT_YUY2:
      buffer_size = stride * height;
      break;
    case GST_VIDEO_FORMAT_I420:
      buffer_size = stride * height + 2 * ((stride >> 1) * ((height + 1) >> 2));
      break;
    case GST_VIDEO_FORMAT_NV12:
      buffer_size = (stride * height * 3) >> 1;
      break;
    default:
      buffer_size = 0;
      break;
  }
  return buffer_size;
}


static void
gst_omx_camera_set_skip_frames (GstOmxCamera * this)
{
  OMX_ERRORTYPE err;
  GstOmxBaseSrc *base = GST_OMX_BASE_SRC(this);
  OMX_CONFIG_VFCC_FRAMESKIP_INFO skip_frames;
  guint32 shifts = 0, skip = 0, i = 0, count = 0;
  shifts = this->skip_frames;

  if (shifts) {
    while (count < MAX_SHIFTS) {
      if ((count + shifts) > MAX_SHIFTS)
        shifts = MAX_SHIFTS - count;

      for (i = 0; i < shifts; i++) {
        skip = (skip << 1) | 1;
        count++;
      }

      if (count < MAX_SHIFTS) {
        skip = skip << 1;
        count++;
      }
    }
  }

  GST_OMX_INIT_STRUCT (&skip_frames,OMX_CONFIG_VFCC_FRAMESKIP_INFO);
  /* OMX_TI_IndexConfigVFCCFrameSkip is for dropping frames in capture,
     it is a binary 30bit value where 1 means drop a frame and 0
     process the frame */
  skip_frames.frameSkipMask = skip;
  g_mutex_lock (&_omx_mutex);
  err =
    OMX_SetParameter (base->handle, OMX_TI_IndexConfigVFCCFrameSkip, &skip_frames);
  g_mutex_unlock (&_omx_mutex);

  if (err != OMX_ErrorNone)
    GST_WARNING_OBJECT (this,
        "Failed to set capture skip frames to %d: %s (0x%08x)", shifts,
        gst_omx_error_to_str (err), err);

  return;
}

/*
static GstFlowReturn
gst_omx_camera_create (GstPushSrc * src, GstBuffer ** buf)
{
  
  GstFlowReturn ret;
  GstOMXCamera *self = GST_OMX_CAMERA (src);
  GstFlowReturn ret;
  GstClock *clock;
  GstClockTime abs_time, base_time, timestamp, duration; 
  

  GST_OBJECT_LOCK (self);
  if ((clock = GST_ELEMENT_CLOCK (self))) {
    //we have a clock, get base time and ref clock 
    base_time = GST_ELEMENT (self)->base_time;
    abs_time = gst_clock_get_time (clock);
  } else {
    // no clock, can't set timestamps 
    base_time = GST_CLOCK_TIME_NONE;
    abs_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (self);

  ret = gst_omx_camera_get_buffer (self, buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    goto error;

  return ret;
}



static GstFlowReturn
gst_omx_camera_get_buffer (GstOMXCamera * self, GstBuffer ** outbuf)
{
  GstOMXPort *port = self->outport;
  GstOMXBuffer *buf = NULL;
  GstOMXAcquireBufferReturn acq_return;
  OMX_ERRORTYPE err;
  GstBufferPool *pool = self->outpool;
  GstFlowReturn flow_ret;

  acq_return = gst_omx_port_acquire_buffer (port, &buf);
  if (acq_return == GST_OMX_ACQUIRE_BUFFER_ERROR) {
    goto component_error;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
    goto flushing;
  } else if (acq_return != GST_OMX_ACQUIRE_BUFFER_OK) {
    return GST_FLOW_ERROR;
  }

  if (gst_omx_port_is_flushing (port)) {
    GST_DEBUG_OBJECT (self, "Flushing");
    gst_omx_port_release_buffer (port, buf);
    goto flushing;
  }

  GST_LOG_OBJECT (self, "Handling buffer: 0x%08x %" G_GUINT64_FORMAT,
      (guint) buf->omx_buf->nFlags, (guint64) buf->omx_buf->nTimeStamp);

  buf->omx_buf->nFilledLen = self->imagesize;
  if (buf->omx_buf->nFilledLen > 0) {
    GstMapInfo map = GST_MAP_INFO_INIT;

    GST_LOG_OBJECT (self, "Handling output data");

    if (self->always_copy) {
      *outbuf = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

      gst_buffer_map (*outbuf, &map, GST_MAP_WRITE);
      memcpy (map.data,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);
      gst_buffer_unmap (*outbuf, &map);

      err = gst_omx_port_release_buffer (port, buf);
      if (err != OMX_ErrorNone)
        goto release_error;
    } else {
      GstBufferPoolAcquireParams params = { 0, };
      gint n = port->buffers->len;
      GstMemory *mem;
      gint i;

      for (i = 0; i < n; i++) {
        GstOMXBuffer *tmp = g_ptr_array_index (port->buffers, i);

        if (tmp == buf)
          break;
      }
      g_assert (i != n);

      GST_OMX_BUFFER_POOL (pool)->current_buffer_index = i;
      flow_ret = gst_buffer_pool_acquire_buffer (pool, outbuf, &params);
      if (flow_ret != GST_FLOW_OK) {
        gst_omx_port_release_buffer (port, buf);
        goto invalid_buffer;
      }
      mem = gst_buffer_get_memory (*outbuf, 0);
      gst_memory_unref (mem);
    }
  } else {
    *outbuf = gst_buffer_new ();
  }

  GST_BUFFER_TIMESTAMP (*outbuf) =
      gst_util_uint64_scale (buf->omx_buf->nTimeStamp, GST_SECOND,
      OMX_TICKS_PER_SECOND) * 1000;

  if (buf->omx_buf->nTickCount != 0)
    GST_BUFFER_DURATION (*outbuf) =
        gst_util_uint64_scale (buf->omx_buf->nTickCount, GST_SECOND,
        OMX_TICKS_PER_SECOND);
  else
    GST_BUFFER_DURATION (*outbuf) = self->duration;

  GST_DEBUG_OBJECT (self,
      "Got buffer from component: %p with timestamp %" GST_TIME_FORMAT
      " duration %" GST_TIME_FORMAT, outbuf,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*outbuf)));
  return GST_FLOW_OK;

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->comp),
            gst_omx_component_get_last_error (self->comp)));
    gst_pad_push_event (GST_BASE_SRC_PAD (self), gst_event_new_eos ());
    self->started = FALSE;
    return GST_FLOW_ERROR;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing");
    self->started = FALSE;
    return GST_FLOW_FLUSHING;
  }
invalid_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Cannot acquire output buffer from pool"));
    return flow_ret;
  }
release_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase output buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    gst_pad_push_event (GST_BASE_SRC_PAD (self), gst_event_new_eos ());
    self->started = FALSE;
    return GST_FLOW_ERROR;
  }
}
*/

