/* GStreamer
 * Copyright (C) <2009> Prajnashi S <prajnashi@gmail.com>
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

#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include "gstsurfaceflingersink.h"

GST_DEBUG_CATEGORY (gst_surfaceflinger_sink_debug);
#define GST_CAT_DEFAULT gst_surfaceflinger_sink_debug

/* elementfactory information */
static const GstElementDetails gst_surfaceflinger_sink_details =
GST_ELEMENT_DETAILS ("android's surface flinger sink",
    "Sink/Video",
    "A linux framebuffer videosink",
    "Prajnashi S <prajnashi@gmail.com>");

enum
{
  ARG_0,
  PROP_SURFACE,
  PROP_VIDEOLAYER,
  PROP_SURFACE_CHANGED,
};

static void gst_surfaceflinger_sink_base_init (gpointer g_class);
static void gst_surfaceflinger_sink_class_init (GstSurfaceFlingerSinkClass * klass);
static void gst_surfaceflinger_sink_get_times (GstBaseSink * basesink,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);

static gboolean gst_surfaceflinger_sink_setcaps (GstBaseSink * bsink, GstCaps * caps);

static GstFlowReturn gst_surfaceflinger_sink_render (GstBaseSink * bsink,
    GstBuffer * buff);
static gboolean gst_surfaceflinger_sink_start (GstBaseSink * bsink);
static gboolean gst_surfaceflinger_sink_stop (GstBaseSink * bsink);

static void gst_surfaceflinger_sink_init (GstSurfaceFlingerSink * surfacesink);
static void gst_surfaceflinger_sink_finalize (GObject * object);
static void gst_surfaceflinger_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_surfaceflinger_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn
gst_surfaceflinger_sink_change_state (GstElement * element, GstStateChange transition);

static GstCaps *gst_surfaceflinger_sink_getcaps (GstBaseSink * bsink);

static GstVideoSinkClass *parent_class = NULL;

/* TODO: support more pixel form in the future */
#define GST_SURFACE_TEMPLATE_CAPS GST_VIDEO_CAPS_RGB_16
#define GST_SURFACE_TEMPLATE_I420_CAPS "video/x-raw-yuv," \
                         "format = (fourcc) { I420 }, " \
                         "width = (int) [ 16, 2048 ], " \
                         "height = (int) [ 16, 2048 ], " \
                         "framerate = (fraction) [ 0, 60 ]"
#define GST_SURFACE_TEMPLATE_UYVY_CAPS "video/x-raw-yuv," \
                                         "format = (fourcc) { UYVY }, " \
                                         "width = (int) [ 16, 2048 ], " \
                                         "height = (int) [ 16, 2048 ], " \
                                         "framerate = (fraction) [ 0, MAX ]"

static GstStaticPadTemplate gst_surfaceflingersink_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_SURFACE_TEMPLATE_CAPS "; "
        GST_SURFACE_TEMPLATE_I420_CAPS "; "
        GST_SURFACE_TEMPLATE_UYVY_CAPS)
    );

static void
gst_surfaceflinger_sink_base_init (gpointer g_class)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_set_details (element_class, &gst_surfaceflinger_sink_details);

    gst_element_class_add_pad_template (element_class,
        gst_static_pad_template_get (&gst_surfaceflingersink_sink_template_factory));
}


static void
gst_surfaceflinger_sink_get_times (GstBaseSink * basesink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (basesink);

    if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) 
    {
        *start = GST_BUFFER_TIMESTAMP (buffer);
        if (GST_BUFFER_DURATION_IS_VALID (buffer)) 
        {
            *end = *start + GST_BUFFER_DURATION (buffer);
        } 
        else 
        {
            if (surfacesink->fps_n > 0) 
            {
                *end = *start +
                gst_util_uint64_scale_int (GST_SECOND, surfacesink->fps_d,
                surfacesink->fps_n);
            }
        }
    }
}

static GstCaps *
gst_surfaceflinger_sink_getcaps (GstBaseSink * bsink)
{
    GstSurfaceFlingerSink *surfacesink;
    GstCaps *caps;

    surfacesink = GST_SURFACEFLINGERSINK (bsink);
    if(videoflinger_get_accelerate_hw(surfacesink->video_layer) == OVERLAY || videoflinger_get_accelerate_hw(surfacesink->video_layer) == GCU )
    {
#ifdef OVERLAY_FORMAT
        if(strcmp(OVERLAY_FORMAT, "422I") == 0)
        {
            caps = gst_caps_from_string (GST_SURFACE_TEMPLATE_UYVY_CAPS);
        }
        else
        {
            caps = gst_caps_from_string (GST_SURFACE_TEMPLATE_I420_CAPS);
        }
#else
        caps = gst_caps_from_string (GST_SURFACE_TEMPLATE_I420_CAPS);
#endif
    }
    else if(videoflinger_get_accelerate_hw(surfacesink->video_layer) == COPYBIT)
    {
        caps = gst_caps_from_string (GST_SURFACE_TEMPLATE_I420_CAPS);
    }
    else
    {
        caps = gst_caps_from_string (GST_SURFACE_TEMPLATE_CAPS);
    }
    return caps;
}

static gboolean
gst_surfaceflinger_sink_setcaps (GstBaseSink * bsink, GstCaps * vscapslist)
{
    GstSurfaceFlingerSink *surfacesink;
    GstStructure *structure;
    const GValue *fps;
	guint32 f4cc;

    surfacesink = GST_SURFACEFLINGERSINK (bsink);

    GST_DEBUG_OBJECT (surfacesink, "caps after linked: %s",  gst_caps_to_string(vscapslist));

    structure = gst_caps_get_structure (vscapslist, 0);
  
    fps = gst_structure_get_value (structure, "framerate");
    surfacesink->fps_n = gst_value_get_fraction_numerator (fps);
    surfacesink->fps_d = gst_value_get_fraction_denominator (fps);

    gst_structure_get_int (structure, "width", &surfacesink->width);
    gst_structure_get_int (structure, "height", &surfacesink->height);

	//check fourcc
	if(FALSE == gst_structure_get_fourcc(structure, "fourcc", &f4cc)) {
		if(FALSE == gst_structure_get_fourcc(structure, "format", &f4cc)) {
			f4cc = 0;
		}
	}

	if(f4cc == GST_MAKE_FOURCC('U','Y','V','Y')) {
		surfacesink->pixel_format  = VIDEO_FLINGER_UYVY;
	}
	else if(f4cc == GST_MAKE_FOURCC('I','4','2','0')) {
		surfacesink->pixel_format  = VIDEO_FLINGER_I420;
	}
	else {
		g_warning("There is no fourcc information in MIME or fourcc isn't UVVY, I420, but 0x%x", f4cc);
	}

	//    gst_structure_get_int (structure, "format", &surfacesink->pixel_format);
//    surfacesink->pixel_format  = VIDEO_FLINGER_UYVY;

    GST_DEBUG_OBJECT (surfacesink, "framerate=%d/%d", surfacesink->fps_n, surfacesink->fps_d);
    GST_DEBUG_OBJECT (surfacesink, "register framebuffers: width=%d,  height=%d,  pixel_format=%d",  
        surfacesink->width, surfacesink->height, surfacesink->pixel_format);

	GST_DEBUG_OBJECT (surfacesink, "gst_surfaceflinger_sink_setcaps return true");
    return TRUE;
}

static void gst_buffer_unref_wrap(void *buf)
{
    GstBuffer *buffer = (GstBuffer*)buf;
    gst_buffer_unref(buffer);
}

static GstFlowReturn
gst_surfaceflinger_sink_preroll (GstBaseSink * bsink, GstBuffer * buf)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (bsink);

	/* Only allowed to be called one time  */
	if( !(surfacesink->initialized) )
		surfacesink->initialized = 1;
	else
		return GST_FLOW_OK; 

    GST_DEBUG_OBJECT(surfacesink, "post buffer=%p, size=%d", GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));

    /* register frame buffer */
    videoflinger_device_register_framebuffers(
        surfacesink->videodev,surfacesink->width, 
        surfacesink->height, surfacesink->pixel_format);

    GST_DEBUG_OBJECT (surfacesink, "gst_surfaceflinger_sink_preroll return GST_FLOW_OK");
    return GST_FLOW_OK;
}

static GstFlowReturn
gst_surfaceflinger_sink_render (GstBaseSink * bsink, GstBuffer * buf)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (bsink);
    GST_DEBUG_OBJECT(surfacesink, "post buffer=%p, size=%d", GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf));
    /* we'll trig surface change immediately */
#if 0
    if( surfacesink->surface_changed ) {
        if( videoflinger_device_setVideoSurface(surfacesink->videodev, surfacesink->isurface)){
            // err happened
            return GST_FLOW_ERROR;
        }
        surfacesink->surface_changed  = 0;
    }
#endif
    surfacesink->render_framecnt++;
    /* Add a reference count, it will be decreased in videoflinger_device_post. */   
    gst_buffer_ref(buf);
	
    /* post frame buffer */
    videoflinger_device_post(surfacesink->videodev, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf), gst_buffer_unref_wrap, buf);

    GST_DEBUG_OBJECT (surfacesink, "gst_surfaceflinger_sink_render return GST_FLOW_OK");
    return GST_FLOW_OK;
}

static gboolean
gst_surfaceflinger_sink_start (GstBaseSink * bsink)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (bsink);

    surfacesink->render_framecnt = 0;
    gettimeofday(&surfacesink->start_clk, NULL);

    /* release previous video device */
    if (surfacesink->videodev != NULL) 
    {
        videoflinger_device_release ( surfacesink->videodev);
        surfacesink->videodev = NULL;
    }

    /* create a new video device */
    GST_DEBUG_OBJECT (surfacesink, "ISurface = %p", surfacesink->isurface);
    GST_DEBUG_OBJECT (surfacesink, "VideoLayer = %s", surfacesink->video_layer);
    surfacesink->videodev = videoflinger_device_create(surfacesink->isurface, surfacesink->video_layer);
    if ( surfacesink->videodev == NULL)
    {
        GST_ERROR_OBJECT (surfacesink, "Failed to create video device.");
        return FALSE;
    }    

    GST_DEBUG_OBJECT (surfacesink, "gst_surfaceflinger_sink_start return TRUE");
    return TRUE;
}

static gboolean
gst_surfaceflinger_sink_stop (GstBaseSink * bsink)
{
    GstSurfaceFlingerSink *surfacesink;
    struct timeval clk;
    gettimeofday(&clk, NULL);
    surfacesink = GST_SURFACEFLINGERSINK (bsink);

    if (surfacesink->videodev != NULL) 
    {
        videoflinger_device_release ( surfacesink->videodev);
        surfacesink->videodev = NULL;
    }

	{
		int	duration_msec = (clk.tv_sec - surfacesink->start_clk.tv_sec) * 1000 + (clk.tv_usec - surfacesink->start_clk.tv_usec) / 1000;
		int fps_int = 0, fps_dot = 0;
		if(duration_msec != 0) {
			//use integer division instead of float division, because on some platform, toolchain probably has some issue for float operation
			int fps_100 = surfacesink->render_framecnt*1000*100/duration_msec;
			fps_int = fps_100/100;
			fps_dot = fps_100 - fps_int*100;
		}
		printf("-------++++++ gstsurfaceflingersink rendered %d frames, in %d msec, fps is %d.%02d.\n", surfacesink->render_framecnt, duration_msec, fps_int, fps_dot);
	}

    return TRUE;
}

static void
gst_surfaceflinger_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (object);

    switch (prop_id) 
    {
    case PROP_SURFACE:
        surfacesink->isurface = g_value_get_pointer(value);
        GST_DEBUG_OBJECT (surfacesink, "set property: ISureface = %p",  surfacesink->isurface);
        break;
    case PROP_VIDEOLAYER:
        surfacesink->video_layer = g_value_dup_string(value);
        GST_DEBUG_OBJECT (surfacesink, "set property: Video layer = %s",  surfacesink->video_layer);
        break;
    case PROP_SURFACE_CHANGED:
        /* trig surface change immediately */
        videoflinger_device_setVideoSurface(surfacesink->videodev, surfacesink->isurface);
        GST_DEBUG_OBJECT (surfacesink, "set property: Surface changed = %d",  surfacesink->surface_changed);		
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


static void
gst_surfaceflinger_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
    GstSurfaceFlingerSink *surfacesink;

    surfacesink = GST_SURFACEFLINGERSINK (object);

    switch (prop_id) 
    {
    case PROP_SURFACE:
        g_value_set_pointer (value, surfacesink->isurface);
        GST_DEBUG_OBJECT (surfacesink, "get property: ISurface = %p.",  surfacesink->isurface);
        break;
    case PROP_VIDEOLAYER:
        g_value_set_string (value, surfacesink->video_layer);
        GST_DEBUG_OBJECT (surfacesink, "set property: Video layer = %s",  surfacesink->video_layer);
        break;
    case PROP_SURFACE_CHANGED:
        g_value_set_int(value, surfacesink->surface_changed);
        GST_DEBUG_OBJECT (surfacesink, "set property: Surface changed = %d",  surfacesink->surface_changed);		
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/* TODO: delete me later */
static GstStateChangeReturn
gst_surfaceflinger_sink_change_state (GstElement * element, GstStateChange transition)
{
    GstSurfaceFlingerSink *surfacesink;
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    g_return_val_if_fail (GST_IS_SURFACEFLINGERSINK (element), GST_STATE_CHANGE_FAILURE);
    surfacesink = GST_SURFACEFLINGERSINK (element);

    ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

    switch (transition) 
    {
    default:
      break;
    }
    return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
    GST_DEBUG_CATEGORY_INIT (gst_surfaceflinger_sink_debug, "surfaceflingersink",
      0, "Video sink plugin");

    if (!gst_element_register (plugin, "surfaceflingersink", GST_RANK_PRIMARY,
          GST_TYPE_SURFACEFLINGERSINK))
    {
        return FALSE;
    }

    return TRUE;
}

static void
gst_surfaceflinger_sink_class_init (GstSurfaceFlingerSinkClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSinkClass *gstvs_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstvs_class = (GstBaseSinkClass *) klass;

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = gst_surfaceflinger_sink_set_property;
    gobject_class->get_property = gst_surfaceflinger_sink_get_property;
    gobject_class->finalize = gst_surfaceflinger_sink_finalize;

    gstelement_class->change_state =
        GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_change_state);

    g_object_class_install_property (gobject_class, PROP_SURFACE,
        g_param_spec_pointer("surface", "Surface",
        "The pointer of ISurface interface", G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_VIDEOLAYER,
        g_param_spec_string("videolayer", "Video Layer",
        "Set the video display layer", NULL, G_PARAM_READWRITE));
    g_object_class_install_property (gobject_class, PROP_SURFACE_CHANGED,
        g_param_spec_int("setsurface", "Set Surface",
        "Set the video display surface", 0, 1, 0, G_PARAM_READWRITE));


    gstvs_class->set_caps = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_setcaps);
    gstvs_class->get_caps = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_getcaps);
    gstvs_class->get_times = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_get_times);
    gstvs_class->preroll = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_preroll);
    gstvs_class->render = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_render);
    gstvs_class->start = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_start);
    gstvs_class->stop = GST_DEBUG_FUNCPTR (gst_surfaceflinger_sink_stop);
}

static void
gst_surfaceflinger_sink_init (GstSurfaceFlingerSink * surfacesink)
{
    surfacesink->isurface = NULL;
    surfacesink->videodev = NULL;
    surfacesink->video_layer = NULL;
    surfacesink->surface_changed = 0;

    surfacesink->fps_n = 0;
    surfacesink->fps_d = 1;
    surfacesink->width = 320;
    surfacesink->height = 240;
    surfacesink->pixel_format = -1;
    /* 20ms is more than enough, 80-130ms is noticable */
    gst_base_sink_set_max_lateness (GST_BASE_SINK (surfacesink), 300 * GST_MSECOND);
    gst_base_sink_set_qos_enabled (GST_BASE_SINK (surfacesink), FALSE);
}

static void
gst_surfaceflinger_sink_finalize (GObject * object)
{
    GstSurfaceFlingerSink *surfacesink = GST_SURFACEFLINGERSINK (object);
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

GType
gst_surfaceflinger_sink_get_type (void)
{
    static GType surfacesink_type = 0;

    if (!surfacesink_type) {
        static const GTypeInfo surfacesink_info = {
            sizeof (GstSurfaceFlingerSinkClass),
            gst_surfaceflinger_sink_base_init,
            NULL,
            (GClassInitFunc) gst_surfaceflinger_sink_class_init,
            NULL,
            NULL,
            sizeof (GstSurfaceFlingerSink),
            0,
            (GInstanceInitFunc) gst_surfaceflinger_sink_init,
            };

        surfacesink_type =
            g_type_register_static (GST_TYPE_BASE_SINK, "GstSurfaceFlingerSink",
            &surfacesink_info, 0);
    }
    return surfacesink_type;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, 
    "surfaceflingersink", "android surface flinger sink", 
    plugin_init, VERSION,"LGPL","GStreamer","http://gstreamer.net/")
