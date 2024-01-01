LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

SAMIPARSE_SOURCES = samiparse.c

LOCAL_SRC_FILES:= 		\
        gstssaparse.c \
        gstsubparse.c \
        $(SAMIPARSE_SOURCES) \
        tmplayerparse.c \
        mpl2parse.c \
        qttextparse.c \

#LOCAL_STATIC_LIBRARIES := libxml2

LOCAL_SHARED_LIBRARIES := \
	libxml2	\
	libgstreamer-0.10	   \
	libgstbase-0.10		 \
	libglib-2.0			 \
	libgthread-2.0		  \
	libgmodule-2.0		  \
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_MODULE:= libgstsubparse

LOCAL_C_INCLUDES := 			\
	$(LOCAL_PATH)			\
	$(GST_PLUGINS_BASE_TOP)		\
	$(GST_PLUGINS_BASE_TOP)/gst-libs\
	$(GST_PLUGINS_BASE_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H	

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
