LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstapp.c

LOCAL_SHARED_LIBRARIES :=	\
	libgstreamer-0.10	\
	libgstapp-0.10	\
	libgstbase-0.10		\
	libgstcontroller-0.10	\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0		\
	libgstinterfaces-0.10

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstapp

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs	\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)