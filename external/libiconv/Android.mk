LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(TARGET_SIMULATOR),true)

#
# libiconv
#
LOCAL_SRC_FILES:= \
        lib/iconv.c libcharset/lib/localcharset.c lib/relocatable.c

LOCAL_CFLAGS += \
#        -DLIBDIR="//system//lib" \
        -DBUILDING_LIBICONV -DBUILDING_DLL \
#        -DENABLE_RELOCATABLE=1 \
        -DIN_LIBRARY \
#        -DINSTALLDIR="$(SYSTEM_LIB)" \
        -DNO_XMALLOC \
        -Dset_relocation_prefix=libiconv_set_relocation_prefix \
#        -Drelocate=libiconv_relocate \
        -DHAVE_CONFIG_H
        	
LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/android \
        $(LOCAL_PATH)/lib \
        $(LOCAL_PATH)/libcharset/include 

LOCAL_CFLAGS += -mabi=aapcs-linux

LOCAL_SHARED_LIBRARIES += libutils libcutils

LOCAL_LDLIBS := -lpthread

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl
endif
ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

#
# define LOCAL_PRELINK_MODULE to false to not use pre-link map
#
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libiconv

include $(BUILD_SHARED_LIBRARY)

endif
