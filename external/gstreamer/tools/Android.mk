LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
         gst-inspect.c

LOCAL_SHARED_LIBRARIES := \
        libgstreamer-0.10       \
        libglib-2.0             \
        libgstcontroller-0.10   \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE:= gst-inspect-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES :=                     \
    $(LOCAL_PATH)                       \
    $(GSTREAMER_TOP)                    \
    $(GSTREAMER_TOP)/gst-libs           \
    $(GSTREAMER_TOP)/libs           \
    $(GSTREAMER_TOP)/android           \
    $(GSTREAMER_TOP)/android/gst        \
    $(TARGET_OUT_HEADERS)/gstreamer        \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
         gst-launch.c

LOCAL_SHARED_LIBRARIES := \
        libgstreamer-0.10       \
        libglib-2.0             \
        libgstcontroller-0.10   \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE:= gst-launch-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES :=                     \
    $(LOCAL_PATH)                       \
    $(GSTREAMER_TOP)                    \
    $(GSTREAMER_TOP)/gst-libs           \
    external/gstreamer/android          \
    external/gstreamer/gst/android      \
    external/gstreamer/gst              \
    external/gstreamer/libs             \
    $(TARGET_OUT_HEADERS)/gstreamer        \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
         gst-run.c

LOCAL_SHARED_LIBRARIES := \
        libgstreamer-0.10       \
        libglib-2.0             \
        libgstcontroller-0.10   \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE:= gst-run-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES :=                     \
    $(LOCAL_PATH)                       \
    $(GSTREAMER_TOP)                    \
    $(GSTREAMER_TOP)/gst-libs           \
    external/gstreamer/android          \
    external/gstreamer/gst/android      \
    external/gstreamer/gst              \
    external/gstreamer/libs             \
    $(TARGET_OUT_HEADERS)/gstreamer        \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
         gst-xmlinspect.c

LOCAL_SHARED_LIBRARIES := \
        libgstreamer-0.10       \
        libglib-2.0             \
        libgstcontroller-0.10   \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE:= gst-xmlinspect-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES :=                     \
    $(LOCAL_PATH)                       \
    $(GSTREAMER_TOP)                    \
    $(GSTREAMER_TOP)/gst-libs           \
    external/gstreamer/android          \
    external/gstreamer/gst/android      \
    external/gstreamer/gst              \
    external/gstreamer/libs             \
    $(TARGET_OUT_HEADERS)/gstreamer        \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
         gst-typefind.c

LOCAL_SHARED_LIBRARIES := \
        libgstreamer-0.10       \
        libglib-2.0             \
        libgstcontroller-0.10   \
        libgthread-2.0          \
        libgmodule-2.0          \
        libgobject-2.0

LOCAL_MODULE:= gst-typefind-$(GST_MAJORMINOR)

LOCAL_C_INCLUDES :=                     \
    $(LOCAL_PATH)                       \
    $(GSTREAMER_TOP)                    \
    $(GSTREAMER_TOP)/gst-libs           \
    external/gstreamer/android          \
    external/gstreamer/gst/android      \
    external/gstreamer/gst              \
    external/gstreamer/libs             \
    $(TARGET_OUT_HEADERS)/gstreamer        \
    $(GSTREAMER_DEP_INCLUDES)

LOCAL_CFLAGS := \
        -DHAVE_CONFIG_H

include $(BUILD_EXECUTABLE)

