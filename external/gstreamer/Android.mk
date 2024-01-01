# This file is the top android makefile for all sub-modules.

LOCAL_PATH := $(call my-dir)

GSTREAMER_TOP := $(LOCAL_PATH)

GST_MAJORMINOR:= 0.10

GST_PLUGINS_PATH := gst

GSTREAMER_DEP_INCLUDES := \
	external/icu4c/common		\
        external/libxml2/include                \
        external/glib                           \
        external/glib/android                   \
        external/glib/glib                      \
        external/glib/gmodule                   \
        external/glib/gobject                   \
        external/glib/gthread			\
	$(TARGET_OUT_HEADERS)/glib/		\

include $(CLEAR_VARS)

include $(GSTREAMER_TOP)/gst/Android.mk
include $(GSTREAMER_TOP)/libs/gst/base/Android.mk
include $(GSTREAMER_TOP)/libs/gst/controller/Android.mk
include $(GSTREAMER_TOP)/libs/gst/dataprotocol/Android.mk
include $(GSTREAMER_TOP)/libs/gst/net/Android.mk
include $(GSTREAMER_TOP)/plugins/elements/Android.mk
include $(GSTREAMER_TOP)/plugins/indexers/Android.mk
include $(GSTREAMER_TOP)/tools/Android.mk

