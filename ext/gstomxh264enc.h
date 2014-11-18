/* 
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2013 Michael Gruner <michael.gruner@ridgerun.com>
 * Copyright (C) 2014 Eugenia Guzman <eugenia.guzman@ridgerun.com>
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

#ifndef __GST_OMX_H264_ENC_H__
#define __GST_OMX_H264_ENC_H__

#include <gst/gst.h>
#include "gstomxpad.h"
#include "gstomxbase.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_H264_ENC \
  (gst_omx_h264_enc_get_type())
#define GST_OMX_H264_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_H264_ENC,GstOmxH264Enc))
#define GST_OMX_H264_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_H264_ENC,GstOmxH264EncClass))
#define GST_IS_OMX_H264_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_H264_ENC))
#define GST_IS_OMX_H264_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_H264_ENC))
typedef struct _GstOmxH264Enc GstOmxH264Enc;
typedef struct _GstOmxH264EncClass GstOmxH264EncClass;

struct _GstOmxH264Enc
{
  GstOmxBase element;

  GstPad *srcpad, *sinkpad;
  GstOmxFormat format;

  /* Properties */
  //~ guint framerate;
  guint bitrate;
  OMX_VIDEO_AVCPROFILETYPE profile;
  OMX_VIDEO_AVCLEVELTYPE level;
  gboolean bytestream;
  guint i_period;
  guint force_idr_period;
  gboolean force_idr;
  gboolean is_interlaced;
  OMX_VIDEO_ENCODING_MODE_PRESETTYPE encodingPreset;
  OMX_VIDEO_RATECONTROL_PRESETTYPE rateControlPreset;
  gint cont;
};

struct _GstOmxH264EncClass
{
  GstOmxBaseClass parent_class;
};

GType gst_omx_h264_enc_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_H264_ENC_H__ */
