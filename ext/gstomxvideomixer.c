/*
 * GStreamer
 * Copyright (C) 2016 Melissa Montero <melissa.montero@ridgerun.com>
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
 * SECTION:element-omx_video_mixer
 *
 * This element will mix multiple video streams
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omx_video_mixer ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "timm_osal_interfaces.h"
#include "gstomxvideomixer.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_video_mixer_debug);
#define GST_CAT_DEFAULT gst_omx_video_mixer_debug


#define GST_TYPE_OMX_VIDEO_MIXER_PAD (gst_omx_video_mixer_pad_get_type())
#define GST_OMX_VIDEO_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_VIDEO_MIXER_PAD, GstOmxVideoMixerPad))
#define GST_VIDEO_OMX_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_VIDEO_MIXER_PAD, GstOmxVideoMixerPadClass))
#define GST_IS_OMX_VIDEO_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_VIDEO_MIXER_PAD))
#define GST_IS_OMX_VIDEO_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_VIDEO_MIXER_PAD))

typedef struct _GstOmxVideoMixerPad GstOmxVideoMixerPad;
typedef struct _GstOmxVideoMixerPadClass GstOmxVideoMixerPadClass;

/* all information needed for one video stream */
struct _GstOmxVideoMixerPad
{
  GstOmxPad parent;             /* subclass the pad */

  /* Caps */
  guint width;
  guint height;
  guint stride;

  /* Properties */
  guint out_x;
  guint out_y;
  guint out_width;
  guint out_height;
  guint in_x;
  guint in_y;
  guint crop_width;
  guint crop_height;
};

struct _GstOmxVideoMixerPadClass
{
  GstOmxPadClass parent_class;
};


enum
{
  PROP_PAD_0,
  PROP_PAD_OUT_X,
  PROP_PAD_OUT_Y,
  PROP_PAD_OUT_WIDTH,
  PROP_PAD_OUT_HEIGHT,
  PROP_PAD_IN_X,
  PROP_PAD_IN_Y,
  PROP_PAD_CROP_WIDTH,
  PROP_PAD_CROP_HEIGHT,
};

#define DEFAULT_PAD_OUT_X        0
#define DEFAULT_PAD_OUT_Y        0
#define DEFAULT_PAD_OUT_WIDTH    0
#define DEFAULT_PAD_OUT_HEIGHT   0
#define DEFAULT_PAD_IN_X         0
#define DEFAULT_PAD_IN_Y         0
#define DEFAULT_PAD_CROP_WIDTH   0
#define DEFAULT_PAD_CROP_HEIGHT  0

static void gst_omx_video_mixer_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_omx_video_mixer_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

GstOmxVideoMixerPad *gst_omx_video_mixer_pad_new_from_template (GstPadTemplate *
    templ, const gchar * name);

GType gst_omx_video_mixer_pad_get_type (void);
G_DEFINE_TYPE (GstOmxVideoMixerPad, gst_omx_video_mixer_pad, TYPE_GST_OMX_PAD);

static void
gst_omx_video_mixer_pad_class_init (GstOmxVideoMixerPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_omx_video_mixer_pad_set_property;
  gobject_class->get_property = gst_omx_video_mixer_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_OUT_X,
      g_param_spec_uint ("outX", "Output X position",
          "X position of the output picture", 0, G_MAXINT,
          DEFAULT_PAD_OUT_X, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_OUT_Y,
      g_param_spec_uint ("outY", "Output Y position",
          "Y position of the output picture", 0, G_MAXINT,
          DEFAULT_PAD_OUT_Y, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_OUT_WIDTH,
      g_param_spec_uint ("outWidth", "Output width",
          "Width of the output picture", 0, G_MAXINT,
          DEFAULT_PAD_OUT_WIDTH, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_OUT_HEIGHT,
      g_param_spec_uint ("outHeight", "Output height",
          "Height of the output picture", 0, G_MAXINT,
          DEFAULT_PAD_OUT_HEIGHT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_IN_X,
      g_param_spec_uint ("inX", "Input X crop position",
          "X position of the crop input picture", 0, G_MAXINT,
          DEFAULT_PAD_IN_X, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_IN_Y,
      g_param_spec_uint ("inY", "Input Y crop position",
          "Y position of the crop input picture", 0, G_MAXINT,
          DEFAULT_PAD_IN_Y, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_WIDTH,
      g_param_spec_uint ("cropWidth", "Input crop width",
          "Width of the crop input picture", 0, G_MAXINT,
          DEFAULT_PAD_CROP_WIDTH, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_HEIGHT,
      g_param_spec_uint ("cropHeight", "Input crop height",
          "Height of the crop input picture", 0, G_MAXINT,
          DEFAULT_PAD_CROP_HEIGHT, G_PARAM_READWRITE));

}

static void
gst_omx_video_mixer_pad_init (GstOmxVideoMixerPad * mixerpad)
{
  mixerpad->out_x = DEFAULT_PAD_OUT_X;
  mixerpad->out_y = DEFAULT_PAD_OUT_Y;
  mixerpad->out_width = DEFAULT_PAD_OUT_WIDTH;
  mixerpad->out_height = DEFAULT_PAD_OUT_HEIGHT;
  mixerpad->in_x = DEFAULT_PAD_IN_X;
  mixerpad->in_y = DEFAULT_PAD_IN_Y;
  mixerpad->crop_width = DEFAULT_PAD_CROP_WIDTH;
  mixerpad->crop_height = DEFAULT_PAD_CROP_HEIGHT;
}


static void
gst_omx_video_mixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxVideoMixerPad *mixerpad = GST_OMX_VIDEO_MIXER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_OUT_X:
      g_value_set_uint (value, mixerpad->out_x);
      break;
    case PROP_PAD_OUT_Y:
      g_value_set_uint (value, mixerpad->out_y);
      break;
    case PROP_PAD_OUT_WIDTH:
      g_value_set_uint (value, mixerpad->out_width);
      break;
    case PROP_PAD_OUT_HEIGHT:
      g_value_set_uint (value, mixerpad->out_height);
      break;
    case PROP_PAD_IN_X:
      g_value_set_uint (value, mixerpad->in_x);
      break;
    case PROP_PAD_IN_Y:
      g_value_set_uint (value, mixerpad->in_y);
      break;
    case PROP_PAD_CROP_WIDTH:
      g_value_set_uint (value, mixerpad->crop_width);
      break;
    case PROP_PAD_CROP_HEIGHT:
      g_value_set_uint (value, mixerpad->crop_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_mixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxVideoMixerPad *mixerpad = GST_OMX_VIDEO_MIXER_PAD (object);

  GST_INFO_OBJECT (mixerpad, "Setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_PAD_OUT_X:
      mixerpad->out_x = g_value_get_uint (value);
      break;
    case PROP_PAD_OUT_Y:
      mixerpad->out_y = g_value_get_uint (value);
      break;
    case PROP_PAD_OUT_WIDTH:
      mixerpad->out_width = g_value_get_uint (value);
      break;
    case PROP_PAD_OUT_HEIGHT:
      mixerpad->out_height = g_value_get_uint (value);
      break;
    case PROP_PAD_IN_X:
      mixerpad->in_x = g_value_get_uint (value);
      break;
    case PROP_PAD_IN_Y:
      mixerpad->in_y = g_value_get_uint (value);
      break;
    case PROP_PAD_CROP_WIDTH:
      mixerpad->crop_width = g_value_get_uint (value);
      break;
    case PROP_PAD_CROP_HEIGHT:
      mixerpad->crop_height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

GstOmxVideoMixerPad *
gst_omx_video_mixer_pad_new_from_template (GstPadTemplate * templ,
    const gchar * name)
{

  return g_object_new (GST_TYPE_OMX_VIDEO_MIXER_PAD,
      "name", name, "template", templ, "direction", templ->direction, NULL);
}

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("YUY2"))
    );

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
};

#define OMX_VIDEO_MIXER_HANDLE_NAME   "OMX.TI.VPSSM3.VFPC.INDTXSCWB"
#define DEFAULT_VIDEO_MIXER_NUM_INPUT_BUFFERS    8
#define DEFAULT_VIDEO_MIXER_NUM_OUTPUT_BUFFERS   8

static void _do_init (GType object_type);
GST_BOILERPLATE_FULL (GstOmxVideoMixer, gst_omx_video_mixer, GstElement,
    GST_TYPE_ELEMENT, _do_init);

static void gst_omx_video_mixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_video_mixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_omx_video_mixer_finalize (GObject * object);
static gboolean gst_omx_video_mixer_set_caps (GstPad * pad, GstCaps * caps);


static GstPad *gst_omx_video_mixer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name);
static void gst_omx_video_mixer_release_pad (GstElement * element,
    GstPad * pad);
static gboolean gst_omx_video_mixer_sink_setcaps (GstPad * pad, GstCaps * caps);

static GstStateChangeReturn gst_omx_video_mixer_change_state (GstElement *
    element, GstStateChange transition);

static GstFlowReturn gst_omx_video_mixer_collected (GstCollectPads2 * pads,
    GstOmxVideoMixer * mixer);
gboolean gst_omx_video_mixer_create_dummy_sink_pads (GstOmxVideoMixer * mixer);
gboolean gst_omx_video_mixer_free_dummy_sink_pads (GstOmxVideoMixer * mixer);

static OMX_ERRORTYPE gst_omx_video_mixer_allocate_omx (GstOmxVideoMixer * mixer,
    gchar * handle_name);
static OMX_ERRORTYPE gst_omx_video_mixer_free_omx (GstOmxVideoMixer * mixer);
static OMX_ERRORTYPE gst_omx_video_mixer_init_ports (GstOmxVideoMixer * mixer);

static void gst_omx_video_mixer_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

static void
_do_init (GType object_type)
{
  static const GInterfaceInfo child_proxy_info = {
    (GInterfaceInitFunc) gst_omx_video_mixer_child_proxy_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (object_type, GST_TYPE_CHILD_PROXY,
      &child_proxy_info);
}

/* GObject vmethod implementations */

static void
gst_omx_video_mixer_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_omx_video_mixer_debug, "omx_video_mixer",
      0, "RidgeRun's OMX video mixer element");
}

/* initialize the omx's class */
static void
gst_omx_video_mixer_class_init (GstOmxVideoMixerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gst_element_class_set_details_simple (gstelement_class,
      "OpenMAX video mixer",
      "Filter/Video",
      "Mix multiple video streams in a mosaic",
      "Melissa Montero <melissa.montero@ridgerun.com>");


  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);

  gobject_class->set_property = gst_omx_video_mixer_set_property;
  gobject_class->get_property = gst_omx_video_mixer_get_property;
  gobject_class->finalize = gst_omx_video_mixer_finalize;


  /* Register the pad class */
  (void) (GST_TYPE_OMX_VIDEO_MIXER_PAD);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_omx_video_mixer_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_omx_video_mixer_release_pad);
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_video_mixer_change_state);

}


/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_video_mixer_init (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerClass * g_class)
{

  mixer->srcpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
  gst_element_add_pad (GST_ELEMENT (mixer), mixer->srcpad);

  GST_INFO_OBJECT (mixer, "Initializing %s", GST_OBJECT_NAME (mixer));

  mixer->started = FALSE;
  mixer->input_buffers = DEFAULT_VIDEO_MIXER_NUM_INPUT_BUFFERS;
  mixer->output_buffers = DEFAULT_VIDEO_MIXER_NUM_OUTPUT_BUFFERS;
  mixer->sinkpads = NULL;
  mixer->srcpads = NULL;

  mixer->collect = gst_collect_pads2_new ();
  gst_collect_pads2_set_function (mixer->collect, (GstCollectPads2Function)
      GST_DEBUG_FUNCPTR (gst_omx_video_mixer_collected), mixer);
}

static void
gst_omx_video_mixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_mixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_mixer_finalize (GObject * object)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (object);

  gst_object_unref (mixer->collect);
  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstPad *
gst_omx_video_mixer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  GstOmxVideoMixer *mixer;
  GstOmxVideoMixerPad *omxpad;
  gchar *name;

  mixer = GST_OMX_VIDEO_MIXER (element);

  if (templ != gst_element_class_get_pad_template (klass, "sink%d"))
    return NULL;

  name = g_strdup_printf ("sink%d", mixer->next_sinkpad++);
  omxpad = gst_omx_video_mixer_pad_new_from_template (templ, name);
  g_free (name);

  /* Setup pad functions */
  gst_pad_set_setcaps_function (GST_PAD (omxpad),
      gst_omx_video_mixer_sink_setcaps);

  gst_collect_pads2_add_pad (mixer->collect, GST_PAD (omxpad),
      sizeof (GstCollectData2));

  mixer->sinkpads = g_list_append (mixer->sinkpads, omxpad);
  mixer->sinkpad_count++;

  GST_DEBUG_OBJECT (element, "Adding pad %s", GST_PAD_NAME (omxpad));
  gst_element_add_pad (element, GST_PAD (omxpad));

  gst_child_proxy_child_added (GST_OBJECT (mixer), GST_OBJECT (omxpad));

  return GST_PAD (omxpad);
}

static void
gst_omx_video_mixer_release_pad (GstElement * element, GstPad * pad)
{
  GstOmxVideoMixer *mixer;

  mixer = GST_OMX_VIDEO_MIXER (element);

  gst_collect_pads2_remove_pad (mixer->collect, pad);

  gst_child_proxy_child_removed (GST_OBJECT (mixer), GST_OBJECT (pad));

  mixer->sinkpads = g_list_remove (mixer->sinkpads, pad);
  mixer->sinkpad_count--;

  GST_DEBUG_OBJECT (element, "Removing pad %s", GST_PAD_NAME (pad));

  gst_element_remove_pad (element, pad);
}


static GstStateChangeReturn
gst_omx_video_mixer_change_state (GstElement * element,
    GstStateChange transition)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (element);
  GstStateChangeReturn ret;
  OMX_ERRORTYPE error;

  switch (transition) {

    case GST_STATE_CHANGE_NULL_TO_READY:
      error =
          gst_omx_video_mixer_allocate_omx (mixer, OMX_VIDEO_MIXER_HANDLE_NAME);
      if (GST_OMX_FAIL (error))
        goto allocate_fail;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_LOG_OBJECT (mixer, "Starting collectpads");
      gst_collect_pads2_start (mixer->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_LOG_OBJECT (mixer, "Stopping collectpads");
      gst_collect_pads2_stop (mixer->collect);
      gst_omx_video_mixer_free_dummy_sink_pads (mixer);
      mixer->started = FALSE;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);


  switch (transition) {

    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_omx_video_mixer_free_omx (mixer);
      break;
    default:
      break;
  }
  return ret;

allocate_fail:
  {
    GST_ELEMENT_ERROR (mixer, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_omx_video_mixer_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstOmxVideoMixerPad *omxpad;
  GstVideoFormat fmt;
  GstStructure *s;
  gint width, height;

  GST_INFO_OBJECT (pad, "Setting caps %" GST_PTR_FORMAT, caps);

  omxpad = GST_OMX_VIDEO_MIXER_PAD (pad);

  if (!gst_video_format_parse_caps (caps, &fmt, &width, &height))
    goto parse_failed;


  omxpad->width = width;
  omxpad->height = height;

  s = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_uint (s, "stride", &omxpad->stride))
    omxpad->stride = omxpad->width;

  return TRUE;

parse_failed:
  {
    GST_ERROR_OBJECT (pad, "Failed to parse caps");
    return FALSE;
  }

}

static gboolean
gst_omx_video_mixer_update_src_caps (GstOmxVideoMixer * mixer)
{
  GList *l;
  guint min_width = 0, min_height = 0;
  GstStructure *s;
  GstCaps *caps, *peercaps;
  gboolean ret = FALSE;

  /* Get the minimun output image size required to contain  
     the mixer mosaic */
  for (l = mixer->sinkpads; l; l = l->next) {
    GstOmxVideoMixerPad *omxpad = l->data;
    guint cur_width = 0, cur_height = 0;

    if (!omxpad->width || !omxpad->height) {
      GST_WARNING_OBJECT (mixer,
          "pad %s not configured, this should not happen",
          GST_OBJECT_NAME (omxpad));
      continue;
    }

    cur_width = omxpad->out_width + omxpad->out_x;
    cur_height = omxpad->out_height + omxpad->out_y;

    if (min_width < cur_width)
      min_width = cur_width;
    if (min_height < cur_height)
      min_height = cur_height;
  }

  /* Set src caps */
  peercaps = gst_pad_peer_get_caps (mixer->srcpad);
  if (peercaps) {
    GstCaps *tmp;

    caps = gst_video_format_new_caps_simple (GST_VIDEO_FORMAT_YUY2, 0,
        "width", GST_TYPE_INT_RANGE, min_width, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, min_height, G_MAXINT, NULL);

    tmp = gst_caps_intersect (caps, peercaps);
    gst_caps_unref (caps);
    gst_caps_unref (peercaps);
    caps = tmp;

    if (gst_caps_is_empty (caps)) {
      ret = FALSE;
      goto done;
    }

    gst_caps_truncate (caps);
    s = gst_caps_get_structure (caps, 0);
    gst_structure_fixate_field_nearest_int (s, "width", min_width);
    gst_structure_fixate_field_nearest_int (s, "height", min_height);

    gst_structure_get_int (s, "width", &mixer->src_width);
    gst_structure_get_int (s, "height", &mixer->src_height);

    mixer->src_stride =
        gst_video_format_get_row_stride (GST_VIDEO_FORMAT_YUY2, 0,
        mixer->src_width);
    ret = gst_pad_set_caps (mixer->srcpad, caps);
    gst_caps_unref (caps);
  }


done:
  return ret;
}


static GstFlowReturn
gst_omx_video_mixer_collected (GstCollectPads2 * pads, GstOmxVideoMixer * mixer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GSList *l;

  if (!mixer->started) {

    if (!gst_omx_video_mixer_update_src_caps (mixer))
      goto caps_failed;

    if (GST_OMX_FAIL (gst_omx_video_mixer_init_ports (mixer)))
      goto init_ports_failed;

    mixer->started = TRUE;
  }

  GST_DEBUG_OBJECT (mixer, "Entering collected");
  for (l = mixer->collect->data; l; l = l->next) {
    GstCollectData2 *data;
    GstBuffer *buffer;

    data = (GstCollectData2 *) l->data;
    buffer = gst_collect_pads2_pop (mixer->collect, data);

    if (buffer) {
      GstOmxPad *omxpad = GST_OMX_PAD (data->pad);

      GST_LOG_OBJECT (omxpad, "Got buffer %p with timestamp %" GST_TIME_FORMAT,
          buffer, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
      gst_buffer_unref (buffer);
    }
  }

  return ret;

caps_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to set src caps");
    return GST_FLOW_NOT_NEGOTIATED;
  }
init_ports_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to initialize omx ports");
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

gboolean
gst_omx_video_mixer_create_dummy_sink_pads (GstOmxVideoMixer * mixer)
{
  GstOmxPad *omxpad;
  guint i;
  gchar *name;

  if (mixer->srcpads)
    gst_omx_video_mixer_free_dummy_sink_pads (mixer);

  mixer->srcpads = g_list_append (mixer->srcpads, mixer->srcpad);

  for (i = 1; i < mixer->sinkpad_count; i++) {
    name = g_strdup_printf ("src%d", i);
    omxpad =
        gst_omx_pad_new_from_template (gst_static_pad_template_get
        (&src_template), name);
    g_free (name);
    mixer->srcpads = g_list_append (mixer->srcpads, omxpad);
  }

  return TRUE;
}

gboolean
gst_omx_video_mixer_free_dummy_sink_pads (GstOmxVideoMixer * mixer)
{
  GstOmxPad *omxpad;
  GList *l;

  mixer->srcpads = g_list_remove (mixer->srcpads, mixer->srcpad);

  for (l = mixer->srcpads; l; l = l->next) {
    omxpad = l->data;
    mixer->srcpads = g_list_remove (mixer->srcpads, omxpad);
    gst_object_unref (omxpad);
  }

  mixer->srcpads = NULL;
  return TRUE;
}

/* GstChildProxy implementation */
static GstObject *
gst_omx_video_mixer_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (child_proxy);
  GstObject *obj;

  GST_OBJECT_LOCK (mixer);
  if ((obj = g_list_nth_data (mixer->sinkpads, index)))
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (mixer);

  return obj;
}

static guint
gst_omx_video_mixer_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (child_proxy);

  GST_OBJECT_LOCK (mixer);
  count = mixer->sinkpad_count;
  GST_OBJECT_UNLOCK (mixer);

  GST_INFO_OBJECT (mixer, "Children Count: %d", count);
  return count;
}

static void
gst_omx_video_mixer_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_INFO ("intializing child proxy interface");
  iface->get_child_by_index =
      gst_omx_video_mixer_child_proxy_get_child_by_index;
  iface->get_children_count =
      gst_omx_video_mixer_child_proxy_get_children_count;
}

/* Omx mixer implementation */

static OMX_ERRORTYPE
gst_omx_video_mixer_event_callback (OMX_HANDLETYPE handle,
    gpointer data,
    OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2, gpointer eventdata)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (data);

  switch (event) {
    case OMX_EventError:
      GST_ERROR_OBJECT (mixer, "OMX error event received: %s",
          gst_omx_error_to_str (nevent1));
      break;
  }
  return error;
}

static OMX_ERRORTYPE
gst_omx_video_mixer_allocate_omx (GstOmxVideoMixer * mixer, gchar * handle_name)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_INFO_OBJECT (mixer, "Allocating OMX resources for %s", handle_name);

  mixer->callbacks = TIMM_OSAL_Malloc (sizeof (OMX_CALLBACKTYPE),
      TIMM_OSAL_TRUE, 0, TIMMOSAL_MEM_SEGMENT_EXT);
  if (!mixer->callbacks) {
    error = OMX_ErrorInsufficientResources;
    goto noresources;
  }

  mixer->callbacks->EventHandler =
      (GstOmxEventHandler) gst_omx_video_mixer_event_callback;

  if (!handle_name) {
    error = OMX_ErrorInvalidComponentName;
    goto nohandlename;
  }

  g_mutex_lock (&_omx_mutex);
  error = OMX_GetHandle (&mixer->handle, handle_name, mixer, mixer->callbacks);
  g_mutex_unlock (&_omx_mutex);
  if ((error != OMX_ErrorNone) || (!mixer->handle))
    goto nohandle;

  return error;

noresources:
  {
    GST_ERROR_OBJECT (mixer, "Insufficient OMX memory resources");
    return error;
  }
nohandlename:
  {
    GST_ERROR_OBJECT (mixer, "The component name has not been defined");
    return error;
  }
nohandle:
  {
    GST_ERROR_OBJECT (mixer, "Unable to grab OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_video_mixer_free_omx (GstOmxVideoMixer * mixer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_INFO_OBJECT (mixer, "Freeing OMX resources");

  TIMM_OSAL_Free (mixer->callbacks);

  g_mutex_lock (&_omx_mutex);
  error = OMX_FreeHandle (mixer->handle);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone)
    goto freehandle;

  return error;

freehandle:
  {
    GST_ERROR_OBJECT (mixer, "Unable to free OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_init_port_memory (GstOmxVideoMixer * mixer)
{
  OMX_PARAM_BUFFER_MEMORYTYPE memory;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxPad *omxpad;
  GList *l;
  guint i;

  for (l = mixer->sinkpads, i = 0; l; l = l->next, i++) {
    omxpad = l->data;
    GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
    memory.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX + i;
    memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;

    GST_DEBUG_OBJECT (omxpad, "Initializing sink pad memory for port %lu",
        memory.nPortIndex);

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetParameter (mixer->handle, OMX_TI_IndexParamBuffMemType, &memory);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      goto memory_failed;
    }
  }

  for (l = mixer->srcpads, i = 0; l; l = l->next, i++) {
    omxpad = l->data;
    GST_OMX_INIT_STRUCT (&memory, OMX_PARAM_BUFFER_MEMORYTYPE);
    memory.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + i;
    memory.eBufMemoryType = OMX_BUFFER_MEMORY_DEFAULT;

    GST_DEBUG_OBJECT (omxpad, "Initializing src pad memory for port %lu",
        memory.nPortIndex);

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetParameter (mixer->handle, OMX_TI_IndexParamBuffMemType, &memory);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      goto memory_failed;
    }
  }

  return error;

memory_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to configure port memory: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_dynamic_configuration (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerPad * mixerpad, guint id)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_CONFIG_VIDCHANNEL_RESOLUTION resolution;

  GST_DEBUG_OBJECT (mixer, "Set input channel %d resolution", id);
  GST_OMX_INIT_STRUCT (&resolution, OMX_CONFIG_VIDCHANNEL_RESOLUTION);
  resolution.Frm0Width = mixerpad->width;
  resolution.Frm0Height = mixerpad->height;
  resolution.Frm0Pitch = mixerpad->stride;
  resolution.Frm1Width = 0;
  resolution.Frm1Height = 0;
  resolution.Frm1Pitch = 0;
  resolution.FrmStartX = mixerpad->in_x;
  resolution.FrmStartY = mixerpad->in_y;
  resolution.FrmCropWidth = mixerpad->crop_width;
  resolution.FrmCropHeight = mixerpad->crop_height;
  resolution.eDir = OMX_DirInput;
  resolution.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX + id;
  resolution.nChId = id;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetConfig (mixer->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexConfigVidChResolution, &resolution);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto chresolution_failed;


  GST_DEBUG_OBJECT (mixer, "Set output channel %d resolution", id);
  GST_OMX_INIT_STRUCT (&resolution, OMX_CONFIG_VIDCHANNEL_RESOLUTION);
  resolution.Frm0Width = mixerpad->out_width;
  resolution.Frm0Height = mixerpad->out_height;
  resolution.Frm0Pitch = mixer->src_stride;
  resolution.Frm1Width = 0;
  resolution.Frm1Height = 0;
  resolution.Frm1Pitch = 0;
  resolution.FrmStartX = mixerpad->out_x * 2;
  resolution.FrmStartY = mixerpad->out_y * 2;
  resolution.FrmCropWidth = 0;
  resolution.FrmCropHeight = 0;
  resolution.eDir = OMX_DirOutput;
  resolution.nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + id;
  resolution.nChId = id;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetConfig (mixer->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexConfigVidChResolution, &resolution);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto chresolution_failed;

  return error;

chresolution_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to change channel %d resolution: %s", id,
        gst_omx_error_to_str (error));
    return error;
  }

}



static OMX_ERRORTYPE
gst_omx_video_mixer_init_ports (GstOmxVideoMixer * mixer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE *port;
  OMX_PARAM_VFPC_NUMCHANNELPERHANDLE channels;
  OMX_CONFIG_ALG_ENABLE enable;
  GstOmxPad *omxpad;
  gchar *portname;
  GList *l;
  guint i;

  gst_omx_video_mixer_create_dummy_sink_pads (mixer);

  error = gst_omx_video_mixer_init_port_memory (mixer);
  if (GST_OMX_FAIL (error))
    goto error;

  for (l = mixer->sinkpads, i = 0; l; l = l->next, i++) {
    GstOmxVideoMixerPad *mixerpad;

    omxpad = l->data;
    mixerpad = GST_OMX_VIDEO_MIXER_PAD (omxpad);

    port = GST_OMX_PAD_PORT (omxpad);
    GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);
    port->nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX + i;
    port->eDir = OMX_DirInput;

    GST_DEBUG_OBJECT (mixerpad, "Initializing sink pad port %lu",
        port->nPortIndex);

    port->format.video.nFrameWidth = mixerpad->width;
    port->format.video.nFrameHeight = mixerpad->height;
    port->format.video.nStride = mixerpad->stride;
    port->format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    port->nBufferSize = (mixerpad->stride * mixerpad->height * 3) / 2;
    port->nBufferCountActual = mixer->input_buffers;
    port->nBufferAlignment = 0;
    port->bBuffersContiguous = 0;

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetParameter (mixer->handle, OMX_IndexParamPortDefinition, port);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      portname = "input";
      goto port_failed;
    }
  }

  for (l = mixer->srcpads, i = 0; l; l = l->next, i++) {
    omxpad = l->data;
    port = GST_OMX_PAD_PORT (omxpad);
    GST_OMX_INIT_STRUCT (port, OMX_PARAM_PORTDEFINITIONTYPE);
    port->nPortIndex = OMX_VFPC_OUTPUT_PORT_START_INDEX + i;
    port->eDir = OMX_DirOutput;

    GST_DEBUG_OBJECT (omxpad, "Initializing src pad port %lu",
        port->nPortIndex);

    port->format.video.nFrameWidth = mixer->src_width;
    port->format.video.nFrameHeight = mixer->src_height;
    port->format.video.nStride = mixer->src_stride;
    port->format.video.eColorFormat = OMX_COLOR_FormatYCbYCr;
    port->nBufferSize = mixer->src_stride * mixer->src_height;
    port->nBufferCountActual = mixer->output_buffers;
    port->nBufferAlignment = 0;
    port->bBuffersContiguous = 0;

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetParameter (mixer->handle, OMX_IndexParamPortDefinition, port);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      portname = "output";
      goto port_failed;
    }

  }

  GST_DEBUG_OBJECT (mixer, "Setting channels per handle");
  GST_OMX_INIT_STRUCT (&channels, OMX_PARAM_VFPC_NUMCHANNELPERHANDLE);
  channels.nNumChannelsPerHandle = mixer->sinkpad_count;

  g_mutex_lock (&_omx_mutex);
  error =
      OMX_SetParameter (mixer->handle,
      (OMX_INDEXTYPE) OMX_TI_IndexParamVFPCNumChPerHandle, &channels);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto channels_failed;

  /* Setting video mixer dinamic configuration */
  for (l = mixer->sinkpads, i = 0; l; l = l->next, i++) {
    GstOmxPad *omxpad = l->data;
    GstOmxVideoMixerPad *mixerpad = GST_OMX_VIDEO_MIXER_PAD (omxpad);

    GST_DEBUG_OBJECT (mixerpad, "Setting dynamic configuration");

    error = gst_omx_video_mixer_dynamic_configuration (mixer, mixerpad, i);
    if (GST_OMX_FAIL (error))
      goto error;

    GST_DEBUG_OBJECT (mixerpad, "Deactivating bypass mode");
    GST_OMX_INIT_STRUCT (&enable, OMX_CONFIG_ALG_ENABLE);
    enable.nPortIndex = OMX_VFPC_INPUT_PORT_START_INDEX + i;
    enable.nChId = i;
    enable.bAlgBypass = OMX_FALSE;

    g_mutex_lock (&_omx_mutex);
    error =
        OMX_SetConfig (mixer->handle,
        (OMX_INDEXTYPE) OMX_TI_IndexConfigAlgEnable, &enable);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto alg_enable_failed;

  }
  return error;

port_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to set %s port parameters", portname);
    return error;
  }
channels_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to set channels per handle");
    return error;
  }
alg_enable_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to enable: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
error:
  {
    return FALSE;
  }
}
