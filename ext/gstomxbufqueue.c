/* 
 * GStreamer
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

#include "gstomx.h"
#include "gstomxbufqueue.h"

GstOmxBufQueue *
gst_omx_buf_queue_new ()
{
  GstOmxBufQueue *bufqueue = NULL;
  bufqueue = g_malloc0 (sizeof (GstOmxBufQueue));
  if (!bufqueue)
    goto exit;

  g_mutex_init (&bufqueue->queuemutex);
  g_cond_init (&bufqueue->queuecond);
  bufqueue->queue = g_queue_new ();
  g_queue_init (bufqueue->queue);
  bufqueue->release = FALSE;

exit:
  return bufqueue;
}


OMX_ERRORTYPE
gst_omx_buf_queue_push_buffer (GstOmxBufQueue * bufqueue,
    OMX_BUFFERHEADERTYPE * buffer)
{
  OMX_ERRORTYPE error;

  g_return_val_if_fail (bufqueue, OMX_ErrorBadParameter);
  g_return_val_if_fail (buffer, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;

  g_mutex_lock (&bufqueue->queuemutex);
  g_queue_push_tail (bufqueue->queue, (gpointer) buffer);
  g_cond_signal (&bufqueue->queuecond);
  g_mutex_unlock (&bufqueue->queuemutex);

  return error;

}


OMX_BUFFERHEADERTYPE *
gst_omx_buf_queue_pop_buffer (GstOmxBufQueue * bufqueue)
{
  OMX_BUFFERHEADERTYPE *buffer = NULL;
  guint64 endtime;
  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  g_mutex_lock (&bufqueue->queuemutex);
retry:

  while (g_queue_is_empty (bufqueue->queue)) {
    if (!g_cond_wait_until (&bufqueue->queuecond, &bufqueue->queuemutex,
            endtime))
      goto timeout;
    else
      goto retry;
  }

  buffer = (OMX_BUFFERHEADERTYPE *) g_queue_pop_head (bufqueue->queue);

  g_mutex_unlock (&bufqueue->queuemutex);

  return buffer;

timeout:
  g_mutex_unlock (&bufqueue->queuemutex);
  return buffer;

}


OMX_BUFFERHEADERTYPE *
gst_omx_buf_queue_pop_buffer_no_wait (GstOmxBufQueue * bufqueue)
{
  OMX_BUFFERHEADERTYPE *buffer = NULL;

  g_mutex_lock (&bufqueue->queuemutex);

  if (g_queue_is_empty (bufqueue->queue)) {
    buffer = NULL;
  } else {
    buffer = (OMX_BUFFERHEADERTYPE *) g_queue_pop_head (bufqueue->queue);
  }
  g_mutex_unlock (&bufqueue->queuemutex);

  return buffer;
}



OMX_BUFFERHEADERTYPE *
gst_omx_buf_queue_pop_buffer_check_release (GstOmxBufQueue * bufqueue)
{
  OMX_BUFFERHEADERTYPE *buffer = NULL;
  guint64 endtime;
  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  g_mutex_lock (&bufqueue->queuemutex);
retry:

  while (g_queue_is_empty (bufqueue->queue) && !bufqueue->release ) {
    if (!g_cond_wait_until (&bufqueue->queuecond, &bufqueue->queuemutex,
            endtime))
      goto timeout;
    else
      goto retry;
  }

  buffer = (OMX_BUFFERHEADERTYPE *) g_queue_pop_head (bufqueue->queue);

  g_mutex_unlock (&bufqueue->queuemutex);

  return buffer;

timeout:
  g_mutex_unlock (&bufqueue->queuemutex);
  return buffer;

}


OMX_ERRORTYPE
gst_omx_buf_queue_release (GstOmxBufQueue * bufqueue)
{
  OMX_ERRORTYPE error;

  g_return_val_if_fail (bufqueue, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  g_mutex_lock (&bufqueue->queuemutex);
  bufqueue->release=TRUE;
  g_cond_signal (&bufqueue->queuecond);
  g_mutex_unlock (&bufqueue->queuemutex);

  return error;

}


OMX_ERRORTYPE
gst_omx_buf_queue_free (GstOmxBufQueue * bufqueue)
{
  OMX_ERRORTYPE error;
  /*Add check of errors?*/
  g_return_val_if_fail (bufqueue, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  g_mutex_lock (&bufqueue->queuemutex);
  g_queue_free(bufqueue->queue);
  g_mutex_unlock (&bufqueue->queuemutex);

  g_free (bufqueue);
 exit:
  return error;
}
