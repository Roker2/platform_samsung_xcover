LOCAL_PATH := $(call my-dir)

FLAC_TOP := $(LOCAL_PATH)

include $(CLEAR_VARS)

include $(FLAC_TOP)/src/libFLAC/Android.mk
