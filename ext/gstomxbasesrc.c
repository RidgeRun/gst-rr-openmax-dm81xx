/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2014 Ronny Jimenez <ronny.jimenez@ridgerun.com>
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

GST_DEBUG_CATEGORY_STATIC (gst_omx_base_src_debug);
#define GST_CAT_DEFAULT gst_omx_base_src_debug


#define GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT    8
#define PROP_ALWAYS_COPY_DEFAULT          FALSE

enum
{
  PROP_0,
  PROP_ALWAYS_COPY,
  PROP_PEER_ALLOC,
  PROP_NUM_OUTPUT_BUFFERS,
};


#define gst_omx_base_src_parent_class parent_class
static GstBaseSrcClass *parent_class = NULL;


static void gst_omx_base_src_base_init (gpointer g_class);
static void gst_omx_base_src_class_init (GstOmxBaseSrcClass * klass);
static void gst_omx_base_src_init (GstOmxBaseSrc * src, gpointer g_class);


GType
gst_omx_base_src_get_type (void)
{
  static volatile gsize omx_base_src_type = 0;

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

    _type = g_type_register_static (GST_TYPE_PUSH_SRC,
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
static OMX_ERRORTYPE gst_omx_base_src_stop (GstOmxBaseSrc * this);
static OMX_ERRORTYPE gst_omx_base_src_free_omx (GstOmxBaseSrc * this);
static OMX_ERRORTYPE gst_omx_base_src_free_buffers (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_allocate_omx (GstOmxBaseSrc * this,
    gchar * handle_name);
static OMX_ERRORTYPE gst_omx_base_src_start (GstOmxBaseSrc * this,
    OMX_BUFFERHEADERTYPE * omxpeerbuf);
static GstFlowReturn gst_omx_base_src_create (GstPushSrc * src,
    GstBuffer ** buf);
static OMX_ERRORTYPE gst_omx_base_src_push_buffers (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_alloc_buffers (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_for_each_pad (GstOmxBaseSrc * this,
    GstOmxBaseSrcPadFunc func, GstPadDirection direction, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_event_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2,
    gpointer eventdata);
static OMX_ERRORTYPE gst_omx_base_src_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static GstStateChangeReturn gst_omx_base_src_change_state (GstElement * element,
    GstStateChange transition);
static OMX_ERRORTYPE gst_omx_base_src_flush_ports (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_set_flushing_pad (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_peer_alloc_buffer (GstOmxBaseSrc * this,
    GstOmxPad * pad, gpointer * pbuffer, guint32 * size);
static gboolean gst_omx_base_src_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn
gst_omx_base_src_get_buffer (GstOmxBaseSrc * this, GstBuffer ** buffer);
static gboolean gst_omx_base_src_check_caps (GstPad * pad, GstCaps * newcaps);
static gboolean gst_omx_base_src_set_caps (GstBaseSrc * src, GstCaps * caps);


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
  this->offset = 0;
  this->peer_alloc = TRUE;
  this->flushing = FALSE;
  this->started = FALSE;
  this->first_buffer = TRUE;
  this->interlaced = FALSE;

  /* Initialize properties */
  this->always_copy = PROP_ALWAYS_COPY_DEFAULT;
  this->output_buffers = GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT;

  this->pads = NULL;
  this->create_ret = GST_FLOW_OK;
  this->state = OMX_StateInvalid;
  g_mutex_init (&this->waitmutex);
  g_cond_init (&this->waitcond);
  this->pending_buffers = gst_omx_buf_queue_new ();

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
  GstPushSrcClass *pushsrc_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  klass->omx_event = NULL;
  klass->omx_create = NULL;
  klass->init_ports = NULL;
  klass->parse_caps = NULL;
  klass->handle_name = NULL;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_base_src_change_state);

  gst_element_class_set_details_simple (gstelement_class,
      "omxdec",
      "Generic/Filter",
      "RidgeRun's OMX based basesrc",
      "Jose Jimenez <jose.jimenez@ridgerun.com>");

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
          1, 16, GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_ALWAYS_COPY,
      g_param_spec_boolean ("always-copy", "Always copy",
          "If the output buffer should be copied or should use the OpenMax buffer",
          PROP_ALWAYS_COPY_DEFAULT, G_PARAM_WRITABLE));

  pushsrc_class->create = GST_DEBUG_FUNCPTR (gst_omx_base_src_create);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_omx_base_src_set_caps);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_omx_base_src_event);

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
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (object);

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
    case PROP_ALWAYS_COPY:
      this->always_copy = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting always_copy to %d", this->always_copy);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_base_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (object);

  switch (prop_id) {
    case PROP_PEER_ALLOC:
      g_value_set_boolean (value, this->peer_alloc);
      break;
    case PROP_NUM_OUTPUT_BUFFERS:
      g_value_set_uint (value, this->output_buffers);
      break;
    case PROP_ALWAYS_COPY:
      g_value_set_boolean (value, this->always_copy);
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

gboolean
gst_omx_base_src_add_pad (GstOmxBaseSrc * this, GstPad * pad)
{
  GST_INFO_OBJECT (this, "Adding pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_object_ref (pad);
  this->pads = g_list_append (this->pads, pad);

  return TRUE;
}


static OMX_ERRORTYPE
gst_omx_base_src_enable_pad (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  guint32 padidx = (guint32) data;

  if (padidx == GST_OMX_PAD_PORT (pad)->nPortIndex) {
    GST_INFO_OBJECT (this, "Enabling port %s:%s", GST_DEBUG_PAD_NAME (pad));
    pad->enabled = TRUE;
  }

  return error;
}

static OMX_ERRORTYPE
gst_omx_base_src_set_flushing_pad (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  guint32 padidx = (guint32) data;

  if (padidx == GST_OMX_PAD_PORT (pad)->nPortIndex) {
    GST_INFO_OBJECT (this, "Finished flushing %s:%s", GST_DEBUG_PAD_NAME (pad));
    GST_OBJECT_LOCK (pad);
    pad->flushing = FALSE;
    GST_OBJECT_UNLOCK (pad);
  }

  return error;
}

static OMX_ERRORTYPE
gst_omx_base_src_event_callback (OMX_HANDLETYPE handle,
    gpointer data,
    OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2, gpointer eventdata)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (data);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  switch (event) {
    case OMX_EventCmdComplete:
      GST_INFO_OBJECT (this,
          "OMX command complete event received: %s (%s) (%d)",
          gst_omx_cmd_to_str (nevent1),
          OMX_CommandStateSet ==
          nevent1 ? gst_omx_state_to_str (nevent2) : "No debug", nevent2);

      if (OMX_CommandStateSet == nevent1) {
        g_mutex_lock (&this->waitmutex);
        this->state = nevent2;
        g_cond_signal (&this->waitcond);
        g_mutex_unlock (&this->waitmutex);
      }

      if (OMX_CommandPortEnable == nevent1) {
        g_mutex_lock (&this->waitmutex);
        gst_omx_base_src_for_each_pad (this, gst_omx_base_src_enable_pad,
            GST_PAD_UNKNOWN, (gpointer) nevent2);
        g_cond_signal (&this->waitcond);
        g_mutex_unlock (&this->waitmutex);
      }

      if (OMX_CommandFlush == nevent1) {
        g_mutex_lock (&this->waitmutex);
        gst_omx_base_src_for_each_pad (this, gst_omx_base_src_set_flushing_pad,
            GST_PAD_UNKNOWN, (gpointer) nevent2);
        g_cond_signal (&this->waitcond);
        g_mutex_unlock (&this->waitmutex);
      }
      break;
    case OMX_EventError:
      GST_ERROR_OBJECT (this, "OMX error event received: %s",
          gst_omx_error_to_str (nevent1));

      /* GST_ELEMENT_ERROR (this, LIBRARY, ENCODE, */
      /*                 (gst_omx_error_to_str (nevent1)), (NULL)); */
      break;
    case OMX_EventMark:
      GST_INFO_OBJECT (this, "OMX mark event received");
      break;
    case OMX_EventPortSettingsChanged:
      /* http://maemo.org/api_refs/5.0/alpha/libomxil-bellagio/_o_m_x___index_8h.html */
      GST_INFO_OBJECT (this,
          "OMX port settings changed event received: Port %d: %d", nevent2,
          nevent1);
      break;
    case OMX_EventBufferFlag:
      GST_INFO_OBJECT (this, "OMX buffer flag event received");
      break;
    case OMX_EventResourcesAcquired:
      GST_INFO_OBJECT (this, "OMX resources acquired event received");
      break;
    case OMX_EventComponentResumed:
      GST_INFO_OBJECT (this, "OMX component resumed event received");
      break;
    case OMX_EventDynamicResourcesAvailable:
      GST_INFO_OBJECT (this, "OMX synamic resources available event received");
      break;
    case OMX_EventPortFormatDetected:
      GST_INFO_OBJECT (this, "OMX port format detected event received");
      break;
    default:
      GST_WARNING_OBJECT (this, "Unknown OMX port event");
      break;
  }

  return error;
}


static OMX_ERRORTYPE
gst_omx_base_src_stop (GstOmxBaseSrc * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (!this->started)
    goto alreadystopped;

  if (!this->flushing) {
    GST_OBJECT_LOCK (this);
    this->flushing = TRUE;
    GST_OBJECT_UNLOCK (this);

    GST_INFO_OBJECT (this, "Flushing ports");
    error =
        gst_omx_base_src_for_each_pad (this, gst_omx_base_src_flush_ports,
        GST_PAD_UNKNOWN, NULL);
    if (GST_OMX_FAIL (error))
      goto noflush;
  }
  GST_INFO_OBJECT (this, "Sending handle to Idle");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_base_src_wait_for_condition (this,
      gst_omx_base_src_condition_state, (gpointer) OMX_StateIdle,
      (gpointer) & this->state);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Sending handle to Loaded");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateLoaded,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Freeing port buffers");
  error =
      gst_omx_base_src_for_each_pad (this, gst_omx_base_src_free_buffers,
      GST_PAD_UNKNOWN, NULL);
  if (GST_OMX_FAIL (error))
    goto nofree;

  GST_INFO_OBJECT (this, "Waiting for handle to become Loaded");
  error = gst_omx_base_src_wait_for_condition (this,
      gst_omx_base_src_condition_state, (gpointer) OMX_StateLoaded,
      (gpointer) & this->state);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_OBJECT_LOCK (this);
  this->flushing = FALSE;
  this->started = FALSE;
  this->first_buffer = TRUE;
  this->create_ret = FALSE;
  GST_OBJECT_UNLOCK (this);

  return error;

alreadystopped:
  {
    GST_WARNING_OBJECT (this, "Component already stopped");
    return error;
  }
noflush:
  {
    GST_ERROR_OBJECT (this, "Unable to flush port: %s",
        gst_omx_error_to_str (error));
    return error;
  }
statechange:
  {
    GST_ERROR_OBJECT (this, "Unable to set component state: %s",
        gst_omx_error_to_str (error));
    return error;
  }

nofree:
  {
    GST_ERROR_OBJECT (this, "Unable to free buffers: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_for_each_pad (GstOmxBaseSrc * this, GstOmxBaseSrcPadFunc func,
    GstPadDirection direction, gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstPad *pad;
  GList *l;

  for (l = this->pads; l; l = l->next) {
    pad = l->data;
    if ((direction == GST_PAD_UNKNOWN)
        || (direction == GST_PAD_DIRECTION (pad))) {
      error = func (this, GST_OMX_PAD (pad), data);
      if (GST_OMX_FAIL (error))
        goto failed;
    }
  }

  return error;

failed:
  {
    GST_ERROR_OBJECT (this, "Iterator failed on pad: %s:%s",
        GST_DEBUG_PAD_NAME (pad));
    return error;
  }
}

OMX_ERRORTYPE
gst_omx_base_src_wait_for_condition (GstOmxBaseSrc * this,
    GstOmxBaseSrcCondition condition, gpointer arg1, gpointer arg2)
{
  guint64 endtime;

  g_mutex_lock (&this->waitmutex);

  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  while (!condition (arg1, arg2))
    if (!g_cond_wait_until (&this->waitcond, &this->waitmutex, endtime))
      goto timeout;

  GST_DEBUG_OBJECT (this, "Wait for condition successful");
  g_mutex_unlock (&this->waitmutex);

  return OMX_ErrorNone;

timeout:
  {
    GST_WARNING_OBJECT (this, "Wait for condition timed out");
    g_mutex_unlock (&this->waitmutex);
    return OMX_ErrorTimeout;
  }
}

gboolean
gst_omx_base_src_condition_state (gpointer targetstate, gpointer currentstate)
{
  OMX_STATETYPE _targetstate = (OMX_STATETYPE) targetstate;
  OMX_STATETYPE _currentstate = *(OMX_STATETYPE *) currentstate;

  return _targetstate == _currentstate;
}

gboolean
gst_omx_base_src_condition_enabled (gpointer enabled, gpointer dummy)
{
  return *(gboolean *) enabled;
}

gboolean
gst_omx_base_src_condition_disabled (gpointer enabled, gpointer dummy)
{
  return !*(gboolean *) enabled;
}


static OMX_ERRORTYPE
gst_omx_base_src_free_buffers (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer = NULL;
  GstOmxBufTabNode *node;
  guint i;
  GList *buffers;

  buffer = gst_omx_buf_queue_pop_buffer (this->pending_buffers);
  while (buffer) {
    GST_LOG_OBJECT (this, "Cleaning pending queue of buffers");
    gst_omx_base_src_release_buffer ((gpointer) buffer);
    buffer = gst_omx_buf_queue_pop_buffer (this->pending_buffers);
  }


  buffers = pad->buffers->table;

  /* No buffers allocated yet */
  if (!buffers)
    return error;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    if (!buffers)
      goto shortread;

    node = (GstOmxBufTabNode *) buffers->data;
    buffer = node->buffer;

    GST_DEBUG_OBJECT (this, "Freeing %s:%s buffer number %u: %p",
        GST_DEBUG_PAD_NAME (pad), i, buffer);

    error = gst_omx_buf_tab_remove_buffer (pad->buffers, buffer);
    if (GST_OMX_FAIL (error))
      goto notintable;

    /* Resync list */
    buffers = pad->buffers->table;

    g_free (buffer->pAppPrivate);
    g_mutex_lock (&_omx_mutex);
    error = OMX_FreeBuffer (this->handle, GST_OMX_PAD_PORT (pad)->nPortIndex,
        buffer);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto nofree;
  }

  GST_OBJECT_LOCK (pad);
  pad->enabled = FALSE;
  GST_OBJECT_UNLOCK (pad);

  return error;

shortread:
  {
    GST_ERROR_OBJECT (this, "Malformed output buffer list");
    /*TODO: should I free buffers? */
    return OMX_ErrorResourcesLost;
  }
notintable:
  {
    GST_ERROR_OBJECT (this, "The buffer list for %s:%s is malformed: %s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)), gst_omx_error_to_str (error));
    return error;
  }
nofree:
  {
    GST_ERROR_OBJECT (this, "Error freeing buffers on %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_free_omx (GstOmxBaseSrc * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_INFO_OBJECT (this, "Freeing OMX resources");

  TIMM_OSAL_Free (this->callbacks);

  g_mutex_lock (&_omx_mutex);
  error = OMX_FreeHandle (this->handle);
  g_mutex_unlock (&_omx_mutex);
  if (error != OMX_ErrorNone)
    goto freehandle;

  return error;

freehandle:
  {
    GST_ERROR_OBJECT (this, "Unable to free OMX handle: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_src_flush_ports (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE *port = GST_OMX_PAD_PORT (pad);

  GST_INFO_OBJECT (this, "Flushing port %d on pad %s:%s",
      (int) port->nPortIndex, GST_DEBUG_PAD_NAME (pad));

  GST_OBJECT_LOCK (pad);
  pad->flushing = TRUE;
  GST_OBJECT_UNLOCK (pad);

  if (GST_PAD_IS_SRC (pad))
    return error;

  g_mutex_lock (&_omx_mutex);
  GST_DEBUG_OBJECT (this, "Setting handle to flush");
  error = OMX_SendCommand (this->handle, OMX_CommandFlush, -1, NULL);
  g_mutex_unlock (&_omx_mutex);

  GST_DEBUG_OBJECT (this, "Waiting for port to flush");
  error = gst_omx_base_src_wait_for_condition (this,
      gst_omx_base_src_condition_disabled, (gpointer) & pad->flushing, NULL);
  if (GST_OMX_FAIL (error))
    goto noflush;

  return error;

noflush:
  {
    GST_ERROR_OBJECT (this, "Unable to flush port %d", (int) port->nPortIndex);
    return error;
  }

}


static GstFlowReturn
gst_omx_base_src_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstFlowReturn ret;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (src);
  GstClock *clock;
  GstClockTime abs_time, base_time, timestamp, duration;
  gboolean flushing;


  GST_OBJECT_LOCK (this);
  flushing = this->flushing;
  GST_OBJECT_UNLOCK (this);

  if (flushing)
    goto flushing;

  if (this->create_ret)
    goto pusherror;

  /* timestamps, LOCK to get clock and base time. */
  GST_OBJECT_LOCK (this);
  if ((clock = GST_ELEMENT_CLOCK (this))) {
    //we have a clock, get base time and ref clock 
    base_time = GST_ELEMENT (this)->base_time;
    abs_time = gst_clock_get_time (clock);
  } else {
    // no clock, can't set timestamps 
    base_time = GST_CLOCK_TIME_NONE;
    abs_time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (this);

  if (!this->started) {
    GST_INFO_OBJECT (this, "Starting component");
    error = gst_omx_base_src_start (this, NULL);
    if (GST_OMX_FAIL (error))
      goto nostart;
  }

  ret = gst_omx_base_src_get_buffer (this, buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK))
    goto error;


  timestamp = GST_BUFFER_TIMESTAMP (*buf);

  if (!this->started) {
    this->running_time = abs_time - base_time;
    if (!this->running_time)
      this->running_time = timestamp;
    this->omx_delay = timestamp - this->running_time;

    GST_DEBUG_OBJECT (this, "OMX delay %" G_GINT64_FORMAT, this->omx_delay);
    this->started = TRUE;
  }

  /* set buffer metadata */
  GST_BUFFER_OFFSET (*buf) = this->offset++;
  GST_BUFFER_OFFSET_END (*buf) = this->offset;

  /* the time now is the time of the clock minus the base time */
  timestamp = timestamp - this->omx_delay;

  GST_DEBUG_OBJECT (this, "Adjusted timestamp %" GST_TIME_FORMAT,
      GST_TIME_ARGS (timestamp));

  GST_BUFFER_TIMESTAMP (*buf) = timestamp;

  GST_DEBUG_OBJECT (this,
      "Got buffer from component: %p with timestamp %" GST_TIME_FORMAT
      " duration %" GST_TIME_FORMAT, buf,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*buf)));

  return ret;

flushing:
  {
    GST_DEBUG_OBJECT (this, "Flushing");
    this->started = FALSE;
    return GST_FLOW_OK;
  }
pusherror:
  {
    /* What to do in case of filret ? */
    return this->create_ret;
  }
nostart:
  {
    /* What to do in case of nostart ? */
    return GST_FLOW_ERROR;
  }
error:
  {
    GST_ERROR_OBJECT (this, "error processing buffer %d (%s)", ret,
        gst_flow_get_name (ret));
    return ret;
  }
}

static GstFlowReturn
gst_omx_base_src_get_buffer (GstOmxBaseSrc * this, GstBuffer ** buffer)
{
  GstFlowReturn flow_ret;
  OMX_BUFFERHEADERTYPE *omx_buf = NULL;
  GstOmxBufferData *bufdata = NULL;
  gboolean i = FALSE;
  GstOmxBaseSrcClass *klass = GST_OMX_BASE_SRC_GET_CLASS (this);

  omx_buf = gst_omx_buf_queue_pop_buffer (this->pending_buffers);

  if (!omx_buf) {
    goto timeout;
  }

  bufdata = (GstOmxBufferData *) omx_buf->pAppPrivate;

  GST_LOG_OBJECT (this, "Handling buffer: 0x%08x %" G_GUINT64_FORMAT,
      (guint) omx_buf->nFlags, (guint64) omx_buf->nTimeStamp);

  GST_LOG_OBJECT (this, "Handling output data");

  if (this->always_copy) {
    /* TODO: Add always_copy handler, copy the data in a
       gstreamer buffer and return the omxbuffer to the buftab 
       For now we do the same in both cases
     */
    *buffer = gst_buffer_new ();
    if (!*buffer)
      goto noalloc;

  } else {
    *buffer = gst_buffer_new ();
    if (!*buffer)
      goto noalloc;
  }

  if (klass->omx_create) {
    this->create_ret = klass->omx_create (this, omx_buf, buffer);
    if (this->create_ret != GST_FLOW_OK) {
      goto cbfailed;
    }
  }

  GST_LOG_OBJECT (this,
      "(Get_buffer %s) Buffer %p size %d reffcount %d bufdat %p->%p",
      GST_OBJECT_NAME (this), omx_buf->pBuffer, GST_BUFFER_SIZE (*buffer),
      GST_OBJECT_REFCOUNT (*buffer), bufdata, bufdata->buffer);
  GST_DEBUG_OBJECT (this,
      "Got buffer from component: %p with timestamp %" GST_TIME_FORMAT
      " duration %" GST_TIME_FORMAT, buffer,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*buffer)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*buffer)));


  return GST_FLOW_OK;
noalloc:

  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Unable to allocate buffer to push"), (NULL));
    return GST_FLOW_ERROR;
  }
timeout:
  {
    GST_ELEMENT_ERROR (this, LIBRARY, SETTINGS, (NULL),
        ("Cannot acquire output buffer from pending queue, check that a video source is connected"));
    return GST_FLOW_ERROR;
  }
cbfailed:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Subclass failed to process buffer (id:%d): %s",
            bufdata->id, gst_flow_get_name (this->create_ret)), (NULL));
    return GST_FLOW_ERROR;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_start (GstOmxBaseSrc * this, OMX_BUFFERHEADERTYPE * omxpeerbuf)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (this->started)
    goto alreadystarted;

  GST_INFO_OBJECT (this, "Sending handle to Idle");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Allocating buffers for src ports");
  error =
      gst_omx_base_src_for_each_pad (this, gst_omx_base_src_alloc_buffers,
      GST_PAD_SRC, NULL);
  if (GST_OMX_FAIL (error))
    goto noalloc;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_base_src_wait_for_condition (this,
      gst_omx_base_src_condition_state, (gpointer) OMX_StateIdle,
      (gpointer) & this->state);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Sending handle to Executing");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet,
      OMX_StateExecuting, NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Waiting for handle to become Executing");
  error = gst_omx_base_src_wait_for_condition (this,
      gst_omx_base_src_condition_state, (gpointer) OMX_StateExecuting,
      (gpointer) & this->state);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Pushing output buffers");
  error =
      gst_omx_base_src_for_each_pad (this, gst_omx_base_src_push_buffers,
      GST_PAD_UNKNOWN, NULL);
  if (GST_OMX_FAIL (error))
    goto nopush;

  //this->started = TRUE;

  return error;

alreadystarted:
  {
    GST_WARNING_OBJECT (this, "Component already started");
    return error;
  }
starthandle:
  {
    GST_ERROR_OBJECT (this, "Unable to set handle to Idle");
    return error;
  }
noalloc:
  {
    GST_ERROR_OBJECT (this, "Unable to allocate resources for buffers");
    return error;
  }
nopush:
  {
    GST_ERROR_OBJECT (this, "Unable to push buffer into the output port");
    return error;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_peer_alloc_buffer (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer * pbuffer, guint32 * size)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxpeerbuffer = NULL;

  GstBuffer *peerbuf = NULL;
  GstFlowReturn ret;

  ret =
      gst_pad_alloc_buffer (GST_PAD (pad), 0,
      GST_OMX_PAD_PORT (pad)->nBufferSize,
      gst_pad_get_negotiated_caps (GST_PAD (pad)), &peerbuf);

  if (GST_FLOW_OK != ret)
    goto nodownstream;

  if (GST_OMX_IS_OMX_BUFFER (peerbuf)) {
    omxpeerbuffer = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_DATA (peerbuf);
    *pbuffer = omxpeerbuffer->pBuffer;
    *size = omxpeerbuffer->nAllocLen;
  } else {
    pbuffer = NULL;
    *size = 0;
  }
  gst_buffer_unref (peerbuf);

  return error;

nodownstream:
  {
    GST_ERROR_OBJECT (this, "Downstream element was unable to provide buffers");
    error = OMX_ErrorInsufficientResources;
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_src_alloc_buffers (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer = NULL;
  GList *peerbuffers = NULL, *currentbuffer = NULL;
  guint i;
  GstOmxBufferData *bufdata = NULL;
  GstOmxBufferData *bufdatatemp = NULL;
  guint32 maxsize, size = 0;
  gboolean divided_buffers = FALSE;
  gboolean top_field = TRUE;
  gpointer pbuffer = NULL;

  if (pad->buffers->table != NULL) {
    GST_DEBUG_OBJECT (this, "Ignoring buffers allocation for %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }

  if (this->interlaced)
    divided_buffers = TRUE;

  GST_DEBUG_OBJECT (this, "Allocating buffers for %s:%s",
      GST_DEBUG_PAD_NAME (GST_PAD (pad)));

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    /* First we try to ask for downstream OMX buffers */
    if (GST_PAD_IS_SRC (pad) && this->peer_alloc) {
      error = gst_omx_base_src_peer_alloc_buffer (this, pad, &pbuffer, &size);
    }

    if (error != OMX_ErrorNone)
      goto noalloc;


    bufdata = (GstOmxBufferData *) g_malloc (sizeof (GstOmxBufferData));
    bufdata->pad = pad;
    bufdata->buffer = NULL;

    if (currentbuffer) {
      bufdata->id = ((GstOmxBufferData *) ((GstOmxBufTabNode *) currentbuffer->data)->buffer->pAppPrivate)->id + (!top_field) * pad->port->nBufferCountActual;  // Ensure bottom fields don't have the same ids as the top
    } else {
      bufdata->id = i;
    }

    if (divided_buffers)
      top_field = !top_field;

    if (pbuffer) {
      GST_DEBUG_OBJECT (this, "Received buffer number %u:"
          "%p of size %d", bufdata->id, pbuffer, size);

      g_mutex_lock (&_omx_mutex);
      error = OMX_UseBuffer (this->handle, &buffer,
          GST_OMX_PAD_PORT (pad)->nPortIndex, bufdata, size, pbuffer);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error))
        goto nouse;
      GST_DEBUG_OBJECT (this, "Saved buffer number %u:"
          "%p->%p", bufdata->id, buffer, pbuffer);
      /* No upstream buffer received or pad is a sink, allocate our own */
    } else {
      maxsize = GST_OMX_PAD_PORT (pad)->nBufferSize > this->requested_size ?
          GST_OMX_PAD_PORT (pad)->nBufferSize : this->requested_size;

      g_mutex_lock (&_omx_mutex);
      error = OMX_AllocateBuffer (this->handle, &buffer,
          GST_OMX_PAD_PORT (pad)->nPortIndex, bufdata, maxsize);
      g_mutex_unlock (&_omx_mutex);
      if (GST_OMX_FAIL (error))
        goto noalloc;
      GST_DEBUG_OBJECT (this, "Allocated buffer number %u: %p->%p", i, buffer,
          buffer->pBuffer);
    }

    error = gst_omx_buf_tab_add_buffer (pad->buffers, buffer);
    if (GST_OMX_FAIL (error))
      goto addbuffer;
  }

out:
  return error;

nouse:
  {
    GST_ERROR_OBJECT (this, "Unable to use buffer provided by downstream: %s",
        gst_omx_error_to_str (error));

    g_free (bufdata);
    return error;
  }
noalloc:
  {
    GST_ERROR_OBJECT (this, "Failed to allocate buffers:  %s",
        gst_omx_error_to_str (error));
    g_free (bufdata);
    /*TODO: should I free buffers? */
    return error;
  }
addbuffer:
  {
    GST_ERROR_OBJECT (this, "Unable to add the buffer to the buftab");
    g_free (bufdata);
    /*TODO: should I free buffers? */
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_src_push_buffers (GstOmxBaseSrc * this, GstOmxPad * pad,
    gpointer data)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GstOmxBufTabNode *node;
  guint i;
  GList *buffers;

  buffers = pad->buffers->table;

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    if (!buffers)
      goto shortread;

    node = (GstOmxBufTabNode *) buffers->data;
    buffer = node->buffer;

    GST_DEBUG_OBJECT (this, "Pushing buffer number %u: %p of size %d", i,
        buffer, (int) buffer->nAllocLen);

    g_mutex_lock (&_omx_mutex);
    error = this->component->FillThisBuffer (this->handle, buffer);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error))
      goto nopush;

    buffers = g_list_next (buffers);
  }

  return error;

nopush:
  {
    GST_ERROR_OBJECT (this, "Failed to push buffers");
    /*TODO: should I free buffers? */
    return error;
  }
shortread:
  {
    GST_ERROR_OBJECT (this, "Malformed output buffer list");
    /*TODO: should I free buffers? */
    return OMX_ErrorResourcesLost;
  }
}


static OMX_ERRORTYPE
gst_omx_base_src_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (data);
  OMX_BUFFERHEADERTYPE *omxbuf;
  gboolean busy;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gboolean flushing;

  GST_LOG_OBJECT (this, "Fill buffer callback for buffer %p->%p", outbuf,
      outbuf->pBuffer);

  GST_OBJECT_LOCK (this);
  flushing = this->flushing;
  GST_OBJECT_UNLOCK (this);

  gst_omx_buf_tab_find_buffer (bufdata->pad->buffers, outbuf, &omxbuf, &busy);

  if (busy)
    goto illegal;

  if (flushing)
    goto flushing;
  /*
     if (this->fill_ret != GST_FLOW_OK)
     goto drop;
   */
  GST_LOG_OBJECT (this, "Current %d Pending %d Target %d Next %d",
      GST_STATE (this), GST_STATE_PENDING (this), GST_STATE_TARGET (this),
      GST_STATE_NEXT (this));
  if (GST_STATE_PAUSED > GST_STATE (this))
    goto flushing;

  gst_omx_buf_tab_use_buffer (bufdata->pad->buffers, outbuf);

  error = gst_omx_buf_queue_push_buffer (this->pending_buffers, outbuf);

  return error;

illegal:
  {
    GST_ERROR_OBJECT (this,
        "Double fill callback for buffer %p->%p, this should not happen",
        outbuf, outbuf->pBuffer);
    /* g_mutex_lock (&_omx_mutex); */
    /* error = this->component->FillThisBuffer (this->handle, outbuf); */
    /* g_mutex_unlock (&_omx_mutex); */
    return error;
  }

flushing:
  {
    GST_DEBUG_OBJECT (this, "Discarding buffer %d while flushing", bufdata->id);
    return error;
  }

  /*drop:
     {
     GST_LOG_OBJECT (this, "Dropping buffer, push error %s",
     gst_flow_get_name (this->fill_ret));
     g_mutex_lock (&_omx_mutex);
     error = this->component->FillThisBuffer (this->handle, outbuf);
     g_mutex_unlock (&_omx_mutex);
     return error;
     } */
}


void
gst_omx_base_src_release_buffer (gpointer data)
{

  OMX_BUFFERHEADERTYPE *buffer = (OMX_BUFFERHEADERTYPE *) data;
  OMX_ERRORTYPE error;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  GstOmxPad *pad = bufdata->pad;
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (GST_OBJECT_PARENT (pad));
  gboolean flushing;

  GST_LOG_OBJECT (this, "Returning buffer %p to table", buffer);

  GST_OBJECT_LOCK (this);
  flushing = this->flushing;
  GST_OBJECT_UNLOCK (this);

  error = gst_omx_buf_tab_return_buffer (pad->buffers, buffer);
  if (GST_OMX_FAIL (error))
    goto noreturn;

  if (flushing)
    goto flushing;

  g_mutex_lock (&_omx_mutex);
  error = this->component->FillThisBuffer (this->handle, buffer);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto nofill;

  return;

noreturn:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), LIBRARY, ENCODE,
        ("Malformed buffer list"), (NULL));
    return;
  }
flushing:
  {
    GST_DEBUG_OBJECT (this,
        "Discarded buffer %p->%p due to flushing component", buffer,
        buffer->pBuffer);
    return;
  }
nofill:
  {
    GST_ERROR_OBJECT (this, "Unable to recycle output buffer: %s",
        gst_omx_error_to_str (error));
  }
}

static gboolean
gst_omx_base_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (src);
  GstOmxBaseSrcClass *klass = GST_OMX_BASE_SRC_GET_CLASS (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (!klass->parse_caps)
    goto noparsecaps;

  if (!klass->parse_caps (src, caps))
    goto capsinvalid;

  if (OMX_StateLoaded < this->state) {
    if (!gst_omx_base_src_check_caps (GST_BASE_SRC_PAD (src), caps))
      goto noresolutionchange;
  }
  GST_INFO_OBJECT (this, "%s:%s resolution changed, calling port renegotiation",
      GST_DEBUG_PAD_NAME (GST_BASE_SRC_PAD (src)));
  if (OMX_StateLoaded < this->state) {
    GST_INFO_OBJECT (this, "Resetting component");
    error = gst_omx_base_src_stop (this);
    if (GST_OMX_FAIL (error))
      goto nostartstop;
  }

  if (!klass->init_ports)
    goto noinitports;

  error = klass->init_ports (this);
  if (GST_OMX_FAIL (error))
    goto nopads;

  GST_DEBUG_OBJECT (this, "Caps %s set successfully",
      gst_caps_to_string (caps));
  return TRUE;

noresolutionchange:
  {
    GST_DEBUG_OBJECT (this,
        "Resolution not changed, ports not need to be reinitialized");
    return TRUE;
  }
noparsecaps:
  {
    GST_ERROR_OBJECT (this, "%s doesn't have a parse caps function",
        GST_OBJECT_NAME (this));
    return FALSE;
  }
capsinvalid:
  {
    GST_ERROR_OBJECT (this, "Unable to parse capabilities");
    return FALSE;
  }
nostartstop:
  {
    GST_ERROR_OBJECT (this, "Unable to start/stop component: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
nopads:
  {
    GST_ERROR_OBJECT (this, "Unable to initializate ports: %s",
        gst_omx_error_to_str (error));
    return FALSE;
  }
noinitports:
  {
    GST_ERROR_OBJECT (this, "%s doesn't have an init ports function",
        GST_OBJECT_NAME (this));
    return FALSE;
  }
}

static gboolean
gst_omx_base_src_check_caps (GstPad * pad, GstCaps * newcaps)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (GST_OBJECT_PARENT (pad));
  GstCaps *caps = NULL;
  GstStructure *structure = NULL;
  GstStructure *newstructure = NULL;
  gint width = 0, height = 0;
  gint newwidth = 0, newheight = 0;

  GST_LOG_OBJECT (this, "Check changes on new caps");

  caps = gst_pad_get_negotiated_caps (pad);
  if (!caps)
    goto nocaps;

  structure = gst_caps_get_structure (caps, 0);
  newstructure = gst_caps_get_structure (newcaps, 0);

  gst_structure_get_int (structure, "width", &width);
  gst_structure_get_int (structure, "height", &height);
  gst_structure_get_int (newstructure, "width", &newwidth);
  gst_structure_get_int (newstructure, "height", &newheight);

  if (width != newwidth || height != newheight)
    goto initializeports;
  else
    goto notinitializeports;

nocaps:
  {
    GST_DEBUG_OBJECT (this, "Caps are not negotiated yet");
    return TRUE;
  }
initializeports:
  {
    GST_DEBUG_OBJECT (this, "There is a resolution change, initializing ports");
    return TRUE;
  }
notinitializeports:
  {
    GST_DEBUG_OBJECT (this,
        "For new caps it's not necessary to initialize ports");
    return FALSE;
  }
}

static gboolean
gst_omx_base_src_event (GstBaseSrc * src, GstEvent * event)
{

  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (src);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (G_UNLIKELY (this == NULL)) {
    gst_event_unref (event);
    return FALSE;
  }

  GST_DEBUG_OBJECT (this, "handling event %p %" GST_PTR_FORMAT, event, event);

  switch (GST_EVENT_TYPE (event)) {
      /* Put the component in flush state so it doesn't 
       * try to process any more buffers. */
    case GST_EVENT_EOS:
    {
      GST_INFO_OBJECT (this, "EOS received, flushing ports");
      GST_OBJECT_LOCK (this);
      this->flushing = TRUE;
      GST_OBJECT_UNLOCK (this);
      error =
          gst_omx_base_src_for_each_pad (this, gst_omx_base_src_flush_ports,
          GST_PAD_UNKNOWN, NULL);
      if (GST_OMX_FAIL (error))
        goto noflush_eos;
      break;
    }
    case GST_EVENT_FLUSH_START:
    {
      GST_INFO_OBJECT (this, "Flush start received, flushing ports");
      GST_OBJECT_LOCK (this);
      this->flushing = TRUE;
      GST_OBJECT_UNLOCK (this);
      error =
          gst_omx_base_src_for_each_pad (this, gst_omx_base_src_flush_ports,
          GST_PAD_UNKNOWN, NULL);
      if (GST_OMX_FAIL (error))
        goto noflush_eos;
      break;
    }
    default:
    {
      GST_INFO_OBJECT (this, "Event:%s received",
          gst_event_type_get_name (GST_EVENT_TYPE (event)));
      break;
    }
  }
  /* Handle everything else as default */
  return GST_BASE_SRC_CLASS (parent_class)->event (src, event);

noflush_eos:
  GST_ERROR_OBJECT (this, "Unable to flush component after EOS: %s ",
      gst_omx_error_to_str (error));
  return FALSE;
}
