LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

GST_MAJORMINOR:= 0.10

LOCAL_SRC_FILES:= \
	audioflinger_wrapper.cpp \
	gstaudioflingersink.c         

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0		\
	libcutils		\
	libutils		\
	libaudioflinger		\
	libmediaplayerservice   \
	libmedia		\
	libgstbase-0.10		\
	libgstaudio-0.10

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstaudioflingersink

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)   \
	$(LOCAL_PATH)/../..   \
	$(LOCAL_PATH)/../../log   \
	$(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
	frameworks/base/libs/audioflinger \
	frameworks/base/media/libmediaplayerservice \
	frameworks/base/media/libmedia	\
	frameworks/base/include/media


LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
	
include $(BUILD_SHARED_LIBRARY)
