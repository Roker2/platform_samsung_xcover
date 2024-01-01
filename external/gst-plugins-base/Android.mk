LOCAL_PATH := $(call my-dir)

GST_MAJORMINOR := 0.10

GST_PLUGINS_BASE_TOP := $(LOCAL_PATH)

GST_PLUGINS_PATH := gst

GST_PLUGINS_BASE_DEP_INCLUDES := \
	external/icu4c/common	\
        external/gstreamer              \
        external/gstreamer/android      \
        external/gstreamer/libs         \
        external/gstreamer/gst          \
        external/glib                   \
        external/glib/android           \
        external/glib/glib              \
        external/glib/gmodule           \
        external/glib/gobject           \
        external/glib/gthread		\
	external/libxml2/include	\
	external/liboil			\
	external/liboil/liboil/android	\
	$(TARGET_OUT_HEADERS)/glib	\
	$(TARGET_OUT_HEADERS)/gstreamer \

include $(CLEAR_VARS)

# base libraries
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/tag/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/audio/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/video/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/riff/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/pbutils/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/rtp/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/rtsp/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/interfaces/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/app/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/sdp/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst-libs/gst/netbuffer/Android.mk

# plugins without external dependencies
include $(GST_PLUGINS_BASE_TOP)/gst/app/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/audiotestsrc/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/audioconvert/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/audiorate/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/audioresample/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/ffmpegcolorspace/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/gdp/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/playback/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/subparse/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/tcp/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/typefind/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/videorate/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/videoscale/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/videotestsrc/Android.mk
include $(GST_PLUGINS_BASE_TOP)/gst/volume/Android.mk

# plugins WITH external dependencies
include $(GST_PLUGINS_BASE_TOP)/ext/alsa/Android.mk
