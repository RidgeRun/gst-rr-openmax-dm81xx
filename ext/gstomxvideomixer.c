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

static GstPad *gst_omx_video_mixer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name);
static void gst_omx_video_mixer_release_pad (GstElement * element,
    GstPad * pad);
static gboolean gst_omx_video_mixer_sink_setcaps (GstPad * pad, GstCaps * caps);

static GstStateChangeReturn gst_omx_video_mixer_change_state (GstElement *
    element, GstStateChange transition);

static GstFlowReturn gst_omx_video_mixer_collected (GstCollectPads2 * pads,
    GstOmxVideoMixer * mixer);
static gboolean gst_omx_video_mixer_create_dummy_sink_pads (GstOmxVideoMixer *
    mixer);
static gboolean gst_omx_video_mixer_free_dummy_sink_pads (GstOmxVideoMixer *
    mixer);
static gboolean gst_omx_video_mixer_free_outbuf_check (GstOmxVideoMixer *
    mixer);

static OMX_ERRORTYPE gst_omx_video_mixer_allocate_omx (GstOmxVideoMixer * mixer,
    gchar * handle_name);
static OMX_ERRORTYPE gst_omx_video_mixer_free_omx (GstOmxVideoMixer * mixer);
static OMX_ERRORTYPE gst_omx_video_mixer_init_ports (GstOmxVideoMixer * mixer);
static OMX_ERRORTYPE gst_omx_video_mixer_start (GstOmxVideoMixer * mixer);
static OMX_ERRORTYPE gst_omx_video_mixer_stop (GstOmxVideoMixer * mixer);
static OMX_ERRORTYPE gst_omx_video_mixer_alloc_buffers (GstOmxVideoMixer *
    mixer, GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_video_mixer_free_buffers (GstOmxVideoMixer * mixer,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_video_mixer_push_buffers (GstOmxVideoMixer * mixer,
    GstOmxPad * pad, gpointer data);

static void gst_omx_video_mixer_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

typedef gboolean (*GstOmxVideoMixerCondition) (gpointer, gpointer);
gboolean gst_omx_video_mixer_condition_enabled (gpointer enabled,
    gpointer dummy);
gboolean gst_omx_video_mixer_condition_disabled (gpointer enabled,
    gpointer dummy);
gboolean gst_omx_video_mixer_condition_state (gpointer targetstate,
    gpointer currentstate);
OMX_ERRORTYPE gst_omx_video_mixer_wait_for_condition (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerCondition condition, gpointer arg1, gpointer arg2);

typedef OMX_ERRORTYPE (*GstOmxVideoMixerPadFunc) (GstOmxVideoMixer *,
    GstOmxPad *, gpointer);
static OMX_ERRORTYPE gst_omx_video_mixer_for_each_pad (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerPadFunc func, GstPadDirection direction, gpointer data);
static OMX_ERRORTYPE gst_omx_video_mixer_enable_pad (GstOmxVideoMixer * mixer,
    GstOmxPad * pad, gpointer data);

static gboolean gst_omx_video_mixer_create_push_task (GstOmxVideoMixer * mixer);
static gboolean gst_omx_video_mixer_start_push_task (GstOmxVideoMixer * mixer);
static gboolean gst_omx_video_mixer_stop_push_task (GstOmxVideoMixer * mixer);
static gboolean gst_omx_video_mixer_destroy_push_task (GstOmxVideoMixer *
    mixer);
static void gst_omx_video_mixer_out_push_loop (void *data);
static gboolean gst_omx_video_mixer_clear_queue (GstOmxVideoMixer * mixer);

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
  mixer->closing = FALSE;

  mixer->input_buffers = DEFAULT_VIDEO_MIXER_NUM_INPUT_BUFFERS;
  mixer->output_buffers = DEFAULT_VIDEO_MIXER_NUM_OUTPUT_BUFFERS;
  mixer->sinkpads = NULL;
  mixer->srcpads = NULL;
  mixer->out_count = NULL;
  mixer->out_ptr_list = NULL;

  g_mutex_init (&mixer->waitmutex);
  g_cond_init (&mixer->waitcond);

  mixer->collect = gst_collect_pads2_new ();
  gst_collect_pads2_set_function (mixer->collect, (GstCollectPads2Function)
      GST_DEBUG_FUNCPTR (gst_omx_video_mixer_collected), mixer);

  mixer->queue_buffers = gst_omx_buf_queue_new ();
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

  g_mutex_clear (&mixer->waitmutex);
  g_cond_clear (&mixer->waitcond);

  gst_object_unref (mixer->collect);

  gst_omx_buf_queue_free (mixer->queue_buffers);

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

      if (!gst_omx_video_mixer_create_push_task (mixer))
        goto task_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_LOG_OBJECT (mixer, "Starting collectpads");
      gst_collect_pads2_start (mixer->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_LOG_OBJECT (mixer, "Stopping collectpads");
      mixer->closing = TRUE;
      if (!gst_omx_video_mixer_stop_push_task (mixer))
        goto task_failed;
      gst_collect_pads2_stop (mixer->collect);
      gst_omx_video_mixer_clear_queue (mixer);
      gst_omx_video_mixer_stop (mixer);
      mixer->started = FALSE;
      gst_omx_video_mixer_free_dummy_sink_pads (mixer);
      gst_omx_video_mixer_free_outbuf_check (mixer);

      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);


  switch (transition) {

    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_omx_video_mixer_free_omx (mixer);

      if (!gst_omx_video_mixer_destroy_push_task (mixer))
        goto task_failed;
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
task_failed:
  {
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

static gboolean
gst_omx_video_mixer_init_outbuf_check (GstOmxVideoMixer * mixer)
{
  OMX_BUFFERHEADERTYPE *omxbuf;
  GstOmxPad *omxpad;
  GList *bufferlist, *b, *l;
  guint numbufs, numports;
  guint i, j;

  numbufs = mixer->output_buffers;
  numports = mixer->sinkpad_count;

  /* Initialize buffers count to 0 */
  mixer->out_count = g_malloc (numbufs * sizeof (guint));
  memset (mixer->out_count, 0, numbufs * sizeof (guint));

  /* Allocate matrix to hold the omx output buffers
   * arranged by index */
  mixer->out_ptr_list = g_malloc (numbufs * sizeof (OMX_BUFFERHEADERTYPE *));
  for (i = 0; i < numbufs; i++) {
    mixer->out_ptr_list[i] =
        g_malloc (numports * sizeof (OMX_BUFFERHEADERTYPE *));
  }

  /* Initialize matrix with pointers to the omx output buffers */
  for (l = mixer->srcpads, j = 0; l; l = l->next, j++) {
    omxpad = l->data;
    bufferlist = g_list_last (omxpad->buffers->table);
    for (b = bufferlist, i = 0; i < numbufs; i++, b = g_list_previous (b)) {
      omxbuf = ((GstOmxBufTabNode *) b->data)->buffer;
      mixer->out_ptr_list[i][j] = omxbuf;
    }
  }

  return TRUE;
}

static gboolean
gst_omx_video_mixer_free_outbuf_check (GstOmxVideoMixer * mixer)
{
  guint numbufs, numports;
  guint i;

  numbufs = mixer->output_buffers;
  numports = mixer->sinkpad_count;

  if (mixer->out_ptr_list) {
    for (i = 0; i < numbufs; i++) {
      g_free (mixer->out_ptr_list[i]);
    }

    g_free (mixer->out_ptr_list);
    mixer->out_ptr_list = NULL;
  }

  if (mixer->out_count) {
    g_free (mixer->out_count);
    mixer->out_count = NULL;
  }

  return TRUE;
}

static GstFlowReturn
gst_omx_video_mixer_collected (GstCollectPads2 * pads, GstOmxVideoMixer * mixer)
{
  OMX_BUFFERHEADERTYPE *omxbuf;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstFlowReturn ret = GST_FLOW_OK;
  GstOmxBufferData *bufdata = NULL;
  GstOmxPad *omxpad;
  GstBuffer *buffer;
  GSList *l;

  if (!mixer->started) {

    if (!gst_omx_video_mixer_update_src_caps (mixer))
      goto caps_failed;

    if (GST_OMX_FAIL (gst_omx_video_mixer_init_ports (mixer)))
      goto init_ports_failed;

    if (GST_OMX_FAIL (gst_omx_video_mixer_start (mixer)))
      goto start_failed;

    gst_omx_video_mixer_init_outbuf_check (mixer);

    if (!gst_omx_video_mixer_start_push_task (mixer))
      goto task_failed;

    GST_OBJECT_LOCK (mixer);
    mixer->started = TRUE;
    GST_OBJECT_UNLOCK (mixer);
  }

  GST_DEBUG_OBJECT (mixer, "Entering collected");
  for (l = mixer->collect->data; l; l = l->next) {
    GstCollectData2 *data;

    data = (GstCollectData2 *) l->data;
    buffer = gst_collect_pads2_pop (mixer->collect, data);

    if (buffer) {
      omxpad = GST_OMX_PAD (data->pad);

      GST_LOG_OBJECT (omxpad, "Got buffer %p with timestamp %" GST_TIME_FORMAT,
          buffer, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

      GST_LOG_OBJECT (mixer, "Not an OMX buffer, requesting a free buffer");
      error = gst_omx_buf_tab_get_free_buffer (omxpad->buffers, &omxbuf);
      if (GST_OMX_FAIL (error))
        goto free_buffer_failed;
      gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);
      GST_LOG_OBJECT (mixer, "Received buffer %p, copying data", omxbuf);
      memcpy (omxbuf->pBuffer, GST_BUFFER_DATA (buffer),
          GST_BUFFER_SIZE (buffer));

      omxbuf->nFilledLen = GST_BUFFER_SIZE (buffer);
      omxbuf->nOffset = 0;
      omxbuf->nTimeStamp = GST_BUFFER_TIMESTAMP (buffer);

      bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;
      bufdata->buffer = buffer;

      GST_LOG_OBJECT (mixer, "Emptying buffer %d %p %p->%p", bufdata->id,
          bufdata, omxbuf, omxbuf->pBuffer);
      g_mutex_lock (&_omx_mutex);
      error = mixer->component->EmptyThisBuffer (mixer->handle, omxbuf);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error)) {
        goto empty_error;
      }

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
start_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to start omx component");
    return GST_FLOW_ERROR;
  }
task_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to start output task");
    return GST_FLOW_ERROR;
  }
free_buffer_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to get a free buffer: %s",
        gst_omx_error_to_str (error));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
empty_error:
  {
    GST_ELEMENT_ERROR (mixer, LIBRARY, ENCODE, (gst_omx_error_to_str (error)),
        (NULL));
    gst_buffer_unref (buffer);  /*If Empty this buffer is not successful we have to unref the buffer manually */
    return GST_FLOW_ERROR;
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

  if (!mixer->srcpads)
    return TRUE;

  GST_DEBUG_OBJECT (mixer, "Freeing dummy sink pads");

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
    case OMX_EventCmdComplete:
      GST_INFO_OBJECT (mixer,
          "OMX command complete event received: %s (%s) (%d)",
          gst_omx_cmd_to_str (nevent1),
          OMX_CommandStateSet ==
          nevent1 ? gst_omx_state_to_str (nevent2) : "No debug", nevent2);

      if (OMX_CommandPortEnable == nevent1) {
        g_mutex_lock (&mixer->waitmutex);
        gst_omx_video_mixer_for_each_pad (mixer, gst_omx_video_mixer_enable_pad,
            GST_PAD_UNKNOWN, (gpointer) nevent2);
        g_cond_signal (&mixer->waitcond);
        g_mutex_unlock (&mixer->waitmutex);
      }

      if (OMX_CommandStateSet == nevent1) {
        g_mutex_lock (&mixer->waitmutex);
        mixer->state = nevent2;
        g_cond_signal (&mixer->waitcond);
        g_mutex_unlock (&mixer->waitmutex);
      }
      break;
    case OMX_EventError:
      GST_ERROR_OBJECT (mixer, "OMX error event received: %s",
          gst_omx_error_to_str (nevent1));
      break;
  }
  return error;
}


static OMX_ERRORTYPE
gst_omx_video_mixer_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (data);
  OMX_BUFFERHEADERTYPE *omxbuf;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBufferData *bufdata;
  gboolean busy;

  bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;

  if (mixer->closing)
    goto discard;

  GST_LOG_OBJECT (mixer, "Fill buffer callback for buffer %p->%p", outbuf,
      outbuf->pBuffer);

  /* Find buffer and mark it as busy */
  gst_omx_buf_tab_find_buffer (bufdata->pad->buffers, outbuf, &omxbuf, &busy);
  if (busy)
    goto illegal;

  gst_omx_buf_tab_use_buffer (bufdata->pad->buffers, outbuf);

  /* Increase buffer count to the bufdata->id index */
  mixer->out_count[bufdata->id]++;

  /* When every port has returned a buffer with index bufdata->id,
   * the output buffer mosaic is complete, so push the buffer to 
   * the output queue in order to be send donwstream */
  if (mixer->out_count[bufdata->id] >= mixer->sinkpad_count) {
    omxbuf = mixer->out_ptr_list[bufdata->id][0];
    error = gst_omx_buf_queue_push_buffer (mixer->queue_buffers, omxbuf);
  }

  return error;

discard:
  {
    GST_DEBUG_OBJECT (mixer, "Discarding buffer %d", bufdata->id);
    return error;
  }
illegal:
  {
    GST_ERROR_OBJECT (mixer,
        "Double fill callback for buffer %p->%p, this should not happen",
        outbuf, outbuf->pBuffer);
    return error;
  }

}


static OMX_ERRORTYPE
gst_omx_video_mixer_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (data);
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  GstBuffer *gstbuf = bufdata->buffer;
  GstOmxPad *pad = bufdata->pad;
  guint8 id = bufdata->id;
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_LOG_OBJECT (mixer, "Empty buffer callback for buffer %d %p->%p->%p", id,
      buffer, buffer->pBuffer, bufdata);

  bufdata->buffer = NULL;
  /*We need to return the buffer first in order to avoid race condition */
  error = gst_omx_buf_tab_return_buffer (pad->buffers, buffer);
  if (GST_OMX_FAIL (error))
    goto noreturn;

  gst_buffer_unref (gstbuf);
  return error;

noreturn:
  {
    GST_ELEMENT_ERROR (mixer, LIBRARY, ENCODE,
        ("Unable to return buffer to buftab: %s",
            gst_omx_error_to_str (error)), (NULL));
    buffer->pAppPrivate = NULL;
    return error;
  }
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
  mixer->callbacks->EmptyBufferDone =
      (GstOmxEmptyBufferDone) gst_omx_video_mixer_empty_callback;
  mixer->callbacks->FillBufferDone =
      (GstOmxFillBufferDone) gst_omx_video_mixer_fill_callback;

  if (!handle_name) {
    error = OMX_ErrorInvalidComponentName;
    goto nohandlename;
  }

  g_mutex_lock (&_omx_mutex);
  error = OMX_GetHandle (&mixer->handle, handle_name, mixer, mixer->callbacks);
  g_mutex_unlock (&_omx_mutex);
  if ((error != OMX_ErrorNone) || (!mixer->handle))
    goto nohandle;

  mixer->component = (OMX_COMPONENTTYPE *) mixer->handle;

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
gst_omx_video_mixer_enable_ports (GstOmxVideoMixer * mixer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxPad *omxpad;
  GList *l;
  gint i, index;

  for (l = mixer->sinkpads, i = 0; l; l = l->next, i++) {
    omxpad = GST_OMX_PAD (l->data);
    index = OMX_VFPC_INPUT_PORT_START_INDEX + i;
    g_mutex_lock (&_omx_mutex);
    OMX_SendCommand (mixer->handle, OMX_CommandPortEnable, index, NULL);
    g_mutex_unlock (&_omx_mutex);

    GST_DEBUG_OBJECT (mixer, "Waiting for input port %d to enable", index);
    error = gst_omx_video_mixer_wait_for_condition (mixer,
        gst_omx_video_mixer_condition_enabled,
        (gpointer) & omxpad->enabled, NULL);
    if (GST_OMX_FAIL (error))
      goto enable_failed;

  }
  for (l = mixer->srcpads, i = 0; l; l = l->next, i++) {

    omxpad = GST_OMX_PAD (l->data);
    index = OMX_VFPC_OUTPUT_PORT_START_INDEX + i;
    g_mutex_lock (&_omx_mutex);
    OMX_SendCommand (mixer->handle, OMX_CommandPortEnable, index, NULL);
    g_mutex_unlock (&_omx_mutex);

    GST_DEBUG_OBJECT (mixer, "Waiting for output port %d to enable", index);
    error = gst_omx_video_mixer_wait_for_condition (mixer,
        gst_omx_video_mixer_condition_enabled,
        (gpointer) & omxpad->enabled, NULL);
    if (GST_OMX_FAIL (error))
      goto enable_failed;

  }
  return error;

enable_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to enable port");
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

  if (mixerpad->crop_width == 0) {
    mixerpad->crop_width = mixerpad->width;
  }

  if (mixerpad->crop_height == 0) {
    mixerpad->crop_height = mixerpad->height;
  }

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
  resolution.FrmStartY = mixerpad->out_y;
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

  GST_DEBUG_OBJECT (mixer, "Enabling mixer ports");
  error = gst_omx_video_mixer_enable_ports (mixer);
  if (GST_OMX_FAIL (error))
    goto error;

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


static OMX_ERRORTYPE
gst_omx_video_mixer_get_peer_buffer_list (GstOmxVideoMixer * mixer,
    GstOmxPad * pad, GList ** bufferlist, OMX_BUFFERHEADERTYPE * omxpeerbuffer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBufferData *peerbufdata = NULL;
  GstOmxPad *peerpad = NULL;
  guint numbufs = pad->port->nBufferCountActual;

  peerbufdata = (GstOmxBufferData *) omxpeerbuffer->pAppPrivate;
  peerpad = peerbufdata->pad;

  if (numbufs > peerpad->port->nBufferCountActual) {
    error = OMX_ErrorInsufficientResources;
    goto nouse;
  }
  *bufferlist = g_list_last (peerpad->buffers->table);
  if (!*bufferlist) {
    g_print ("NULL pointer\n");
  }
  return error;

nouse:
  {
    GST_ERROR_OBJECT (mixer,
        "Not enough buffers provided by the peer, can't share buffers: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_alloc_buffers (GstOmxVideoMixer * mixer, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer = NULL;
  GstOmxBufferData *bufdata = NULL;
  GList *peerbuffers = NULL;
  guint32 size = 0;
  guint i;

  if (pad->buffers->table != NULL) {
    GST_DEBUG_OBJECT (mixer, "Ignoring buffers allocation for %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }

  if (data) {
    OMX_BUFFERHEADERTYPE *omxpeerbuffer = (OMX_BUFFERHEADERTYPE *) data;
    error =
        gst_omx_video_mixer_get_peer_buffer_list (mixer, pad, &peerbuffers,
        omxpeerbuffer);
    if (error != OMX_ErrorNone)
      goto out;
  }

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {
    bufdata = (GstOmxBufferData *) g_malloc (sizeof (GstOmxBufferData));
    bufdata->pad = pad;
    bufdata->buffer = NULL;
    bufdata->id = i;


    if (!data) {
      size = GST_OMX_PAD_PORT (pad)->nBufferSize;

      g_mutex_lock (&_omx_mutex);
      error = OMX_AllocateBuffer (mixer->handle, &buffer,
          GST_OMX_PAD_PORT (pad)->nPortIndex, bufdata, size);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error))
        goto noalloc;

      GST_DEBUG_OBJECT (pad, "Allocated buffer number %u: %p->%p", i, buffer,
          buffer->pBuffer);

    } else {
      OMX_BUFFERHEADERTYPE *omxpeerbuffer = NULL;
      gpointer pbuffer;

      omxpeerbuffer = ((GstOmxBufTabNode *) peerbuffers->data)->buffer;
      peerbuffers = g_list_previous (peerbuffers);

      size = omxpeerbuffer->nAllocLen;
      pbuffer = omxpeerbuffer->pBuffer;

      g_mutex_lock (&_omx_mutex);
      error = OMX_UseBuffer (mixer->handle, &buffer,
          GST_OMX_PAD_PORT (pad)->nPortIndex, bufdata, size, pbuffer);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error))
        goto nouse;
      GST_DEBUG_OBJECT (pad, "Use buffer number %u:"
          "%p->%p", bufdata->id, buffer, pbuffer);
    }

    error = gst_omx_buf_tab_add_buffer (pad->buffers, buffer);
    if (GST_OMX_FAIL (error))
      goto addbuffer;
  }

out:
  return error;

nouse:
  {
    GST_ERROR_OBJECT (mixer, "Unable to use buffer provided by downstream: %s",
        gst_omx_error_to_str (error));

    g_free (bufdata);
    return error;
  }
noalloc:
  {
    GST_ERROR_OBJECT (mixer, "Failed to allocate buffers");
    g_free (bufdata);
    /*TODO: should I free buffers? */
    return error;
  }
addbuffer:
  {
    GST_ERROR_OBJECT (mixer, "Unable to add the buffer to the buftab");
    g_free (bufdata);
    /*TODO: should I free buffers? */
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_free_buffers (GstOmxVideoMixer * mixer, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GstOmxBufTabNode *node;
  guint i;
  GList *buffers;

  buffers = pad->buffers->table;

  /* No buffers allocated yet */
  if (!buffers)
    return error;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    if (!buffers)
      goto short_read;

    node = (GstOmxBufTabNode *) buffers->data;
    buffer = node->buffer;

    GST_DEBUG_OBJECT (mixer, "Freeing %s:%s buffer number %u: %p",
        GST_DEBUG_PAD_NAME (pad), i, buffer);

    error = gst_omx_buf_tab_remove_buffer (pad->buffers, buffer);
    if (GST_OMX_FAIL (error))
      goto not_in_table;

    /* Resync list */
    buffers = pad->buffers->table;

    g_free (buffer->pAppPrivate);
    g_mutex_lock (&_omx_mutex);
    error = OMX_FreeBuffer (mixer->handle, GST_OMX_PAD_PORT (pad)->nPortIndex,
        buffer);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto free_failed;
  }

  GST_OBJECT_LOCK (pad);
  pad->enabled = FALSE;
  GST_OBJECT_UNLOCK (pad);

  return error;

short_read:
  {
    GST_ERROR_OBJECT (mixer, "Malformed output buffer list");
    /*TODO: should I free buffers? */
    return OMX_ErrorResourcesLost;
  }
not_in_table:
  {
    GST_ERROR_OBJECT (mixer, "The buffer list for %s:%s is malformed: %s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)), gst_omx_error_to_str (error));
    return error;
  }
free_failed:
  {
    GST_ERROR_OBJECT (mixer, "Error freeing buffers on %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_start (GstOmxVideoMixer * mixer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *peerbuffer;
  GstOmxBufTabNode *node;
  GstOmxPad *omxpad;

  if (mixer->started)
    goto already_started;

  GST_INFO_OBJECT (mixer, "Sending handle to Idle");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (mixer->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto idle_failed;

  GST_INFO_OBJECT (mixer, "Allocating buffers for src port");
  error =
      gst_omx_video_mixer_alloc_buffers (mixer, GST_OMX_PAD (mixer->srcpad),
      NULL);
  if (GST_OMX_FAIL (error))
    goto alloc_failed;

  omxpad = GST_OMX_PAD (mixer->srcpad);
  node = omxpad->buffers->table->data;
  peerbuffer = node->buffer;
  error =
      gst_omx_video_mixer_for_each_pad (mixer,
      gst_omx_video_mixer_alloc_buffers, GST_PAD_SRC, peerbuffer);
  if (GST_OMX_FAIL (error))
    goto alloc_failed;

  GST_INFO_OBJECT (mixer, "Allocating buffers for sink ports");
  error =
      gst_omx_video_mixer_for_each_pad (mixer,
      gst_omx_video_mixer_alloc_buffers, GST_PAD_SINK, NULL);
  if (GST_OMX_FAIL (error))
    goto alloc_failed;

  GST_INFO_OBJECT (mixer, "Waiting for handle to become Idle");
  error = gst_omx_video_mixer_wait_for_condition (mixer,
      gst_omx_video_mixer_condition_state, (gpointer) OMX_StateIdle,
      (gpointer) & mixer->state);
  if (GST_OMX_FAIL (error))
    goto idle_failed;

  GST_INFO_OBJECT (mixer, "Sending handle to Executing");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (mixer->handle, OMX_CommandStateSet,
      OMX_StateExecuting, NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto exec_failed;

  GST_INFO_OBJECT (mixer, "Waiting for handle to become Executing");
  error = gst_omx_video_mixer_wait_for_condition (mixer,
      gst_omx_video_mixer_condition_state, (gpointer) OMX_StateExecuting,
      (gpointer) & mixer->state);
  if (GST_OMX_FAIL (error))
    goto exec_failed;

  GST_INFO_OBJECT (mixer, "Pushing output buffers");
  error =
      gst_omx_video_mixer_for_each_pad (mixer, gst_omx_video_mixer_push_buffers,
      GST_PAD_SRC, NULL);
  if (GST_OMX_FAIL (error))
    goto push_failed;

  mixer->started = TRUE;

already_started:
  {
    GST_WARNING_OBJECT (mixer, "Component already started");
    return error;
  }
idle_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to set component to Idle");
    return error;
  }
alloc_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to allocate resources for buffers");
    return error;
  }
exec_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to set component to Executing");
    return error;
  }
push_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to push buffer into the output port");
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_video_mixer_stop (GstOmxVideoMixer * mixer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (!mixer->started)
    goto already_stopped;

  GST_INFO_OBJECT (mixer, "Sending handle to Idle");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (mixer->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto idle_failed;

  GST_INFO_OBJECT (mixer, "Waiting for handle to become Idle");
  error = gst_omx_video_mixer_wait_for_condition (mixer,
      gst_omx_video_mixer_condition_state, (gpointer) OMX_StateIdle,
      (gpointer) & mixer->state);
  if (GST_OMX_FAIL (error))
    goto idle_failed;

  GST_INFO_OBJECT (mixer, "Sending handle to Loaded");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (mixer->handle, OMX_CommandStateSet, OMX_StateLoaded,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto loaded_failed;

  GST_INFO_OBJECT (mixer, "Freeing port buffers");
  error =
      gst_omx_video_mixer_for_each_pad (mixer, gst_omx_video_mixer_free_buffers,
      GST_PAD_UNKNOWN, NULL);
  if (GST_OMX_FAIL (error))
    goto free_failed;

  GST_INFO_OBJECT (mixer, "Waiting for handle to become Loaded");
  error = gst_omx_video_mixer_wait_for_condition (mixer,
      gst_omx_video_mixer_condition_state, (gpointer) OMX_StateLoaded,
      (gpointer) & mixer->state);
  if (GST_OMX_FAIL (error))
    goto loaded_failed;

  return error;

already_stopped:
  {
    GST_WARNING_OBJECT (mixer, "Component already stopped");
    return error;
  }
idle_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to set component to idle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
loaded_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to set component to loaded: %s",
        gst_omx_error_to_str (error));
    return error;
  }
free_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to free buffers: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_video_mixer_push_buffers (GstOmxVideoMixer * mixer, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GstOmxBufTabNode *node;
  guint i;
  GList *buffers;

  if (GST_PAD_SINK == GST_PAD (pad)->direction)
    goto sinkpad;

  buffers = pad->buffers->table;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    if (!buffers)
      goto short_read;

    node = (GstOmxBufTabNode *) buffers->data;
    buffer = node->buffer;

    GST_DEBUG_OBJECT (pad, "Pushing buffer number %u: %p of size %d", i,
        buffer, (int) buffer->nAllocLen);

    g_mutex_lock (&_omx_mutex);
    error = mixer->component->FillThisBuffer (mixer->handle, buffer);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto push_failed;

    buffers = g_list_next (buffers);
  }

  return error;

sinkpad:
  {
    GST_DEBUG_OBJECT (mixer, "Skipping sink pad %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }
push_failed:
  {
    GST_ERROR_OBJECT (mixer, "Failed to push buffers");
    /*TODO: should I free buffers? */
    return error;
  }
short_read:
  {
    GST_ERROR_OBJECT (mixer, "Malformed output buffer list");
    /*TODO: should I free buffers? */
    return OMX_ErrorResourcesLost;
  }
}

/* Conditionals and control implementation*/
OMX_ERRORTYPE
gst_omx_video_mixer_wait_for_condition (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerCondition condition, gpointer arg1, gpointer arg2)
{
  guint64 endtime;

  g_mutex_lock (&mixer->waitmutex);

  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  while (!condition (arg1, arg2))
    if (!g_cond_wait_until (&mixer->waitcond, &mixer->waitmutex, endtime))
      goto timeout;

  GST_DEBUG_OBJECT (mixer, "Wait for condition successful");
  g_mutex_unlock (&mixer->waitmutex);

  return OMX_ErrorNone;

timeout:
  {
    GST_WARNING_OBJECT (mixer, "Wait for condition timed out");
    g_mutex_unlock (&mixer->waitmutex);
    return OMX_ErrorTimeout;
  }
}

gboolean
gst_omx_video_mixer_condition_enabled (gpointer enabled, gpointer dummy)
{
  return *(gboolean *) enabled;
}

gboolean
gst_omx_video_mixer_condition_disabled (gpointer enabled, gpointer dummy)
{
  return !*(gboolean *) enabled;
}

gboolean
gst_omx_video_mixer_condition_state (gpointer targetstate,
    gpointer currentstate)
{
  OMX_STATETYPE _targetstate = (OMX_STATETYPE) targetstate;
  OMX_STATETYPE _currentstate = *(OMX_STATETYPE *) currentstate;

  return _targetstate == _currentstate;
}

static OMX_ERRORTYPE
gst_omx_video_mixer_for_each_pad (GstOmxVideoMixer * mixer,
    GstOmxVideoMixerPadFunc func, GstPadDirection direction, gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstPad *pad;
  GList *l;

  if (direction == GST_PAD_SRC || direction == GST_PAD_UNKNOWN) {
    for (l = mixer->srcpads; l; l = l->next) {
      pad = l->data;
      error = func (mixer, GST_OMX_PAD (pad), data);
      if (GST_OMX_FAIL (error))
        goto failed;
    }
  }

  if (direction == GST_PAD_SINK || direction == GST_PAD_UNKNOWN) {
    for (l = mixer->sinkpads; l; l = l->next) {
      pad = l->data;
      error = func (mixer, GST_OMX_PAD (pad), data);
      if (GST_OMX_FAIL (error))
        goto failed;
    }
  }

  return error;

failed:
  {
    GST_ERROR_OBJECT (mixer, "Iterator failed on pad: %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_video_mixer_enable_pad (GstOmxVideoMixer * mixer, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  guint32 padidx = (guint32) data;

  if (padidx == GST_OMX_PAD_PORT (pad)->nPortIndex) {
    GST_INFO_OBJECT (mixer, "Enabling port %s:%s", GST_DEBUG_PAD_NAME (pad));
    pad->enabled = TRUE;
  }

  return error;
}

/* Output tasks*/
static gboolean
gst_omx_video_mixer_create_push_task (GstOmxVideoMixer * mixer)
{
  GST_INFO_OBJECT (mixer, "Creating Push task...");
  mixer->pushtask =
      gst_task_create (gst_omx_video_mixer_out_push_loop, (gpointer) mixer);

  if (!mixer->pushtask) {
    GST_ERROR_OBJECT (mixer, "Failed to create Push task");
    return FALSE;
  }

  g_static_rec_mutex_init (&mixer->taskmutex);
  gst_task_set_lock (mixer->pushtask, &mixer->taskmutex);
  GST_INFO_OBJECT (mixer, "Push task created");
  return TRUE;
}

static gboolean
gst_omx_video_mixer_start_push_task (GstOmxVideoMixer * mixer)
{
  gst_omx_buf_queue_release (mixer->queue_buffers, FALSE);

  GST_INFO_OBJECT (mixer, "Starting push task... ");
  if (!gst_task_start (mixer->pushtask)) {
    GST_WARNING_OBJECT (mixer, "Failed to start push task");
    return FALSE;
  }

  GST_INFO_OBJECT (mixer, "Push task started");
  return TRUE;
}

static gboolean
gst_omx_video_mixer_stop_push_task (GstOmxVideoMixer * mixer)
{
  GST_INFO_OBJECT (mixer, "Stopping task on srcpad...");

  gst_omx_buf_queue_release (mixer->queue_buffers, TRUE);

  if (!gst_task_join (mixer->pushtask)) {
    GST_WARNING_OBJECT (mixer, "Failed stop task ");
    return FALSE;
  }

  GST_INFO_OBJECT (mixer, "Finished push task");

  return TRUE;
}

static gboolean
gst_omx_video_mixer_destroy_push_task (GstOmxVideoMixer * mixer)
{
  GST_INFO_OBJECT (mixer, "Stopping task on srcpad...");

  if (gst_task_get_state (mixer->pushtask) != GST_TASK_STOPPED) {
    gst_omx_video_mixer_stop_push_task (mixer);
    return FALSE;
  }
  GST_INFO_OBJECT (mixer, "Unref push task");
  gst_object_unref (mixer->pushtask);
  GST_INFO_OBJECT (mixer, "Finished task on srcpad");

  return TRUE;
}

void
gst_omx_video_mixer_release_buffer (gpointer data)
{

  OMX_ERRORTYPE error;
  OMX_BUFFERHEADERTYPE *omxbuf = (OMX_BUFFERHEADERTYPE *) data;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;
  GstOmxPad *omxpad = bufdata->pad;
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (GST_OBJECT_PARENT (omxpad));
  guint i, bufid;

  bufid = bufdata->id;

  /* Reset output buffer count for buffers with index bufid */
  mixer->out_count[bufid] = 0;

  /* Marks as free and return to the omx component the buffer
   * with index bufid for each output port */
  for (i = 0; i < mixer->sinkpad_count; i++) {
    omxbuf = mixer->out_ptr_list[bufid][i];
    omxpad = ((GstOmxBufferData *) omxbuf->pAppPrivate)->pad;

    GST_LOG_OBJECT (omxpad, "Returning buffer %p to table", omxbuf);

    error = gst_omx_buf_tab_return_buffer (omxpad->buffers, omxbuf);
    if (GST_OMX_FAIL (error))
      goto buftab_failed;

    g_mutex_lock (&_omx_mutex);
    error = mixer->component->FillThisBuffer (mixer->handle, omxbuf);
    if (GST_OMX_FAIL (error))
      goto fill_failed;
    g_mutex_unlock (&_omx_mutex);
  }

  return;

buftab_failed:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (mixer), LIBRARY, ENCODE,
        ("Malformed buffer list"), (NULL));
    return;
  }
fill_failed:
  {
    GST_ERROR_OBJECT (mixer, "Unable to reuse output buffer: %s",
        gst_omx_error_to_str (error));
  }
}

static void
gst_omx_video_mixer_out_push_loop (void *data)
{
  GstOmxVideoMixer *mixer = GST_OMX_VIDEO_MIXER (data);
  OMX_BUFFERHEADERTYPE *omxbuf = NULL;
  GstOmxBufferData *bufdata = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;

  gboolean closing;

  GST_LOG_OBJECT (mixer, "Entering push task");

  GST_OBJECT_LOCK (mixer);
  closing = mixer->closing;
  GST_OBJECT_UNLOCK (mixer);

  if (closing) {
    goto discard;
  }

  caps = gst_pad_get_negotiated_caps (mixer->srcpad);
  if (!caps)
    goto no_caps;

  /* Obtain processed buffer */
  omxbuf = gst_omx_buf_queue_pop_buffer_check_release (mixer->queue_buffers);
  if (!omxbuf) {
    goto timeout;
  }
  bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;

  /* Prepare gstreamer buffer */
  buffer = gst_buffer_new ();
  if (!buffer)
    goto alloc_failed;

  GST_BUFFER_SIZE (buffer) =
      GST_OMX_PAD_PORT (GST_OMX_PAD (mixer->srcpad))->nBufferSize;
  GST_BUFFER_CAPS (buffer) = caps;
  GST_BUFFER_DATA (buffer) = omxbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (buffer) = (guint8 *) omxbuf;
  GST_BUFFER_FREE_FUNC (buffer) = gst_omx_video_mixer_release_buffer;

  GST_BUFFER_TIMESTAMP (buffer) = omxbuf->nTimeStamp;
  GST_BUFFER_FLAG_SET (buffer, GST_OMX_BUFFER_FLAG);
  bufdata->buffer = buffer;

  GST_LOG_OBJECT (mixer, "Pushing buffer %p->%p to %s:%s",
      omxbuf, omxbuf->pBuffer, GST_DEBUG_PAD_NAME (mixer->srcpad));
  ret = gst_pad_push (mixer->srcpad, buffer);
  if (GST_FLOW_OK != ret)
    goto push_failed;

  return;

discard:
  {
    GST_INFO_OBJECT (mixer, "Discarding buffer closing");
    return;
  }
no_caps:
  {
    GST_ERROR_OBJECT (mixer, "Unable get caps from pad");
    return;
  }
timeout:
  {
    GST_ERROR_OBJECT (mixer, "Cannot acquire output buffer from pending queue");
    return;
  }
alloc_failed:
  {
    GST_ERROR_OBJECT (mixer,
        "Unable to allocate gstreamer buffer, drop omx buffer");
    gst_omx_video_mixer_release_buffer (omxbuf);
    return;
  }

push_failed:
  {
    GST_WARNING_OBJECT (mixer, "Failed to push buffer dowstream (id:%d): %s",
        bufdata->id, gst_flow_get_name (ret));
    return;
  }

}

static gboolean
gst_omx_video_mixer_clear_queue (GstOmxVideoMixer * mixer)
{

  OMX_BUFFERHEADERTYPE *omxbuf = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBufferData *bufdata = NULL;
  GstOmxPad *omxpad = NULL;
  guint i, bufid;

  /* Drop the buffers remaining in the output queue */
  omxbuf = gst_omx_buf_queue_pop_buffer_no_wait (mixer->queue_buffers);
  while (omxbuf) {
    bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;
    bufid = bufdata->id;

    for (i = 0; i < mixer->sinkpad_count; i++) {
      omxbuf = mixer->out_ptr_list[bufid][i];
      omxpad = ((GstOmxBufferData *) omxbuf->pAppPrivate)->pad;
      GST_LOG_OBJECT (omxpad, "Dropping buffer %d %p %p->%p", bufdata->id,
          bufdata, omxbuf, omxbuf->pBuffer);
      error = gst_omx_buf_tab_return_buffer (omxpad->buffers, omxbuf);
    }
    omxbuf = gst_omx_buf_queue_pop_buffer_no_wait (mixer->queue_buffers);
  }
  GST_INFO_OBJECT (mixer, " Pushed Queue empty");
  return error;
}
