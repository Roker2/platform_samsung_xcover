LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstaudioconvert.c 	\
	audioconvert.c 		\
	gstchannelmix.c 	\
	gstaudioquantize.c 	\
	plugin.c                \
        gstaudioconvertorc.c   \


LOCAL_SHARED_LIBRARIES := \
	libgstaudio-0.10   \
	libgstreamer-0.10	   \
	libgstbase-0.10		 \
	libglib-2.0			 \
	libgthread-2.0		  \
	libgmodule-2.0		  \
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_MODULE:= libgstaudioconvert

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H  \
        -DDISABLE_ORC    \

	

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
