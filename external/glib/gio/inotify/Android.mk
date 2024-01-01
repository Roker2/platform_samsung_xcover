LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libinotify
#
LOCAL_SRC_FILES :=                 \
        inotify-kernel.c                \
        inotify-sub.c                   \
        inotify-path.c                  \
        inotify-missing.c               \
        inotify-helper.c                \
        inotify-diag.c                  \
        ginotifydirectorymonitor.c      \
        ginotifyfilemonitor.c

LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/ \
        $(GLIB_TOP_PATH)/android \
        $(GLIB_TOP_PATH)/glib \
        $(GLIB_TOP_PATH)/gmodule \
        $(GLIB_TOP_PATH)/gobject/android \
        $(LOCAL_PATH)/ \
        $(LOCAL_PATH)/../ \
	$(TARGET_OUT_HEADERS)/glib/gio	\
	$(TARGET_OUT_HEADERS)/glib/	\

LOCAL_CFLAGS = \
        -DG_LOG_DOMAIN=\"GLib-GIO\"     \
        -DGIO_MODULE_DIR=\"/system/lib\"  \
        -DGIO_COMPILATION               \
        -DG_DISABLE_DEPRECATED

LOCAL_MODULE := libinotify

include $(BUILD_STATIC_LIBRARY)

