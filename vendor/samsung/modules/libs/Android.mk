LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

LOCAL_PREBUILT_LIBS := libpmemhelper.so libphycontmem.so libgcu.so libbmm.so

LOCAL_MODULE_TAGS := user

include $(BUILD_MULTI_PREBUILT)

