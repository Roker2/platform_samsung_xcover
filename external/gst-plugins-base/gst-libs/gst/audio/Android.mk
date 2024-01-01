LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 			\
	audio.c 			\
	gstaudioclock.c 		\
	mixerutils.c			\
	multichannel.c 			\
	gstaudiofilter.c 		\
	gstaudiosink.c 			\
	gstaudiosrc.c 			\
	gstbaseaudiosink.c 		\
	gstbaseaudiosrc.c 		\
	gstringbuffer.c   		\

LOCAL_SHARED_LIBRARIES :=	\
	libgstinterfaces-0.10	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE:= libgstaudio-$(GST_MAJORMINOR)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

audio_enum_headers :=              \
        $(LOCAL_PATH)/multichannel.h          \
        $(LOCAL_PATH)/gstringbuffer.h

audio_enum_headers1 :=              \
        multichannel.h          \
        gstringbuffer.h

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/audio/audio-enumtypes.h
$(GEN): PRIVATE_INPUT_FILE := $(audio_enum_headers)
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#ifndef __GST_AUDIO_ENUM_TYPES_H__\n#define __GST_AUDIO_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
        --fprod "\n/* enumerations from \"@filename@\" */\n" \
        --vhead "GType @enum_name@_get_type();\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
        --ftail "G_END_DECLS\n\n#endif /* __GST_AUDIO_ENUM_TYPES_H__ */" $(PRIVATE_INPUT_FILE) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/audio-enumtypes.cc
$(GEN): PRIVATE_INPUT_FILE := $(audio_enum_headers)
$(GEN): MY_ENUM_HEADERS := $(foreach h,$(audio_enum_headers1),\n#include \"$(h)\")
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#include \"audio-enumtypes.h\"\n$(MY_ENUM_HEADERS)" \
        --fprod "\n/* enumerations from \"@filename@\" */" \
        --vhead "GType\n@enum_name@_get_type (void)\n{\n  static volatile gsize g_define_type_id__volatile = 0;\n  if (g_once_init_enter (&g_define_type_id__volatile)) {\n    static const G@Type@Value values[] = {"     \
        --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
        --vtail "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n  }\n  return g_define_type_id__volatile;\n}\n" \
        $(PRIVATE_INPUT_FILE) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/audio-enumtypes.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(TARGET_OUT_HEADERS)/gstreamer/gst/audio \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
