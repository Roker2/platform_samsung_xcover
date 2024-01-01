LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	\
	quicktime.c gstrtpxqtdepay.c qtdemux.c qtdemux_types.c qtdemux_dump.c qtdemux_lang.c

LOCAL_SHARED_LIBRARIES :=	\
	libz			\
	libgsttag-0.10		\
        libgstpbutils-0.10      \
        libgstriff-0.10         \
	libgstrtp-0.10		\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstqtdemux

LOCAL_C_INCLUDES := 				\
	$(LOCAL_PATH)				\
	$(GST_PLUGINS_GOOD_TOP)			\
	$(GST_PLUGINS_GOOD_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
