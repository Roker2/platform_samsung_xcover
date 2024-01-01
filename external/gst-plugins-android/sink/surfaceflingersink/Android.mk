LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	surfaceflinger_wrap.cpp \
	gstsurfaceflingersink.c  
	
LOCAL_SHARED_LIBRARIES := \
    libgstreamer-0.10       \
    libglib-2.0             \
    libgthread-2.0          \
    libgmodule-2.0          \
    libgobject-2.0          \
    libgstbase-0.10         \
	libgstvideo-0.10		\
    libpmemhelper               \
    libcutils               \
    libutils                \
	libui					\
	libsurfaceflinger

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstsurfaceflingersink

LOCAL_TOP_PATH := $(LOCAL_PATH)/../../../

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/../..   \
    $(LOCAL_PATH)/../../log   \
    $(GST_PLUGINS_ANDROID_DEP_INCLUDES) \
    frameworks/base/media/libmediaplayerservice	\
    frameworks/base/media/libmedia \
    vendor/marvell/generic/gst-plugins-marvell/src/include \
    vendor/marvell/generic/phycontmem-lib/pmemhelper \
    vendor/marvell/generic/overlay-hal

BOARD_HAVE_MARVELL_GCU := true

MARVELL_GCU_INCLUDE_PATH := vendor/marvell/generic/graphics/include

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			
ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

ifeq ($(BOARD_ENABLE_FAST_OVERLAY),true)
LOCAL_CFLAGS += -DENABLE_FAST_OVERLAY
LOCAL_C_INCLUDES += \
    vendor/marvell/generic/bmm-lib/lib
LOCAL_SHARED_LIBRARIES += libbmm
endif

ifeq ($(BOARD_HAVE_MARVELL_GCU), true)
LOCAL_CFLAGS += -DHAVE_MARVELL_GCU
LOCAL_C_INCLUDES += \
    $(MARVELL_GCU_INCLUDE_PATH)
LOCAL_SHARED_LIBRARIES += libgcu
endif

ifeq ($(BOARD_OVERLAY_FORMAT),422I)
LOCAL_CFLAGS += -DOVERLAY_FORMAT=\"422I\"
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifneq ($(PLATFORM_SDK_VERSION), 3)
ifneq ($(PLATFORM_SDK_VERSION), 4)
LOCAL_SHARED_LIBRARIES += libbinder
endif
endif

#PLATFORM_SDK_VERSION >= 8
ifeq ($(findstring $(PLATFORM_SDK_VERSION),1 2 3 4 5 6 7),)
LOCAL_SHARED_LIBRARIES += libsurfaceflinger_client
endif

include $(BUILD_SHARED_LIBRARY)

#Disable the surfaceflinger test application by default
ifeq ($(ENABLE_SURFACEFILINGER_TEST_APP), 1)

# build surfaceflinger test application
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	surfaceflinger_wrap.cpp \
	wrap_test.cpp  
	
LOCAL_SHARED_LIBRARIES := \
    libcutils               \
    libutils                \
	libui					\
	libsurfaceflinger

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)   \
    $(LOCAL_PATH)/../..   \
    $(LOCAL_PATH)/../../log   \
    frameworks/base/media/libmediaplayerservice	\
    frameworks/base/media/libmedia \
    vendor/marvell/generic/gst-plugins-marvell/src/include \
    vendor/marvell/generic/overlay-hal

LOCAL_MODULE:= surfacetest

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifneq ($(PLATFORM_SDK_VERSION), 3)
ifneq ($(PLATFORM_SDK_VERSION), 4)
LOCAL_SHARED_LIBRARIES += libbinder
endif
endif

#PLATFORM_SDK_VERSION >= 8
ifeq ($(findstring $(PLATFORM_SDK_VERSION),1 2 3 4 5 6 7),)
LOCAL_SHARED_LIBRARIES += libsurfaceflinger_client
endif

ifeq ($(BOARD_ENABLE_FAST_OVERLAY),true)
LOCAL_CFLAGS += -DENABLE_FAST_OVERLAY
LOCAL_C_INCLUDES += \
    vendor/marvell/generic/bmm-lib/lib
LOCAL_SHARED_LIBRARIES += libbmm
endif

ifeq ($(BOARD_HAVE_MARVELL_GCU), true)
LOCAL_CFLAGS += -DHAVE_MARVELL_GCU
LOCAL_C_INCLUDES += \
    $(MARVELL_GCU_INCLUDE_PATH)
LOCAL_SHARED_LIBRARIES += libgcu
endif
include $(BUILD_EXECUTABLE)
endif

