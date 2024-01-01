LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	colorbalance.c		\
	colorbalancechannel.c	\
	mixer.c			\
	mixeroptions.c		\
	mixertrack.c		\
	navigation.c		\
	propertyprobe.c		\
        streamvolume.c          \
	tuner.c			\
	tunernorm.c		\
	tunerchannel.c		\
	videoorientation.c	\
	xoverlay.c		\

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE:= libgstinterfaces-$(GST_MAJORMINOR)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/interfaces/interfaces-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/interfaces-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --header --prefix=gst_interfaces_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/interfaces-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/interfaces-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/interfaces-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --body --prefix=gst_interfaces_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/interfaces-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/interfaces-marshal.c

headers_interfaces :=            \
        $(LOCAL_PATH)/colorbalance.h          \
        $(LOCAL_PATH)/colorbalancechannel.h   \
        $(LOCAL_PATH)/mixer.h                 \
        $(LOCAL_PATH)/mixeroptions.h          \
        $(LOCAL_PATH)/mixertrack.h            \
        $(LOCAL_PATH)/navigation.h            \
        $(LOCAL_PATH)/propertyprobe.h         \
        $(LOCAL_PATH)/tuner.h                 \
        $(LOCAL_PATH)/tunernorm.h             \
        $(LOCAL_PATH)/tunerchannel.h          \
        $(LOCAL_PATH)/videoorientation.h      \
        $(LOCAL_PATH)/xoverlay.h

header_interfaces_1 := \
        colorbalance.h          \
        colorbalancechannel.h   \
        mixer.h                 \
        mixeroptions.h          \
        mixertrack.h            \
        navigation.h            \
        propertyprobe.h         \
        tuner.h                 \
        tunernorm.h             \
        tunerchannel.h          \
        videoorientation.h      \
        xoverlay.h

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/interfaces/interfaces-enumtypes.h
$(GEN): PRIVATE_INPUT_FILE := $(headers_interfaces)
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#ifndef __GST_INTERFACES_ENUM_TYPES_H__\n#define __GST_INTERFACES_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
        --fprod "\n/* enumerations from \"@filename@\" */\n" \
        --vhead "GType @enum_name@_get_type();\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
        --ftail "G_END_DECLS\n\n#endif /* __GST_INTERFACES_ENUM_TYPES_H__ */" $(PRIVATE_INPUT_FILE) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/interfaces-enumtypes.cc
$(GEN): PRIVATE_INPUT_FILE := $(headers_interfaces)
$(GEN): MY_ENUM_HEADERS := $(foreach h,$(header_interfaces_1),\n#include \"$(h)\")
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#include \"interfaces-enumtypes.h\"\n$(MY_ENUM_HEADERS)" \
        --fprod "\n/* enumerations from \"@filename@\" */" \
        --vhead "GType\n@enum_name@_get_type (void)\n{\n  static volatile gsize g_define_type_id__volatile = 0;\n  if (g_once_init_enter (&g_define_type_id__volatile)) {\n    static const G@Type@Value values[] = {"     \
        --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
        --vtail "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n  }\n  return g_define_type_id__volatile;\n}\n" \
        $(PRIVATE_INPUT_FILE) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/interfaces-enumtypes.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(TARGET_OUT_HEADERS)/gstreamer/gst/interfaces \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
