LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 			\
	bitmath.c    cpu.c    float.c   md5.c                 metadata_object.c     ogg_helper.c      stream_encoder.c \
	bitreader.c  crc.c    format.c  memory.c              ogg_decoder_aspect.c  ogg_mapping.c     stream_encoder_framing.c \
	bitwriter.c  fixed.c  lpc.c     metadata_iterators.c  ogg_encoder_aspect.c  stream_decoder.c  window.c

LOCAL_MODULE:= libflac

LOCAL_CFLAGS += -DHAVE_CONFIG_H 

LOCAL_C_INCLUDES := 	\
	$(LOCAL_PATH)	\
	$(LOCAL_PATH)/../../include \
	$(LOCAL_PATH)/../../android \
	$(LOCAL_PATH)/include \
	external/libogg/include 

LOCAL_STATIC_LIBRARIES += libogg

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
