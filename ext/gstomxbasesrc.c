/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
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

GST_DEBUG_CATEGORY_STATIC (gst_omx_base_src_debug);
#define GST_CAT_DEFAULT gst_omx_base_src_debug


#define GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT    8

enum
{
  PROP_0,
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
static OMX_ERRORTYPE
gst_omx_base_src_allocate_omx (GstOmxBaseSrc * this, gchar * handle_name);
static OMX_ERRORTYPE
gst_omx_base_src_for_each_pad (GstOmxBaseSrc * this, GstOmxBaseSrcPadFunc func,
			       GstPadDirection direction, gpointer data);
static OMX_ERRORTYPE gst_omx_base_src_event_callback (OMX_HANDLETYPE handle,
	gpointer data, OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2, gpointer eventdata);
static OMX_ERRORTYPE gst_omx_base_src_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_base_src_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static GstStateChangeReturn gst_omx_base_src_change_state (GstElement * element,
    GstStateChange transition);
static OMX_ERRORTYPE
gst_omx_base_src_flush_ports (GstOmxBaseSrc * this, GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE
gst_omx_base_src_set_flushing_pad (GstOmxBaseSrc * this, GstOmxPad * pad, gpointer data);

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
          1, 16, GST_OMX_BASE_SRC_NUM_OUTPUT_BUFFERS_DEFAULT, G_PARAM_READWRITE));
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
  this->callbacks->EmptyBufferDone =
      (GstOmxEmptyBufferDone) gst_omx_base_src_empty_callback;
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
gst_omx_base_src_enable_pad (GstOmxBaseSrc * this, GstOmxPad * pad, gpointer data)
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
  this->fill_ret = FALSE;
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
gst_omx_base_src_free_buffers (GstOmxBaseSrc * this, GstOmxPad * pad, gpointer data)
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
gst_omx_base_src_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer)
{
  GstOmxBaseSrc *this = GST_OMX_BASE_SRC (data);
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  GstBuffer *gstbuf = bufdata->buffer;
  GstOmxPad *pad = bufdata->pad;
  guint8 id = bufdata->id;

  OMX_ERRORTYPE error = OMX_ErrorNone;


  GST_LOG_OBJECT (this, "Empty buffer callback for buffer %d %p->%p->%p", id,
      buffer, buffer->pBuffer, bufdata);

  bufdata->buffer = NULL;
  /*We need to return the buffer first in order to avoid race condition */
  error = gst_omx_buf_tab_return_buffer (pad->buffers, buffer);
  if (GST_OMX_FAIL (error))
    goto noreturn;

  gst_buffer_unref (gstbuf);

/*
  g_mutex_lock (&this->waitmutex);
  g_cond_signal (&this->waitcond);
  g_mutex_unlock (&this->waitmutex);
*/
  return error;

noreturn:
  {
    GST_ELEMENT_ERROR (this, LIBRARY, ENCODE,
        ("Unable to return buffer to buftab: %s",
            gst_omx_error_to_str (error)), (NULL));
    buffer->pAppPrivate = NULL;
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
gst_omx_base_src_flush_ports (GstOmxBaseSrc * this, GstOmxPad * pad, gpointer data)
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
