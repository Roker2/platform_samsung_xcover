/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gstaudiocachesink.h:
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


#ifndef __GST_AUDIOCACHESINK_H__
#define __GST_AUDIOCACHESINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "audiocache_wrapper.h"

G_BEGIN_DECLS

#define GST_TYPE_AUDIOCACHESINK            (gst_audiocache_sink_get_type())
#define GST_AUDIOCACHESINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIOCACHESINK,GstAudioCacheSink))
#define GST_AUDIOCACHESINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIOCACHESINK,GstAudioCacheSinkClass))
#define GST_IS_AUDIOCACHESINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIOCACHESINK))
#define GST_IS_AUDIOCACHESINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIOCACHESINK))

typedef struct _GstAudioCacheSink GstAudioCacheSink;
typedef struct _GstAudioCacheSinkClass GstAudioCacheSinkClass;

/**
 * GstAudioCacheSink:
 *
 * Opaque #GstAudioCacheSink structure.
 */
struct _GstAudioCacheSink {
  GstBaseSink parent;

  /*< private >*/
  gint    rate;
  gint    channels;
  gint    width;
  gint    bps;

  GstClockTime eos_time;

  gboolean init;
  gpointer audiosink;
  GstCaps  *probed_caps;
  AudioCacheDeviceHandle audiocache_device;
};

struct _GstAudioCacheSinkClass {
  GstBaseSinkClass parent_class;
};

GType gst_audiocache_sink_get_type(void);

G_END_DECLS

#endif /* __GST_FILE_SINK_H__ */
