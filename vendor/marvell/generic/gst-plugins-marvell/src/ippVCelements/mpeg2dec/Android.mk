LOCAL_PATH:= $(call my-dir)

$(call add-prebuilt-file, deinterlace2.a, STATIC_LIBRARIES)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	\
        gstmpeg2dec.c \

LOCAL_SHARED_LIBRARIES :=	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_SHARED_LIBRARIES += 	\
	libcodecmpeg2dec \
	libmiscgen

LOCAL_STATIC_LIBRARIES += \
	deinterlace2

LOCAL_SHARED_LIBRARIES += \
	libutils

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstippmpeg2dec

LOCAL_C_INCLUDES := 				\
	$(LOCAL_PATH)				\
        $(LOCAL_PATH)/../../include     \
	$(GST_PLUGINS_MARVELL_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
