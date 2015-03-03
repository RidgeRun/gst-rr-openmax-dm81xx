/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
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
 * SECTION:element-omx_base
 *
 * FIXME:Describe omx_base here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omx_base ! fakesink silent=TRUE
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

#include "gstomxbase.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_base_debug);
#define GST_CAT_DEFAULT gst_omx_base_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_PEER_ALLOC,
  PROP_NUM_INPUT_BUFFERS,
  PROP_NUM_OUTPUT_BUFFERS,
  PROP_NUM_BUFFERS
};

#define GST_OMX_BASE_NUM_INPUT_BUFFERS_DEFAULT    8
#define GST_OMX_BASE_NUM_OUTPUT_BUFFERS_DEFAULT   8
#define GST_OMX_BASE_NUM_BUFFERS_DEFAULT   	      0

#define gst_omx_base_parent_class parent_class
static GstElementClass *parent_class = NULL;

static void gst_omx_base_base_init (gpointer g_class);
static void gst_omx_base_class_init (GstOmxBaseClass * klass);
static void gst_omx_base_init (GstOmxBase * src, gpointer g_class);

GType
gst_omx_base_get_type (void)
{
  static volatile gsize omx_base_type = 0;

  if (g_once_init_enter (&omx_base_type)) {
    GType _type;
    static const GTypeInfo omx_base_info = {
      sizeof (GstOmxBaseClass),
      (GBaseInitFunc) gst_omx_base_base_init,
      NULL,
      (GClassInitFunc) gst_omx_base_class_init,
      NULL,
      NULL,
      sizeof (GstOmxBase),
      0,
      (GInstanceInitFunc) gst_omx_base_init,
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstOmxBase", &omx_base_info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&omx_base_type, _type);
  }

  return omx_base_type;
}

static void gst_omx_base_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_base_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_omx_base_finalize (GObject * object);
static OMX_ERRORTYPE gst_omx_base_allocate_omx (GstOmxBase * this,
    gchar * handle_name);
static OMX_ERRORTYPE gst_omx_base_free_omx (GstOmxBase * this);
static OMX_ERRORTYPE gst_omx_base_start (GstOmxBase * this,
    OMX_BUFFERHEADERTYPE * omxpeerbuf);
static OMX_ERRORTYPE gst_omx_base_stop (GstOmxBase * this);
static OMX_ERRORTYPE gst_omx_base_alloc_buffers (GstOmxBase * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_free_buffers (GstOmxBase * this,
    GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE gst_omx_base_for_each_pad (GstOmxBase * this,
    GstOmxBasePadFunc func, GstPadDirection direction, gpointer data);
static OMX_ERRORTYPE gst_omx_base_event_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2,
    gpointer eventdata);
static OMX_ERRORTYPE gst_omx_base_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_base_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer);
static OMX_ERRORTYPE gst_omx_base_push_buffers (GstOmxBase * this,
    GstOmxPad * pad, gpointer data);
static GstStateChangeReturn gst_omx_base_change_state (GstElement * element,
    GstStateChange transition);
static GstFlowReturn gst_omx_base_chain (GstPad * pad, GstBuffer * buf);
static gboolean gst_omx_base_event_handler (GstPad * pad, GstEvent * event);
static gboolean gst_omx_base_set_caps (GstPad * pad, GstCaps * caps);

static gboolean gst_omx_base_alloc_buffer (GstPad * pad, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buffer);
static OMX_ERRORTYPE
gst_omx_base_flush_ports (GstOmxBase * this, GstOmxPad * pad, gpointer data);
static OMX_ERRORTYPE
gst_omx_base_set_flushing_pad (GstOmxBase * this, GstOmxPad * pad,
    gpointer data);

/* GObject vmethod implementations */

static void
gst_omx_base_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (gst_omx_base_debug, "omx_base",
      0, "RidgeRun's OMX base element");
}

/* initialize the omx's class */
static void
gst_omx_base_class_init (GstOmxBaseClass * klass)
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
      GST_DEBUG_FUNCPTR (gst_omx_base_change_state);

  gst_element_class_set_details_simple (gstelement_class,
      "omxdec",
      "Generic/Filter",
      "RidgeRun's OMX based base",
      "Michael Gruner <michael.gruner@ridgerun.com>, "
      "Jose Jimenez <jose.jimenez@ridgerun.com>");

  gobject_class->set_property = gst_omx_base_set_property;
  gobject_class->get_property = gst_omx_base_get_property;
  gobject_class->finalize = gst_omx_base_finalize;

  g_object_class_install_property (gobject_class, PROP_PEER_ALLOC,
      g_param_spec_boolean ("peer-alloc",
          "Try to use buffers from downstream element",
          "Try to use buffers from downstream element", FALSE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_NUM_INPUT_BUFFERS,
      g_param_spec_uint ("input-buffers", "Input buffers",
          "OMX input buffers number",
          1, 10, GST_OMX_BASE_NUM_INPUT_BUFFERS_DEFAULT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_NUM_OUTPUT_BUFFERS,
      g_param_spec_uint ("output-buffers", "Output buffers",
          "OMX output buffers number",
          1, 16, GST_OMX_BASE_NUM_OUTPUT_BUFFERS_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_NUM_BUFFERS,
      g_param_spec_int ("num-buffers", "Number of buffers",
          "The number of Buffers to be processed (0 : process all buffers)",
          0, G_MAXINT, GST_OMX_BASE_NUM_BUFFERS_DEFAULT, G_PARAM_READWRITE));
}

static OMX_ERRORTYPE
gst_omx_base_allocate_omx (GstOmxBase * this, gchar * handle_name)
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
      (GstOmxEventHandler) gst_omx_base_event_callback;
  this->callbacks->EmptyBufferDone =
      (GstOmxEmptyBufferDone) gst_omx_base_empty_callback;
  this->callbacks->FillBufferDone =
      (GstOmxFillBufferDone) gst_omx_base_fill_callback;

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

  if (!strstr (handle_name, "DSP")) {
    GST_INFO ("Video component");
    GST_OMX_INIT_STRUCT (&init, OMX_PORT_PARAM_TYPE);
    init.nPorts = 2;
    init.nStartPortNumber = 0;
    g_mutex_lock (&_omx_mutex);
    error = OMX_SetParameter (this->handle, OMX_IndexParamVideoInit, &init);
    g_mutex_unlock (&_omx_mutex);
    if (error != OMX_ErrorNone)
      goto initport;
  } else {
    GST_INFO ("Audio component");
    GST_OMX_INIT_STRUCT (&init, OMX_PORT_PARAM_TYPE);
    init.nPorts = 2;
    init.nStartPortNumber = 0;
    this->audio_component = TRUE;
    g_mutex_lock (&_omx_mutex);
    error = OMX_SetParameter (this->handle, OMX_IndexParamAudioInit, &init);
    g_mutex_unlock (&_omx_mutex);
    if (error != OMX_ErrorNone)
      goto initport;
  }
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
initport:
  {
    /* TODO: should I free the handle here? */
    GST_ERROR_OBJECT (this, "Unable to init component ports: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_free_omx (GstOmxBase * this)
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

/* initialize the new element
 * initialize instance structure
 */
static void
gst_omx_base_init (GstOmxBase * this, gpointer g_class)
{
  GstOmxBaseClass *klass = GST_OMX_BASE_CLASS (g_class);
  OMX_ERRORTYPE error;

  GST_INFO_OBJECT (this, "Initializing %s", GST_OBJECT_NAME (this));

  this->requested_size = 0;
  this->audio_component = FALSE;
  this->peer_alloc = FALSE;
  this->flushing = FALSE;
  this->started = FALSE;
  this->first_buffer = TRUE;
  this->interlaced = FALSE;

  this->input_buffers = GST_OMX_BASE_NUM_INPUT_BUFFERS_DEFAULT;
  this->output_buffers = GST_OMX_BASE_NUM_OUTPUT_BUFFERS_DEFAULT;

  this->pads = NULL;
  this->fill_ret = GST_FLOW_OK;
  this->state = OMX_StateInvalid;
  g_mutex_init (&this->waitmutex);
  g_cond_init (&this->waitcond);

  this->num_buffers = 0;
  this->cont = 0;
  this->num_buffers_mutex = g_mutex_new();
  this->num_buffers_cond  = g_cond_new();
  error = gst_omx_base_allocate_omx (this, klass->handle_name);
  if (GST_OMX_FAIL (error)) {
    GST_ELEMENT_ERROR (this, LIBRARY,
        INIT, (gst_omx_error_to_str (error)), (NULL));
  }
}

static void
gst_omx_base_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxBase *this = GST_OMX_BASE (object);

  switch (prop_id) {
    case PROP_PEER_ALLOC:
      this->peer_alloc = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting peer-alloc to %d", this->peer_alloc);
      break;
    case PROP_NUM_INPUT_BUFFERS:
      this->input_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting input-buffers to %d",
          this->input_buffers);
      break;
    case PROP_NUM_OUTPUT_BUFFERS:
      this->output_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting output-buffers to %d",
          this->output_buffers);
      break;
    case PROP_NUM_BUFFERS:
      this->num_buffers = g_value_get_int (value);
      GST_INFO_OBJECT (this, "Setting num-buffers to %d",
          this->num_buffers);
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
    case PROP_NUM_INPUT_BUFFERS:
      g_value_set_uint (value, this->input_buffers);
      break;
    case PROP_NUM_OUTPUT_BUFFERS:
      g_value_set_uint (value, this->output_buffers);
      break;
    case PROP_NUM_BUFFERS:
      g_value_set_int (value, this->num_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_base_mark_free (gpointer data, gpointer user_data)
{
  GstOmxBufTab *buftab = (GstOmxBufTab *) user_data;
  GstOmxBufTabNode *node = (GstOmxBufTabNode *) data;

  GST_DEBUG ("Marking %p as free ", node->buffer);
  gst_omx_buf_tab_return_buffer (buftab, node->buffer);
}

static GstFlowReturn
gst_omx_base_chain (GstPad * pad, GstBuffer * buf)
{
  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxbuf;
  gboolean busy;
  OMX_BUFFERHEADERTYPE *omxpeerbuf = NULL;
  GstOmxPad *omxpad = GST_OMX_PAD (pad);
  GstOmxBufferData *bufdata = NULL;
  gboolean flushing;

  GST_OBJECT_LOCK (this);
  flushing = this->flushing;
  GST_OBJECT_UNLOCK (this);

  if (flushing)
    goto flushing;

  if (this->fill_ret)
    goto pusherror;

  if (!this->started) {
    if (GST_OMX_IS_OMX_BUFFER (buf)) {
      GST_INFO_OBJECT (this, "Sharing upstream peer buffers");
      if (buf->parent != NULL) {
		omxpeerbuf = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_MALLOCDATA (buf->parent);
	  } else {
		omxpeerbuf = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_MALLOCDATA (buf);
	  }
    }

    GST_INFO_OBJECT (this, "Starting component");
    error = gst_omx_base_start (this, omxpeerbuf);
    if (GST_OMX_FAIL (error))
      goto nostart;
  }

  /* If an upstream asked for buffer allocations, we may have buffers
     marked as busy even though no buffers have been processed yet. This
     is the time to mark them as free and start the steady state */
  if (this->first_buffer) {
    g_list_foreach (omxpad->buffers->table, gst_omx_base_mark_free,
        omxpad->buffers);
    this->first_buffer = FALSE;
  }

  if (GST_OMX_IS_OMX_BUFFER (buf)) {

    if (buf->parent != NULL) {
      omxpeerbuf = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_MALLOCDATA (buf->parent);
    } else {
      omxpeerbuf = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_MALLOCDATA (buf);
    }
    GST_LOG_OBJECT (this, "Received an OMX buffer %p->%p", omxpeerbuf,
        omxpeerbuf->pBuffer);

    error =
        gst_omx_buf_tab_find_buffer (omxpad->buffers, omxpeerbuf, &omxbuf,
        &busy);
    if (GST_OMX_FAIL (error))
      goto notfound;

    if (busy) {
      GST_ERROR_OBJECT (this, "Buffer in buffer list is busy");
    }
    gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);
  } else {

    GST_LOG_OBJECT (this, "Not an OMX buffer, requesting a free buffer");
    error = gst_omx_buf_tab_get_free_buffer (omxpad->buffers, &omxbuf);
    if (GST_OMX_FAIL (error))
      goto nofreebuffer;
    gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);
    GST_LOG_OBJECT (this, "Received buffer %p, copying data", omxbuf);
    memcpy (omxbuf->pBuffer, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
  }

  if (omxpeerbuf != NULL) {
    omxbuf->nFilledLen = omxpeerbuf->nFilledLen;
    omxbuf->nOffset = omxpeerbuf->nOffset;
  } else {
    omxbuf->nFilledLen = GST_BUFFER_SIZE (buf);
    omxbuf->nOffset = 0;
  }
  omxbuf->nTimeStamp = GST_BUFFER_TIMESTAMP (buf);

  bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;
/* We need to grab a reference for the buffer since EmptyThisBuffer callback might 
return before we check if the buffer is interlaced */
  bufdata->buffer = gst_buffer_ref (buf);

  if (this->interlaced)
    omxbuf->nFlags = OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE;


  GST_LOG_OBJECT (this, "Emptying buffer %d %p %p->%p", bufdata->id, bufdata,
      omxbuf, omxbuf->pBuffer);
  g_mutex_lock (&_omx_mutex);
  error = this->component->EmptyThisBuffer (this->handle, omxbuf);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error)) {
    goto noempty;
  }

  if (this->interlaced && GST_OMX_IS_OMX_BUFFER (buf)) {
    OMX_BUFFERHEADERTYPE tmpbuf;
    gint tmpid;
    tmpbuf.pBuffer = omxbuf->pBuffer + this->field_offset;

    GST_LOG_OBJECT (this, "Getting bottom field buffer  %p->%p", &tmpbuf,
        tmpbuf.pBuffer);

    tmpbuf.pAppPrivate = bufdata;

    tmpid = ((GstOmxBufferData *) (tmpbuf.pAppPrivate))->id;

    ((GstOmxBufferData *) (tmpbuf.pAppPrivate))->id =
        ((GstOmxBufferData *) (tmpbuf.pAppPrivate))->id +
        omxpad->port->nBufferCountActual;

    error =
        gst_omx_buf_tab_find_buffer (omxpad->buffers, &tmpbuf, &omxbuf, &busy);
    if (GST_OMX_FAIL (error)) {
      goto notfound;
    }
    if (busy) {
      GST_ERROR_OBJECT (this, "Buffer in buffer list is busy");
    }

    gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);

    ((GstOmxBufferData *) (tmpbuf.pAppPrivate))->id = tmpid;

    /* FilledLen calculated to achive the sencond field chroma position 
     * to be at 2/3 of the buffer size */
    omxbuf->nFilledLen =
        (((GST_BUFFER_SIZE (buf)) * 3) >> 2) - omxpeerbuf->nOffset;
    omxbuf->nOffset = omxpeerbuf->nOffset;
    omxbuf->nTimeStamp = GST_BUFFER_TIMESTAMP (buf);
    omxbuf->nFlags = OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE |
        OMX_TI_BUFFERFLAG_VIDEO_FRAME_TYPE_INTERLACE_BOTTOM;
    bufdata = (GstOmxBufferData *) omxbuf->pAppPrivate;
    bufdata->buffer = gst_buffer_ref (buf);

    GST_LOG_OBJECT (this, "Emptying buffer %d %p %p->%p", bufdata->id, bufdata,
        omxbuf, omxbuf->pBuffer);
    g_mutex_lock (&_omx_mutex);
    error = this->component->EmptyThisBuffer (this->handle, omxbuf);
    g_mutex_unlock (&_omx_mutex);
    if (GST_OMX_FAIL (error)) {
      gst_buffer_unref (buf);
      goto noempty;
    }
  }
  /* g_mutex_lock (&this->waitmutex); */
  /* g_cond_wait (&this->waitcond, &this->waitmutex); */
  /* g_mutex_unlock (&this->waitmutex); */
  gst_buffer_unref (buf);
  return GST_FLOW_OK;

flushing:
  {
    GST_ERROR_OBJECT (this, "Discarding buffer %d while flushing", bufdata->id);
    gst_buffer_unref (buf);
    return GST_FLOW_OK;
  }
pusherror:
  {
    GST_LOG_OBJECT (this, "Dropping buffer, push error %s",
        gst_flow_get_name (this->fill_ret));
    gst_buffer_unref (buf);
    return this->fill_ret;
  }
nostart:
  {
    GST_ERROR_OBJECT (this, "Unable to start component: %s",
        gst_omx_error_to_str (error));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
notfound:
  {
    GST_ERROR_OBJECT (this,
        "Buffer is marked as OMX, but was not found on buftab: %s",
        gst_omx_error_to_str (error));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
nofreebuffer:
  {
    GST_ERROR_OBJECT (this, "Unable to get a free buffer: %s",
        gst_omx_error_to_str (error));
    GList *buffers;
    OMX_BUFFERHEADERTYPE *buffer;
    GstOmxBufTabNode *node;
    gint j = 0;

    buffers = omxpad->buffers->table;
    GST_ERROR_OBJECT (this, "Printing list of buffers");
    while (buffers) {
      node = (GstOmxBufTabNode *) buffers->data;
      buffer = node->buffer;
      GST_DEBUG_OBJECT (this, "Buffer number %u: %p->%p", j, buffer,
          buffer->pBuffer);
      GST_DEBUG_OBJECT (this, "This buffer is marked as %s ",
          node->busy ? "Used" : "Free");
      j++;
      buffers = g_list_next (buffers);
    }
    gst_buffer_unref (buf);
    return GST_FLOW_WRONG_STATE;
  }
noempty:
  {
    GST_ELEMENT_ERROR (this, LIBRARY, ENCODE, (gst_omx_error_to_str (error)),
        (NULL));
    gst_buffer_unref (buf);
    gst_buffer_unref (buf);     /*If Empty this buffer is not successful we have to unref the buffer manually */
    return GST_FLOW_ERROR;
  }
}

static void
gst_omx_base_finalize (GObject * object)
{
  GstOmxBase *this = GST_OMX_BASE (object);

  GST_INFO_OBJECT (this, "Finalizing %s", GST_OBJECT_NAME (this));

  g_list_free_full (this->pads, gst_object_unref);
  gst_omx_base_free_omx (this);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_omx_base_check_caps (GstPad * pad, GstCaps * newcaps)
{
  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
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

/* vmethod implementations */
static gboolean
gst_omx_base_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
  GstOmxBaseClass *klass = GST_OMX_BASE_GET_CLASS (this);
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (!klass->parse_caps)
    goto noparsecaps;

  if (!klass->parse_caps (pad, caps))
    goto capsinvalid;

  if (!gst_omx_base_check_caps (pad, caps))
    goto noresolutionchange;

  GST_INFO_OBJECT (this, "%s:%s resolution changed, calling port renegotiation",
      GST_DEBUG_PAD_NAME (pad));
  if (OMX_StateLoaded < this->state) {
    GST_INFO_OBJECT (this, "Resetting component");
    error = gst_omx_base_stop (this);
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

static OMX_ERRORTYPE
gst_omx_base_start (GstOmxBase * this, OMX_BUFFERHEADERTYPE * omxpeerbuf)
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
      gst_omx_base_for_each_pad (this, gst_omx_base_alloc_buffers, GST_PAD_SRC,
      NULL);
  if (GST_OMX_FAIL (error))
    goto noalloc;

  GST_INFO_OBJECT (this, "Allocating buffers for sink ports");
  error =
      gst_omx_base_for_each_pad (this, gst_omx_base_alloc_buffers, GST_PAD_SINK,
      omxpeerbuf);
  if (GST_OMX_FAIL (error))
    goto noalloc;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_base_wait_for_condition (this,
      gst_omx_base_condition_state, (gpointer) OMX_StateIdle,
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
  error = gst_omx_base_wait_for_condition (this,
      gst_omx_base_condition_state, (gpointer) OMX_StateExecuting,
      (gpointer) & this->state);
  if (GST_OMX_FAIL (error))
    goto starthandle;

  GST_INFO_OBJECT (this, "Pushing output buffers");
  error =
      gst_omx_base_for_each_pad (this, gst_omx_base_push_buffers,
      GST_PAD_UNKNOWN, NULL);
  if (GST_OMX_FAIL (error))
    goto nopush;

  this->started = TRUE;

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
gst_omx_base_stop (GstOmxBase * this)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (!this->started)
    goto alreadystopped;

  if (!this->flushing) {
    GST_OBJECT_LOCK (this);
    this->flushing = TRUE;
    GST_OBJECT_UNLOCK (this);
    /* DSP does not support flush ports */
    if (!(this->audio_component)) {
      GST_INFO_OBJECT (this, "Flushing ports");
      error =
          gst_omx_base_for_each_pad (this, gst_omx_base_flush_ports,
          GST_PAD_UNKNOWN, NULL);
      if (GST_OMX_FAIL (error))
        goto noflush;
    }
  }
  GST_INFO_OBJECT (this, "Sending handle to Idle");
  g_mutex_lock (&_omx_mutex);
  error = OMX_SendCommand (this->handle, OMX_CommandStateSet, OMX_StateIdle,
      NULL);
  g_mutex_unlock (&_omx_mutex);
  if (GST_OMX_FAIL (error))
    goto statechange;

  GST_INFO_OBJECT (this, "Waiting for handle to become Idle");
  error = gst_omx_base_wait_for_condition (this,
      gst_omx_base_condition_state, (gpointer) OMX_StateIdle,
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
      gst_omx_base_for_each_pad (this, gst_omx_base_free_buffers,
      GST_PAD_UNKNOWN, NULL);
  if (GST_OMX_FAIL (error))
    goto nofree;

  GST_INFO_OBJECT (this, "Waiting for handle to become Loaded");
  error = gst_omx_base_wait_for_condition (this,
      gst_omx_base_condition_state, (gpointer) OMX_StateLoaded,
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

static GstStateChangeReturn
gst_omx_base_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxBase *this = GST_OMX_BASE (element);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /*Start processing buffers for DSP components */
      if (this->audio_component) {
        GST_OBJECT_LOCK (this);
        this->flushing = FALSE;
        GST_OBJECT_UNLOCK (this);
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      /*Dont try to push any more buffers for DSP components */
      if (this->audio_component) {
        GST_OBJECT_LOCK (this);
        this->flushing = TRUE;
        GST_OBJECT_UNLOCK (this);
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_omx_base_stop (this);
      break;
    default:
      break;
  }

  return ret;
}

static OMX_ERRORTYPE
gst_omx_base_for_each_pad (GstOmxBase * this, GstOmxBasePadFunc func,
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

static OMX_ERRORTYPE
gst_omx_base_peer_alloc_buffer (GstOmxBase * this, GstOmxPad * pad,
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
gst_omx_base_use_buffer (GstOmxBase * this, GList ** bufferlist,
    gpointer * pbuffer, guint32 * size)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxpeerbuffer = NULL;
  GList *node;
  if (!bufferlist)
    goto nobuffer;

  node = *bufferlist;
  omxpeerbuffer = ((GstOmxBufTabNode *) node->data)->buffer;
  *bufferlist = g_list_next (node);
  *pbuffer = omxpeerbuffer->pBuffer;
  *size = omxpeerbuffer->nAllocLen;

  GST_LOG_OBJECT (this, "Using buffer %p->%p", omxpeerbuffer, pbuffer);
  return error;

nobuffer:
  {
    GST_ERROR_OBJECT (this, "No buffer left on the list");
    error = OMX_ErrorInsufficientResources;
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_init_use_buffer (GstOmxBase * this, GstOmxPad * pad,
    GList ** bufferlist, OMX_BUFFERHEADERTYPE * omxpeerbuffer)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxBufferData *peerbufdata = NULL;
  GstOmxPad *peerpad = NULL;
  guint numbufs = pad->port->nBufferCountActual;

  peerbufdata = (GstOmxBufferData *) omxpeerbuffer->pAppPrivate;
  peerpad = peerbufdata->pad;

  if (this->interlaced) {
    numbufs = numbufs >> 1;
    this->field_offset =
        (omxpeerbuffer->nFilledLen + omxpeerbuffer->nOffset) / 3;
  }

  if (numbufs > peerpad->port->nBufferCountActual) {
    error = OMX_ErrorInsufficientResources;
    goto nouse;
  }
  *bufferlist = peerpad->buffers->table;
  if (!*bufferlist) {
    g_print ("NULL pointer\n");
  }
  return error;

nouse:
  {
    GST_ERROR_OBJECT (this,
        "Not enough buffers provided by the peer, can't share buffers: %s",
        gst_omx_error_to_str (error));
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_alloc_buffers (GstOmxBase * this, GstOmxPad * pad, gpointer data)
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

  if (data) {
    OMX_BUFFERHEADERTYPE *omxpeerbuffer = (OMX_BUFFERHEADERTYPE *) data;
    error =
        gst_omx_base_init_use_buffer (this, pad, &peerbuffers, omxpeerbuffer);
    if (error != OMX_ErrorNone)
      goto out;
  }

  if (this->interlaced)
    divided_buffers = TRUE;

  GST_DEBUG_OBJECT (this, "Allocating buffers for %s:%s",
      GST_DEBUG_PAD_NAME (GST_PAD (pad)));

  for (i = 0; i < pad->port->nBufferCountActual; ++i) {

    /* First we try to ask for downstream OMX buffers */
    if (GST_PAD_IS_SRC (pad) && this->peer_alloc) {
      error = gst_omx_base_peer_alloc_buffer (this, pad, &pbuffer, &size);
    } else if (GST_PAD_IS_SINK (pad) && data) {
      if (top_field) {
        currentbuffer = peerbuffers;
        error = gst_omx_base_use_buffer (this, &peerbuffers, &pbuffer, &size);
      } else {
        pbuffer = pbuffer + this->field_offset;
      }
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
    GST_ERROR_OBJECT (this, "Failed to allocate buffers");
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
gst_omx_base_free_buffers (GstOmxBase * this, GstOmxPad * pad, gpointer data)
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
gst_omx_base_push_buffers (GstOmxBase * this, GstOmxPad * pad, gpointer data)
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

sinkpad:
  {
    GST_DEBUG_OBJECT (this, "Skipping sink pad %s:%s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)));
    return error;
  }

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
gst_omx_base_flush_ports (GstOmxBase * this, GstOmxPad * pad, gpointer data)
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
  error = gst_omx_base_wait_for_condition (this,
      gst_omx_base_condition_disabled, (gpointer) & pad->flushing, NULL);
  if (GST_OMX_FAIL (error))
    goto noflush;

  return error;

noflush:
  {
    GST_ERROR_OBJECT (this, "Unable to flush port %d", (int) port->nPortIndex);
    return error;
  }

}

static OMX_ERRORTYPE
gst_omx_base_enable_pad (GstOmxBase * this, GstOmxPad * pad, gpointer data)
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
gst_omx_base_set_flushing_pad (GstOmxBase * this, GstOmxPad * pad,
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
gst_omx_base_event_callback (OMX_HANDLETYPE handle,
    gpointer data,
    OMX_EVENTTYPE event, guint32 nevent1, guint32 nevent2, gpointer eventdata)
{
  GstOmxBase *this = GST_OMX_BASE (data);
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
        gst_omx_base_for_each_pad (this, gst_omx_base_enable_pad,
            GST_PAD_UNKNOWN, (gpointer) nevent2);
        g_cond_signal (&this->waitcond);
        g_mutex_unlock (&this->waitmutex);
      }

      if (OMX_CommandFlush == nevent1) {
        g_mutex_lock (&this->waitmutex);
        gst_omx_base_for_each_pad (this, gst_omx_base_set_flushing_pad,
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
gst_omx_base_fill_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * outbuf)
{
  GstOmxBase *this = GST_OMX_BASE (data);
  GstOmxBaseClass *klass = GST_OMX_BASE_GET_CLASS (this);
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

  if (this->fill_ret != GST_FLOW_OK)
    goto drop;

  GST_LOG_OBJECT (this, "Current %d Pending %d Target %d Next %d",
      GST_STATE (this), GST_STATE_PENDING (this), GST_STATE_TARGET (this),
      GST_STATE_NEXT (this));
  if (GST_STATE_PAUSED > GST_STATE (this))
    goto flushing;

  gst_omx_buf_tab_use_buffer (bufdata->pad->buffers, outbuf);

  if (klass->omx_fill_buffer) {
    this->fill_ret = klass->omx_fill_buffer (this, outbuf);
    if (this->fill_ret != GST_FLOW_OK) {
      goto cbfailed;
    }
  }

  /* In some cases the EoS event arrives before we encode the
  *  desired amount of frames using the num_buffers property we
  *  can be sure that we will encode this amount of frames (i.e.snapshots)
  */

  if(this->num_buffers) {
	this->cont++;
	if(this->cont >= this->num_buffers) {
	    g_cond_signal(this->num_buffers_cond);
	    this->cont = 0;
	}
  }
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

cbfailed:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (this), CORE, PAD,
        ("Subclass failed to process buffer (id:%d): %s",
            bufdata->id, gst_flow_get_name (this->fill_ret)), (NULL));

    return error;
  }
drop:
  {
    GST_LOG_OBJECT (this, "Dropping buffer, push error %s",
        gst_flow_get_name (this->fill_ret));
    g_mutex_lock (&_omx_mutex);
    error = this->component->FillThisBuffer (this->handle, outbuf);
    g_mutex_unlock (&_omx_mutex);
    return error;
  }
}

static OMX_ERRORTYPE
gst_omx_base_empty_callback (OMX_HANDLETYPE handle,
    gpointer data, OMX_BUFFERHEADERTYPE * buffer)
{
  GstOmxBase *this = GST_OMX_BASE (data);
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  while (!bufdata->buffer) {
  };
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

gboolean
gst_omx_base_add_pad (GstOmxBase * this, GstPad * pad)
{
  GST_INFO_OBJECT (this, "Adding pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  if (GST_PAD_SINK == GST_PAD_DIRECTION (pad)) {
    gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (gst_omx_base_chain));
    gst_pad_set_event_function (pad,
        GST_DEBUG_FUNCPTR (gst_omx_base_event_handler));
    gst_pad_set_setcaps_function (pad,
        GST_DEBUG_FUNCPTR (gst_omx_base_set_caps));
    gst_pad_set_bufferalloc_function (pad, gst_omx_base_alloc_buffer);
  }

  gst_object_ref (pad);
  this->pads = g_list_append (this->pads, pad);

  return TRUE;
}


OMX_ERRORTYPE
gst_omx_base_wait_for_condition (GstOmxBase * this,
    GstOmxBaseCondition condition, gpointer arg1, gpointer arg2)
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
gst_omx_base_condition_state (gpointer targetstate, gpointer currentstate)
{
  OMX_STATETYPE _targetstate = (OMX_STATETYPE) targetstate;
  OMX_STATETYPE _currentstate = *(OMX_STATETYPE *) currentstate;

  return _targetstate == _currentstate;
}

gboolean
gst_omx_base_condition_enabled (gpointer enabled, gpointer dummy)
{
  return *(gboolean *) enabled;
}

gboolean
gst_omx_base_condition_disabled (gpointer enabled, gpointer dummy)
{
  return !*(gboolean *) enabled;
}

static GstFlowReturn
gst_omx_base_alloc_buffer (GstPad * pad, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buffer)
{
  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
  GstOmxPad *omxpad = GST_OMX_PAD (pad);
  gchar *capsdesc;
  OMX_BUFFERHEADERTYPE *omxbuf;
  OMX_ERRORTYPE error;

  capsdesc = gst_caps_to_string (caps);
  *buffer = NULL;

  GST_DEBUG_OBJECT (this,
      "Alloc buffer called to %s:%s with caps %s and size %d",
      GST_DEBUG_PAD_NAME (pad), capsdesc, size);
  g_free (capsdesc);

  this->requested_size = size;
  if (!gst_pad_set_caps (pad, caps))
    goto invalidcaps;

  if (!this->started) {
    GST_INFO_OBJECT (this, "Starting component");
    error = gst_omx_base_start (this, NULL);
    if (GST_OMX_FAIL (error))
      goto nostart;
  }

  /* If we are here, buffers were successfully allocated */
  error = gst_omx_buf_tab_get_free_buffer (omxpad->buffers, &omxbuf);
  if (GST_OMX_FAIL (error))
    goto nofreebuf;

  gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);

  GST_DEBUG_OBJECT (this, "Alloc buffer returned buffer with size %d",
      (int) omxbuf->nAllocLen);

  *buffer = gst_buffer_new ();
  GST_BUFFER_SIZE (*buffer) = omxbuf->nAllocLen;
  GST_BUFFER_DATA (*buffer) = (guint8 *) omxbuf;
  GST_BUFFER_MALLOCDATA (*buffer) = NULL;
  GST_BUFFER_CAPS (*buffer) = gst_caps_ref (caps);
  GST_BUFFER_FLAGS (*buffer) |= GST_OMX_BUFFER_FLAG;

  return GST_FLOW_OK;

invalidcaps:
  {
    GST_ERROR_OBJECT (this, "The caps weren't accepted");
    return GST_FLOW_NOT_NEGOTIATED;
  }
nofreebuf:
  {
    GST_ERROR_OBJECT (this, "Unable to get free buffer: %s",
        gst_omx_error_to_str (error));
    return GST_FLOW_ERROR;
  }
nostart:
  {
    GST_ERROR_OBJECT (this, "Unable to start component: %s",
        gst_omx_error_to_str (error));
    return GST_FLOW_ERROR;
  }
}


void
gst_omx_base_release_buffer (gpointer data)
{

  OMX_BUFFERHEADERTYPE *buffer = (OMX_BUFFERHEADERTYPE *) data;
  OMX_ERRORTYPE error;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  GstOmxPad *pad = bufdata->pad;
  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
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
gst_omx_base_event_handler (GstPad * pad, GstEvent * event)
{

  GstOmxBase *this = GST_OMX_BASE (GST_OBJECT_PARENT (pad));
  OMX_ERRORTYPE error = OMX_ErrorNone;

  if (G_UNLIKELY (this == NULL)) {
    gst_event_unref (event);
    return FALSE;
  }

  GST_DEBUG_OBJECT (this, "handling event %p %" GST_PTR_FORMAT, event, event);

  switch (GST_EVENT_TYPE (event)) {
      /* We only care for the EOS event, put the component in flush state so it doesn't 
       * try to process any more buffers. */
    case GST_EVENT_EOS:
    {
	  /* In some cases the EoS event arrives before we encode the
	  * desired amount of frames using the num_buffers property we
      *  can be sure that we will encode this amount of frames (i.e.snapshots)
      */

	  if(this->num_buffers){
	    g_mutex_lock(this->num_buffers_mutex);
	    g_cond_wait(this->num_buffers_cond,this->num_buffers_mutex);
	    g_mutex_unlock(this->num_buffers_mutex);
	  }
      GST_INFO_OBJECT (this, "EOS received, flushing ports");
      GST_OBJECT_LOCK (this);
      this->flushing = TRUE;
      GST_OBJECT_UNLOCK (this);
      /*  DSP does not support flush ports */
      if (!(this->audio_component)) {
        error =
            gst_omx_base_for_each_pad (this, gst_omx_base_flush_ports,
            GST_PAD_UNKNOWN, NULL);
        if (GST_OMX_FAIL (error))
          goto noflush_eos;
      }
      break;
    }
    default:
      break;
  }
  /* Handle everything else as default */
  gst_pad_event_default (pad, event);

  return TRUE;

noflush_eos:
  GST_ERROR_OBJECT (this, "Unable to flush component after EOS: %s ",
      gst_omx_error_to_str (error));
  return FALSE;
}
