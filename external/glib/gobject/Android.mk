LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libgobject
#
LOCAL_SRC_FILES:= \
        gatomicarray.c          \
        gboxed.c                \
        gclosure.c              \
        genums.c                \
        gobject.c               \
        gparam.c                \
        gparamspecs.c           \
        gsignal.c               \
        gsourceclosure.c        \
        gtype.c                 \
        gtypemodule.c           \
        gtypeplugin.c           \
        gvalue.c                \
        gvaluearray.c           \
        gvaluetransform.c       \
        gvaluetypes.c		\


LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/ \
        $(GLIB_TOP_PATH)/glib \
	$(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH) \
	$(TARGET_OUT_HEADERS)/glib/ \
	$(TARGET_OUT_HEADERS)/glib/gobject \

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GLib-GObject\"         \
        -DG_DISABLE_DEPRECATED                  \
        -DGOBJECT_COMPILATION                   \
        -DG_DISABLE_CONST_RETURNS

LOCAL_SHARED_LIBRARIES += libglib-2.0
LOCAL_SHARED_LIBRARIES += libgthread-2.0
#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
#LOCAL_PRELINK_MODULE := false
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libgobject-2.0

#intermediates := $(TARGET_OUT_HEADERS)
#GEN := $(intermediates)/glib/gobject/gobjectalias.h
#$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gobject.symbols
#$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegobjectalias.pl < $(PRIVATE_INPUT_FILE)
#$(GEN): $(LOCAL_PATH)/gobject.symbols
#	mkdir -p $(dir $@)
#	$(PRIVATE_CUSTOM_TOOL) > $@ 
#all_copied_headers:$(GEN)

intermediates := $(TARGET_OUT_HEADERS)
GEN := $(intermediates)/glib/gobject/gmarshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gmarshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --nostdinc --prefix=g_cclosure_marshal $(PRIVATE_INPUT_FILE) --header
$(GEN): $(LOCAL_PATH)/gmarshal.list
	mkdir -p $(dir $@)
	echo "#ifndef __G_MARSHAL_H__" > $@ \
        && echo "#define __G_MARSHAL_H__" >> $@ \
        && $(PRIVATE_CUSTOM_TOOL) >> $@ \
        && echo "#endif /* __G_MARSHAL_H__ */" >> $@
all_copied_headers:$(GEN)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

#intermediates := $(local-intermediates-dir)
#GEN := $(intermediates)/gobjectaliasdef.c
#$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gobject.symbols
#$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegobjectalias.pl -def < $(PRIVATE_INPUT_FILE)
#$(GEN): $(LOCAL_PATH)/gobject.symbols
#	mkdir -p $(dir $@)
#	$(PRIVATE_CUSTOM_TOOL) > $@
#all_copied_header:$(GEN)

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gmarshal.c
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gmarshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --nostdinc --prefix=g_cclosure_marshal $(PRIVATE_INPUT_FILE) --body
$(GEN): $(LOCAL_PATH)/gmarshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

include $(BUILD_SHARED_LIBRARY)
