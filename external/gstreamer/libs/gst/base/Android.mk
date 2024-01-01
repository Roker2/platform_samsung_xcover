LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    gstadapter.c		\
    gstbasesink.c		\
    gstbasesrc.c		\
    gstbasetransform.c	\
    gstbitreader.c	\
    gstbytereader.c	\
    gstbytewriter.c         \
    gstcollectpads.c	\
    gstpushsrc.c		\
    gsttypefindhelper.c	\
    gstdataqueue.c

LOCAL_SHARED_LIBRARIES := \
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0

LOCAL_MODULE:= libgstbase-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES := 			\
    $(LOCAL_PATH)   			\
    external/gstreamer       		\
    external/gstreamer/android   	\
    external/gstreamer/gst		\
    external/gstreamer/gst/android	\
    external/gstreamer/libs 		\
	$(TARGET_OUT_HEADERS)/gstreamer \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
    -DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
