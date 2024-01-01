LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libgmodule
#
LOCAL_SRC_FILES:= \
	gmodule.c \

LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/ \
	$(GLIB_TOP_PATH)/android \
	$(GLIB_TOP_PATH)/glib \
        $(LOCAL_PATH) \
        $(LOCAL_PATH)/android \

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GModule\"      \
        -DG_DISABLE_DEPRECATED

LOCAL_SHARED_LIBRARIES += libglib-2.0 libdl

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
#LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libgmodule-2.0

include $(BUILD_SHARED_LIBRARY)
