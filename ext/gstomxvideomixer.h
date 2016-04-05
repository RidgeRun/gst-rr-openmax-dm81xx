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

#ifndef __GST_OMX_VIDEO_MIXER_H__
#define __GST_OMX_VIDEO_MIXER_H__

#include <gst/base/gstcollectpads2.h>
#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_VIDEO_MIXER			\
  (gst_omx_video_mixer_get_type())
#define GST_OMX_VIDEO_MIXER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_VIDEO_MIXER,GstOmxVideoMixer))
#define GST_OMX_VIDEO_MIXER_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_VIDEO_MIXER,GstOmxVideoMixerClass))
#define GST_OMX_VIDEO_MIXER_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMX_VIDEO_MIXER, GstOmxVideoMixerClass))
#define GST_IS_OMX_VIDEO_MIXER(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_VIDEO_MIXER))
#define GST_IS_OMX_VIDEO_MIXER_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_VIDEO_MIXER))
typedef struct _GstOmxVideoMixer GstOmxVideoMixer;
typedef struct _GstOmxVideoMixerClass GstOmxVideoMixerClass;

struct _GstOmxVideoMixer
{
  GstElement element;

  GstPad *srcpad;
  GList *sinkpads;
  GList *srcpads;
  guint sinkpad_count;
  guint next_sinkpad;

  /* sink pads using Collect Pads 2 */
  GstCollectPads2 *collect;

  gboolean started;

  /* Caps */
  guint src_width;
  guint src_height;
  guint src_stride;

  /* Properties */
  guint input_buffers;
  guint output_buffers;

  /* Omx */
  OMX_HANDLETYPE handle;
  OMX_CALLBACKTYPE *callbacks;
  OMX_STATETYPE state;

  /* Conditions */
  GMutex waitmutex;
  GCond waitcond;
};

struct _GstOmxVideoMixerClass
{
  GstElementClass parent_class;

};

GType gst_omx_video_mixer_get_type (void);

G_END_DECLS
#endif /* __GST_OMX_VIDEO_MIXER_H__ */
