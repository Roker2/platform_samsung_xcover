LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstplayback.c 		\
	gstplaybin.c 		\
	gstplaybin2.c 		\
	gstplaysink.c 		\
	gstplaybasebin.c 	\
	gstplay-enum.c 		\
	gststreaminfo.c 	\
	gststreamselector.c	\
        gstsubtitleoverlay.c    \
        gststreamsynchronizer.c \

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
        libgstvideo-0.10        \
        libgstinterfaces-0.10   \
	libgstpbutils-0.10	\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstplaybin

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --header --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --body --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstplay-marshal.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)

###### DECODEBIN

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=               \
        gstdecodebin.c         \
	gstplay-enum.c		\

LOCAL_SHARED_LIBRARIES :=       \
        libgstreamer-0.10       \
        libgstbase-0.10         \
        libgstpbutils-0.10      \
        libglib-2.0             \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstdecodebin

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --header --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --body --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstplay-marshal.c

LOCAL_C_INCLUDES :=                     \
        $(LOCAL_PATH)                   \
        $(LOCAL_PATH)/android           \
        $(GST_PLUGINS_BASE_TOP)         \
        $(GST_PLUGINS_BASE_TOP)/android \
        $(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES) \

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_SHARED_LIBRARY)

###### DECODEBIN2

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstdecodebin2.c		\
	gsturidecodebin.c	\
	gstplay-enum.c		\

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libgstpbutils-0.10	\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstdecodebin2

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --header --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstplay-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstplay-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --body --prefix=gst_play_marshal $(PRIVATE_INPUT_FILE) > $@
$(GEN): $(LOCAL_PATH)/gstplay-marshal.list
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstplay-marshal.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES) \

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)

