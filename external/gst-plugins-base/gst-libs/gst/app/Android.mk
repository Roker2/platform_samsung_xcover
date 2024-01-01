LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstappbuffer.c		\
	gstappsink.c		\
	gstappsrc.c			\

LOCAL_SHARED_LIBRARIES := \
	libgstreamer-0.10	   \
	libgstbase-0.10		 \
	libglib-2.0			 \
	libgthread-2.0		  \
	libgmodule-2.0		  \
	libgobject-2.0

LOCAL_MODULE:= libgstapp-$(GST_MAJORMINOR)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstapp-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstapp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --header --prefix=__gst_app_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gstapp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstapp-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstapp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --body --prefix=__gst_app_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gstapp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstapp-marshal.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)				\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
