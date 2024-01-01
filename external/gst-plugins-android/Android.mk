LOCAL_PATH := $(call my-dir)

GST_PLUGIN_ANDROID_TOP := $(LOCAL_PATH)

GST_MAJORMINOR := 0.10

GST_PLUGINS_PATH := gst

GST_PLUGINS_ANDROID_DEP_INCLUDES := \
	external/icu4c/common	\
        external/gst-plugins-base/gst-libs      \
        external/gst-plugins-base/gst-libs/gst/audio      \
        external/gst-plugins-base/gst-libs/gst/video      \
        external/gst-plugins-base/gst-libs/gst/app      \
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
        $(TARGET_OUT_HEADERS)/gstreamer \
        $(TARGET_OUT_HEADERS)/glib

include $(CLEAR_VARS)

include $(GST_PLUGIN_ANDROID_TOP)/sink/audioflingersink/Android.mk
include $(GST_PLUGIN_ANDROID_TOP)/sink/audiocachesink/Android.mk
include $(GST_PLUGIN_ANDROID_TOP)/sink/surfaceflingersink/Android.mk
include $(GST_PLUGIN_ANDROID_TOP)/player/Android.mk
include $(GST_PLUGIN_ANDROID_TOP)/metadataretriever/Android.mk