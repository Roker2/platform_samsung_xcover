/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2006 Wim Taymans <wim@fluendo.com>
 *
 * GstAudioCacheSink.c:
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
 *
 * Write incoming data to audio cache.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst-i18n-lib.h>

#include <stdio.h>              /* for fseeko() */
#ifdef HAVE_STDIO_EXT_H
#include <stdio_ext.h>          /* for __fbufsize, for debugging */
#endif
#include <errno.h>
#include "gstaudiocachesink.h"
#include <string.h>
#include <sys/types.h>

#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
#ifdef _MSC_VER                 /* Check if we are using MSVC, fileno is deprecated in favour */
#define fileno _fileno          /* of _fileno */
#endif
#endif

#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) { " G_STRINGIFY (G_BYTE_ORDER) " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; ")
    );

static GstElementClass *parent_class = NULL;

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_BUFFER_MODE,
  PROP_BUFFER_SIZE,
  PROP_AUDIO_SINK,
  PROP_LAST
};

GST_DEBUG_CATEGORY_STATIC (audiocachesink);
#define GST_CAT_DEFAULT audiocachesink

static void gst_audiocache_sink_dispose (GObject * object);
static void gst_audiocache_sink_finalize (GObject * object);

static void gst_audiocache_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static GstCaps *gst_audiocache_sink_getcaps (GstBaseSink * bsink);
static gboolean gst_audiocache_sink_open (GstAudioCacheSink * sink);
static void gst_audiocache_sink_reset (GstAudioCacheSink * sink);
static gboolean gst_audiocache_sink_parse_caps (GstAudioCacheSink * sink, GstCaps * caps);

static gboolean gst_audiocache_sink_start (GstBaseSink * bsink);
static gboolean gst_audiocache_sink_stop (GstBaseSink * bsink);
static gboolean gst_audiocache_sink_event (GstBaseSink * bsink, GstEvent * event);
static GstFlowReturn gst_audiocache_sink_render (GstBaseSink * bsink, GstBuffer * buffer);

static gboolean gst_audiocache_sink_setcaps (GstBaseSink * bsink, GstCaps * caps);
static void gst_audiocache_sink_fixate (GstBaseSink * bsink, GstCaps * caps);


static void
gst_audiocache_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "Audio Cache Sink",
      "Sink/Cache", "Write stream to memory",
      "Thomas Vander Stichele <thomas at apestaart dot org>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
  GST_DEBUG_CATEGORY_INIT (audiocachesink, "audiocachesink", 0,
      "audiocache sink trace");
}

static void
gst_audiocache_sink_class_init (GstAudioCacheSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);
  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_audiocache_sink_dispose;
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_audiocache_sink_finalize);

  gobject_class->set_property = gst_audiocache_sink_set_property;

  g_object_class_install_property (gobject_class, PROP_AUDIO_SINK,
      g_param_spec_pointer ("audiosink", "Audiosink",
          "The pointer of MediaPlayerBase::AudioSink", G_PARAM_WRITABLE));

  gstbasesink_class->get_times = NULL;
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_audiocache_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_audiocache_sink_stop);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_audiocache_sink_render);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_audiocache_sink_event);
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_audiocache_sink_getcaps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_audiocache_sink_setcaps);
  gstbasesink_class->fixate = GST_DEBUG_FUNCPTR (gst_audiocache_sink_fixate);
}

static void
gst_audiocache_sink_init (GstAudioCacheSink * sink, GstAudioCacheSinkClass * g_class)
{
  GST_DEBUG_OBJECT (sink, "Enter");
  gst_audiocache_sink_reset (sink);

  gst_base_sink_set_sync (GST_BASE_SINK (sink), FALSE);
}

static void
gst_audiocache_sink_reset (GstAudioCacheSink * sink)
{
  GST_DEBUG_OBJECT (sink, "Enter");
  if (sink->audiocache_device != NULL) {
    audiocache_device_release (sink->audiocache_device);
    sink->audiocache_device = NULL;
  }

  sink->audiosink = NULL;
  sink->init = FALSE;
  sink->eos_time = -1;
}

static void
gst_audiocache_sink_finalize (GObject * object)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (object);

  gst_audiocache_sink_reset (sink);

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (object));
  GST_DEBUG_OBJECT (sink, "finalize audio cache sink");
}

GType
gst_audiocache_sink_get_type (void)
{
  static GType audiocachesink_type = 0;

  if (!audiocachesink_type) {
    static const GTypeInfo audiocachesink_info = {
      sizeof (GstAudioCacheSinkClass),
      gst_audiocache_sink_base_init,
      NULL,
      (GClassInitFunc) gst_audiocache_sink_class_init,
      NULL,
      NULL,
      sizeof (GstAudioCacheSink),
      0,
      (GInstanceInitFunc) gst_audiocache_sink_init,
    };

    audiocachesink_type =
        g_type_register_static (GST_TYPE_BASE_SINK, "GstAudioCacheSink",
        &audiocachesink_info, 0);
  }

  return audiocachesink_type;
}

static void
gst_audiocache_sink_dispose (GObject * object)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (object);

  if (sink->probed_caps) {
    gst_caps_unref (sink->probed_caps);
    sink->probed_caps = NULL;
  }
  G_OBJECT_CLASS (parent_class)->dispose (object);

  GST_DEBUG_OBJECT (sink, "dispose audio cache sink");
}

static void
gst_audiocache_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      GST_DEBUG_OBJECT (sink, "set property location");
      break;
    case PROP_BUFFER_MODE:
      GST_DEBUG_OBJECT (sink, "set property buffer mode");
      break;
    case PROP_BUFFER_SIZE:
      GST_DEBUG_OBJECT (sink, "set property buffer size");
      break;
    case PROP_AUDIO_SINK:
      sink->audiosink = g_value_get_pointer (value);
      GST_DEBUG_OBJECT (sink, "set audiosink: %p", sink->audiosink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_audiocache_sink_getcaps (GstBaseSink * bsink)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (bsink);
  GstCaps *caps;

  if (sink->audiocache_device == NULL || sink->init == FALSE) {
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD
            (bsink)));
  } else if (sink->probed_caps) {
    caps = gst_caps_copy (sink->probed_caps);
  } else {
    caps = gst_caps_new_any ();
    if (caps && !gst_caps_is_empty (caps)) {
      sink->probed_caps = gst_caps_copy (caps);
    }
  }
  return caps;
}

static gboolean
gst_audiocache_sink_parse_caps (GstAudioCacheSink * sink, GstCaps * caps)
{
  GST_DEBUG_OBJECT (sink, "parse caps");
  const gchar *mimetype;
  GstStructure *structure;

  structure = gst_caps_get_structure (caps, 0);

  /* we have to differentiate between int and float formats */
  mimetype = gst_structure_get_name (structure);

  if (!strncmp (mimetype, "audio/x-raw-int", 15)) {
    gint endianness;

    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate) &&
            gst_structure_get_int (structure, "channels", &sink->channels) &&
            gst_structure_get_int (structure, "width", &sink->width)))
      goto parse_error;

    /* extract endianness if needed */
    if (sink->width > 8) {
      if (!gst_structure_get_int (structure, "endianness", &endianness))
        goto parse_error;
    } else {
      endianness = G_BYTE_ORDER;
    }

  } else if (!strncmp (mimetype, "audio/x-raw-float", 17)) {
    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate) &&
            gst_structure_get_int (structure, "channels", &sink->channels) &&
            gst_structure_get_int (structure, "width", &sink->width)))
      goto parse_error;

  } else if (!strncmp (mimetype, "audio/x-alaw", 12)) {
    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate) &&
            gst_structure_get_int (structure, "channels", &sink->channels)))
      goto parse_error;

    sink->width = 8;

  } else if (!strncmp (mimetype, "audio/x-mulaw", 13)) {
    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate) &&
            gst_structure_get_int (structure, "channels", &sink->channels)))
      goto parse_error;

    sink->width = 8;

  } else if (!strncmp (mimetype, "audio/x-iec958", 14)) {
    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate)))
      goto parse_error;

    sink->width = 16;
    sink->channels = 2;

  } else if (!strncmp (mimetype, "audio/x-ac3", 11)) {
    /* extract the needed information from the cap */
    if (!(gst_structure_get_int (structure, "rate", &sink->rate)))
      goto parse_error;

    sink->width = 16;
    sink->channels = 2;

  } else {
    goto parse_error;
  }

  sink->bps = (sink->width >> 3) * sink->channels;
  GST_DEBUG_OBJECT (sink, "parse caps: samplerate %d, channels %d", sink->rate, sink->channels);

  return TRUE;

  /* ERRORS */
parse_error:
  {
    GST_DEBUG_OBJECT (sink, "could not parse caps");
    return FALSE;
  }
}

static gboolean
gst_audiocache_sink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (bsink);

  GST_DEBUG_OBJECT (sink, "set caps");

  /* parse new caps */
  if (!gst_audiocache_sink_parse_caps (sink, caps))
    goto parse_error;

  return TRUE;

  /* ERRORS */
parse_error:
  {
    GST_ERROR_OBJECT (sink, "could not parse caps");
    return FALSE;
  }
}

static void
gst_audiocache_sink_fixate (GstBaseSink * bsink, GstCaps * caps)
{
  GstStructure *s;
  gint width, depth;
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (bsink);

  s = gst_caps_get_structure (caps, 0);
  GST_DEBUG_OBJECT (sink, "Enter");

  /* fields for all formats */
  gst_structure_fixate_field_nearest_int (s, "rate", 44100);
  gst_structure_fixate_field_nearest_int (s, "channels", 2);
  gst_structure_fixate_field_nearest_int (s, "width", 16);

  /* fields for int */
  if (gst_structure_has_field (s, "depth")) {
    gst_structure_get_int (s, "width", &width);
    /* round width to nearest multiple of 8 for the depth */
    depth = GST_ROUND_UP_8 (width);
    gst_structure_fixate_field_nearest_int (s, "depth", depth);
  }
  if (gst_structure_has_field (s, "signed"))
    gst_structure_fixate_field_boolean (s, "signed", TRUE);
  if (gst_structure_has_field (s, "endianness"))
    gst_structure_fixate_field_nearest_int (s, "endianness", G_BYTE_ORDER);
}

static gboolean
gst_audiocache_sink_open (GstAudioCacheSink * sink)
{
  gint mode;

  GST_DEBUG_OBJECT (sink, "Enter");
//  g_return_val_if_fail (sink != NULL, FALSE);

  if (sink->audiocache_device == NULL) {
    if (sink->audiosink)  {
      if (!(sink->audiocache_device = audiocache_device_open(sink->audiosink))) {
        GST_ERROR_OBJECT (sink, "open audio cache device failed ");
        return FALSE;
      }
      GST_DEBUG_OBJECT (sink, "open an audio cache sink, %p", sink->audiocache_device);
    }
  }

  return TRUE;
}

/* handle events (search) */
static gboolean
gst_audiocache_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstEventType type;
  GstAudioCacheSink *sink;

  sink = GST_AUDIOCACHESINK (bsink);

  type = GST_EVENT_TYPE (event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (sink, "Audio cache event flush start");
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (sink, "Audio cache event flush stop");
      break;
    case GST_EVENT_EOS:
      /* now wait till we played everything */
      if (sink->eos_time != -1)
        gst_base_sink_wait_eos (GST_BASE_SINK (sink), sink->eos_time, NULL);
      GST_DEBUG_OBJECT (sink, "Audio cache event EOS");
      break;
    case GST_EVENT_NEWSEGMENT:
    {
      audiocache_device_set (sink->audiocache_device, sink->channels, sink->rate);
      sink->init = TRUE;
      gdouble rate;

      /* we only need the rate */
      gst_event_parse_new_segment_full (event, NULL, &rate, NULL, NULL,
          NULL, NULL, NULL);

      GST_DEBUG_OBJECT (sink, "new segment rate of %f", rate);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

static GstFlowReturn
gst_audiocache_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstAudioCacheSink *sink;
  guint size, samples;
  guint8 *data;
  gint ret = 0;
  GstClockTime time, stop, render_start, render_stop;

  sink = GST_AUDIOCACHESINK (bsink);

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);
  time = GST_BUFFER_TIMESTAMP (buffer);

  samples = size / sink->bps;
  stop = time + gst_util_uint64_scale_int (samples, GST_SECOND, sink->rate);

  render_start =
      gst_segment_to_running_time (&bsink->segment, GST_FORMAT_TIME, time);
  render_stop =
      gst_segment_to_running_time (&bsink->segment, GST_FORMAT_TIME, stop);

  if (bsink->segment.rate >= 0.0) {
    sink->eos_time = render_stop;
  } else {
    sink->eos_time = render_start;
  }

  if (size > 0 && data != NULL) {
    if (sink->audiocache_device != NULL) {
      ret = audiocache_device_write (sink->audiocache_device, data, size);
      if (ret <= 0) {
        GST_WARNING_OBJECT (sink, "Write failure");
        return GST_FLOW_ERROR;
      }
    }
  }
  return GST_FLOW_OK;
}

static gboolean
gst_audiocache_sink_start (GstBaseSink * bsink)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (bsink);

  if (gst_audiocache_sink_open (sink) ) {
    audiocache_device_start (sink->audiocache_device);
  }
  return TRUE;
}

static gboolean
gst_audiocache_sink_stop (GstBaseSink * bsink)
{
  GstAudioCacheSink *sink = GST_AUDIOCACHESINK (bsink);
  GST_DEBUG_OBJECT (sink, "Enter");

  if (sink->audiocache_device != NULL) {
    GST_DEBUG_OBJECT (sink, "audio cache sink stop");
    audiocache_device_stop (sink->audiocache_device);
  }
  return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "audiocachesink", GST_RANK_PRIMARY,
      GST_TYPE_AUDIOCACHESINK);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "audiocachesink",
    "audiocache sink audio", plugin_init, VERSION, "LGPL", "GStreamer",
    "http://gstreamer.net/")
