LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# libpcre
#
LOCAL_SRC_FILES:= \
        pcre_compile.c \
        pcre_chartables.c \
        pcre_config.c \
        pcre_dfa_exec.c \
        pcre_exec.c \
        pcre_fullinfo.c \
        pcre_get.c \
        pcre_globals.c \
        pcre_info.c \
        pcre_maketables.c \
        pcre_newline.c \
        pcre_ord2utf8.c \
        pcre_refcount.c \
        pcre_study.c \
        pcre_tables.c \
        pcre_try_flipped.c \
        pcre_ucp_searchfuncs.c \
        pcre_valid_utf8.c \
        pcre_version.c \
        pcre_xclass.c \

LOCAL_C_INCLUDES += \
	$(GLIB_TOP_PATH) \
        $(GLIB_TOP_PATH)/glib \
        $(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH)/ \
        $(LOCAL_PATH)/../ \
        $(LOCAL_PATH)/../android \
        $(LOCAL_PATH)/../libcharset \
	$(TARGET_OUT_HEADERS)/glib/glib \

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GLib-GRegex\" \
        -DSUPPORT_UCP \
        -DSUPPORT_UTF8 \
        -DNEWLINE=-1 \
        -DMATCH_LIMIT=10000000 \
        -DMATCH_LIMIT_RECURSION=8192 \
        -DMAX_NAME_SIZE=32 \
        -DMAX_NAME_COUNT=10000 \
        -DMAX_DUPLENGTH=30000 \
        -DLINK_SIZE=2 \
        -DPOSIX_MALLOC_THRESHOLD=10 \
        -DPCRE_STATIC \
        -DG_DISABLE_DEPRECATED \
        -DGLIB_COMPILATION \

LOCAL_MODULE := libpcre

include $(BUILD_STATIC_LIBRARY)
