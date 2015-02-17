/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/controller/gstcontroller.h>

#include <OMX_Core.h>

#include "gstomxmpeg2dec.h"
#include "gstomxh264dec.h"
#include "gstomxh264enc.h"
#include "gstomxjpegenc.h"
#include "gstomxaacenc.h"
#include "gstomxaacdec.h"
#include "gstomxscaler.h"
#include "gstomxdeiscaler.h"
#include "gstomxcamera.h"
#include "gstomxrrparser.h"
#include "gstomxnoisefilter.h"
#include "gstomxbufferalloc.h"

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
omx_init (GstPlugin * omx)
{
  /* initialize gst controller library */
  gst_controller_init (NULL, NULL);

  /* Initialize OMX subsystem */
  OMX_Init ();
  g_mutex_init (&_omx_mutex);

#if 0
  ConfigureUIA uiaCfg;
  uiaCfg.enableAnalysisEvents = 0;
  /* can be 0 or 1 */
  uiaCfg.enableStatusLogger = 1;
  /* can be OMX_DEBUG_LEVEL1|2|3|4|5 */
  uiaCfg.debugLevel = OMX_DEBUG_LEVEL1;
  /* configureUiaLoggerClient( COREID, &Cfg); */
  configureUiaLoggerClient (1, &uiaCfg);
#endif

  /* Horrible hack to avoid segfaulting on omx callbacks */
  /* if (!g_setenv ("GST_DEBUG_FILE", "/dev/console", FALSE)) { */
  /*   g_printerr ("Unable to hack debugging subsystem, I will segfault if my debug is enabled\n"); */
  /* } else { */
  /*   _gst_debug_init (); */
  /* } */


  if (!gst_element_register (omx, "omx_mpeg2dec", GST_RANK_NONE,
          GST_TYPE_OMX_MPEG2_DEC))
    return FALSE;

  if (!gst_element_register (omx, "omx_h264dec", GST_RANK_NONE,
          GST_TYPE_OMX_H264_DEC))
    return FALSE;

  if (!gst_element_register (omx, "omx_h264enc", GST_RANK_NONE,
          GST_TYPE_OMX_H264_ENC))
    return FALSE;

  if (!gst_element_register (omx, "omx_jpegenc", GST_RANK_NONE,
          GST_TYPE_OMX_JPEG_ENC))
    return FALSE;

  if (!gst_element_register (omx, "omx_aacenc", GST_RANK_NONE,
          GST_TYPE_OMX_AAC_ENC))
    return FALSE;

  if (!gst_element_register (omx, "omx_aacdec", GST_RANK_NONE,
          GST_TYPE_OMX_AAC_DEC))
    return FALSE;

  if (!gst_element_register (omx, "omx_scaler", GST_RANK_NONE,
          GST_TYPE_OMX_SCALER))
    return FALSE;

  if (!gst_element_register (omx, "omx_hdeiscaler", GST_RANK_NONE,
          GST_TYPE_OMX_HDEISCALER))
    return FALSE;

  if (!gst_element_register (omx, "omx_mdeiscaler", GST_RANK_NONE,
          GST_TYPE_OMX_MDEISCALER))
    return FALSE;
  if (!gst_element_register (omx, "omx_camera", GST_RANK_NONE,
          GST_TYPE_OMX_CAMERA))
    return FALSE;
  if (!gst_element_register (omx, "rr_h264parser", GST_RANK_NONE,
          GST_TYPE_RRPARSER))
    return FALSE;

  if (!gst_element_register (omx, "omx_noisefilter", GST_RANK_NONE,
			     GST_TYPE_OMX_NOISE_FILTER))
    return FALSE;
  
  if (!gst_element_register (omx, "omxbufferalloc", GST_RANK_NONE,
          GST_TYPE_OMXBUFFERALLOC))
    return FALSE;
  
return TRUE;
}

/* gstreamer looks for this structure to register omxs
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "rromx",
    "RidgeRun's OMX GStreamer plugin",
    omx_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
