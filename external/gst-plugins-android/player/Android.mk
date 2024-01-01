LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_TOP_PATH := $(LOCAL_PATH)/../..

LOCAL_SRC_FILES:= \
    GstPlayer.cpp \
    GstPlayerPipeline.cpp 
 
LOCAL_SHARED_LIBRARIES := \
    libmedia \
    libgstapp-0.10		\
    libgstreamer-0.10  	\
    libglib-2.0     \
    libgthread-2.0  \
    libgmodule-2.0  \
    libgobject-2.0  \
    libutils	\
    libcutils

LOCAL_MODULE:= libgst_player

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/..   \
    $(LOCAL_PATH)/../log   \
    $(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
    $(call include-path-for, graphics corecg)

LOCAL_CFLAGS :=  \
        -DENABLE_GST_PLAYER_LOG \
		-DBUILD_WITH_GST

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

include $(BUILD_SHARED_LIBRARY)

BUILD_PLAYER_PIPELINE_TEST:=0
ifeq ($(BUILD_PLAYER_PIPELINE_TEST),1)
# build pipeline test application
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    GstPlayer.cpp \
    GstPlayerPipeline.cpp \
    pipeline_test.cpp
	
LOCAL_SHARED_LIBRARIES := \
    libmedia \
    libgstapp-0.10		\
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0			\
    libutils 

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/..   \
    $(LOCAL_PATH)/../log   \
    $(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
    $(call include-path-for, graphics corecg)

LOCAL_CFLAGS :=  \
        -DENABLE_GST_PLAYER_LOG \
		-DBUILD_WITH_GST

LOCAL_MODULE:= pipelinetest

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

include $(BUILD_EXECUTABLE)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := gst.conf
LOCAL_MODULE_TAGS := user
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/etc
LOCAL_SRC_FILES := gst.conf
include $(BUILD_PREBUILT)

