LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	\
				gstrtpmanager.c \
                              gstrtpbin.c \
                              gstrtpjitterbuffer.c \
                              gstrtpptdemux.c \
                              gstrtpssrcdemux.c \
                              rtpjitterbuffer.c      \
                              rtpsession.c      \
                              rtpsource.c      \
                              rtpstats.c      \
                              gstrtpsession.c

LOCAL_SHARED_LIBRARIES :=	\
	libgstrtp-0.10	\
	libgstnetbuffer-0.10	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstrtpmanager

LOCAL_C_INCLUDES := 				\
	$(LOCAL_PATH)				\
	$(GST_PLUGINS_GOOD_TOP)			\
	$(GST_PLUGINS_GOOD_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstrtpbin-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstrtpbin-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --header --prefix=gst_rtp_bin_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstrtpbin-marshal.list
	mkdir -p $(dir $@)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstrtpbin-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstrtpbin-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --body --prefix=gst_rtp_bin_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstrtpbin-marshal.list
	mkdir -p $(dir $@)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstrtpbin-marshal.c

include $(BUILD_SHARED_LIBRARY)
