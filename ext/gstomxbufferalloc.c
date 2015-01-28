/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2014 Eugenia Guzman <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/* Filter signals and args */
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

GstFlowReturn gst_omx_buffer_alloc_allocate_buffer (GstPad *pad, guint64 offset, guint size,
                                      GstCaps *caps, GstBuffer **buf);
                                      
static GstStateChangeReturn gst_omx_buffer_alloc_change_state (GstElement *element,
              GstStateChange transition);                                      

static GMutex *imp_mutex;
static GHashTable *implementations;

/* GObject vmethod implementations */

static void
gst_omx_buffer_alloc_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "omxbufferalloc",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "Eugenia Guzman <eugenia.guzman@ridgerun.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
      
      
}

/* initialize the omxbufferalloc's class */
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
          1, 16, GST_OMX_BUFFERALLOC_NUMBUFFERS_DEFAULT,
          G_PARAM_READWRITE));
          
  GST_DEBUG_CATEGORY_INIT (gst_omxbufferalloc_debug, "omxbufferalloc",
      0, "Template omxbufferalloc");        
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_omx_buffer_alloc_init (GstOmxBufferAlloc * this,
    GstOmxBufferAllocClass * gclass)
{
  this->silent = GST_OMX_BUFFERALLOC_SILENT_DEFAULT;
  this->num_buffers = GST_OMX_BUFFERALLOC_NUMBUFFERS_DEFAULT;
  this->buffers = NULL;
  this->omx_library = "libOMX_Core.so";
  this->cnt = 0;

  this->sinkpad = GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&sink_template), "sink"));
   
  gst_pad_set_setcaps_function (this->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_set_caps));
  gst_pad_set_getcaps_function (this->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (this->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_chain));
  gst_pad_set_bufferalloc_function(this->sinkpad,
							  GST_DEBUG_FUNCPTR(gst_omx_buffer_alloc_allocate_buffer));

  gst_pad_set_active (this->sinkpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (this), this->sinkpad);
  
  this->srcpad = GST_PAD (gst_omx_pad_new_from_template (gst_static_pad_template_get
          (&src_template), "src"));
  gst_pad_set_getcaps_function (this->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  
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
      GST_INFO_OBJECT (this, "Setting silent to %d",
          this->silent);
      break;
    case PROP_NUMBUFFERS:
      this->num_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting numBuffers to %d",
          this->num_buffers);
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
      GST_INFO_OBJECT (this, "Setting silent to %d",
          this->silent);
      break;
    case PROP_NUMBUFFERS:
      this->num_buffers = g_value_get_uint (value);
      GST_INFO_OBJECT (this, "Setting numBuffers to %d",
          this->num_buffers);
      break;  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
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

GstBuffer* gst_omx_buffer_alloc_clone (GstPad * pad, GstBuffer *parent)
{
	OMX_BUFFERHEADERTYPE *omxbuf;
	OMX_BUFFERHEADERTYPE omxpeerbuf;
	OMX_ERRORTYPE error = OMX_ErrorNone;
	GstOmxPad *omxpad = GST_OMX_PAD (pad);
	GstBuffer *buffer = NULL;	
	gboolean busy;
	
	//~ if (GST_OMX_IS_OMX_BUFFER (parent)) {
		buffer = gst_buffer_make_metadata_writable(parent);
		GST_BUFFER_FLAGS(buffer) |= GST_OMX_BUFFER_FLAG;
		omxpeerbuf.pBuffer = GST_BUFFER_DATA(parent);
		//~ printf ("Buffer %p\n", omxpeerbuf.pBuffer);
		error = gst_omx_buf_tab_find_buffer (omxpad->buffers, &omxpeerbuf, &omxbuf, &busy);
		if (GST_OMX_FAIL (error))
			goto notfound;
		GST_BUFFER_MALLOCDATA(buffer) = omxbuf;
		GST_BUFFER_DATA(buffer) = omxbuf->pBuffer;
		
	//~ }
	return buffer;	

notfound:
  {
    GST_ERROR_OBJECT (pad,
        "Buffer is marked as OMX, but was not found on buftab: %s",
        gst_omx_error_to_str (error));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_omx_buffer_alloc_chain (GstPad * pad, GstBuffer * buf)
{
  GstOmxBufferAlloc *this;
  
  this = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));
  
  buf = gst_omx_buffer_alloc_clone (pad, buf);
  //~ buf = gst_omxbuffertransport_clone (buf, &(this->out_port));
  /* just push out the incoming buffer without touching it */
  return gst_pad_push (this->srcpad, buf);
}

void 
gst_omx_buffer_alloc_allocate_buffers (GstOmxBufferAlloc *this, GstOmxPad * pad, guint size)
{
  OMX_ERRORTYPE error = OMX_ErrorNone;
  guint i;
  GstOmxBufferData *bufdata = NULL;
    
  GST_DEBUG_OBJECT (this, "Allocating buffers for %s:%s",
      GST_DEBUG_PAD_NAME (GST_PAD (pad)));
  
  bufdata = (GstOmxBufferData *) g_malloc (sizeof (GstOmxBufferData));
  bufdata->pad = pad;
  bufdata->buffer = NULL;

  printf("allocating %d buffers of size:%d!!\n",this->num_buffers,size);
  this->buffers = g_new0 (OMX_BUFFERHEADERTYPE *, this->num_buffers);
  this->heap = SharedRegion_getHeap(2);
  for (i = 0; i < this->num_buffers; i++) {
	 this->buffers[i] = malloc(sizeof(OMX_BUFFERHEADERTYPE));
	 this->buffers[i]->pBuffer = Memory_alloc (this->heap, size, 128, NULL);
  	 this->buffers[i]->nAllocLen = size;
  	 this->buffers[i]->pAppPrivate = bufdata;
     error = gst_omx_buf_tab_add_buffer (pad->buffers, this->buffers[i]);
     if (GST_OMX_FAIL (error))
		goto noaddbuffer;
	 printf("allocated outbuf:%p\n",this->buffers[i]->pBuffer);
	 bufdata->id = i;
  }
  this->allocSize = size;
  return;
  
noaddbuffer:
  {
    GST_ERROR_OBJECT (this, "Unable to add the buffer to the buftab");
    g_free (bufdata);
    /*TODO: should I free buffers? */
  }  
}

GstFlowReturn 
gst_omx_buffer_alloc_allocate_buffer (GstPad *pad, guint64 offset, guint size,
                                      GstCaps *caps, GstBuffer **buffer)
{
  GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (GST_OBJECT_PARENT (pad));
  GstOmxPad *omxpad = GST_OMX_PAD (pad);
  OMX_ERRORTYPE error = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omxbuf;
  
  if(this->buffers == NULL)
	gst_omx_buffer_alloc_allocate_buffers (this, omxpad, size);

  error = gst_omx_buf_tab_get_free_buffer (omxpad->buffers, &omxbuf);
  if (GST_OMX_FAIL (error))
    goto nofreebuf;
    
  gst_omx_buf_tab_use_buffer (omxpad->buffers, omxbuf);
  GST_DEBUG_OBJECT (this, "Alloc buffer returned buffer with size %d",
      (int) omxbuf->nAllocLen);
  
  *buffer = gst_buffer_new ();
  GST_BUFFER_SIZE (*buffer) = omxbuf->nAllocLen;
  GST_BUFFER_DATA (*buffer) = omxbuf->pBuffer;
  GST_BUFFER_MALLOCDATA (*buffer) = NULL;
  //~ printf ("OMX BUFFERALLOC buf = %p size = %d data = %p MallocData = %p\n", buffer, GST_BUFFER_SIZE(*buffer), GST_BUFFER_DATA(*buffer), GST_BUFFER_MALLOCDATA(*buffer));
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

void
g_omx_release_imp (GOmxImp *imp)
{
    g_mutex_lock (imp->mutex);
    imp->client_count--;
    if (imp->client_count == 0)
    {
        #ifdef USE_STATIC
        OMX_Deinit();
        #else
        imp->sym_table.deinit ();
        #endif
    }
    g_mutex_unlock (imp->mutex);
}

static GOmxImp *
imp_new (const gchar *name)
{
    GOmxImp *imp;

    imp = g_new0 (GOmxImp, 1);

    #ifdef USE_STATIC
        imp->mutex = g_mutex_new ();
    #else
    /* Load the OpenMAX IL symbols */
    {
        void *handle;

        imp->dl_handle = handle = dlopen (name, RTLD_LAZY);
        GST_DEBUG ("dlopen(%s) -> %p", name, handle);
        if (!handle)
        {
            g_warning ("%s\n", dlerror ());
            g_free (imp);
            return NULL;
        }

        imp->mutex = g_mutex_new ();
        imp->sym_table.init = dlsym (handle, "OMX_Init");
        imp->sym_table.deinit = dlsym (handle, "OMX_Deinit");
        imp->sym_table.get_handle = dlsym (handle, "OMX_GetHandle");
        imp->sym_table.free_handle = dlsym (handle, "OMX_FreeHandle");
    }
    #endif

    return imp;
}

static void
imp_free (GOmxImp *imp)
{
    if (imp->dl_handle)
    {
        dlclose (imp->dl_handle);
    }
    g_mutex_free (imp->mutex);
    g_free (imp);
}


GOmxImp *
g_omx_request_imp (const gchar *name)
{
    GOmxImp *imp = NULL;
    
    imp_mutex = g_mutex_new ();
    implementations = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify) imp_free);
    g_mutex_lock (imp_mutex);
    imp = g_hash_table_lookup (implementations, name);
    
    if (!imp)
    {
        imp = imp_new (name);
        if (imp)
            g_hash_table_insert (implementations, g_strdup (name), imp);
    }

    g_mutex_unlock (imp_mutex);
	
	if (!imp)
        return NULL;

	g_mutex_lock (imp->mutex);
    if (imp->client_count == 0)
    {
        OMX_ERRORTYPE omx_error;

        #ifdef USE_STATIC
        omx_error = OMX_Init ();
        #else
        omx_error = imp->sym_table.init ();
        #endif
        if (omx_error)
        {
            g_mutex_unlock (imp->mutex);
            return NULL;
        }
    }
    imp->client_count++;
    g_mutex_unlock (imp->mutex);
	
	return imp;
}

static GstStateChangeReturn
gst_omx_buffer_alloc_change_state (GstElement *element,
              GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstOmxBufferAlloc *this = GST_OMXBUFFERALLOC (element);
    guint ii;
    GstOmxPad *omxpad = GST_OMX_PAD (this->sinkpad);
    
    
    GST_DEBUG_OBJECT (this, "Initializing %s", __FUNCTION__); 
    switch (transition)
    {
        case GST_STATE_CHANGE_NULL_TO_READY:
			this->imp = g_omx_request_imp (this->omx_library);
            break;

        default:
			break;
    }

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE)
        goto leave;

	switch (transition)
    {
        /* FIXME: This is a workaround to avoid a big mem leak. Resources should
	   be freed on the READY_TO_NULL transition */
	   
	    case GST_STATE_CHANGE_PAUSED_TO_READY:
			GST_DEBUG_OBJECT (this, "Changing state from paused to ready"); 
			if(this->buffers) {
              for(ii = 0; ii < this->num_buffers; ii++) {
				  gst_omx_buf_tab_return_buffer (omxpad->buffers, this->buffers[ii]);
				  Memory_free(this->heap,this->buffers[ii]->pBuffer,this->allocSize);
              }
              g_free(this->buffers);
            }
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
			GST_DEBUG_OBJECT (this, "Changing state from ready to null");
			if (this->imp) {
              g_omx_release_imp (this->imp);
              this->imp = NULL;
            }
            break;

        default:
			GST_DEBUG_OBJECT (this, "Changing state to default");
            break;
    }
	
leave:
    return ret;
}
