LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    lib.c \
    gstcontroller.c \
    gstinterpolation.c \
    gsthelper.c \
    gstcontrolsource.c \
    gstinterpolationcontrolsource.c \
    gstlfocontrolsource.c

LOCAL_SHARED_LIBRARIES := \
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0

LOCAL_MODULE:= libgstcontroller-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES := 			\
	$(GSTREAMER_TOP)		\
	external/gstreamer 		\
	external/gstreamer/android 	\
	external/gstreamer/libs		\
	external/gstreamer/gst		\
	external/gstreamer/gst/android	\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
