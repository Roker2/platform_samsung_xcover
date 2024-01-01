LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
#
# libxdgmime
#
LOCAL_SRC_FILES := \
        xdgmime.c       \
        xdgmimealias.c  \
        xdgmimecache.c  \
        xdgmimeglob.c   \
        xdgmimeicon.c   \
        xdgmimeint.c    \
        xdgmimemagic.c  \
        xdgmimeparent.c \

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/../ \
        $(LOCAL_PATH)/ \

LOCAL_CFLAGS += \
	-DXDG_PREFIX=_gio_xdg

LOCAL_MODULE := libxdgmime

include $(BUILD_STATIC_LIBRARY)
