LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstvideotestsrc.c   	\
	videotestsrc.c          \
        gstvideotestsrcorc.c   \

LOCAL_SHARED_LIBRARIES :=	\
	liboil			\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstvideotestsrc

LOCAL_C_INCLUDES := 					\
	$(LOCAL_PATH)					\
	$(GST_PLUGINS_BASE_TOP)				\
	$(GST_PLUGINS_BASE_TOP)/android			\
	$(GST_PLUGINS_BASE_TOP)/gst-libs/gst/video/android \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H \
        -DDISABLE_ORC \

include $(BUILD_SHARED_LIBRARY)
