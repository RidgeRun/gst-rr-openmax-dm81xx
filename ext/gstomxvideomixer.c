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

GST_BOILERPLATE (GstOmxVideoMixer, gst_omx_video_mixer, GstElement,
    GST_TYPE_ELEMENT);

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
static GstStateChangeReturn gst_omx_video_mixer_change_state (GstElement *
    element, GstStateChange transition);

static GstFlowReturn gst_omx_video_mixer_collected (GstCollectPads2 * pads,
    GstOmxVideoMixer * mixer);


static OMX_ERRORTYPE gst_omx_video_mixer_allocate_omx (GstOmxVideoMixer * this,
    gchar * handle_name);
static OMX_ERRORTYPE gst_omx_video_mixer_free_omx (GstOmxVideoMixer * mixer);

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
  GstOmxPad *omxpad;
  gchar *name;

  mixer = GST_OMX_VIDEO_MIXER (element);

  if (templ != gst_element_class_get_pad_template (klass, "sink%d"))
    return NULL;

  name = g_strdup_printf ("sink%d", mixer->next_sinkpad++);
  omxpad = gst_omx_pad_new_from_template (templ, name);
  g_free (name);

  gst_collect_pads2_add_pad (mixer->collect, GST_PAD (omxpad),
      sizeof (GstCollectData2));

  mixer->sinkpads = g_list_append (mixer->sinkpads, omxpad);
  mixer->sinkpad_count++;

  GST_DEBUG_OBJECT (element, "Adding pad %s", GST_PAD_NAME (omxpad));
  gst_element_add_pad (element, GST_PAD (omxpad));

  return GST_PAD (omxpad);
}

static void
gst_omx_video_mixer_release_pad (GstElement * element, GstPad * pad)
{
  GstOmxVideoMixer *mixer;

  mixer = GST_OMX_VIDEO_MIXER (element);

  gst_collect_pads2_remove_pad (mixer->collect, pad);

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
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return ret;

allocate_fail:
  {
    GST_ELEMENT_ERROR (mixer, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
    return GST_STATE_CHANGE_FAILURE;
  }
}

static GstFlowReturn
gst_omx_video_mixer_collected (GstCollectPads2 * pads, GstOmxVideoMixer * mixer)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GSList *l;

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
