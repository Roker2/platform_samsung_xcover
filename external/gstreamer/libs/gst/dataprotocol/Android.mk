LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    dataprotocol.c

LOCAL_SHARED_LIBRARIES := \
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0

LOCAL_MODULE:= libgstdataprotocol-$(GST_MAJORMINOR)

#LOCAL_TOP_PATH := $(LOCAL_PATH)/../../../..

LOCAL_C_INCLUDES := 			\
	$(GSTREAMER_TOP) 		\
	$(GSTREAMER_TOP)/libs		\
	external/gstreamer	   	\
	external/gstreamer/android	\
	external/gstreamer/gst		\
	external/gstreamer/gst/android	\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
