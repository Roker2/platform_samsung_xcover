LOCAL_PATH := $(call my-dir)

GST_PLUGINS_BAD_TOP := $(LOCAL_PATH)

GST_MAJORMINOR := 0.10

GST_PLUGINS_PATH := gst

GST_PLUGINS_BAD_DEP_INCLUDES := \
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
        external/icu4c/common   \
	$(TARGET_OUT_HEADERS)/glib	\
	$(TARGET_OUT_HEADERS)/gstreamer	\

include $(CLEAR_VARS)

include $(GST_PLUGINS_BAD_TOP)/gst/autoconvert/Android.mk
include $(GST_PLUGINS_BAD_TOP)/gst/rtpmux/Android.mk

