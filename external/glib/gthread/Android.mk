LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libgthread
#
LOCAL_SRC_FILES:= \
	gthread-impl.c

LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/ \
        $(GLIB_TOP_PATH)/glib \
	$(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH) \

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GThread\"      \
        -DG_DISABLE_DEPRECATED

LOCAL_SHARED_LIBRARIES += libglib-2.0

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
#LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libgthread-2.0

include $(BUILD_SHARED_LIBRARY)
