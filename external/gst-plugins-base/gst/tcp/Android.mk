LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 			\
	gsttcpplugin.c 			\
	gsttcp.c 			\
	gstmultifdsink.c  		\
	gsttcpclientsrc.c 		\
	gsttcpclientsink.c 		\
	gsttcpserversrc.c 		\
	gsttcpserversink.c 		\

LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0		\
	libgstdataprotocol-0.10

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgsttcp

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gsttcp-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gsttcp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --header --prefix=gst_tcp_marshal $(PRIVATE_INPUT_FILE) 
$(GEN): $(LOCAL_PATH)/gsttcp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gsttcp-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gsttcp-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --body --prefix=gst_tcp_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gsttcp-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gsttcp-marshal.c

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gsttcp-enumtypes.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gsttcp.h
$(GEN): $(LOCAL_PATH)/gsttcp.h
	mkdir -p $(dir $@)
	glib-mkenums \
	--fhead "#ifndef __GST_TCP_ENUM_TYPES_H__\n#define __GST_TCP_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "\n/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type();\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
        --ftail "G_END_DECLS\n\n#endif /* __GST_TCP_ENUM_TYPES_H__ */" $(PRIVATE_INPUT_FILE) > $@
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gsttcp-enumtypes.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gsttcp.h
$(GEN): $(LOCAL_PATH)/gsttcp.h
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#include \"gsttcp-enumtypes.h\"\n#include \"gsttcp.h\"\n" \
        --fprod "\n/* enumerations from \"@filename@\" */" \
        --vhead "GType\n@enum_name@_get_type (void)\n{\n  static volatile gsize g_define_type_id__volatile = 0;\n  if (g_once_init_enter (&g_define_type_id__volatile)) {\n    static const G@Type@Value values[] = {"     \
        --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
        --vtail "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n  }\n  return g_define_type_id__volatile;\n}\n" \
	$(PRIVATE_INPUT_FILE) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gsttcp-enumtypes.c

include $(BUILD_SHARED_LIBRARY)
