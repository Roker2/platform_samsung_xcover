LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

gatomic_c:= gatomic.c
gregex_c:= gregex.c
gregex_h:= gregex.h

EXTRA_libglib_2_0_la_SOURCES:=\
        giounix.c       \
        gspawn.c        \

LOCAL_SRC_FILES:=  	\
	garray.c		\
	gasyncqueue.c		\
	$(gatomic_c)		\
	gbacktrace.c		\
	gbase64.c		\
	gbitlock.c		\
	gbookmarkfile.c 	\
	gbsearcharray.h		\
	gbuffer.c		\
	gbuffer.h		\
	gcache.c		\
	gchecksum.c		\
	gcompletion.c		\
	gconvert.c		\
	gdataset.c		\
	gdatasetprivate.h	\
	gdate.c         	\
	gdir.c			\
	gerror.c		\
	gfileutils.c		\
	ghash.c			\
	ghook.c			\
	ghostutils.c		\
	giochannel.c    	\
	gkeyfile.c        	\
	glibintl.h		\
	glist.c			\
	gmain.c         	\
	gmappedfile.c		\
	gmarkup.c		\
	gmem.c			\
	gmessages.c		\
	gmirroringtable.h	\
	gnode.c			\
	goption.c		\
	gpattern.c		\
	gpoll.c			\
	gprimes.c		\
	gqsort.c		\
	gqueue.c		\
	grel.c			\
	grand.c			\
	$(gregex_c)		\
	gscanner.c		\
	gscripttable.h		\
	gsequence.c		\
	gshell.c		\
	gslice.c		\
	gslist.c		\
	gstdio.c		\
	gstrfuncs.c		\
	gstring.c		\
	gtestutils.c		\
	gthread.c       	\
	gthreadprivate.h	\
	gthreadpool.c   	\
	gtimer.c		\
	gtree.c			\
	guniprop.c		\
	gutf8.c			\
	gunibreak.h		\
	gunibreak.c		\
	gunichartables.h	\
	gunicollate.c		\
	gunicomp.h		\
	gunidecomp.h		\
	gunidecomp.c		\
	gunicodeprivate.h	\
	gurifuncs.c 		\
	gutils.c		\
	gvariant.h		\
	gvariant.c		\
	gvariant-core.h		\
	gvariant-core.c		\
	gvariant-internal.h	\
	gvariant-parser.c	\
	gvariant-serialiser.h	\
	gvariant-serialiser.c	\
	gvarianttypeinfo.h	\
	gvarianttypeinfo.c	\
	gvarianttype.c		\
	galias.h                \
        gdebug.h		\
	gprintf.c		\
	gprintfint.h           \
        $(EXTRA_libglib_2_0_la_SOURCES) \


glibinclude_HEADERS:=   \
	glib-object.h	\
	glib.h

glibsubinclude_HEADERS:=   \
	galloca.h	\
	garray.h	\
	gasyncqueue.h	\
	gatomic.h	\
	gbacktrace.h	\
	gbase64.h	\
	gbitlock.h	\
	gbookmarkfile.h \
	gcache.h	\
	gchecksum.h	\
	gcompletion.h	\
	gconvert.h	\
	gdataset.h	\
	gdate.h		\
	gdir.h		\
	gerror.h	\
	gfileutils.h	\
	ghash.h		\
	ghook.h		\
	ghostutils.h	\
	gi18n.h		\
	gi18n-lib.h	\
	giochannel.h	\
	gkeyfile.h 	\
	glist.h		\
	gmacros.h	\
	gmain.h		\
	gmappedfile.h	\
	gmarkup.h	\
	gmem.h		\
	gmessages.h	\
	gnode.h		\
	goption.h	\
	gpattern.h	\
	gpoll.h		\
	gprimes.h	\
	gqsort.h	\
	gquark.h	\
	gqueue.h	\
	grand.h		\
	$(gregex_h)	\
	grel.h		\
	gscanner.h	\
	gsequence.h	\
	gshell.h	\
	gslice.h	\
	gslist.h	\
	gspawn.h	\
	gstdio.h	\
	gstrfuncs.h	\
	gtestutils.h	\
	gstring.h	\
	gthread.h	\
	gthreadpool.h	\
	gtimer.h	\
	gtree.h		\
	gtypes.h	\
	gunicode.h	\
	gurifuncs.h 		\
	gutils.h	\
	gvarianttype.h	\
	gvariant.h	\
	gprintf.h


LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH) \
        $(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH)/ \
        $(LOCAL_PATH)/android \
        $(LOCAL_PATH)/pcre \
        $(LOCAL_PATH)/libcharset \
        external/libiconv/include \
        $(TARGET_OUT_HEADERS)/glib/glib \

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GLib\"         \
        -DLIBDIR=\"/system/lib\"        \
        -DG_DISABLE_DEPRECATED          \
        -DGLIB_COMPILATION              \
        -DPCRE_STATIC                   \

LOCAL_SHARED_LIBRARIES += libiconv
LOCAL_STATIC_LIBRARIES += libpcre libcharset


LOCAL_ARM_MODE := arm
LOCAL_MODULE := libglib-2.0

intermediates := $(TARGET_OUT_HEADERS)
GEN := $(intermediates)/glib/glib/galias.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/glib.symbols
$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegalias.pl < $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/glib.symbols
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/galiasdef.c
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/glib.symbols
$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegalias.pl -def < $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/glib.symbols
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)


include $(BUILD_SHARED_LIBRARY)

