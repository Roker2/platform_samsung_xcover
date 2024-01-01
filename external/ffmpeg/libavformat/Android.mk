LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libavformat
#
SOURCE_APE_DEMUXER := ape.c

LOCAL_SRC_FILES := \
	allformats.c cutils.c metadata.c metadata_compat.c options.c os_support.c sdp.c utils.c	\
	avio.c aviobuf.c

LOCAL_SRC_FILES += $(SOURCE_APE_DEMUXER)

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/ \
	$(LOCAL_PATH)/.. \
	external/zlib

LOCAL_CFLAGS += -O4 -mno-thumb-interwork -mno-thumb -marm -DHAVE_AV_CONFIG_H

LOCAL_MODULE := libavformat

include $(BUILD_STATIC_LIBRARY)
