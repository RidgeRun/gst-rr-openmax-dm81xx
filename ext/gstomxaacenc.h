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

#ifndef __GST_OMX_AAC_ENC_H__
#define __GST_OMX_AAC_ENC_H__

#include <gst/gst.h>
#include "gstomxpad.h"
#include "gstomxbase.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_AAC_ENC \
  (gst_omx_aac_enc_get_type())
#define GST_OMX_AAC_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_AAC_ENC,GstOmxAACEnc))
#define GST_OMX_AAC_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_AAC_ENC,GstOmxAACEncClass))
#define GST_IS_OMX_AAC_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_AAC_ENC))
#define GST_IS_OMX_AAC_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_AAC_ENC))
typedef struct _GstOmxAACEnc GstOmxAACEnc;
typedef struct _GstOmxAACEncClass GstOmxAACEncClass;

struct _GstOmxAACEnc
{
  GstOmxBase element;

  GstPad *srcpad, *sinkpad;
  GstOmxFormat format;

  /* Properties */
  guint rate;
  guint channels;
  guint bitrate;
  gint profile;
  gint output_format;
};

struct _GstOmxAACEncClass
{
  GstOmxBaseClass parent_class;
};

GType gst_omx_aac_enc_get_type (void);

G_END_DECLS

#endif /* __GST_OMX_AAC_ENC_H__ */
