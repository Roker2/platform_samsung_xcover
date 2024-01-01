LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
			  gstffmpeg.c	\
			  gstffmpegprotocol.c	\
			  gstffmpegcodecmap.c	\
			  gstffmpegenc.c	\
			  gstffmpegdec.c	\
			  gstffmpegcfg.c	\
			  gstffmpegdemux.c	\
			  gstffmpegmux.c	\
			  gstffmpegdeinterlace.c	\
			  gstffmpegaudioresample.c

#
#pay attention here, the sequence for thest three static libraries must not be adjusted
#otherwise Android will complain unresolved symbol
#
LOCAL_STATIC_LIBRARIES :=	\
	libavformat		\
	libavcodec		\
	libavutil

LOCAL_SHARED_LIBRARIES := \
	libgstbase-0.10	   \
	libgstreamer-0.10	 \
	libgstaudio-0.10	\
	libgstvideo-0.10	\
	libglib-2.0		   \
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstffmpeg

LOCAL_C_INCLUDES := 			\
	$(GST_FFMPEG_TOP)/android	\
	$(GST_FFMPEG_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
