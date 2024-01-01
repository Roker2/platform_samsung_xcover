LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstpluginsbaseversion.c \
        pbutils.c         	\
        codec-utils.c     \
	descriptions.c    	\
        encoding-profile.c      \
        encoding-target.c       \
	install-plugins.c 	\
	missing-plugins.c	\
        gstdiscoverer.c   \
        gstdiscoverer-types.c  \


LOCAL_SHARED_LIBRARIES := 	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
        libgstvideo-0.10        \
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

headers_pbutils := \
        $(LOCAL_PATH)/pbutils.h         \
        $(LOCAL_PATH)/descriptions.h    \
        $(LOCAL_PATH)/install-plugins.h \
        $(LOCAL_PATH)/missing-plugins.h

headers_pbutils1 := \
        pbutils.h         \
        descriptions.h    \
        install-plugins.h \
        missing-plugins.h

LOCAL_MODULE:= libgstpbutils-$(GST_MAJORMINOR)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

GEN := $(TARGET_OUT_HEADERS)/gstreamer/gst/pbutils/pbutils-enumtypes.h
$(GEN): PRIVATE_INPUT_FILE := $(headers_pbutils)
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#ifndef __GST_PBUTILS_ENUM_TYPES_H__\n#define __GST_PBUTILS_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
        --fprod "\n/* enumerations from \"@filename@\" */\n" \
        --vhead "GType @enum_name@_get_type();\n#define GST_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
        --ftail "G_END_DECLS\n\n#endif /* __GST_PBUTILS_ENUM_TYPES_H__ */" $(PRIVATE_INPUT_FILE) > $@
all_copied_headers:$(GEN)


intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/pbutils-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/pbutils-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --header --prefix=pbutils_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/pbutils-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@	
all_copied_headers:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/pbutils-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/pbutils-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL = glib-genmarshal --body --prefix=pbutils_marshal $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/pbutils-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/pbutils-marshal.c

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/pbutils-enumtypes.cc
$(GEN): PRIVATE_INPUT_FILE := $(headers_pbutils)
$(GEN): MY_ENUM_HEADERS := $(foreach h,$(headers_pbutils1),\n#include \"$(h)\")
$(GEN): 
	mkdir -p $(dir $@)
	glib-mkenums \
        --fhead "#include \"pbutils-enumtypes.h\"\n$(MY_ENUM_HEADERS)\n" \
        --fprod "\n/* enumerations from \"@filename@\" */" \
        --vhead "GType\n@enum_name@_get_type (void)\n{\n  static volatile gsize g_define_type_id__volatile = 0;\n  if (g_once_init_enter (&g_define_type_id__volatile)) {\n    static const G@Type@Value values[] = {"     \
        --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
        --vtail "      { 0, NULL, NULL }\n    };\n    GType g_define_type_id = g_@type@_register_static (\"@EnumName@\", values);\n    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);\n  }\n  return g_define_type_id__volatile;\n}\n" \
        $(PRIVATE_INPUT_FILE) > $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/pbutils-enumtypes.c

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(LOCAL_PATH)/android		\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/android	\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(TARGET_OUT_HEADERS)/gstreamer/gst/pbutils \
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

include $(BUILD_SHARED_LIBRARY)
