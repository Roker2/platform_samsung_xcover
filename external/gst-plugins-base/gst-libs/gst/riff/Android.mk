LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	\
	riff.c 		\
	riff-media.c 	\
	riff-read.c

LOCAL_SHARED_LIBRARIES :=	\
	libgsttag-0.10		\
	libgstaudio-0.10	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE:= libgstriff-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES := 				\
	$(LOCAL_PATH)				\
	$(GST_PLUGINS_BASE_TOP)			\
	$(GST_PLUGINS_BASE_TOP)/android		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs/gst/audio/android	\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
