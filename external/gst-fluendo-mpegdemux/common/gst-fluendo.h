#ifndef _INCLUDE_NOPIC_H_
#define _INCLUDE_NOPIC_H_

#ifdef ENABLE_STATIC_PLUGIN
#if ENABLE_DEMO_PLUGIN
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license, pkg, url) \
    gboolean gst_##fun##_init_static() { \
      return gst_plugin_register_static(major, minor, name, desc " [Demo Version]", init, \
          version, license, "Fluendo",pkg,url); \
    }
#else
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license, pkg, url) \
    gboolean gst_##fun##_init_static() { \
      return gst_plugin_register_static(major, minor, name, desc, init, \
          version, license, "Fluendo",pkg,url); \
    }

#endif /* ENABLE_DEMO_PLUGIN */
#else
#if ENABLE_DEMO_PLUGIN
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license,pkg,url) \
  GST_PLUGIN_DEFINE(major, minor, name, desc " [Demo Version]", init, version, license, pkg, url)
#else
#define FLUENDO_PLUGIN_DEFINE(major, minor, name, fun, desc, init, version, \
  license,pkg,url) \
  GST_PLUGIN_DEFINE(major, minor, name, desc, init, version, license, pkg, url)
#endif /* ENABLE_DEMO_PLUGIN */
#endif /* ENABLE_STATIC_PLUGIN */

#if ENABLE_DEMO_PLUGIN

typedef struct _GstFluStatistics {
  gint64 max_duration;
  gint64 decoded_duration;
} GstFluStatistics;

#define GSTFLU_SETUP_STATISTICS(sink, stats) gstflu_setup_statistics (sink, stats)
#define GSTFLU_PAD_PUSH(src, buf, stats) gstflu_pad_push (src, buf, stats)
#define GSTFLU_STATISTICS GstFluStatistics stats;

static inline void
gstflu_setup_statistics (GstPad *sink, GstFluStatistics *stats)
{
  GstQuery *q;
  GstPad *peer;
  GstFormat fmt;
  gint64 duration;

  /* 30 seconds in case we can't figure out the duration of the clip */
  stats->max_duration = G_GINT64_CONSTANT (30000000000);
  stats->decoded_duration = 0;
  peer = gst_pad_get_peer (sink);

  if (!peer)
    return;

  q = gst_query_new_duration (GST_FORMAT_TIME);
  if (!q) goto p_out;

  if (!gst_pad_query (peer, q))
    goto q_out;

  gst_query_parse_duration (q, &fmt, &duration);
  if (fmt == GST_FORMAT_TIME && GST_CLOCK_TIME_IS_VALID (duration)) {
    stats->max_duration = gst_util_uint64_scale_int (duration, DEMO_PERCENT, 100);
    /* max 30 minutes */
    if (stats->max_duration > G_GINT64_CONSTANT (30 * 60 * 100000000))
      stats->max_duration = G_GINT64_CONSTANT (30 * 60 * 100000000);
    /* min 30 seconds */
    else if (stats->max_duration < G_GINT64_CONSTANT (30 * 100000000))
      stats->max_duration = G_GINT64_CONSTANT (30 * 100000000);
  }

q_out:
  gst_query_unref (q);
p_out:
  gst_object_unref (peer);
}

static inline GstFlowReturn
gstflu_pad_push (GstPad *src, GstBuffer *out_buf, GstFluStatistics *stats)
{
  GstElement *element;

  element = gst_pad_get_parent_element (src);

  if (GST_STATE_PLAYING != GST_STATE (element)) {
    gst_object_unref (element);
    return gst_pad_push (src, out_buf);
  }

  stats->decoded_duration += GST_BUFFER_DURATION (out_buf);
  if (G_UNLIKELY (stats->decoded_duration >= stats->max_duration)) {
    gst_pad_push_event (src, gst_event_new_eos());
    GST_ELEMENT_ERROR (element, STREAM, FAILED,
        ("Fluendo decoders terminated playback of this media"
         " stream as this is an evaluation version of Fluendo's"
         " technology. To get a licensed copy of this Fluendo"
         " product please contact sales@fluendo.com."),
         (NULL));
    gst_object_unref (element);
    gst_buffer_unref (out_buf);
    return GST_FLOW_ERROR;
  }
  else {
    gst_object_unref (element);
    return gst_pad_push (src, out_buf);
  }
}

#else

#define GSTFLU_SETUP_STATISTICS(sink, stats)
#define GSTFLU_PAD_PUSH(src, buf, stats) gst_pad_push (src, buf)
#define GSTFLU_STATISTICS

#endif /* ENABLE_DEMO_PLUGIN */

#endif /* _INCLUDE_NOPIC_H_ */
