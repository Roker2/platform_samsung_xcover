LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstrtspbase64.c 	\
	gstrtspconnection.c 	\
        gstrtspdefs.c       	\
        gstrtspextension.c  	\
	gstrtspmessage.c    	\
	gstrtsprange.c      	\
	gstrtsptransport.c  	\
	gstrtspurl.c        	\

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE:= libgstrtsp-$(GST_MAJORMINOR)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/rtsp/gstrtsp-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstrtsp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --header --prefix=__gst_rtsp_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gstrtsp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstrtsp-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gstrtsp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --body --prefix=__gst_rtsp_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gstrtsp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstrtsp-marshal.c

rtsp_enum_headers :=              \
        $(LOCAL_PATH)/gstrtspdefs.h          \

rtsp_enum_headers1 :=              \
        gstrtspdefs.h          \

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/rtsp/gstrtsp-enumtypes.h
$(GEN): PRIVATE_INPUT_FILE := $(rtsp_enum_headers)
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#ifndef __GST_RTSP_ENUM_TYPES_H__\n#define __GST_RTSP_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
        --fprod "\n/* enumerations from \"@filename@\" */\n" \
        --vhead "GType @enum_name@_get_type();\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
        --ftail "G_END_DECLS\n\n#endif /* __GST_RTSP_ENUM_TYPES_H__ */" $(PRIVATE_INPUT_FILE) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gstrtsp-enumtypes.cc
$(GEN): PRIVATE_INPUT_FILE := $(rtsp_enum_headers)
$(GEN): MY_ENUM_HEADERS := $(foreach h,$(rtsp_enum_headers1),\n#include \"$(h)\")
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#include \"gstrtsp-enumtypes.h\"\n$(MY_ENUM_HEADERS)" \
        --fprod "\n/* enumerations from \"@filename@\" */" \
        --vhead "GType\n@enum_name@_get_type (void)\n{\n  static volatile gsize g_define_type_id__volatile = 0;\n  if (g_once_init_enter (&g_define_type_id__volatile)) {\n    static const G@Type@Value values[] = {"     \
        --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
        --vtail "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n  }\n  return g_define_type_id__volatile;\n}\n" \
        $(PRIVATE_INPUT_FILE) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gstrtsp-enumtypes.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(TARGET_OUT_HEADERS)/gstreamer/gst/rtsp \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
