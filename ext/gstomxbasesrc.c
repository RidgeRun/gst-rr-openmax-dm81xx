/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Melissa Montero <melissa.montero@ridgerun.com>
 * Copyright (C) 2014 Jose Jimenez <jose.jimenez@ridgerun.com>
 * Copyright (C) 2014 Ronny Jimenez <ronny.jimenez@ridgerun.com>
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
 * SECTION:element-omx_src_base
 *
 * FIXME:Describe omx_src_base here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m  omx_src_base ! identity ! fakesink silent=TRUE
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

#include "gstomxbasesrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_src_base_debug);
#define GST_CAT_DEFAULT gst_omx_src_base_debug


#define GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT    8

enum
{
  PROP_0,
  PROP_PEER_ALLOC,
  PROP_NUM_OUTPUT_BUFFERS,
};

static void gst_omx_base_src_base_init (gpointer g_class);
static void gst_omx_base_class_src_init (GstOmxBaseSrcClass * klass);
static void gst_omx_base_src_init (GstOmxBaseSrc * src, gpointer g_class);


GType
gst_omx_base_src_get_type (void)
{
  static volatile gsize omx_base_scr_type = 0;

  if (g_once_init_enter (&omx_base_src_type)) {
    GType _type;
    static const GTypeInfo omx_base_src_info = {
      sizeof (GstOmxBaseSrcClass),
      (GBaseInitFunc) gst_omx_base_src_base_init,
      NULL,
      (GClassInitFunc) gst_omx_base_src_class_init,
      NULL,
      NULL,
      sizeof (GstOmxBaseSrc),
      0,
      (GInstanceInitFunc) gst_omx_base_src_init,
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstOmxBaseSrc", &omx_base_src_info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&omx_base_src_type, _type);
  }

  return omx_base_src_type;
}


static void gst_omx_base_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_base_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_omx_base_src_finalize (GObject * object);
static GstStateChangeReturn gst_omx_base_src_change_state (GstElement * element,
    GstStateChange transition);


/* GObject vmethod implementations */

static void
gst_omx_base_src_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_omx_base_src_debug, "omx_base_src",
      0, "RidgeRun's OMX basesrc element");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_base_src_init (GstOmxBaseSrc * this, gpointer g_class)
{
  GstOmxBaseSrcClass *klass = GST_OMX_BASE_SRC_CLASS (g_class);
  OMX_ERRORTYPE error;

  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->requested_size = 0;
  this->peer_alloc = TRUE;
  this->flushing = FALSE;
  this->started = FALSE;
  this->first_buffer = TRUE;
  this->interlaced = FALSE;

  this->output_buffers = GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT;

  this->pads = NULL;
  this->fill_ret = GST_FLOW_OK;
  this->state = OMX_StateInvalid;
  g_mutex_init (&this->waitmutex);
  g_cond_init (&this->waitcond);

  error = gst_omx_base_src_allocate_omx (this, klass->handle_name);
  if (GST_OMX_FAIL (error)) {
    GST_ELEMENT_ERROR (this, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
  }
}



static void
gst_omx_base_src_class_init (GstOmxBaseSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  parent_class = g_type_class_peek_parent (klass);

  klass->omx_event = NULL;
  klass->omx_fill_buffer = NULL;
  klass->omx_empty_buffer = NULL;
  klass->parse_caps = NULL;
  klass->parse_buffer = NULL;
  klass->init_ports = NULL;

  klass->handle_name = NULL;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_base_src_change_state);

  gst_element_class_set_details_simple (gstelement_class,
      "omxdec",
      "Generic/Filter",
      "RidgeRun's OMX based basesrc",
      "Michael Gruner <michael.gruner@ridgerun.com>, "
      "Jose Jimenez <jose.jimenez@ridgerun.com>,"
      "Ronny Jimenez <ronny.jimenez@ridgerun.com>");

  gobject_class->set_property = gst_omx_base_src_set_property;
  gobject_class->get_property = gst_omx_base_src_get_property;
  gobject_class->finalize = gst_omx_base_src_finalize;

  g_object_class_install_property (gobject_class, PROP_PEER_ALLOC,
      g_param_spec_boolean ("peer-alloc",
          "Try to use buffers from downstream element",
          "Try to use buffers from downstream element", TRUE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_NUM_OUTPUT_BUFFERS,
      g_param_spec_uint ("output-buffers", "Output buffers",
          "OMX output buffers number",
          1, 16, GST_OMX_BASE_NUM_OUTPUT_BUFFERS_DEFAULT, G_PARAM_READWRITE));
}


static void
gst_omx_base_src_finalize (GObject * object)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (object);

  GST_INFO_OBJECT (this, "Finalizing %s", GST_OBJECT_NAME (this));

  g_list_free_full (this->pads, gst_object_unref);
  gst_omx_base_src_free_omx (this);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}



static void
gst_omx_base_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxBase *this = GST_OMX_BASE_SRC (object);

  switch (prop_id) {
    case PROP_PEER_ALLOC:
      this->peer_alloc = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting peer-alloc to %d", this->peer_alloc);
      break;
    case PROP_NUM_OUTPUT_BUFFERS:
      this->output_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting output-buffers to %d",
          this->output_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_base_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxBase *this = GST_OMX_BASE (object);

  switch (prop_id) {
    case PROP_PEER_ALLOC:
      g_value_set_boolean (value, this->peer_alloc);
      break;
    case PROP_NUM_OUTPUT_BUFFERS:
      g_value_set_uint (value, this->output_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_allocate_omx (GstOmxBaseSrc * this, gchar * handle_name)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PORT_PARAM_TYPE init;

  GST_INFO_OBJECT (this, "Allocating OMX resources for %s", handle_name);

  this->callbacks = TIMM_OSAL_Malloc (sizeof (OMX_CALLBACKTYPE),
      TIMM_OSAL_TRUE, 0, TIMMOSAL_MEM_SEGMENT_EXT);
  if (!this->callbacks) {
    error = OMX_ErrorInsufficientResources;
    goto noresources;
  }

  this->callbacks->EventHandler =
      (GstOmxEventHandler) gst_omx_base_src_event_callback;
  this->callbacks->FillBufferDone =
      (GstOmxFillBufferDone) gst_omx_base_src_fill_callback;
  if (!handle_name) {
    error = OMX_ErrorInvalidComponentName;
    goto nohandlename;
  }

  g_mutex_lock (&_omx_mutex);
  error = OMX_GetHandle (&this->handle, handle_name, this, this->callbacks);
  g_mutex_unlock (&_omx_mutex);
  if ((error != OMX_ErrorNone) || (!this->handle))
    goto nohandle;

  this->component = (OMX_COMPONENTTYPE *) this->handle;
  /*
  GST_OMX_INIT_STRUCT (&init, OMX_PORT_PARAM_TYPE);
  init.nPorts = 1;
  init.nStartPortNumber = 0;
  g_mutex_lock (&_omx_mutex);
  error = OMX_SetParameter (this->handle, OMX_IndexParamVideoInit, &init);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone)
    goto initport;
  */
  return error;

noresources:
  {
    GST_ERROR_OBJECT (this, "Insufficient OMX memory resources");
    return error;
  }
nohandlename:
  {
    GST_ERROR_OBJECT (this, "The component name has not been defined");
    return error;
  }
nohandle:
  {
    GST_ERROR_OBJECT (this, "Unable to grab OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }

}


static GstStateChangeReturn
gst_omx_base_src_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_omx_base_src_stop (this);
      break;
    default:
      break;
  }

  return ret;
}
