LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libcharset
#
LOCAL_SRC_FILES:= \
	localcharset.c

LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/../ \
        $(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH)/ \

LOCAL_CFLAGS += \
	-DLIBDIR=\"/system/lib\"

LOCAL_MODULE := libcharset

include $(BUILD_STATIC_LIBRARY)
