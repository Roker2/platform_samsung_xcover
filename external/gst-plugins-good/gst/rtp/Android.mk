LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= 	\
	fnv1hash.c \
        gstrtp.c \
        gstrtpchannels.c \
        gstrtpdepay.c \
        gstrtpac3depay.c \
        gstrtpac3pay.c \
        gstrtpbvdepay.c \
        gstrtpbvpay.c \
        gstrtpceltdepay.c \
        gstrtpceltpay.c \
        gstrtpdvdepay.c \
        gstrtpdvpay.c \
        gstrtpgstdepay.c \
        gstrtpgstpay.c \
        gstrtpilbcdepay.c \
        gstrtpilbcpay.c \
        gstrtpmpadepay.c \
        gstrtpmpapay.c \
        gstrtpmparobustdepay.c \
        gstrtpmpvdepay.c \
        gstrtpmpvpay.c \
        gstrtppcmadepay.c \
        gstrtppcmudepay.c \
        gstrtppcmupay.c \
        gstrtppcmapay.c \
        gstrtpg722depay.c \
        gstrtpg722pay.c \
        gstrtpg723depay.c \
        gstrtpg723pay.c \
        gstrtpg726pay.c \
        gstrtpg726depay.c \
        gstrtpg729pay.c \
        gstrtpg729depay.c \
        gstrtpgsmdepay.c \
        gstrtpgsmpay.c \
        gstrtpamrdepay.c \
        gstrtpamrpay.c \
        gstrtph263pdepay.c \
        gstrtph263ppay.c \
        gstrtph263depay.c \
        gstrtph263pay.c \
        gstrtph264depay.c \
        gstrtph264pay.c \
        gstrtpj2kdepay.c \
        gstrtpj2kpay.c \
        gstrtpjpegdepay.c \
        gstrtpjpegpay.c \
        gstrtpL16depay.c \
        gstrtpL16pay.c \
        gstasteriskh263.c \
        gstrtpmp1sdepay.c \
        gstrtpmp2tdepay.c \
	gstrtpmp2tpay.c \
        gstrtpmp4vdepay.c \
        gstrtpmp4vpay.c \
        gstrtpmp4gdepay.c \
        gstrtpmp4gpay.c \
        gstrtpmp4adepay.c \
        gstrtpmp4apay.c \
        gstrtpqcelpdepay.c \
        gstrtpqdmdepay.c \
        gstrtpsirenpay.c \
        gstrtpsirendepay.c \
        gstrtpspeexdepay.c \
        gstrtpspeexpay.c \
        gstrtpsv3vdepay.c \
        gstrtptheoradepay.c \
        gstrtptheorapay.c \
        gstrtpvorbisdepay.c \
        gstrtpvorbispay.c  \
        gstrtpvrawdepay.c  \
        gstrtpvrawpay.c

LOCAL_SHARED_LIBRARIES :=	\
	libgsttag-0.10	\
	libgstrtp-0.10	\
	libgstaudio-0.10	\
	libgstreamer-0.10	\
	libgstbase-0.10		\
	libglib-2.0		\
	libgthread-2.0		\
	libgmodule-2.0		\
	libgobject-2.0

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(GST_PLUGINS_PATH)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= libgstrtp

LOCAL_C_INCLUDES := 				\
	$(LOCAL_PATH)				\
	$(GST_PLUGINS_GOOD_TOP)			\
	$(GST_PLUGINS_GOOD_DEP_INCLUDES)

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H			

include $(BUILD_SHARED_LIBRARY)
