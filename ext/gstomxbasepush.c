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
 * SECTION:element-omx_base_push
 *
 * FIXME:Describe omx_base_push here.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>
#include <gst/video/video.h>

#include "timm_osal_interfaces.h"

#include "gstomxbasepush.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_base_push_debug);
#define GST_CAT_DEFAULT gst_omx_base_push_debug

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


#define gst_omx_base_push_parent_class parent_class
G_DEFINE_TYPE (GstOmxBasePush, gst_omx_base_push, GST_TYPE_OMX_BASE);

static void gst_omx_base_push_class_init (GstOmxBasePushClass * klass);
static void gst_omx_base_push_init (GstOmxBasePush * src);
static void gst_omx_base_push_finalize (GObject * object);
static GstStateChangeReturn gst_omx_base_push_change_state (GstElement *
    element, GstStateChange transition);
static GstFlowReturn gst_omx_base_push_fill_buffer (GstOmxBase * base,
    OMX_BUFFERHEADERTYPE * outbuf);
static gboolean gst_omx_base_push_event (GstOmxBase * base, GstPad * pad,
    GstEvent * event);
static OMX_ERRORTYPE gst_omx_base_push_create_task (GstOmxBasePush * this);
static OMX_ERRORTYPE gst_omx_base_push_pause_task (GstOmxBasePush * this);
static OMX_ERRORTYPE gst_omx_base_push_start_task (GstOmxBasePush * this);
static OMX_ERRORTYPE gst_omx_base_push_stop_task (GstOmxBasePush * this);
static OMX_ERRORTYPE gst_omx_base_push_destroy_task (GstOmxBasePush * this);
static OMX_ERRORTYPE gst_omx_base_push_clear_queue (GstOmxBasePush * this,
    gboolean fill);

/* GObject vmethod implementations */

/* initialize the omx's class */
static void
gst_omx_base_push_class_init (GstOmxBasePushClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstOmxBaseClass *gstomxbase_class;

  parent_class = g_type_class_peek_parent (klass);

  klass->push_buffer = NULL;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstomxbase_class = GST_OMX_BASE_CLASS (klass);

  gobject_class->finalize = gst_omx_base_push_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_base_push_change_state);
  gstomxbase_class->omx_fill_buffer =
      GST_DEBUG_FUNCPTR (gst_omx_base_push_fill_buffer);
  gstomxbase_class->event = GST_DEBUG_FUNCPTR (gst_omx_base_push_event);

  GST_DEBUG_CATEGORY_INIT (gst_omx_base_push_debug, "omx_base_push",
      0, "RidgeRun's OMX base push tasks");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_base_push_init (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error;

  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->fill_ret = GST_FLOW_OK;
  this->queue_buffers = gst_omx_buf_queue_new ();

  GST_INFO_OBJECT (this, "Initializing buffer push task");
  error = gst_omx_base_push_create_task (this);
  if (GST_OMX_FAIL (error)) {
    GST_ELEMENT_ERROR (this, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
  }
}

static void
gst_omx_base_push_finalize (GObject * object)
{
  GstOmxBasePush *this = GST_OMX_BASE_PUSH (object);

  GST_INFO_OBJECT (this, "Finalizing %s", GST_OBJECT_NAME (this));

  gst_omx_buf_queue_release (this->queue_buffers, TRUE);
  gst_omx_base_push_destroy_task (this);

  gst_omx_buf_queue_free (this->queue_buffers);
  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_omx_base_push_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxBasePush *this = GST_OMX_BASE_PUSH (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (this, "Starting push task");
      gst_omx_base_push_start_task (this);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (this, "Stopping push task");
      gst_omx_base_push_stop_task (this);
      break;
    default:
      break;
  }
  return ret;
}

static GstFlowReturn
gst_omx_base_push_fill_buffer (GstOmxBase * base, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxBasePush *this;
  OMX_ERRORTYPE error = OMX_ErrorNone;

  this = GST_OMX_BASE_PUSH (base);

  /* Pushing the processed buffer to the output queue */
  error = gst_omx_buf_queue_push_buffer (this->queue_buffers, outbuf);
  if (GST_OMX_FAIL (error))
    goto push_fail;

  return this->fill_ret;

push_fail:
  {
    GstOmxBufferData *bufdata = (GstOmxBufferData *) outbuf->pAppPrivate;
    GST_WARNING_OBJECT (this, "Failed to push buffer to output queue");
    error = gst_omx_buf_tab_return_buffer (bufdata->pad->buffers, outbuf);
    return GST_FLOW_ERROR;
  }
}


static gboolean
gst_omx_base_push_event (GstOmxBase * base, GstPad * pad, GstEvent * event)
{
  GstOmxBasePush *this;
  gboolean ret = FALSE;

  this = GST_OMX_BASE_PUSH (base);

  switch (GST_EVENT_TYPE (event)) {
      /* We only care for the EOS event, put the component in flush state so it doesn't 
       * try to process any more buffers. */
    case GST_EVENT_EOS:
    {
      gst_omx_base_push_stop_task (this);
      gst_omx_base_push_clear_queue (this, TRUE);
      break;
    }
    case GST_EVENT_FLUSH_START:
    {
      gst_omx_base_push_pause_task (this);
      gst_omx_buf_queue_release (this->queue_buffers, TRUE);
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      ret = GST_OMX_BASE_CLASS (parent_class)->event (base, pad, event);

      if (gst_task_get_state (this->pushtask) != GST_TASK_STARTED) {
        GST_INFO_OBJECT (this, "Clearing buffers in queue");
        gst_omx_base_push_clear_queue (this, TRUE);

        GST_INFO_OBJECT (this, "Startig push task");
        gst_omx_base_push_start_task (this);
      }

      goto done;
      break;
    }
    default:
      break;
  }

  ret = GST_OMX_BASE_CLASS (parent_class)->event (base, pad, event);

done:
  return ret;
}


void
gst_omx_base_push_task (void *data)
{

  GstOmxBasePush *this;
  GstOmxBasePushClass *klass;
  GstOmxBufferData *bufdata = NULL;
  OMX_BUFFERHEADERTYPE *omx_buf = NULL;

  this = GST_OMX_BASE_PUSH (data);
  klass = GST_OMX_BASE_PUSH_GET_CLASS (this);

  GST_LOG_OBJECT (this, "Obtaining output buffer from queue");

  omx_buf = gst_omx_buf_queue_pop_buffer_check_release (this->queue_buffers);
  if (!omx_buf) {
    goto timeout;
  }

  bufdata = (GstOmxBufferData *) omx_buf->pAppPrivate;

  GST_LOG_OBJECT (this, "Pushing buffer %i", bufdata->id);
  if (klass->push_buffer) {
    this->fill_ret = klass->push_buffer (this, omx_buf);
    if (this->fill_ret != GST_FLOW_OK) {
      goto cbfailed;
    }
  }
  return;

cbfailed:
  {
    GST_WARNING_OBJECT (this, "Subclass failed to process buffer (id:%d): %s",
        bufdata->id, gst_flow_get_name (this->fill_ret));
    gst_omx_base_push_pause_task (this);
    return;
  }

timeout:
  {
    GST_WARNING_OBJECT (this,
        "Cannot acquire output buffer from pending queue");
    return;
  }
}

/* Push task implementation */
static OMX_ERRORTYPE
gst_omx_base_push_create_task (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_DEBUG_OBJECT (this, "Creating Push task...");
  this->pushtask = gst_task_create (gst_omx_base_push_task, (gpointer) this);
  if (!this->pushtask)
    GST_ERROR_OBJECT (this, "Failed to create Push task");

  g_static_rec_mutex_init (&this->taskmutex);
  gst_task_set_lock (this->pushtask, &this->taskmutex);

  GST_DEBUG_OBJECT (this, "Push task created");
  return error;
}

static OMX_ERRORTYPE
gst_omx_base_push_destroy_task (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_DEBUG_OBJECT (this, "Stopping task on srcpad...");

  if (gst_task_get_state (this->pushtask) != GST_TASK_STOPPED) {
    gst_omx_base_push_stop_task (this);
  }

  gst_object_unref (this->pushtask);
  GST_DEBUG_OBJECT (this, "Finished task on srcpad");

  return error;
}

static OMX_ERRORTYPE
gst_omx_base_push_pause_task (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_DEBUG_OBJECT (this, "Pausing push task ");
  /* gst_omx_buf_queue_release (this->queue_buffers, TRUE); */

  if (gst_task_get_state (this->pushtask) == GST_TASK_STOPPED)
    return error;

  if (!gst_task_pause (this->pushtask))
    GST_WARNING_OBJECT (this, "Failed to pause push task");

  GST_DEBUG_OBJECT (this, "Push task paused");
  return error;
}

static OMX_ERRORTYPE
gst_omx_base_push_start_task (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  gst_omx_buf_queue_release (this->queue_buffers, FALSE);

  GST_INFO_OBJECT (this, "Starting push task ");
  if (!gst_task_start (this->pushtask))
    GST_WARNING_OBJECT (this, "Failed to start push task");

  GST_INFO_OBJECT (this, "Push task started");
  return error;
}

static OMX_ERRORTYPE
gst_omx_base_push_stop_task (GstOmxBasePush * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  GST_DEBUG_OBJECT (this, "Stopping task on srcpad...");

  gst_omx_buf_queue_release (this->queue_buffers, TRUE);

  gst_omx_base_push_clear_queue (this, FALSE);

  if (!gst_task_join (this->pushtask))
    GST_WARNING_OBJECT (this, "Failed stop task ");

  GST_DEBUG_OBJECT (this, "Finished push task");

  return error;
}

static OMX_ERRORTYPE
gst_omx_base_push_clear_queue (GstOmxBasePush * this, gboolean fill)
{
  OMX_BUFFERHEADERTYPE *omx_buf = NULL;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBase *base;
  GstOmxBufferData *bufdata = NULL;
  GstOmxPad *pad = NULL;

  base = GST_OMX_BASE (this);

  omx_buf = gst_omx_buf_queue_pop_buffer_no_wait (this->queue_buffers);
  while (omx_buf) {
    bufdata = (GstOmxBufferData *) omx_buf->pAppPrivate;
    pad = bufdata->pad;
    GST_DEBUG_OBJECT (this, "Dropping buffer %d %p %p->%p", bufdata->id,
        bufdata, omx_buf, omx_buf->pBuffer);

    g_mutex_lock (&_omx_mutex);
    error = gst_omx_buf_tab_return_buffer (pad->buffers, omx_buf);
    if (fill)
      error = base->component->FillThisBuffer (base->handle, omx_buf);
    g_mutex_unlock (&_omx_mutex);

    omx_buf = gst_omx_buf_queue_pop_buffer_no_wait (this->queue_buffers);
  }
  GST_DEBUG_OBJECT (this, " Pushed queue empty");
  return error;
}
