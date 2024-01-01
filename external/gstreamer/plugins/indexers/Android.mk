LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    gstindexers.c   \
    gstfileindex.c   \
    gstmemindex.c

LOCAL_SHARED_LIBRARIES := \
    libxml2		\
    libgstbase-0.10       \
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_MODULE:= libgstcoreindexers

LOCAL_C_INCLUDES := 			\
	external/gstreamer       	\
	external/gstreamer/android      \
	external/gstreamer/libs 	\
	external/gstreamer/gst		\
	external/gstreamer/gst/android	\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
