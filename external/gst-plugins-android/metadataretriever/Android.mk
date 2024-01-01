LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_TOP_PATH := $(LOCAL_PATH)/../..

LOCAL_SRC_FILES:= \
    GstMetadataRetrieverPipeline.cpp \
    gstscreenshot.c
 
LOCAL_SHARED_LIBRARIES := \
    libgstapp-0.10		\
    libgstreamer-0.10  	\
    libgstvideo-0.10  	\
    libglib-2.0     \
    libgthread-2.0  \
    libgmodule-2.0  \
    libgobject-2.0  \
    libcutils \
    libutils

LOCAL_MODULE:= libgstmetadataretriever

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/..   \
    $(LOCAL_PATH)/../player   \
    $(LOCAL_PATH)/../log   \
    $(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
    $(call include-path-for, graphics corecg) \
    vendor/marvell/generic/gst-plugins-marvell/src/include


LOCAL_CFLAGS :=  \
        -DENABLE_GST_PLAYER_LOG \
		-DBUILD_WITH_GST

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

BUILD_PIPELINE_TEST:=0
ifeq ($(BUILD_PIPELINE_TEST),1)
# build pipeline test application
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    pipeline_test.cpp
	
LOCAL_SHARED_LIBRARIES := \
    libgstmetadataretriever \
    libgstapp-0.10		\
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0			\
    libutils \
    libsgl 

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/..   \
    $(LOCAL_PATH)/../log   \
    $(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
    $(call include-path-for, graphics corecg) \
    external/skia/include/core \
    external/skia/include/images \
    external/skia/include/utils

LOCAL_CFLAGS :=  \
        -DENABLE_GST_PLAYER_LOG \
		-DBUILD_WITH_GST

LOCAL_MODULE:= metadatatest

include $(BUILD_EXECUTABLE)
endif
