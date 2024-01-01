LOCAL_PATH := $(call my-dir)

GST_PLUGINS_GOOD_TOP := $(LOCAL_PATH)

GST_MAJORMINOR := 0.10

GST_PLUGINS_PATH := gst

GST_PLUGINS_GOOD_DEP_INCLUDES := \
	external/icu4c/common	\
        external/gst-plugins-base/gst-libs      \
        external/gstreamer              \
        external/gstreamer/android      \
        external/gstreamer/libs         \
        external/gstreamer/gst          \
        external/glib                   \
        external/glib/android           \
        external/glib/glib              \
        external/glib/gmodule           \
        external/glib/gobject           \
        external/glib/gthread           \
        external/libxml2/include        \
	$(TARGET_OUT_HEADERS)/glib	\
	$(TARGET_OUT_HEADERS)/gstreamer	\

include $(CLEAR_VARS)

include $(GST_PLUGINS_GOOD_TOP)/gst/autodetect/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/avi/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/flv/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/matroska/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/id3demux/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/apetag/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/qtdemux/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/rtp/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/rtpmanager/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/rtsp/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/udp/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/gst/wavparse/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/ext/flac/Android.mk
include $(GST_PLUGINS_GOOD_TOP)/ext/soup/Android.mk

include $(GST_PLUGINS_GOOD_TOP)/gst/law/Android.mk
