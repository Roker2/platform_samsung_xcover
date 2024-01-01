LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
        gstalsadeviceprobe.c \
        gstalsamixer.c  \
        gstalsamixerelement.c \
        gstalsamixertrack.c \
        gstalsamixeroptions.c \
        gstalsaplugin.c \
        gstalsasink.c   \
        gstalsasrc.c \
        gstalsa.c

LOCAL_SHARED_LIBRARIES :=	\
	libgstinterfaces-0.10	\
	libgstaudio-0.10	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0		\
	libasound

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstalsasink

LOCAL_C_INCLUDES := 					\
	$(LOCAL_PATH)					\
	$(GST_PLUGINS_BASE_TOP)				\
	$(GST_PLUGINS_BASE_TOP)/android			\
	$(GST_PLUGINS_BASE_TOP)/gst-libs		\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)		\
	external/alsa-lib/include/

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H -D_POSIX_SOURCE	

include $(BUILD_SHARED_LIBRARY)
