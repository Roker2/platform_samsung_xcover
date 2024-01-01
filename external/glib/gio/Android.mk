LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

marshal_sources:= \
        gio-marshal.h	\
        gio-marshal.c	\

appinfo_sources:=       \
        gdesktopappinfo.c \
        gdesktopappinfo.h \

local_sources:= \
	glocaldirectorymonitor.c 	\
	glocaldirectorymonitor.h 	\
	glocalfile.c 			\
	glocalfile.h 			\
	glocalfileenumerator.c 		\
	glocalfileenumerator.h 		\
	glocalfileinfo.c 		\
	glocalfileinfo.h 		\
	glocalfileinputstream.c 	\
	glocalfileinputstream.h 	\
	glocalfilemonitor.c 		\
	glocalfilemonitor.h 		\
	glocalfileoutputstream.c 	\
	glocalfileoutputstream.h 	\
	glocalfileiostream.c		\
	glocalfileiostream.h		\
	glocalvfs.c 			\
	glocalvfs.h 			\

#unix_sources:=                  \
	gunixconnection.c	\
	gunixfdlist.c		\
	gunixfdmessage.c	\
	gunixmount.c		\
	gunixmount.h		\
	gunixmounts.c 		\
	gunixmounts.h 		\
	gunixresolver.c		\
	gunixresolver.h		\
	gunixsocketaddress.c	\
	gunixvolume.c 		\
	gunixvolume.h 		\
	gunixvolumemonitor.c 	\
	gunixvolumemonitor.h 	\
	gunixinputstream.c 	\
	gunixoutputstream.c 	\


unix_sources:=                  \
        gunixmount.c            \
        gunixmount.h            \
        gunixmounts.c           \
        gunixmounts.h           \
        gunixvolume.c           \
        gunixvolume.h           \
        gunixvolumemonitor.c    \
        gunixvolumemonitor.h    \
        gunixinputstream.c      \
        gunixoutputstream.c     \


giounixinclude_HEADERS:=        \
	gdesktopappinfo.h	\
	gfiledescriptorbased.h  \
	gunixconnection.h	\
	gunixmounts.h 		\
	gunixfdlist.h		\
	gunixfdmessage.h	\
	gunixinputstream.h 	\
	gunixoutputstream.h 	\
	gunixsocketaddress.h	\

LOCAL_SRC_FILES:=		\
	gappinfo.c 		\
	gasynchelper.c 		\
	gasynchelper.h 		\
	gasyncinitable.c	\
	gasyncresult.c 		\
	gbufferedinputstream.c 	\
	gbufferedoutputstream.c \
	gcancellable.c 		\
	gcontenttype.c 		\
	gcontenttypeprivate.h 	\
	gcharsetconverter.c	\
	gconverter.c		\
	gconverterinputstream.c	\
	gconverteroutputstream.c	\
	gdatainputstream.c 	\
	gdataoutputstream.c 	\
	gdrive.c 		\
	gdummyfile.h 		\
	gdummyfile.c 		\
	gemblem.h 		\
	gemblem.c 		\
	gemblemedicon.h		\
	gemblemedicon.c		\
	gfile.c 		\
	gfileattribute.c 	\
	gfileattribute-priv.h 	\
	gfiledescriptorbased.h  \
	gfiledescriptorbased.c  \
	gfileenumerator.c 	\
	gfileicon.c 		\
	gfileinfo.c 		\
	gfileinfo-priv.h 	\
	gfileinputstream.c 	\
	gfilemonitor.c 		\
	gfilenamecompleter.c 	\
	gfileoutputstream.c 	\
	gfileiostream.c		\
	gfilterinputstream.c 	\
	gfilteroutputstream.c 	\
	gicon.c 		\
	ginitable.c		\
	ginputstream.c 		\
	gioenums.h		\
	gioerror.c 		\
	giomodule.c 		\
	giomodule-priv.h	\
	gioscheduler.c 		\
	giostream.c		\
	gloadableicon.c 	\
	gmount.c 		\
	gmemoryinputstream.c 	\
	gmemoryoutputstream.c 	\
	gmountoperation.c 	\
	gnativevolumemonitor.c 	\
	gnativevolumemonitor.h 	\
	gnetworkingprivate.h	\
	goutputstream.c 	\
	gpollfilemonitor.c 	\
	gpollfilemonitor.h 	\
	gseekable.c 		\
	gsimpleasyncresult.c 	\
	gsrvtarget.c		\
	gthemedicon.c 		\
	gunionvolumemonitor.c 	\
	gunionvolumemonitor.h 	\
	gvfs.c 			\
	gvolume.c 		\
	gvolumemonitor.c 	\
	gmountprivate.h 	\
        gioenumtypes.h          \
        gioenumtypes.c          \
	$(appinfo_sources) 	\
	$(unix_sources) 	\
	$(local_sources) 	\

LOCAL_C_INCLUDES += \
        $(GLIB_TOP_PATH)/ \
        $(GLIB_TOP_PATH)/glib \
        $(GLIB_TOP_PATH)/gmodule \
        $(GLIB_TOP_PATH)/gobject \
        $(GLIB_TOP_PATH)/gobject/android \
        $(GLIB_TOP_PATH)/android \
        $(LOCAL_PATH) \
        $(TARGET_OUT_HEADERS)/glib/gio \
        $(TARGET_OUT_HEADERS)/glib/ \

gio_headers:= \
        gappinfo.h              \
        gasyncinitable.h        \
        gasyncresult.h 		\
	gbufferedinputstream.h 	\
	gbufferedoutputstream.h \
	gcancellable.h 		\
	gcontenttype.h 		\
	gcharsetconverter.h	\
	gconverter.h		\
	gconverterinputstream.h	\
	gconverteroutputstream.h	\
	gdatainputstream.h 	\
	gdataoutputstream.h 	\
	gdrive.h 		\
	gemblem.h 		\
	gemblemedicon.h		\
	gfile.h 		\
	gfileattribute.h 	\
	gfileenumerator.h 	\
	gfileicon.h 		\
	gfileinfo.h 		\
	gfileinputstream.h 	\
	gfilemonitor.h 		\
	gfilenamecompleter.h 	\
	gfileoutputstream.h 	\
	gfileiostream.h		\
	gfilterinputstream.h 	\
	gfilteroutputstream.h 	\
	gicon.h 		\
	ginetaddress.h		\
	ginetsocketaddress.h	\
	ginputstream.h 		\
	ginitable.h		\
	gio.h			\
	giotypes.h		\
	gioenums.h		\
	gioerror.h 		\
	giomodule.h 		\
	gioscheduler.h 		\
	giostream.h		\
	gloadableicon.h 	\
	gmount.h 		\
	gmemoryinputstream.h 	\
	gmemoryoutputstream.h 	\
	gmountoperation.h 	\
	gnativevolumemonitor.h 	\
	gnetworkaddress.h	\
	gnetworkservice.h	\
	goutputstream.h 	\
	gseekable.h 		\
	gsimpleasyncresult.h 	\
	gsocket.h		\
	gsocketaddress.h	\
	gsocketaddressenumerator.h \
	gsocketclient.h		\
	gsocketconnectable.h	\
	gsocketconnection.h	\
	gsocketcontrolmessage.h	\
	gsocketlistener.h	\
	gsocketservice.h	\
	gsrvtarget.h		\
	gtcpconnection.h	\
	gthreadedsocketservice.h\
	gthemedicon.h 		\
	gvfs.h 			\
	gvolume.h 		\
	gvolumemonitor.h 	\


#	gnetworkservice.c	\
 	gresolver.h		\
	gresolver.c		\
	gthreadedresolver.h	\
	gthreadedresolver.c	\
	gzlibdecompressor.h	\
	gzlibcompressor.h	\
	gzlibdecompressor.c	\
	gzlibcompressor.c	\
	ginetaddress.c		\
	ginetsocketaddress.c	\
	gnetworkaddress.c	\
	gsocketaddress.c	\
	gsocket.c		\
        gsocketaddressenumerator.c \
        gsocketclient.c         \
        gsocketconnectable.c    \
        gsocketconnection.c     \
        gsocketcontrolmessage.c \
        gsocketinputstream.c    \
        gsocketinputstream.h    \
        gsocketlistener.c       \
        gsocketoutputstream.c   \
        gsocketoutputstream.h   \
        gsocketservice.c        \
	gtcpconnection.c	\
	gthreadedsocketservice.c\

gioinclude_HEADERS = 		\
	$(gio_headers)		\
	gioenumtypes.h

LOCAL_CFLAGS += \
        -DG_LOG_DOMAIN=\"GLib-GIO\"                     \
        -DG_DISABLE_DEPRECATED                          \
        -DGIO_COMPILATION                               \
        -DGIO_MODULE_DIR=\"/system/lib\"                \

LOCAL_MODULE := libgio-2.0
LOCAL_STATIC_LIBRARIES += libxdgmime libinotify
LOCAL_SHARED_LIBRARIES += libglib-2.0 libgmodule-2.0 libgobject-2.0
LOCAL_ARM_MODE := arm

#intermediates := $(TARGET_OUT_HEADERS)
#GEN := $(intermediates)/glib/gio/gioenumtypes.h
#$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gioenumtypes.h.template $(gio_headers)
#$(GEN): PRIVATE_CUSTOM_TOOL := glib-mkenums --template $(LOCAL_PATH)/gioenumtypes.h.template $(gio_headers)
#$(GEN): $(LOCAL_PATH)/gioenumtypes.h.template
#	mkdir -p $(dir $@)
#	$(PRIVATE_CUSTOM_TOOL) > $@
#all_copied_headers:$(GEN)

intermediates := $(TARGET_OUT_HEADERS)
GEN := $(intermediates)/glib/gio/gioalias.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gio.symbols
$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegioalias.pl < $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gio.symbols
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

intermediates := $(TARGET_OUT_HEADERS)
GEN := $(intermediates)/glib/gio/gio-marshal.h
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gio-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --prefix=_gio_marshal $(PRIVATE_INPUT_FILE) --header --internal
$(GEN): $(LOCAL_PATH)/gio-marshal.list
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
all_copied_headers:$(GEN)

LOCAL_MODULE_CLASS := SHARED_LIBRARIES

#intermediates := $(local-intermediates-dir)
#GEN := $(intermediates)/gioenumtypes.cc
#$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gioenumtypes.c.template 
#$(GEN): PRIVATE_CUSTOM_TOOL := glib-mkenums --template $(LOCAL_PATH)/gioenumtypes.c.template $(gio_headers)
#$(GEN): $(LOCAL_PATH)/gioenumtypes.c.template 
#	mkdir -p $(dir $@)
#	glib-mkenums --template $(LOCAL_PATH)/gioenumtypes.c.template $(gio_headers) > $@
#LOCAL_GENERATED_SOURCES += $(GEN)
#LOCAL_SRC_FILES += android/gioenumtypes.c

intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gioaliasdef.c
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gio.symbols
$(GEN): PRIVATE_CUSTOM_TOOL := perl $(LOCAL_PATH)/makegioalias.pl -def < $(PRIVATE_INPUT_FILE)
$(GEN): $(LOCAL_PATH)/gio.symbols
	mkdir -p $(dir $@)
	$(PRIVATE_CUSTOM_TOOL) > $@
LOCAL_GENERATED_SOURCES += $(GEN)


intermediates := $(local-intermediates-dir)
GEN := $(intermediates)/gio-marshal.cc
$(GEN): PRIVATE_INPUT_FILE := $(LOCAL_PATH)/gio-marshal.list
$(GEN): PRIVATE_CUSTOM_TOOL := glib-genmarshal --prefix=_gio_marshal $(PRIVATE_INPUT_FILE) --body --internal
$(GEN): $(LOCAL_PATH)/gio-marshal.list
	mkdir -p $(dir $@)
	echo '#include "gio-marshal.h"' > $@
	$(PRIVATE_CUSTOM_TOOL) >> $@
LOCAL_GENERATED_SOURCES += $(GEN)
LOCAL_SRC_FILES += android/gio-marshal.c


#LOCAL_COPY_HEADERS_TO := glib-2.0/gio/
#LOCAL_COPY_HEADERS := $(gioinclude_HEADERS)

include $(BUILD_SHARED_LIBRARY)

