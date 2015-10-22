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

#ifndef __GST_OMX_BASEDECODER_H__
#define __GST_OMX_BASEDECODER_H__

#include "gstomx.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_BASEDECODER			\
  (gst_omx_basedecoder_get_type())
#define GST_OMX_BASEDECODER(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_BASEDECODER,GstOmxBaseDecoder))
#define GST_OMX_BASEDECODER_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_BASEDECODER,GstOmxBaseDecoderClass))
#define GST_OMX_BASEDECODER_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMX_BASEDECODER, GstOmxBaseDecoderClass))
#define GST_IS_OMX_BASEDECODER(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_BASEDECODER))
#define GST_IS_OMX_BASEDECODER_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_BASEDECODER))
typedef struct _GstOmxBaseDecoder GstOmxBaseDecoder;
typedef struct _GstOmxBaseDecoderClass GstOmxBaseDecoderClass;

typedef OMX_ERRORTYPE (*GstOmxBaseDecoderPadFunc) (GstOmxBaseDecoder *,
    GstOmxPad *, gpointer);

struct _GstOmxBaseDecoder
{
  GstElement element;

  OMX_HANDLETYPE handle;
  OMX_COMPONENTTYPE *component;
  OMX_CALLBACKTYPE *callbacks;

  guint32 requested_size;
  guint32 field_offset;

  guint input_buffers;
  guint output_buffers;

  guint num_buffers;
  guint cont;
  GCond num_buffers_cond;
  GMutex num_buffers_mutex;

  gboolean peer_alloc;
  gboolean flushing;
  gboolean started;
  gboolean first_buffer;
  gboolean interlaced;
  gboolean joined_fields;
  gboolean audio_component;


  OMX_STATETYPE state;
  GMutex waitmutex;
  GCond waitcond;

  /*Conditions for Paused State */
  GMutex stream_mutex;

  /* Queue of buffers pushed on the output */
  GstOmxBufQueue *queue_buffers;
  GstTask *pushtask;
  GStaticRecMutex taskmutex;

  GstFlowReturn fill_ret;

  gboolean wait_keyframe;
  gboolean drop_frame;

  GList *pads;

  guint skip_frame_count;
  guint skip_frame;
};

struct _GstOmxBaseDecoderClass
{
  GstElementClass parent_class;

  gchar *handle_name;

    OMX_ERRORTYPE (*omx_event) (GstOmxBaseDecoder *, OMX_EVENTTYPE, guint32,
      guint32, gpointer);
    GstFlowReturn (*omx_fill_buffer) (GstOmxBaseDecoder *,
      OMX_BUFFERHEADERTYPE *);
    GstFlowReturn (*omx_empty_buffer) (GstOmxBaseDecoder *,
      OMX_BUFFERHEADERTYPE *);
    OMX_ERRORTYPE (*init_ports) (GstOmxBaseDecoder *);
    gboolean (*parse_caps) (GstPad *, GstCaps *);
  GstCaps *(*parse_buffer) (GstOmxBaseDecoder *, GstBuffer *);

};

GType gst_omx_basedecoder_get_type (void);
gboolean gst_omx_basedecoder_add_pad (GstOmxBaseDecoder *, GstPad *);

typedef gboolean (*GstOmxBaseDecoderCondition) (gpointer, gpointer);
gboolean gst_omx_basedecoder_condition_state (gpointer targetstate,
    gpointer currentstate);
gboolean gst_omx_basedecoder_condition_enabled (gpointer enabled,
    gpointer dummy);
gboolean gst_omx_basedecoder_condition_disabled (gpointer enabled,
    gpointer dummy);

OMX_ERRORTYPE gst_omx_basedecoder_wait_for_condition (GstOmxBaseDecoder *,
    GstOmxBaseDecoderCondition, gpointer, gpointer);

void gst_omx_basedecoder_release_buffer (gpointer data);

G_END_DECLS
#endif /* __GST_OMX_BASEDECODER_H__ */
