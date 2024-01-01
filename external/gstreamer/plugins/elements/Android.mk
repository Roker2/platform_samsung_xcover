LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 		\
	gstcapsfilter.c	 	\
	gstelements.c		\
	gstfakesrc.c		\
	gstfakesink.c	   	\
	gstfdsrc.c		\
	gstfdsink.c		\
	gstfilesink.c	   	\
	gstfilesrc.c		\
	gstidentity.c	   	\
	gstinputselector.c	\
	gstoutputselector.c	\
	gstmultiqueue.c		\
	gstqueue.c		\
	gstqueue2.c		\
	gsttee.c		\
	gsttypefindelement.c	\
	gstvalve.c

LOCAL_SHARED_LIBRARIES := \
	libgstbase-0.10	   \
	libgstreamer-0.10	 \
	libglib-2.0		   \
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_MODULE:= libgstcoreelements

LOCAL_C_INCLUDES := 			\
	external/gstreamer	   	\
	external/gstreamer/android	\
	external/gstreamer/libs 	\
	external/gstreamer/gst		\
	external/gstreamer/gst/android	\
	$(TARGET_OUT_HEADERS)/gstreamer \
	$(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
