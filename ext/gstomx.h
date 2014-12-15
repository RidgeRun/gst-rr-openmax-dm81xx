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
#ifndef __GST_OMX_H__
#define __GST_OMX_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_TI_Common.h>
#include <OMX_TI_Index.h>
#include <OMX_TI_Video.h>
#include <omx_vfpc.h>
#include "timm_osal_interfaces.h"

#include "string.h"

#include "gstomxbuftab.h"
#include "gstomxbufqueue.h"
#include "gstomxpad.h"
#include "gstomxerror.h"

G_BEGIN_DECLS typedef struct _GstOmxFormat GstOmxFormat;
typedef struct _GstOmxBufferData GstOmxBufferData;

typedef OMX_ERRORTYPE (*GstOmxEventHandler) (OMX_HANDLETYPE,
    OMX_PTR, OMX_EVENTTYPE, OMX_U32, OMX_U32, OMX_PTR);
typedef OMX_ERRORTYPE (*GstOmxEmptyBufferDone) (OMX_HANDLETYPE,
    OMX_PTR, OMX_BUFFERHEADERTYPE *);
typedef OMX_ERRORTYPE (*GstOmxFillBufferDone) (OMX_HANDLETYPE,
    OMX_PTR, OMX_BUFFERHEADERTYPE *);

struct _GstOmxFormat
{
  /*Video */
  gint width;
  gint width_padded;            //aka: stride, pitch
  gint height;
  gint height_padded;
  gint framerate_num;
  gint framerate_den;
  gint aspectratio_num;
  gint aspectratio_den;
  GstVideoFormat format;
  guint size;
  guint size_padded;
  gboolean interlaced;
  /*Audio */
  gint rate;
  gint channels;

};

struct _GstOmxBufferData
{
  GstBuffer *buffer;
  GstOmxPad *pad;
  guint8 id;                    /*  ID of the buffer used by the buftab  */
};

#define GST_OMX_INIT_STRUCT(_s_, _name_)	\
  memset((_s_), 0x0, sizeof(_name_));		\
  (_s_)->nSize = sizeof(_name_);		\
  (_s_)->nVersion.s.nVersionMajor = 0x1;	\
  (_s_)->nVersion.s.nVersionMinor = 0x1;	\
  (_s_)->nVersion.s.nRevision  = 0x0;		\
  (_s_)->nVersion.s.nStep   = 0x0;

#define GST_OMX_ALIGN(a,b)  ((((guint32)(a)) + (b)-1) & (~((guint32)((b)-1))))

#define GST_OMX_BUFFER_FLAG (GST_BUFFER_FLAG_LAST << 0)
#define GST_OMX_IS_OMX_BUFFER(buffer) \
  (GST_BUFFER_FLAGS(buffer) & GST_OMX_BUFFER_FLAG)

GMutex _omx_mutex;

G_END_DECLS
#endif // __GST_OMX_H__
