/*
 * GStreamer
 * Copyright (C) 2015 Eugenia Guzman <eugenia.guzman@ridgerun.com>
 * Copyright (C) 2014 Diego Solano <diego.solano@ridgerun.com>
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
 * SECTION:element-omxbufferalloc
 *
 * FIXME:Describe omxbufferalloc here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! omxbufferalloc ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <dlfcn.h>
#include <string.h>

#include "gstomxbufferalloc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omxbufferalloc_debug);
#define GST_CAT_DEFAULT gst_omxbufferalloc_debug

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
  PROP_NUMBUFFERS
};

#define GST_OMX_BUFFERALLOC_SILENT_DEFAULT	FALSE
#define GST_OMX_BUFFERALLOC_NUMBUFFERS_DEFAULT	10

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstOmxBufferAlloc, gst_omx_buffer_alloc, GstElement,
    GST_TYPE_ELEMENT);

static void gst_omx_buffer_alloc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_buffer_alloc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_omx_buffer_alloc_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_omx_buffer_alloc_chain (GstPad * pad, GstBuffer * buf);
static OMX_ERRORTYPE gst_omx_buffer_alloc_free_buffers (GstOmxBufferAlloc *
    this, GstOmxPad * pad);

GstFlowReturn gst_omx_buffer_alloc_allocate_buffer (GstPad * pad,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);

static GstStateChangeReturn gst_omx_buffer_alloc_change_state (GstElement *
    element, GstStateChange transition);

static GMutex *imp_mutex;
static GHashTable *implementations;

static void
gst_omx_buffer_alloc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "omxbufferalloc",
      "FIXME:Generic",
      "FIXME:Generic Template Element",
      "Eugenia Guzman <eugenia.guzman@ridgerun.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));


}

static void
gst_omx_buffer_alloc_class_init (GstOmxBufferAllocClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_omx_buffer_alloc_set_property;
  gobject_class->get_property = gst_omx_buffer_alloc_get_property;
  gstelement_class->change_state = gst_omx_buffer_alloc_change_state;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          GST_OMX_BUFFERALLOC_SILENT_DEFAULT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_NUMBUFFERS,
      g_param_spec_uint ("numBuffers", "Number of buffers",
          "Number of buffers to be allocated by component",
          1, 16, GST_OMX_BUFFERALLOC_NUMBUFFERS_DEFAULT, G_PARAM_READWRITE));

  GST_DEBUG_CATEGORY_INIT (gst_omxbufferalloc_debug, "omxbufferalloc",
      0, "Template omxbufferalloc");
}

static void
gst_omx_buffer_alloc_init (GstOmxBufferAlloc * this,
    GstOmxBufferAllocClass * gclass)
{
  this->silent = GST_OMX_BUFFERALLOC_SILENT_DEFAULT;
  this->num_buffers = GST_OMX_BUFFERALLOC_NUMBUFFERS_DEFAULT;
  this->buffers = NULL;
  this->omx_library = "libOMX_Core.so";
  this->cnt = -1;

  this->sinkpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&sink_template), "sink"));

  gst_pad_set_setcaps_function (this->sinkpad,
      GST_DEBUG_FUNCPTR (gst_omx_buffer_alloc_set_caps));
  gst_pad_set_getcaps_function (this->sinkpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (this->sinkpad,
      GST_DEBUG_FUNCPTR (gst_omx_buffer_alloc_chain));
  gst_pad_set_bufferalloc_function (this->sinkpad,
      GST_DEBUG_FUNCPTR (gst_omx_buffer_alloc_allocate_buffer));

  gst_pad_set_active (this->sinkpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (this), this->sinkpad);

  this->srcpad =
      GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
  gst_pad_set_getcaps_function (this->srcpad,
      GST_DEBUG_FUNCPTR (gst_pad_proxy_getcaps));

  gst_pad_set_active (this->srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (this), this->srcpad);

}

static void
gst_omx_buffer_alloc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (object);

  switch (prop_id) {
    case PROP_SILENT:
      this->silent = g_value_get_boolean (value);
      GST_INFO_OBJECT (this, "Setting silent to %d", this->silent);
      break;
    case PROP_NUMBUFFERS:
      this->num_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting numBuffers to %d", this->num_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_buffer_alloc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, this->silent);
      GST_INFO_OBJECT (this, "Setting silent to %d", this->silent);
      break;
    case PROP_NUMBUFFERS:
      this->num_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting numBuffers to %d", this->num_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_omx_buffer_alloc_set_caps (GstPad * pad, GstCaps * caps)
{
  GstOmxBufferAlloc *this;
  GstPad *otherpad;

  this = GST_OMXBUFFERALLOC (gst_pad_get_parent (pad));
  otherpad = (pad == this->srcpad) ? this->sinkpad : this->srcpad;
  gst_object_unref (this);

  return gst_pad_set_caps (otherpad, caps);
}


GstBuffer *
gst_omx_buffer_alloc_clone (GstPad * pad, GstBuffer * parent)
{
  OMX_BUFFERHEADERTYPE *omxbuf;
  OMX_BUFFERHEADERTYPE omxpeerbuf;
  OMX_ERRORTYPE error = OMX_ErrorNone;
  GstOmxPad *omxpad = GST_OMX_PAD (pad);
  GstOmxBufferData *omxdata;
  GstOmxBufferAlloc *this;
  GstBuffer *buffer = NULL;
  gboolean busy;

  GST_LOG_OBJECT (this, "### CLONE IN CHAIN");
  GST_LOG_OBJECT (this, "### Looking for MALLOC_DATA is %p",
      GST_BUFFER_MALLOCDATA (parent));
  omxbuf = (OMX_BUFFERHEADERTYPE *) GST_BUFFER_MALLOCDATA (parent);
  GST_LOG_OBJECT (this, "### Looking for pbuffer is %p", omxbuf->pBuffer);
  GST_LOG_OBJECT (this, "### Looking for header Alloc size %d",
      omxbuf->nAllocLen);
  GST_LOG_OBJECT (this, "### Looking for header Filled size %d",
      omxbuf->nFilledLen);
  GST_LOG_OBJECT (this, "### Looking for header offset %d", omxbuf->nOffset);
  omxdata = omxbuf->pAppPrivate;
  GST_DEBUG_OBJECT (this, "### Looking for buffer_id is %d", omxdata->id);
  GST_DEBUG_OBJECT (this, "### Looking for pbuffer of incoming GstBuffer is %p",
      GST_BUFFER_DATA (parent));
  GST_DEBUG_OBJECT (this, "### Looking incoming GstBuffer size %d",
      GST_BUFFER_SIZE (parent));

  omxbuf->nFilledLen =
      (GST_BUFFER_SIZE (parent) >
      omxbuf->nAllocLen) ? omxbuf->nAllocLen : GST_BUFFER_SIZE (parent);
  omxbuf->nOffset = 0;

  buffer = gst_buffer_make_metadata_writable (parent);
  GST_BUFFER_FLAGS (buffer) |= GST_OMX_BUFFER_FLAG;
  omxdata->buffer = buffer;

  return buffer;
}


static GstFlowReturn
gst_omx_buffer_alloc_chain (GstPad * pad, GstBuffer * buf)
{
  GstOmxBufferAlloc *this;

  this = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));

  buf = gst_omx_buffer_alloc_clone (pad, buf);

  return gst_pad_push (this->srcpad, buf);
}

void
gst_omx_buffer_alloc_allocate_buffers (GstOmxBufferAlloc * this,
    GstOmxPad * pad, guint size)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  gint i;
  GstOmxBufferData *bufdata = NULL;

  GST_INFO_OBJECT (this, "Allocating buffers for %s:%s",
      GST_DEBUG_PAD_NAME (GST_PAD (pad)));

  printf ("allocating %d buffers of size:%d!!\n", this->num_buffers, size);
  this->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, this->num_buffers);
  this->heap = SharedRegion_getHeap (2);
  for (i = 0; i < this->num_buffers; i++) {
    bufdata = (GstOmxBufferData *) g_malloc (sizeof (GstOmxBufferData));
    bufdata->pad = pad;
    bufdata->buffer = NULL;
    bufdata->id = i;
    this->buffers[i] = g_malloc (sizeof (OMX_BUFFERHEADERTYPE));
    this->buffers[i]->pBuffer = Memory_alloc (this->heap, size, 128, NULL);
    this->buffers[i]->nAllocLen = size;
    this->buffers[i]->pAppPrivate = bufdata;
    printf
        ("allocated outbuf (bufferheadertype):%p, bufferdata %p, pbuffer %p, id %d\n",
        this->buffers[i], bufdata, this->buffers[i]->pBuffer, i);
  }

  for (i = this->num_buffers - 1; i >= 0; i--) {
    error = gst_omx_buf_tab_add_buffer (pad->buffers, this->buffers[i]);
    if (GST_OMX_FAIL (error))
      goto noaddbuffer;
  }
  this->allocSize = size;
  return;

noaddbuffer:
  {
    GST_ERROR_OBJECT (this, "Unable to add the buffer to the buftab");
    g_free (bufdata);
  }
}

void
gst_omxbufferalloc_keep_mallocdata (gpointer data)
{
  OMX_BUFFERHEADERTYPE *buffer = (OMX_BUFFERHEADERTYPE *) data;
  OMX_ERRORTYPE error;
  GstOmxBufferData *bufdata = (GstOmxBufferData *) buffer->pAppPrivate;
  GstOmxPad *pad = bufdata->pad;
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));

  GST_INFO_OBJECT (this, "Buffer got free but MALLOCDATA still ok");

  return;
}

GstFlowReturn
gst_omx_buffer_alloc_allocate_buffer (GstPad * pad, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buffer)
{
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));
  GstOmxPad *omxpad = GST_OMX_PAD (pad);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxbuf;
  OMX_BUFFERHEADERTYPE *omxbuftemp;
  GstOmxBufferData *omxbufdata;
  GstOmxBufTabNode *node;
  GstOmxBufTab *buftab;
  GList *table;

  if (this->buffers == NULL)
    gst_omx_buffer_alloc_allocate_buffers (this, omxpad, size);

  GST_LOG_OBJECT (this, "## Getting free buffer from buftab");
  error = gst_omx_buf_tab_get_free_buffer (omxpad->buffers, &omxbuf);
  if (GST_OMX_FAIL (error))
    goto nofreebuf;

  GST_LOG_OBJECT (this, "## Got buffer %p", omxbuf);

  GST_LOG_OBJECT (this, "## Marking used buffer %p", omxbuf);
  gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);
  GST_INFO_OBJECT (this, "Alloc buffer returned buffer with size %d",
      (int) omxbuf->nAllocLen);

  /* LOGIC TO CHECK BUFTAB STATUS */
  buftab = omxpad->buffers;
  table = buftab->table;
  GST_LOG_OBJECT (this, "### CHECKING BUFTAB");

  while (table) {
    node = (GstOmxBufTabNode *) table->data;
    omxbuftemp = (OMX_BUFFERHEADERTYPE *) node->buffer;
    omxbufdata = omxbuftemp->pAppPrivate;
    GST_LOG_OBJECT (this,
        "BUFTAB: id %d, busy %d, omxheader %p, bufdata %p, pBuffer %p",
        omxbufdata->id, node->busy, omxbuftemp, omxbufdata,
        omxbuftemp->pBuffer);
    table = g_list_next (table);
  }

  *buffer = gst_buffer_new ();
  GST_BUFFER_SIZE (*buffer) = omxbuf->nAllocLen;
  GST_BUFFER_DATA (*buffer) = omxbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (*buffer) = omxbuf;
  GST_BUFFER_FREE_FUNC (*buffer) = gst_omxbufferalloc_keep_mallocdata;
  GST_INFO_OBJECT (this,
      "OMX BUFFERALLOC buf = %p size = %d data = %p MallocData = %p", *buffer,
      GST_BUFFER_SIZE (*buffer), GST_BUFFER_DATA (*buffer),
      GST_BUFFER_MALLOCDATA (*buffer));
  GST_BUFFER_CAPS (*buffer) = gst_caps_ref (caps);
  GST_BUFFER_FLAGS (*buffer) |= GST_OMX_BUFFER_FLAG;

  return GST_FLOW_OK;

nofreebuf:
  {
    GST_ERROR_OBJECT (this, "Unable to get free buffer: %s",
        gst_omx_error_to_str (error));
    return GST_FLOW_ERROR;
  }
}

static OMX_ERRORTYPE
gst_omx_buffer_alloc_free_buffers (GstOmxBufferAlloc * this, GstOmxPad * pad)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *buffer;
  GstOmxBufTabNode *node;
  guint i;
  GList *buffers;

  buffers = pad->buffers->table;

  if (!buffers)
    return error;

  GST_DEBUG_OBJECT (this, "buffer count %d", this->num_buffers);
  for (i = 0; i < this->num_buffers; ++i) {

    if (!buffers)
      goto shortread;

    node = (GstOmxBufTabNode *) buffers->data;
    buffer = node->buffer;

    GST_DEBUG_OBJECT (this, "Freeing %s:%s buffer number %u: %p",
        GST_DEBUG_PAD_NAME (pad), i, buffer);

    error = gst_omx_buf_tab_remove_buffer (pad->buffers, buffer);
    if (GST_OMX_FAIL (error))
      goto notintable;

    buffers = pad->buffers->table;

    g_free (buffer->pAppPrivate);

  }

  GST_OBJECT_LOCK (pad);
  pad->enabled = FALSE;
  GST_OBJECT_UNLOCK (pad);

  return error;

shortread:
  {
    GST_ERROR_OBJECT (this, "Malformed output buffer list");
    return OMX_ErrorResourcesLost;
  }
notintable:
  {
    GST_ERROR_OBJECT (this, "The buffer list for %s:%s is malformed: %s",
        GST_DEBUG_PAD_NAME (GST_PAD (pad)), gst_omx_error_to_str (error));
    return error;
  }

}

static GstStateChangeReturn
gst_omx_buffer_alloc_change_state (GstElement * element,
    GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (element);
  guint ii;
  GstOmxPad *omxpad = GST_OMX_PAD (this->sinkpad);


  GST_DEBUG_OBJECT (this, "Initializing %s", __FUNCTION__);
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (this, "###### Changing state from null to ready");
      break;

    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    goto leave;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT (this, "###### Changing state from paused to ready");
      if (this->buffers) {
        for (ii = 0; ii < this->num_buffers; ii++) {
          gst_omx_buf_tab_return_buffer (omxpad->buffers, this->buffers[ii]);
          Memory_free (this->heap, this->buffers[ii]->pBuffer, this->allocSize);
          GST_DEBUG_OBJECT (this, "###### Buffer %d already free", ii);
        }
      }
      gst_omx_buffer_alloc_free_buffers (this, omxpad);
      g_free (this->buffers);
      this->buffers = NULL;
      this->cnt = -1;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG_OBJECT (this, "Changing state from ready to null");
      break;

    default:
      GST_DEBUG_OBJECT (this, "Changing state to default");
      break;
  }

leave:
  return ret;
}
