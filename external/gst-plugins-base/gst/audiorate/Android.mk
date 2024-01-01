LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstaudiorate.c

LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10	   \
	libgstbase-0.10		 \
	libglib-2.0			 \
	libgthread-2.0		  \
	libgmodule-2.0		  \
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_MODULE:= libgstaudiorate

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
