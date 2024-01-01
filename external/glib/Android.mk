LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

GLIB_TOP_PATH := $(LOCAL_PATH)
include $(GLIB_TOP_PATH)/glib/Android.mk
include $(GLIB_TOP_PATH)/gobject/Android.mk
include $(GLIB_TOP_PATH)/gmodule/Android.mk
include $(GLIB_TOP_PATH)/gio/Android.mk
include $(GLIB_TOP_PATH)/gthread/Android.mk
include $(GLIB_TOP_PATH)/gio/xdgmime/Android.mk
include $(GLIB_TOP_PATH)/gio/inotify/Android.mk
include $(GLIB_TOP_PATH)/glib/pcre/Android.mk
include $(GLIB_TOP_PATH)/glib/libcharset/Android.mk

