/* 
 * GStreamer
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
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

GST_DEBUG_CATEGORY_STATIC (gst_omx_buf_tab_debug);
#define GST_CAT_DEFAULT gst_omx_buf_tab_debug

static gboolean gst_omx_buf_tab_debug_register = FALSE;

OMX_ERRORTYPE
gst_omx_buf_tab_mark_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer, gboolean busy);
static gint gst_omx_buf_tab_compare (gconstpointer _node,
    gconstpointer _buffer);

static gint
gst_omx_buf_tab_compare (gconstpointer _node, gconstpointer _buffer)
{
  GstOmxBufTabNode *node;
  GstOmxBufferData *nodedata;
  GstOmxBufferData *bufferdata;
  OMX_BUFFERHEADERTYPE *buffer;

  node = (GstOmxBufTabNode *) _node;
  buffer = (OMX_BUFFERHEADERTYPE *) _buffer;
  nodedata = (GstOmxBufferData *) node->buffer->pAppPrivate;
  bufferdata = (GstOmxBufferData *) buffer->pAppPrivate;

  GST_LOG ("ID of Buffer in buftab: %i , ID of External buffer %i ",
      nodedata->id, bufferdata->id);
  return nodedata->id - bufferdata->id;
}

OMX_ERRORTYPE
gst_omx_buf_tab_find_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * peerbuffer, OMX_BUFFERHEADERTYPE ** buffer,
    gboolean * busy)
{
  OMX_ERRORTYPE error;
  GList *toreturn;
  GstOmxBufTabNode *node;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);
  g_return_val_if_fail (peerbuffer, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  toreturn = NULL;
  node = NULL;

  g_mutex_lock (&buftab->tabmutex);
  GST_LOG ("Finding buffer ...");
  toreturn = g_list_find_custom (buftab->table, (gconstpointer) peerbuffer,
      (GCompareFunc) gst_omx_buf_tab_compare);
  if (!toreturn)
    goto notfound;

  node = (GstOmxBufTabNode *) toreturn->data;
  *buffer = node->buffer;
  *busy = node->busy;
  GST_LOG (" Buffer found ...");
  g_mutex_unlock (&buftab->tabmutex);

  return error;

notfound:
  GST_ERROR (" Resources Lost ...");
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorResourcesLost;
  return error;

}

GstOmxBufTab *
gst_omx_buf_tab_new ()
{
  GstOmxBufTab *buftab = NULL;
  if (!gst_omx_buf_tab_debug_register) {
    /* debug category for fltering log messages */
    GST_DEBUG_CATEGORY_INIT (gst_omx_buf_tab_debug, "buftab", 0, "OMX buftab");
    gst_omx_buf_tab_debug_register = TRUE;
  }

  buftab = g_malloc0 (sizeof (GstOmxBufTab));
  if (!buftab)
    goto exit;

  g_mutex_init (&buftab->tabmutex);
  g_cond_init (&buftab->tabcond);

exit:
  return buftab;
}

OMX_ERRORTYPE
gst_omx_buf_tab_add_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer)
{
  OMX_ERRORTYPE error;
  GstOmxBufTabNode *node;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);
  g_return_val_if_fail (buffer, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;

  node = g_malloc0 (sizeof (GstOmxBufTabNode));
  if (!node)
    goto nomem;

  node->buffer = buffer;
  node->phys_addr = buffer->pBuffer;

  g_mutex_lock (&buftab->tabmutex);

  buftab->table = g_list_prepend (buftab->table, (gpointer) node);

  g_mutex_unlock (&buftab->tabmutex);

  return error;

nomem:
  error = OMX_ErrorInsufficientResources;
  return error;
}

OMX_ERRORTYPE
gst_omx_buf_tab_use_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer)
{
  return gst_omx_buf_tab_mark_buffer (buftab, buffer, TRUE);
}

OMX_ERRORTYPE
gst_omx_buf_tab_return_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer)
{
  return gst_omx_buf_tab_mark_buffer (buftab, buffer, FALSE);
}

OMX_ERRORTYPE
gst_omx_buf_tab_mark_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer, gboolean busy)
{
  OMX_ERRORTYPE error;
  GList *toreturn;
  GstOmxBufTabNode *node;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);
  g_return_val_if_fail (buffer, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  toreturn = NULL;
  node = NULL;
  GST_LOG ("Marking buffer ... ");
  g_mutex_lock (&buftab->tabmutex);

  toreturn = g_list_find_custom (buftab->table, (gconstpointer) buffer,
      (GCompareFunc) gst_omx_buf_tab_compare);
  if (!toreturn)
    goto notfound;
  node = (GstOmxBufTabNode *) toreturn->data;
  GST_LOG ("Marking Buffer %p -> %p as %s ", node->buffer,
      node->buffer->pBuffer, busy ? "Used" : "Free");
  if (node->busy != busy) {
    node->busy = busy;
    buftab->tabused += busy ? 1 : -1;
  }
  GST_LOG ("Buffer %p -> %p set as %s ", node->buffer, node->buffer->pBuffer,
      busy ? "Used" : "Free");
  g_cond_signal (&buftab->tabcond);
  g_mutex_unlock (&buftab->tabmutex);

  return error;

notfound:
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorResourcesLost;
  return error;
}

OMX_ERRORTYPE
gst_omx_buf_tab_remove_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE * buffer)
{
  OMX_ERRORTYPE error;
  GList *toremove;
  guint64 endtime;
  GstOmxBufTabNode *node;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);
  g_return_val_if_fail (buffer, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  toremove = NULL;

  g_mutex_lock (&buftab->tabmutex);

  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  toremove = g_list_find_custom (buftab->table, (gconstpointer) buffer,
      (GCompareFunc) gst_omx_buf_tab_compare);
  if (!toremove)
    goto notfound;
  node = (GstOmxBufTabNode *) toremove->data;

  while (node->busy)
    if (!g_cond_wait_until (&buftab->tabcond, &buftab->tabmutex, endtime))
      goto timeout;

  buftab->table = g_list_remove (buftab->table, (gpointer) toremove->data);

  g_mutex_unlock (&buftab->tabmutex);

  return error;

notfound:
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorResourcesLost;
  return error;

timeout:
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorTimeout;
  return error;
}

OMX_ERRORTYPE
gst_omx_buf_tab_get_free_buffer (GstOmxBufTab * buftab,
    OMX_BUFFERHEADERTYPE ** buffer)
{
  OMX_ERRORTYPE error;
  GList *table;
  GstOmxBufTabNode *node;
  guint64 endtime;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  table = buftab->table;
  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  *buffer = NULL;

  g_mutex_lock (&buftab->tabmutex);

retry:
  table = buftab->table;

  while (table) {
    node = (GstOmxBufTabNode *) table->data;
    if (!node->busy) {
      *buffer = node->buffer;
      break;
    }

    table = g_list_next (table);
  }

  while (!*buffer) {
    if (!g_cond_wait_until (&buftab->tabcond, &buftab->tabmutex, endtime))
      goto timeout;
    else
      goto retry;
  }

  g_mutex_unlock (&buftab->tabmutex);
  return error;

timeout:
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorTimeout;
  return error;

}

OMX_ERRORTYPE
gst_omx_buf_tab_wait_free (GstOmxBufTab * buftab)
{
  OMX_ERRORTYPE error;
  guint64 endtime;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;
  endtime = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;

  g_mutex_lock (&buftab->tabmutex);

  /* Wait until all the buffers have return to the mothership */
  while (buftab->tabused)
    if (!g_cond_wait_until (&buftab->tabcond, &buftab->tabmutex, endtime))
      goto timeout;

  g_mutex_unlock (&buftab->tabmutex);

  return error;

timeout:
  g_print ("Failed %p, tabused %d\n", buftab, buftab->tabused);
  g_mutex_unlock (&buftab->tabmutex);
  error = OMX_ErrorTimeout;
  return error;
}

OMX_ERRORTYPE
gst_omx_buf_tab_free (GstOmxBufTab * buftab)
{
  OMX_ERRORTYPE error;

  g_return_val_if_fail (buftab, OMX_ErrorBadParameter);

  error = OMX_ErrorNone;

  error = gst_omx_buf_tab_wait_free (buftab);
  if (OMX_ErrorNone != error)
    goto nowait;

  g_mutex_lock (&buftab->tabmutex);

  g_list_free_full (buftab->table, (GDestroyNotify) g_free);
  buftab->table = NULL;

  g_mutex_unlock (&buftab->tabmutex);

  g_free (buftab);

  return error;

nowait:
  return error;
}
