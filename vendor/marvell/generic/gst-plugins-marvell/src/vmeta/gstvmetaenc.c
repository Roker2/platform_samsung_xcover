/*
Copyright (c) 2009, Marvell International Ltd.
All Rights Reserved.

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h> //to include memcpy, memset()
#include <strings.h>	//to include strcasecmp
#include <dlfcn.h>		//to include dlopen,...
#include <sys/time.h>	//to include time functions and structures
#include <time.h>

#include "gstvmetaenc.h"
#include "vdec_os_api.h"
#include "misc.h"

GST_DEBUG_CATEGORY_STATIC (vmetaenc_debug);
#define GST_CAT_DEFAULT vmetaenc_debug


#define IPPGSTVMETA_STMFORMAT_H264	1
#define IPPGSTVMETA_STMFORMAT_MPEG4	2
#define IPPGSTVMETA_STMFORMAT_H263	3
#define IPPGSTVMETA_STMFORMAT_JPEG	4	//the difference between IPPGSTVMETA_STMFORMAT_JPEG and IPPGSTVMETA_STMFORMAT_MJPEG is only on src caps statement
#define IPPGSTVMETA_STMFORMAT_MJPEG	5

#define IPPGSTVMETA_STMFORMAT_MIN	IPPGSTVMETA_STMFORMAT_H264

#if GSTVMETAENC_PLATFORM == GSTVMETAENC_PLATFORM_HASMJPEG
#define IPPGSTVMETA_STMFORMAT_MAX	IPPGSTVMETA_STMFORMAT_MJPEG
#else
#define IPPGSTVMETA_STMFORMAT_MAX	IPPGSTVMETA_STMFORMAT_H263
#endif

#define IPPGST_CHECK_VIDEO_FORMAT_IS_SUPPORTED(format)		((format)>=IPPGSTVMETA_STMFORMAT_MIN && (format)<=IPPGSTVMETA_STMFORMAT_MAX)

/******************************************************************************
// GStreamer plug-in element implementation
******************************************************************************/
enum {

	/* fill above with user defined signals */
	LAST_SIGNAL
};

enum {
	ARG_0,
	/* fill below with user defined arguments */
	ARG_FORCE_STARTTIME,
	ARG_VIDEOFORMAT,
	ARG_TARGET_BITRATE,
	ARG_DISABLE_RATECTRL,
	ARG_QP,
	ARG_ONLY_I,
	ARG_SUPPORTMULTIINSTANCE,
	ARG_HUNGRYSTRATEGY,


#ifdef DEBUG_PERFORMANCE
	ARG_TOTALFRAME,
	ARG_CODECTIME,
#endif
};

//SUYV isn't an standard fourcc, used by Samsung, it equal to UYVY
static GstStaticPadTemplate sink_factory = \
	GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, \
	GST_STATIC_CAPS ("video/x-raw-yuv," \
					 "format = (fourcc) { UYVY, SUYV }, " \
					 "framerate = (fraction) [ 1/1, 60/1 ], " \
					 "width = (int) [ 16, 2048 ], " \
					 "height = (int) [ 16, 2048 ]" \
					 ));


#if GSTVMETAENC_PLATFORM == GSTVMETAENC_PLATFORM_HASMJPEG
static GstStaticPadTemplate src_factory = \
	GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS, \
	GST_STATIC_CAPS ("video/x-h264, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";video/mpeg, mpegversion = (int)4, systemstream = (boolean) FALSE, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";video/x-h263, variant = (string)itu, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";video/x-jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";image/jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	));
#else
static GstStaticPadTemplate src_factory = \
	GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS, \
	GST_STATIC_CAPS ("video/x-h264, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";video/mpeg, mpegversion = (int)4, systemstream = (boolean) FALSE, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	";video/x-h263, variant = (string)itu, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 1/1, 60/1 ]"
	));
#endif


GST_BOILERPLATE (GstVmetaEnc, gst_vmetaenc, GstElement, GST_TYPE_ELEMENT);

#define IPPGST_DEFAULT_TARGETBITRATE	300000
#define IPPGST_DEFAULT_STEAMBUFSZMIN	(128*1024)
#define IPPGST_ENCODER_DEFAULTQP		30

#define IPPGST_VMETAENC_TS_MAX_CAPACITY		60


//#define IPPGSTVMETAENC_ECHO
#ifdef IPPGSTVMETAENC_ECHO
#define assist_myecho(...)	printf(__VA_ARGS__)
#else
#define assist_myecho(...)
#endif

//#define IPPGSTVMETAENC_ECHOMORE
#ifdef IPPGSTVMETAENC_ECHOMORE
#define assist_myechomore(...)	printf(__VA_ARGS__)
#else
#define assist_myechomore(...)
#endif

typedef struct{
	struct timeval EncAPI_clk0;
	struct timeval EncAPI_clk1;
	int bufdistinguish_cnt0;
	int bufdistinguish_cnt1;
	int bufdistinguish_cnt2;
}VmetaProbeData;

#define SET_IPPVMETAFRAME_BUSYFLAG(pframe, flag)	(((IppVmetaPicture*)(pframe))->pUsrData0 = (void*)flag)
#define IS_IPPVMETA_FRAME_BUSY(pframe)				(((IppVmetaPicture*)(pframe))->pUsrData0 != (void*)0)
#define IPPVMETAFRAME_ATTCHEDGSTBUF(pframe)			(((IppVmetaPicture*)(pframe))->pUsrData1)
#define IS_IPPVMETAFRAME_NOTFROMREPO(pframe)		(((IppVmetaPicture*)(pframe))->pUsrData2 != (void*)0)

#define IS_IPPVMETA_STM_BUSY(pframe)				(((IppVmetaBitstream*)(pframe))->pUsrData0 != (void*)0)
#define SET_IPPVMETASTM_BUSYFLAG(pstm, flag)		(((IppVmetaBitstream*)(pstm))->pUsrData0 = (void*)flag)

//a subclass from GstBuffer to hook GstBuffer finalize
typedef struct {
	GstBuffer						gstbuf;
	IppVmetaPicture*				pVmetaFrame;
	RES_HOLD_BY_EXTUSER_WATCHMAN	watcher;
}GstVmetaEncFrameBuffer;

#define GST_TYPE_VMETAENC_BUFFER		(gst_vmetaenc_buffer_get_type())
#define GST_IS_VMETAENC_BUFFER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VMETAENC_BUFFER))
#define GST_VMETAENC_BUFFER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VMETAENC_BUFFER, GstVmetaEncFrameBuffer))
#define GST_VMETAENC_BUFFER_CAST(obj)	((GstVmetaEncFrameBuffer *)(obj))

static GstMiniObjectClass* vmetaenc_buffer_parent_class = NULL;

static void
gst_vmetaenc_buffer_finalize(GstVmetaEncFrameBuffer * buf)
{
	SET_IPPVMETAFRAME_BUSYFLAG(buf->pVmetaFrame, 0);
	RES_HOLD_BY_EXTUSER_WATCHMAN_UNREF(buf->watcher);
	vmetaenc_buffer_parent_class->finalize(GST_MINI_OBJECT_CAST(buf));
	return;	
}

static void
gst_vmetaenc_buffer_class_init(gpointer g_class, gpointer class_data)
{
	GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS(g_class);
	vmetaenc_buffer_parent_class = g_type_class_peek_parent(g_class);
	mini_object_class->finalize = (GstMiniObjectFinalizeFunction)gst_vmetaenc_buffer_finalize;
	return;
}

static void
gst_vmetaenc_buffer_init(GTypeInstance * instance, gpointer g_class)
{
	return;
}

GType
gst_vmetaenc_buffer_get_type(void)
{
	static GType buffer_type;

	if (G_UNLIKELY(buffer_type == 0)) {
		static const GTypeInfo buffer_info = {
			sizeof(GstBufferClass),
			NULL,
			NULL,
			gst_vmetaenc_buffer_class_init,
			NULL,
			NULL,
			sizeof(GstVmetaEncFrameBuffer),
			0,
			gst_vmetaenc_buffer_init,
			NULL
		};
		buffer_type = g_type_register_static(GST_TYPE_BUFFER, "GstVmetaEncFrameBuffer", &buffer_info, 0);
	}
	return buffer_type;
}

static __inline GstVmetaEncFrameBuffer *
gst_vMetaEncBuffer_new()
{
	return (GstVmetaEncFrameBuffer*)gst_mini_object_new(GST_TYPE_VMETAENC_BUFFER);
}
//end of a subclass from GstBuffer to hook GstBuffer finalize




#define IPPGST_VMETAENC_IS_2ALIGN(x)		(((unsigned int)(x)&1) == 0)
#define IPPGST_VMETAENC_IS_16ALIGN(x)		(((unsigned int)(x)&15) == 0)

static int
IPP_videoCheckPar_vMetaEnc(int w, int h, int FR, int bR)
{
	if(! IPPGST_VMETAENC_IS_16ALIGN(w)) {
		g_warning("src image width %d isn't 16 aligned!", w);
		return -1;
	}
	if(! IPPGST_VMETAENC_IS_2ALIGN(h)) {
		g_warning("src image height %d isn't 2 aligned!", h);
		return -2;
	}
	if(FR <= 0) {
		g_warning("src framerate %d exceeds range, and don't support variable framerate!", FR);
		return -3;
	}
	if(bR <= 0) {
		g_warning("target bitrate %d exceeds range!", bR);
		return -4;
	}
	return 0;
}



static void 
IPP_SetEncPar_vMetaEnc(GstVmetaEnc *venc)
{
	assist_myecho("Set vmeta encoder parameter, w %d, h %d, FR %d, bR %d\n", venc->iEncW, venc->iEncH, venc->iEncFrameRate, venc->iTargtBitRate);
	venc->vMetaEnc_Par.nWidth = venc->iEncW;
	venc->vMetaEnc_Par.nHeight = venc->iEncH;
	venc->vMetaEnc_Par.nFrameRateNum = venc->iEncFrameRate;
	if(venc->bOnlyIFrame == 0) {
		venc->vMetaEnc_Par.nPBetweenI = venc->iEncFrameRate - 1 > 0 ? venc->iEncFrameRate - 1 : 1;
	}else{
		venc->vMetaEnc_Par.nPBetweenI = 0;
	}
	venc->vMetaEnc_Par.bRCEnable = venc->bDisableRateControl == 0 ? 1 : 0;
	venc->vMetaEnc_Par.nQP = venc->iQP;
	venc->vMetaEnc_Par.nRCBitRate = venc->iTargtBitRate;
	assist_myecho("vmeta encoder parameter is set to: eInputYUVFmt %d, eOutputStrmFmt %d, nWidth %d, nHeight %d, nFrameRateNum %d, nPBetweenI %d, bRCEnable %d, nQP %d, nRCBitRate %d\n", \
		venc->vMetaEnc_Par.eInputYUVFmt, venc->vMetaEnc_Par.eOutputStrmFmt, venc->vMetaEnc_Par.nWidth, venc->vMetaEnc_Par.nHeight, venc->vMetaEnc_Par.nFrameRateNum, venc->vMetaEnc_Par.nPBetweenI, venc->vMetaEnc_Par.bRCEnable, venc->vMetaEnc_Par.nQP, venc->vMetaEnc_Par.nRCBitRate);
	return;
}

static __inline GstCaps *
IPP_NewCaps(GstVmetaEnc *venc)
{
	GstCaps* pTmp = NULL;
	if(venc->iVideoStmFormat == IPPGSTVMETA_STMFORMAT_H264) {
		pTmp = gst_caps_new_simple ("video/x-h264", 
		"width", G_TYPE_INT, venc->iEncW, 
		"height", G_TYPE_INT, venc->iEncH, 
		"framerate", GST_TYPE_FRACTION,  venc->iEncFrameRate, 1,
		NULL);
	}else if(venc->iVideoStmFormat == IPPGSTVMETA_STMFORMAT_H263) {
		pTmp = gst_caps_new_simple ("video/x-h263", 
			"width", G_TYPE_INT, venc->iEncW, 
			"height", G_TYPE_INT, venc->iEncH, 
			"variant", G_TYPE_STRING, "itu",
			"framerate", GST_TYPE_FRACTION,  venc->iEncFrameRate, 1,
			NULL);
	}else if(venc->iVideoStmFormat == IPPGSTVMETA_STMFORMAT_MPEG4) {
		pTmp = gst_caps_new_simple ("video/mpeg", 
			"width", G_TYPE_INT, venc->iEncW, 
			"height", G_TYPE_INT, venc->iEncH, 
			"mpegversion", G_TYPE_INT, 4,
			"systemstream", G_TYPE_BOOLEAN, FALSE,
			"framerate", GST_TYPE_FRACTION,  venc->iEncFrameRate, 1,
			NULL);
	}else if(venc->iVideoStmFormat == IPPGSTVMETA_STMFORMAT_MJPEG) {
		pTmp = gst_caps_new_simple ("video/x-jpeg",
			"width", G_TYPE_INT, venc->iEncW,
			"height", G_TYPE_INT, venc->iEncH,
			"framerate", GST_TYPE_FRACTION,  venc->iEncFrameRate, 1,
			NULL);
	}else if(venc->iVideoStmFormat == IPPGSTVMETA_STMFORMAT_JPEG) {
		pTmp = gst_caps_new_simple ("image/jpeg",
			"width", G_TYPE_INT, venc->iEncW,
			"height", G_TYPE_INT, venc->iEncH,
			"framerate", GST_TYPE_FRACTION,  venc->iEncFrameRate, 1,
			NULL);
	}
	return pTmp;
}

static gboolean 
gst_vmetaenc_sinkpad_setcaps(GstPad *pad, GstCaps *caps)
{
	GstVmetaEnc *venc = GST_VMETAENC (GST_OBJECT_PARENT (pad));
#ifdef IPPGSTVMETAENC_ECHOMORE
	gchar* capstring = gst_caps_to_string(caps);
	assist_myechomore("caps is %s\n", capstring==NULL? "none" : capstring);
	if(capstring) {
		g_free(capstring);
	}
#endif

	guint stru_num = gst_caps_get_size (caps);
	if(stru_num == 0) {
		g_warning("No content in vmetaenc sinkpad setcaps!");
		GST_ERROR_OBJECT(venc, "No content in vmetaenc sinkpad setcaps!");
		return FALSE;
	}
	

	GstStructure *str = gst_caps_get_structure (caps, 0);

	const char *mime = gst_structure_get_name (str);
	if (strcmp (mime, "video/x-raw-yuv") != 0) {
		g_warning("The cap mime type %s isn't video/x-raw-yuv!", mime);
		GST_ERROR_OBJECT(venc, "The cap mime type %s isn't video/x-raw-yuv!", mime);
		return FALSE;
	}

	guint32 fourcc = 0;
	if( FALSE == gst_structure_get_fourcc(str, "format", &fourcc) || (fourcc != GST_STR_FOURCC("UYVY") && fourcc != GST_STR_FOURCC("SUYV")) ) {
		g_warning("Couldn't find UYVY/SUYV fourcc in setcaps()!");
		GST_ERROR_OBJECT(venc, "Couldn't find UYVY/SUYV fourcc in setcaps()!");
		return FALSE;
	}


	if (FALSE == gst_structure_get_int(str, "width", &(venc->iEncW))) {
		g_warning("Couldn't get src image width in setcaps()!");
		GST_ERROR_OBJECT(venc, "Couldn't get src image width in setcaps()!");
		return FALSE;
	}

	if (FALSE == gst_structure_get_int(str, "height", &(venc->iEncH))) {
		g_warning("Couldn't get src image height in setcaps()!");
		GST_ERROR_OBJECT(venc, "Couldn't get src image height in setcaps()!");
		return FALSE;
	}

	int iFRateNum, iFRateDen;
	if (FALSE == gst_structure_get_fraction(str, "framerate", &iFRateNum, &iFRateDen)) {
		g_warning("Couldn't get framerate in setcaps()!");
		GST_ERROR_OBJECT(venc, "Couldn't get framerate in setcaps()!");
		return FALSE;
	}

	if(iFRateDen == 0) {
		g_warning("frame rate info from caps isn't correct, (%d)/(%d)!", iFRateNum, iFRateDen);
		GST_ERROR_OBJECT(venc, "frame rate info from caps isn't correct, (%d)/(%d)!", iFRateNum, iFRateDen);
		return FALSE;
	}
	venc->iEncFrameRate = iFRateNum/iFRateDen;
	if(iFRateDen * venc->iEncFrameRate != iFRateNum) {	//current vmeta encoder only support interger fps
		g_warning("Couldn't support fractional framerate (%d)/(%d) fps!", iFRateNum, iFRateDen);
		GST_ERROR_OBJECT(venc, "Couldn't support fractional framerate (%d)/(%d) fps!", iFRateNum, iFRateDen);
		return FALSE;
	}
	if(venc->iEncFrameRate) {
		venc->iFixedFRateDuration = gst_util_uint64_scale_int(GST_SECOND, 1, venc->iEncFrameRate);
		assist_myecho("venc->iFixedFRateDuration is set %lld\n", venc->iFixedFRateDuration);
	}
	
	if( 0 != IPP_videoCheckPar_vMetaEnc(venc->iEncW, venc->iEncH, venc->iEncFrameRate, venc->iTargtBitRate) ) {
		GST_ERROR_OBJECT(venc, "The parameter for encoder is wrong!!!");
		return FALSE;
	}
	
	GstCaps *TmpCap;
	TmpCap = IPP_NewCaps(venc);
	GST_INFO_OBJECT(venc, "Set caps on srcpad");
	if(!gst_pad_set_caps(venc->srcpad, TmpCap)) {
		gst_caps_unref(TmpCap);
		g_warning("Set caps on srcpad fail!");
		GST_ERROR_OBJECT(venc, "Set caps on srcpad fail!");
		return FALSE;
	}
	gst_caps_unref(TmpCap);

	venc->i1SrcFrameDataLen = (venc->iEncW*venc->iEncH)<<1;
	if(venc->iWantedStmBufSz < ((venc->iEncW*venc->iEncH)>>2)) {
		venc->iWantedStmBufSz = (venc->iEncW*venc->iEncH)>>2;
	}
	if(venc->iWantedStmBufSz*venc->iEncFrameRate*8 < (venc->iTargtBitRate*3>>1) ) {
		venc->iWantedStmBufSz = (3*(venc->iTargtBitRate>>3)/venc->iEncFrameRate)>>1;
	}

	IPP_SetEncPar_vMetaEnc(venc);

	return TRUE;

}

static __inline void
IPP_AdjustStmSz(GstVmetaEnc *venc, int iCurCompressedDataLen)
{
	if(iCurCompressedDataLen > venc->iWantedStmBufSz*3>>2) {
		venc->iWantedStmBufSz = venc->iWantedStmBufSz*5>>2;
	}
	return;
}

static __inline IppVmetaPicture*
IPP_PopFrameFromQueue(GstVmetaEnc* venc)
{
	return (IppVmetaPicture*)g_queue_pop_tail(&venc->vMetaFrameCandidateQueue);
}

static __inline void
IPP_PushFrameToQueue(GstVmetaEnc* venc, IppVmetaPicture* pFrame)
{
	g_queue_push_head(&venc->vMetaFrameCandidateQueue, pFrame);
	return;
}

static GstClockTime
IPP_PopTSFromQueue(GstVmetaEnc* venc)
{
	GstClockTime* pTs = (GstClockTime*)g_queue_pop_tail(&venc->vMetaStmTSCandidateQueue);
	GstClockTime ts;
	if(pTs == NULL) {
		g_warning("No timestamp left in TS queue could be pop!");
		return GST_CLOCK_TIME_NONE;
	}else{
		ts = *pTs;
		g_slice_free(GstClockTime, pTs);
		return ts;
	}
}

static void
IPP_PushTSToQueue(GstVmetaEnc* venc, GstClockTime ts)
{
	GstClockTime* pTs = g_slice_new(GstClockTime);
	*pTs = ts;
	g_queue_push_head(&venc->vMetaStmTSCandidateQueue, pTs);

	//remove redundant TS to avoid queue overflow
	if(g_queue_get_length(&venc->vMetaStmTSCandidateQueue) > IPPGST_VMETAENC_TS_MAX_CAPACITY) {
		GstClockTime* pRedundantTs = (GstClockTime*)g_queue_pop_tail(&venc->vMetaStmTSCandidateQueue);
		g_slice_free(GstClockTime, pRedundantTs);
	}
	return;
}

static void
IPP_IdleFrameBuf(IppVmetaPicture* pFrame)
{
	if(pFrame != NULL) {
		GstBuffer* buf = (GstBuffer*)IPPVMETAFRAME_ATTCHEDGSTBUF(pFrame);
		if(buf != NULL) {
			if(IS_IPPVMETAFRAME_NOTFROMREPO(pFrame)) {
				//this IppVmetaPicture is not from repo
				g_slice_free(IppVmetaPicture, pFrame);
			}
			gst_buffer_unref(buf);
		}else{
			SET_IPPVMETAFRAME_BUSYFLAG(pFrame, 0);
		}
	}
	return;
}

static __inline void
IPP_IdleStmBuf(IppVmetaBitstream* pStm)
{
	if(pStm != NULL) {
		SET_IPPVMETASTM_BUSYFLAG(pStm, 0);
	}
	return;
}

static void
IPP_ClearCandidateQueus(GstVmetaEnc* venc)
{
	assist_myecho("Before clear frame queue, there is %d frames in queue\n", g_queue_get_length(&venc->vMetaFrameCandidateQueue));
	IppVmetaPicture* pFrame;
	while(NULL != (pFrame=(IppVmetaPicture*)g_queue_pop_tail(&venc->vMetaFrameCandidateQueue))) {
		IPP_IdleFrameBuf(pFrame);	
	}

	assist_myecho("Before clear ts queue, there is %d ts in queue\n", g_queue_get_length(&venc->vMetaStmTSCandidateQueue));
	GstClockTime* pTs;
	while(NULL != (pTs=(GstClockTime*)g_queue_pop_tail(&venc->vMetaStmTSCandidateQueue))) {
		g_slice_free(GstClockTime, pTs);
	}
	return;
}

#define IPP_STMREPO_CNT(venc)				(g_slist_length((venc)->vMetaStmBufRepo))
#define IPP_FRAMEREPO_CNT(venc)				(g_slist_length((venc)->vMetaFrameBufRepo))
#define IPP_STMREPO_TOTALSZ(venc)			(IPP_GetRepoBufSz((venc)->vMetaStmBufRepo, 0))
#define IPP_FRAMEREPO_TOTALSZ(venc)			(IPP_GetRepoBufSz((venc)->vMetaFrameBufRepo, 1))

static int
IPP_GetRepoBufSz(GSList* list, int type)
{
	int sz = 0;
	while(list) {
		if(type == 0) {
			sz += ((IppVmetaBitstream*)(list->data))->nBufSize;
		}else{
			sz += ((IppVmetaPicture*)(list->data))->nBufSize;
		}
		list = list->next;
	}
	return sz;
}

static IppVmetaPicture*
IPP_NewFrameBufForRepo(int sz)
{
	Ipp32u nPhyAddr;
	unsigned char* pBuf = vdec_os_api_dma_alloc(sz, VMETA_DIS_BUF_ALIGN, &nPhyAddr);
	if(pBuf != NULL) {
		IppVmetaPicture* pFrame = g_slice_new0(IppVmetaPicture);
		if(pFrame != NULL) {
			pFrame->pBuf = pBuf;
			pFrame->nPhyAddr = nPhyAddr;
			pFrame->nBufSize = sz;
			return pFrame;
		}else{
			vdec_os_api_dma_free(pBuf);
		}
	}
	return NULL;
}

static void
IPP_DestroyIdelFramesInRepos(GstVmetaEnc* venc)
{
	GSList* OldList = venc->vMetaFrameBufRepo;
	GSList* NewBusyList = NULL;
	GSList* pNode;
	while(OldList) {
		pNode = OldList;
		//OldList = g_slist_remove_link(OldList, OldList);
		OldList = OldList->next;
		pNode->next = NULL;
		if(IS_IPPVMETA_FRAME_BUSY(pNode->data)) {
			//NewBusyList = g_slist_concat(pNode, NewBusyList);
			pNode->next = NewBusyList;
			NewBusyList = pNode;
		}else{
			vdec_os_api_dma_free(((IppVmetaPicture*)(pNode->data))->pBuf);
			g_slice_free(IppVmetaPicture, pNode->data);
			g_slist_free1(pNode);
		}
	}
	venc->vMetaFrameBufRepo = NewBusyList;
	return;
}

#define IPPGST_TOOMUCH_IDLEFSMALLRAME_INREPO		3
#define IPPGST_TOOMUCH_IDLESMALLSTM_INREPO			5

static IppVmetaPicture*
IPP_RentFrameBufferFromRepo(GstVmetaEnc* venc, int wantedsz)
{
	GSList* pNode = venc->vMetaFrameBufRepo;
	int cnt = 0, idlecnt = 0;
	IppVmetaPicture* pFrame;
	while(pNode) {
		cnt++;
		pFrame = (IppVmetaPicture*)(pNode->data);
		if(!IS_IPPVMETA_FRAME_BUSY(pFrame)) {
			idlecnt++;
			if((int)(pFrame->nBufSize) >= wantedsz) {
				pFrame->pUsrData1 = (void*)0;
				pFrame->pUsrData2 = (void*)0;
				pFrame->nDataLen = 0;
				SET_IPPVMETAFRAME_BUSYFLAG(pFrame, 1);
				return pFrame;
			}
		}
		pNode = pNode->next;
	}
	
	if(idlecnt >= IPPGST_TOOMUCH_IDLEFSMALLRAME_INREPO) {
		IPP_DestroyIdelFramesInRepos(venc);
	}

	pFrame = IPP_NewFrameBufForRepo(wantedsz);
	if(pFrame) {
		SET_IPPVMETAFRAME_BUSYFLAG(pFrame, 1);
		venc->vMetaFrameBufRepo = g_slist_prepend(venc->vMetaFrameBufRepo, pFrame);
	}
	return pFrame;
}

static IppVmetaBitstream*
IPP_NewStmBufForRepo(int sz)
{
	Ipp32u nPhyAddr;
	unsigned char* pBuf = vdec_os_api_dma_alloc(sz, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
	if(pBuf != NULL) {
		IppVmetaBitstream* pStm = g_slice_new0(IppVmetaBitstream);
		if(pStm != NULL) {
			pStm->pBuf = pBuf;
			pStm->nPhyAddr = nPhyAddr;
			pStm->nBufSize = sz;
			return pStm;
		}else{
			vdec_os_api_dma_free(pBuf);
		}
	}
	return NULL;
}

static void
IPP_DestroyIdelStmsInRepos(GstVmetaEnc* venc)
{
	GSList* OldList = venc->vMetaStmBufRepo;
	GSList* NewBusyList = NULL;
	GSList* pNode;
	while(OldList) {
		pNode = OldList;
		//OldList = g_slist_remove_link(OldList, OldList);
		OldList = OldList->next;
		pNode->next = NULL;
		if(IS_IPPVMETA_STM_BUSY(pNode->data)) {
			//NewBusyList = g_slist_concat(pNode, NewBusyList);
			pNode->next = NewBusyList;
			NewBusyList = pNode;
		}else{
			vdec_os_api_dma_free(((IppVmetaBitstream*)(pNode->data))->pBuf);
			g_slice_free(IppVmetaBitstream, pNode->data);
			g_slist_free1(pNode);
		}
	}
	venc->vMetaStmBufRepo = NewBusyList;
	return;
}


static IppVmetaBitstream*
IPP_RentStmBufferFromRepo(GstVmetaEnc* venc, int wantedsz)
{
	GSList* pNode = venc->vMetaStmBufRepo;
	int cnt = 0, idlecnt = 0;
	IppVmetaBitstream* pStm;
	while(pNode) {
		cnt++;
		pStm = (IppVmetaBitstream*)(pNode->data);
		if(!IS_IPPVMETA_STM_BUSY(pStm)) {
			idlecnt++;
			if((int)(pStm->nBufSize) >= wantedsz) {
				pStm->nDataLen = 0;
				SET_IPPVMETASTM_BUSYFLAG(pStm, 1);
				return pStm;
			}
		}
		pNode = pNode->next;
	}
	
	if(idlecnt >= IPPGST_TOOMUCH_IDLESMALLSTM_INREPO) {
		IPP_DestroyIdelStmsInRepos(venc);
	}

	pStm = IPP_NewStmBufForRepo(wantedsz);
	if(pStm) {
		SET_IPPVMETASTM_BUSYFLAG(pStm, 1);
		venc->vMetaStmBufRepo = g_slist_prepend(venc->vMetaStmBufRepo, pStm);
	}
	return pStm;
}


#define IPPGSTBUFTYPE_DIRECTUSABLE_ENCPLUGIN_ALLOCATED	0
#define IPPGSTBUFTYPE_DIRECTUSABLE_SRCPLUGIN_ALLOCATED	1
#define IPPGSTBUFTYPE_NOTDIRECTUSABLE					-1
static int
IPP_DistinguishGstBufFromUpstream(GstVmetaEnc* venc, GstBuffer* buf, int* pnPhyAddr)
{
	if(GST_IS_VMETAENC_BUFFER(buf))	{
		return IPPGSTBUFTYPE_DIRECTUSABLE_ENCPLUGIN_ALLOCATED;
	}else{
		unsigned int PhyAddr = (unsigned int)vdec_os_api_get_pa((unsigned int)(GST_BUFFER_DATA(buf)));
		if(PhyAddr != 0) {
			*pnPhyAddr = (int)PhyAddr;
			return IPPGSTBUFTYPE_DIRECTUSABLE_SRCPLUGIN_ALLOCATED;
		}
	}
	return IPPGSTBUFTYPE_NOTDIRECTUSABLE;
}

static __inline int
IPP_IsWholeFrameInputMode(GstVmetaEnc* venc, int iFragdataLen)
{
	if(G_LIKELY(venc->iWholeFrameInputModeFlag != -1)) {
		return venc->iWholeFrameInputModeFlag;
	}else{
		if(iFragdataLen == venc->i1SrcFrameDataLen) {
			venc->iWholeFrameInputModeFlag = 1;
			assist_myecho("It's whole frame input mode, input fragdata length is %d!!!\n", iFragdataLen);
			return 1;
		}else{
			venc->iWholeFrameInputModeFlag = 0;
			assist_myecho("It's not whole frame input mode, input fragdata length is %d!!!\n", iFragdataLen);
			return 0;
		}
	}
}


#define LOOPVMETA_ERR_FATAL						0x80000000
#define LOOPVMETA_ERR_RENTSTMBUF				(1<<0)
#define LOOPVMETA_ERR_RENTFRAMEBUF				(1<<1)
#define LOOPVMETA_ERR_NEWPUSHDOWNBUF			(1<<2)
#define LOOPVMETA_ERR_PUSHSTMDOWN				(1<<3)
#define LOOPVMETA_SUCRETURN_AT_NEEDINPUT		(1<<8)
#define LOOPVMETA_SUCRETURN_AT_EOS				(1<<9)
#define LOOPVMETA_SUCRETURN_AT_PICENCODED		(1<<10)
#define LOOPVMETA_ERR_VMETAAPI_CMDEOS			(1<<16)
#define LOOPVMETA_ERR_VMETAAPI_PUSHFRAMEBUF		(1<<17)
#define LOOPVMETA_ERR_VMETAAPI_PUSHSTMBUF		(1<<18)
#define LOOPVMETA_ERR_VMETAAPI_POPFRAMEBUF		(1<<19)
#define LOOPVMETA_ERR_VMETAAPI_POPSTMBUF		(1<<20)
#define LOOPVMETA_ERR_VMETAAPI_RETUNWANTEDEOS	(1<<21)
#define LOOPVMETA_ERR_VMETAAPI_RETUNWANTED		(1<<22)

static int
IPP_LoopVmeta(GstVmetaEnc *venc, int not_push_stream_down, int bStopCodec, int bExitAtEndofpic)
{
	int ret = 0;
	IppCodecStatus codecRet;
	IppVmetaPicture* pFrame = NULL;
	IppVmetaBitstream* pStm = NULL;
	void ** ppTmp;
	for(;;) {
#ifdef DEBUG_PERFORMANCE
		gettimeofday(&((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk0, NULL);
#endif
		codecRet = EncodeFrame_Vmeta(&venc->vMetaEnc_Info, venc->vMetaEnc_Handle);
#ifdef DEBUG_PERFORMANCE
		gettimeofday(&((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk1, NULL);
		venc->codec_time += (((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk1.tv_sec - ((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk0.tv_sec)*1000000 + (((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk1.tv_usec - ((VmetaProbeData*)(venc->pProbeData))->EncAPI_clk0.tv_usec);
#endif
		GST_LOG_OBJECT(venc, "EncodeFrame_Vmeta() return %d", codecRet);
		if(codecRet == IPP_STATUS_NEED_INPUT) {
			GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_NEED_INPUT");
			if(bStopCodec == 1) {
				codecRet = EncodeSendCmd_Vmeta(IPPVC_END_OF_STREAM, NULL, NULL, venc->vMetaEnc_Handle);
				if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
					g_warning("EncodeSendCmd_Vmeta(IPPVC_END_OF_STREAM,...) fail, return %d!", codecRet);
					GST_ERROR_OBJECT(venc, "EncodeSendCmd_Vmeta(IPPVC_END_OF_STREAM,...) fail, return %d!", codecRet);
					ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_CMDEOS);
					return ret;
				}else{
					continue;
				}
			}

			pFrame = IPP_PopFrameFromQueue(venc);
			if(pFrame == NULL) {
				return (ret | LOOPVMETA_SUCRETURN_AT_NEEDINPUT);		//exit loop
			}else{
				GST_LOG_OBJECT(venc, "Get a frame from candidate queue");
				if(G_UNLIKELY(pFrame->nBufSize < venc->vMetaEnc_Info.dis_buf_size)) {
					//the buf size isn't meet vmeta request
					GST_LOG_OBJECT(venc, "Frame buffer size(%d) doesn't meet the request(%d) of vmeta, do additional copy", pFrame->nBufSize, venc->vMetaEnc_Info.dis_buf_size);
					IppVmetaPicture* oldpFrame = pFrame;
					int wantedsz = venc->i1SrcFrameDataLen > (int)(venc->vMetaEnc_Info.dis_buf_size) ? venc->i1SrcFrameDataLen : (int)(venc->vMetaEnc_Info.dis_buf_size);
					pFrame = IPP_RentFrameBufferFromRepo(venc, wantedsz);
					if(pFrame) {
						memcpy(pFrame->pBuf, oldpFrame->pBuf, venc->i1SrcFrameDataLen);
						pFrame->nDataLen = venc->i1SrcFrameDataLen;
						IPP_IdleFrameBuf(oldpFrame);
					}else{
						g_warning("IPP_RentFrameBufferFromRepo() fail when extending frame buffer size, total frame buffer cnt %d size %d in repo, new frame buffer wanted size %d", IPP_FRAMEREPO_CNT(venc), IPP_FRAMEREPO_TOTALSZ(venc), wantedsz);
						GST_ERROR_OBJECT(venc, "IPP_RentFrameBufferFromRepo() fail when extending frame buffer size, total frame buffer cnt %d size %d in repo, new frame buffer wanted size %d", IPP_FRAMEREPO_CNT(venc), IPP_FRAMEREPO_TOTALSZ(venc), wantedsz);
						IPP_IdleFrameBuf(oldpFrame);
						ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_RENTFRAMEBUF);
						return ret;
					}
				}
				codecRet = EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, (void*)pFrame, venc->vMetaEnc_Handle);
				GST_LOG_OBJECT(venc, "push a buffer from candidate queue");
				if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
					g_warning("EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
					GST_ERROR_OBJECT(venc, "EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
					ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_PUSHFRAMEBUF);
					IPP_IdleFrameBuf(pFrame);
					return ret;
				}
			}
		}else if(codecRet == IPP_STATUS_NEED_OUTPUT_BUF) {
			GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_NEED_OUTPUT_BUF");
			pStm = IPP_RentStmBufferFromRepo(venc, venc->iWantedStmBufSz);
			if(G_UNLIKELY(pStm == NULL)) {
				g_warning("IPP_RentStmBufferFromRepo() fail, has allocated %d stream buffers, total size %d, new stream buffer wanted size %d", IPP_STMREPO_CNT(venc), IPP_STMREPO_TOTALSZ(venc), venc->iWantedStmBufSz);
				GST_ERROR_OBJECT(venc, "IPP_RentStmBufferFromRepo() fail, has allocated %d stream buffers, total size %d, new stream buffer wanted size %d", IPP_STMREPO_CNT(venc), IPP_STMREPO_TOTALSZ(venc), venc->iWantedStmBufSz);
				ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_RENTSTMBUF);
				return ret;
			}else{
				codecRet = EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, (void*)pStm, venc->vMetaEnc_Handle);
				if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
					g_warning("EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
					GST_ERROR_OBJECT(venc, "EncoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
					ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_PUSHSTMBUF);
					IPP_IdleStmBuf(pStm);
					return ret;
				}
			}
		}else if(codecRet == IPP_STATUS_END_OF_PICTURE) {
			GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_PICTURE");
			for(;;) {
				ppTmp = (void**)(&pFrame);
				codecRet = EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, ppTmp, venc->vMetaEnc_Handle);
				if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
					g_warning("EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
					GST_ERROR_OBJECT(venc, "EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
					ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_POPFRAMEBUF);
					return ret;
				}
				if(pFrame == NULL) {
					break;
				}
				GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_PICTURE:  pop a pic buffer");
				IPP_IdleFrameBuf(pFrame);
			}

			GstClockTime ts;
			for(;;) {
				ppTmp = (void**)(&pStm);
				codecRet = EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, ppTmp, venc->vMetaEnc_Handle);
				if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
					g_warning("EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
					GST_ERROR_OBJECT(venc, "EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
					ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_POPSTMBUF);
					return ret;
				}
				if(pStm == NULL) {
					break;
				}
				if(G_LIKELY(pStm->nDataLen > 0)) {
					venc->iEncProducedByteCnt += pStm->nDataLen;
					venc->iEncProducedCompressedFrameCnt++;
				}else{
					g_warning("vMeta encoder produced an abnormal compressed frame whose length is %d!", pStm->nDataLen);
					break;
				}
				GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_PICTURE:  pop a stream buffer");
				ts = IPP_PopTSFromQueue(venc);

				IPP_AdjustStmSz(venc, pStm->nDataLen);

				if(not_push_stream_down == 0) {
					GstBuffer* down_buf = gst_buffer_new_and_alloc(pStm->nDataLen);
					if(G_LIKELY(down_buf != NULL)) {
						memcpy(GST_BUFFER_DATA(down_buf), pStm->pBuf, pStm->nDataLen);
						IPP_IdleStmBuf(pStm);
					}else{
						IPP_IdleStmBuf(pStm);
						assist_myecho("Allocate push down buffer fail, give up this compressed frame, continue encoding");
						g_warning("Allocate push down buffer fail, give up this compressed frame, continue encoding");
						ret |= LOOPVMETA_ERR_NEWPUSHDOWNBUF;
						continue;
					}

					//push the data to downstream
					GST_BUFFER_TIMESTAMP(down_buf) = ts;
					GST_BUFFER_DURATION(down_buf) = venc->iFixedFRateDuration;
					gst_buffer_set_caps(down_buf, GST_PAD_CAPS(venc->srcpad));
					
					GST_LOG_OBJECT(venc, "Pushdown buffer sz %d, TS %lld, DU %lld", GST_BUFFER_SIZE(down_buf), GST_BUFFER_TIMESTAMP(down_buf), GST_BUFFER_DURATION(down_buf));
					GstFlowReturn pushret = gst_pad_push(venc->srcpad, down_buf);
					if(pushret != GST_FLOW_OK) {
						GST_LOG_OBJECT(venc, "gst_pad_push fail, return %d", pushret);
						if(pushret != GST_FLOW_WRONG_STATE) {	//During pipeline state change, gst_pad_push() offen return GST_FLOW_WRONG_STATE, it's normal. Therefore, we don't print this warning.
							GST_WARNING_OBJECT(venc, "gst_pad_push fail, return %d\n", pushret);
						}
						ret |= LOOPVMETA_ERR_PUSHSTMDOWN;
					}
				}else{
					IPP_IdleStmBuf(pStm);
				}
			}
			GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_PIC:  done");
			if(bExitAtEndofpic) {
				GST_LOG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_PIC:  exit");
				return (ret | LOOPVMETA_SUCRETURN_AT_PICENCODED);
			}
		}else if(codecRet == IPP_STATUS_END_OF_STREAM) {
			GST_DEBUG_OBJECT(venc, "venc:  IPP_STATUS_END_OF_STREAM");
			if(bStopCodec == 0) {
				g_warning("EncodeFrame_Vmeta return IPP_STATUS_END_OF_STREAM, but not expected!");
				GST_ERROR_OBJECT(venc, "EncodeFrame_Vmeta return IPP_STATUS_END_OF_STREAM, but not expected!");
				ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_RETUNWANTEDEOS);
				return ret;
			}else{
				for(;;) {
					ppTmp = (void**)(&pFrame);
					codecRet = EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, ppTmp, venc->vMetaEnc_Handle);
					if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
						g_warning("EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
						GST_ERROR_OBJECT(venc, "EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC,...) fail, return %d", codecRet);
						ret |= LOOPVMETA_ERR_VMETAAPI_POPFRAMEBUF;
						continue;
					}
					if(pFrame == NULL) {
						break;
					}
					IPP_IdleFrameBuf(pFrame);
				}

				for(;;) {
					ppTmp = (void**)(&pStm);
					codecRet = EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, ppTmp, venc->vMetaEnc_Handle);
					if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
						g_warning("EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
						GST_ERROR_OBJECT(venc, "EncoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM,...) fail, return %d", codecRet);
						ret |= LOOPVMETA_ERR_VMETAAPI_POPSTMBUF;
						continue;
					}
					if(pStm == NULL) {
						break;
					}
					IPP_IdleStmBuf(pStm);
				}
				return (ret | LOOPVMETA_SUCRETURN_AT_EOS);		//exit loop
			}
		}else if(codecRet == IPP_STATUS_RETURN_INPUT_BUF) {
			assist_myecho("EncodeFrame_Vmeta() return IPP_STATUS_RETURN_INPUT_BUF, don't handle this according to vmeta encoder sample code!!!\n");
		}else if(codecRet == IPP_STATUS_OUTPUT_DATA) {
			GST_LOG_OBJECT(venc, "EncodeFrame_Vmeta() return IPP_STATUS_OUTPUT_DATA, don't handle this according to vmeta encoder sample code!!!");
		}else{
			g_warning("EncodeFrame_Vmeta() return unwanted code %d, encoding fail!", codecRet);
			GST_ERROR_OBJECT(venc, "EncodeFrame_Vmeta() return unwanted code %d, encoding fail!", codecRet);
			ret |= (LOOPVMETA_ERR_FATAL | LOOPVMETA_ERR_VMETAAPI_RETUNWANTED);
			return ret;
		}
	}

	return ret;
}

static int
IPP_PrepareFrameTSFromFileSrc(GstVmetaEnc *venc, GstBuffer* buf)
{
	int iSrcLen = GST_BUFFER_SIZE(buf);
	unsigned char* p = GST_BUFFER_DATA(buf);
	int copylen;
	int framebufsz = venc->i1SrcFrameDataLen > (int)(venc->vMetaEnc_Info.dis_buf_size) ? venc->i1SrcFrameDataLen : (int)(venc->vMetaEnc_Info.dis_buf_size);
	while(iSrcLen > 0) {
		if(venc->FrameBufferForFileSrc == NULL) {
			venc->FrameBufferForFileSrc = IPP_RentFrameBufferFromRepo(venc, framebufsz);
			if(venc->FrameBufferForFileSrc == NULL) {
				g_warning("IPP_RentFrameBufferFromRepo() fail when copy data from filesrc, total frame cnt %d sz %d in repo, new frame wanted sz %d", IPP_FRAMEREPO_CNT(venc), IPP_FRAMEREPO_TOTALSZ(venc), framebufsz);
				return -1;
			}
		}
		copylen = venc->i1SrcFrameDataLen - venc->FrameBufferForFileSrc->nDataLen;
		if(iSrcLen < copylen) {
			copylen = iSrcLen;
		}
		memcpy(venc->FrameBufferForFileSrc->pBuf + venc->FrameBufferForFileSrc->nDataLen, p, copylen);
		p += copylen;
		venc->FrameBufferForFileSrc->nDataLen += copylen;
		iSrcLen -= copylen;
		if((int)(venc->FrameBufferForFileSrc->nDataLen) == venc->i1SrcFrameDataLen) {
			assist_myechomore("gather a frame from streaming input, frame data length is %d\n", venc->FrameBufferForFileSrc->nDataLen);
			IPP_PushFrameToQueue(venc, venc->FrameBufferForFileSrc);
			venc->FrameBufferForFileSrc = NULL;
			IPP_PushTSToQueue(venc, venc->AotoIncTimestamp);
			venc->AotoIncTimestamp += venc->iFixedFRateDuration;
		}
	}
	return 0;
}


static GstFlowReturn 
gst_vmetaenc_chain(GstPad *pad, GstBuffer *buf)
{
	GstVmetaEnc *venc = GST_VMETAENC (GST_OBJECT_PARENT (pad));
	int loopret;
	IppVmetaPicture* pFrame = NULL;
	IppCodecStatus codecRet;
	
	assist_myechomore("Input buffer sz %d, TS %lld, DU %lld\n", GST_BUFFER_SIZE(buf), GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_DURATION(buf));

	if(venc->driver_init_ret < 0) {
		gst_buffer_unref(buf);
		g_warning("vMeta driver hasn't been inited!");
		GST_ERROR_OBJECT(venc, "vMeta driver hasn't been inited!");
		goto VMETAENC_CHAIN_FATALERR;
	}

	if(venc->vMetaEnc_Par.nWidth == 0) {
		gst_buffer_unref(buf);
		g_warning("encoder parameter is still unset in chain()!");
		GST_ERROR_OBJECT(venc, "encoder parameter is still unset in chain()!");
		goto VMETAENC_CHAIN_FATALERR;
	}

	if(venc->vMetaEnc_Handle == NULL) {
		MiscGeneralCallbackTable CBTable;
		CBTable.fMemMalloc  = IPP_MemMalloc;
		CBTable.fMemFree    = IPP_MemFree;
		venc->vMetaEnc_Par.bMultiIns = venc->SupportMultiinstance == 0 ? 0 : 1;
		venc->vMetaEnc_Par.bFirstUser = venc->SupportMultiinstance != 2 ? 0 : 1;
		assist_myecho("Calling EncoderInitAlloc_Vmeta()...\n");
		codecRet = EncoderInitAlloc_Vmeta(&venc->vMetaEnc_Par, &CBTable, &venc->vMetaEnc_Handle);
		assist_myecho("EncoderInitAlloc_Vmeta() return %d\n", codecRet);
		if(G_UNLIKELY(codecRet != IPP_STATUS_NOERR)) {
			gst_buffer_unref(buf);
			g_warning("EncoderInitAlloc_Vmeta() fail, ret %d", codecRet);
			GST_ERROR_OBJECT(venc, "EncoderInitAlloc_Vmeta() fail, ret %d", codecRet);
			venc->vMetaEnc_Handle = NULL;
			goto VMETAENC_CHAIN_FATALERR;
		}

		if(venc->ForceStartTimestamp != -1) {
			venc->AotoIncTimestamp = venc->ForceStartTimestamp;
		}else if(! IPP_IsWholeFrameInputMode(venc, GST_BUFFER_SIZE(buf))) {
			venc->AotoIncTimestamp = 0;
		}
	}

	if(! IPP_IsWholeFrameInputMode(venc, GST_BUFFER_SIZE(buf))) {
		//stream input
		int ret = IPP_PrepareFrameTSFromFileSrc(venc, buf);
		gst_buffer_unref(buf);
		if(ret < 0) {
			g_warning("IPP_CopyDataFromFileSrc() fail, ret %d!", ret);
			GST_ERROR_OBJECT(venc, "IPP_CopyDataFromFileSrc() fail, ret %d!", ret);
			goto VMETAENC_CHAIN_FATALERR;
		}
	}else{
		//frame input
		if(venc->AotoIncTimestamp != GST_CLOCK_TIME_NONE) {
			IPP_PushTSToQueue(venc, venc->AotoIncTimestamp);
			venc->AotoIncTimestamp += venc->iFixedFRateDuration;
		}else{
			IPP_PushTSToQueue(venc, GST_BUFFER_TIMESTAMP(buf));
		}
		int nPhyAddr;
		int gstbuftype = IPP_DistinguishGstBufFromUpstream(venc, buf, &nPhyAddr);
		if( gstbuftype == IPPGSTBUFTYPE_DIRECTUSABLE_ENCPLUGIN_ALLOCATED ) {
#ifdef IPPGSTVMETAENC_ECHO
			((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt0++;
#endif
			pFrame = GST_VMETAENC_BUFFER_CAST(buf)->pVmetaFrame;
			IPPVMETAFRAME_ATTCHEDGSTBUF(pFrame) = (void*)buf;
			pFrame->nDataLen = venc->i1SrcFrameDataLen;
			IPP_PushFrameToQueue(venc, pFrame);
		}else if(gstbuftype == IPPGSTBUFTYPE_DIRECTUSABLE_SRCPLUGIN_ALLOCATED ) {
#ifdef IPPGSTVMETAENC_ECHO
			((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt1++;
#endif
			pFrame = g_slice_new0(IppVmetaPicture);
			pFrame->pBuf = GST_BUFFER_DATA(buf);
			pFrame->nPhyAddr = nPhyAddr;
			pFrame->nBufSize = GST_BUFFER_SIZE(buf);
			pFrame->nDataLen = venc->i1SrcFrameDataLen;
			IPPVMETAFRAME_ATTCHEDGSTBUF(pFrame) = (void*)buf;
			pFrame->pUsrData2 = (void*)1;	//indicate this IppVmetaPicture isn't from repo
			IPP_PushFrameToQueue(venc, pFrame);
		}else{
#ifdef IPPGSTVMETAENC_ECHO
			((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt2++;
#endif
			int wantedsz = venc->i1SrcFrameDataLen > (int)(venc->vMetaEnc_Info.dis_buf_size) ? venc->i1SrcFrameDataLen : (int)(venc->vMetaEnc_Info.dis_buf_size);
			pFrame = IPP_RentFrameBufferFromRepo(venc, wantedsz);
			if(pFrame) {
				memcpy(pFrame->pBuf, GST_BUFFER_DATA(buf), venc->i1SrcFrameDataLen);
				pFrame->nDataLen = venc->i1SrcFrameDataLen;
				IPP_PushFrameToQueue(venc, pFrame);
			}else{
				g_warning("IPP_RentFrameBufferFromRepo() fail when copy data from upstream frame, total frame cnt %d sz %d in repo, new frame wanted sz %d", IPP_FRAMEREPO_CNT(venc), IPP_FRAMEREPO_TOTALSZ(venc), wantedsz);
			}
			gst_buffer_unref(buf);
		}
	}
	
	if(!g_queue_is_empty(&venc->vMetaFrameCandidateQueue)) {
		//only when there is frame candidate, call vMeta API
		loopret = IPP_LoopVmeta(venc, 0, 0, (venc->HungryStrategy==0));
		if(loopret & LOOPVMETA_ERR_FATAL) {
			g_warning("Loop vMeta encoder fatal error occur!");
			goto VMETAENC_CHAIN_FATALERR;
		}
	}
	return GST_FLOW_OK;

VMETAENC_CHAIN_FATALERR:
	GST_ELEMENT_ERROR(venc, STREAM, ENCODE, (NULL), ("IPP GST vMeta encoder plug-in fatal error in _chain"));
	return GST_FLOW_ERROR;
}

static void
IPP_ExtHoldingResWatcher_on_Finalize(gpointer data)
{
	assist_myecho("IPP_ExtHoldingResWatcher_on_Finalize() is calling!\n");
	GstVmetaEnc* venc = (GstVmetaEnc*)data;
	IPP_DestroyIdelFramesInRepos(venc);
	if(venc->vMetaFrameBufRepo != NULL) {
		g_warning("Still frame buffers (%d) in repo are not idle!", IPP_FRAMEREPO_CNT(venc));
		GST_ERROR_OBJECT(venc,"Still frame buffers (%d) in repo are not idle!", IPP_FRAMEREPO_CNT(venc));
	}
	assist_myecho("call vdec_os_driver_clean()...\n");
	vdec_os_driver_clean();
	gst_object_unref(venc);	//since the os_driver is cleaned, vmetaenc could be finalized.
	assist_myecho("IPP_ExtHoldingResWatcher_on_Finalize() is finished!\n");
	return;
}


static int
vmetaenc_null2ready(GstVmetaEnc* venc)
{
	assist_myecho("calling vdec_os_driver_init()...\n");
	venc->driver_init_ret = vdec_os_driver_init();
	assist_myecho("vdec_os_driver_init() return %d\n", venc->driver_init_ret);
	if(venc->driver_init_ret < 0) {
		g_warning("vdec_os_driver_init() fail, return %d\n", venc->driver_init_ret);
		GST_ERROR_OBJECT(venc, "vdec_os_driver_init() fail, return %d\n", venc->driver_init_ret);
		return -1;
	}
	venc->ExtHoldingResWatcher = RES_HOLD_BY_EXTUSER_WATCHMAN_CREATE();
	GST_BUFFER_MALLOCDATA(venc->ExtHoldingResWatcher) = (guint8*)gst_object_ref(venc);
	assist_myecho("ref vmetaenc object for ExtHoldingResWatcher!!!\n");
	GST_BUFFER_FREE_FUNC(venc->ExtHoldingResWatcher) = (GFreeFunc)IPP_ExtHoldingResWatcher_on_Finalize;

	return 0;
}


static int
vmetaenc_ready2null(GstVmetaEnc* venc)
{
	//terminate encoder
	if(venc->vMetaEnc_Handle) {
		assist_myecho("terminate encoder handle: loop vemta to EOS when ready2null\n");
		int loopret = IPP_LoopVmeta(venc, 1, 1, 0);
		assist_myecho("terminate encoder handle: loop vemta to EOS when ready2null, it return %d\n", loopret);
		if(loopret & LOOPVMETA_ERR_FATAL) {
			g_warning("IPP_LoopVmeta(venc, 1, 1, 0) fail, ret %d", loopret);
		}
		assist_myecho("terminate encoder handle: call EncoderFree_Vmeta()\n");
		IppCodecStatus codecRet = EncoderFree_Vmeta(&(venc->vMetaEnc_Handle));
		venc->vMetaEnc_Handle = NULL;
		if(codecRet != IPP_STATUS_NOERR) {
			g_warning("EncoderFree_Vmeta() fail, return %d", codecRet);
			GST_ERROR_OBJECT(venc, "EncoderFree_Vmeta() fail, return %d", codecRet);
			return -1;
		}
	}

	//clear idle resource
	if(venc->FrameBufferForFileSrc) {
		IPP_IdleFrameBuf(venc->FrameBufferForFileSrc);
		venc->FrameBufferForFileSrc = NULL;
	}
	IPP_ClearCandidateQueus(venc);
	IPP_DestroyIdelFramesInRepos(venc);
	IPP_DestroyIdelStmsInRepos(venc);
	
	if(venc->vMetaStmBufRepo != NULL) {
		g_warning("Still stream buffers (%d) in repo are not idle!", IPP_STMREPO_CNT(venc));
		GST_ERROR_OBJECT(venc,"Still stream buffers (%d) in repo are not idle!", IPP_STMREPO_CNT(venc));
	}

	if(venc->ExtHoldingResWatcher) {
		assist_myecho("unref ExtHoldingResWatcher of vmetaenc during ready2null\n");
		RES_HOLD_BY_EXTUSER_WATCHMAN_UNREF(venc->ExtHoldingResWatcher);
		venc->ExtHoldingResWatcher = NULL;
	}

#ifdef DEBUG_PERFORMANCE
	//print information
	if(venc->pProbeData) {
		assist_myecho("vmetaenc codec closed, totally outputted %lld bytes and %d frames, input frame type counters are %d,%d,%d.\n", venc->iEncProducedByteCnt, venc->iEncProducedCompressedFrameCnt, ((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt0, ((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt1, ((VmetaProbeData*)(venc->pProbeData))->bufdistinguish_cnt2);
	}

	printf("vmetaenc pure codec outputted %d compressed frames and costed %lld microseconds!!!\n", venc->iEncProducedCompressedFrameCnt, venc->codec_time);
#endif

	return 0;
}

static GstStateChangeReturn
gst_vmetaenc_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

	assist_myecho("GST vmetaenc state change trans from %d to %d\n", transition>>3, transition&7);

	switch (transition)
	{
	case GST_STATE_CHANGE_NULL_TO_READY:
		{
			GstVmetaEnc* venc = GST_VMETAENC(element);
			int retcode = vmetaenc_null2ready(venc);
			if(retcode != 0) {
				g_warning("vmetaenc_null2ready() fail, ret %d", retcode);
				GST_ERROR_OBJECT(venc, "vmetaenc_null2ready() fail, ret %d", retcode);
				return GST_STATE_CHANGE_FAILURE;
			}
		}
		break;
	default:
		break;
	}

	ret = parent_class->change_state(element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE){
		return ret;
	}

	switch (transition)
	{
	case GST_STATE_CHANGE_READY_TO_NULL:
		{
			GstVmetaEnc* venc = GST_VMETAENC(element);
			int retcode = vmetaenc_ready2null(venc);
			if(retcode != 0) {
				g_warning("vmetaenc_ready2null() fail, ret %d", retcode);
				GST_ERROR_OBJECT(venc, "vmetaenc_ready2null() fail, ret %d", retcode);
				return GST_STATE_CHANGE_FAILURE;
			}
		}
		break;

	default:
		break;
	}

	return ret;
}

static int
set_encoderpar_streamformat(IppVmetaEncParSet* pEncPar, int plugin_stmformat)
{
	switch(plugin_stmformat) {
	case IPPGSTVMETA_STMFORMAT_H264:
		pEncPar->eOutputStrmFmt = IPP_VIDEO_STRM_FMT_H264;
		break;
	case IPPGSTVMETA_STMFORMAT_MPEG4:
		pEncPar->eOutputStrmFmt = IPP_VIDEO_STRM_FMT_MPG4;
		break;
	case IPPGSTVMETA_STMFORMAT_H263:
		pEncPar->eOutputStrmFmt = IPP_VIDEO_STRM_FMT_H263;
		break;
	case IPPGSTVMETA_STMFORMAT_MJPEG:
	case IPPGSTVMETA_STMFORMAT_JPEG:
		pEncPar->eOutputStrmFmt = IPP_VIDEO_STRM_FMT_MJPG;
		break;
	default:
		return -1;
	}
	return 0;
}

static void 
gst_vmetaenc_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GstVmetaEnc *venc = GST_VMETAENC (object);


	switch (prop_id)
	{
	case ARG_FORCE_STARTTIME:
		{
			gint64 st = g_value_get_int64(value);
			if(st<(gint64)(-1)) {
				g_warning("force-starttime %lld exceed range!", st);
			}else{
				venc->ForceStartTimestamp = st;
			}
		}
		break;

	case ARG_VIDEOFORMAT:
		{
			int format = g_value_get_int(value);
			if(! IPPGST_CHECK_VIDEO_FORMAT_IS_SUPPORTED(format) ) {
				g_warning("videoformat %d exceed range!", format);
			}else{
				venc->iVideoStmFormat = format;
				set_encoderpar_streamformat(&venc->vMetaEnc_Par, format);
			}
		}
		break;

	case ARG_TARGET_BITRATE:
		{
			int bR = g_value_get_int(value);
			if(bR <= 0) {
				g_warning("bitrate %d exceed range!", bR);
			}else{
				venc->iTargtBitRate = bR;
			}
		}
		break;

	case ARG_DISABLE_RATECTRL:
		{
			int bDisable = g_value_get_int(value);
			if(bDisable != 0 && bDisable != 1) {
				g_warning("disable-ratecontrol %d exceed range!", bDisable);
			}else{
				venc->bDisableRateControl = bDisable;
			}
		}
		break;

	case ARG_QP:
		{
			int qp = g_value_get_int(value);
			if(qp < 1) {
				g_warning("qp %d exceed range!", qp);
			}else{
				venc->iQP = qp;
			}
		}
		break;
	case ARG_ONLY_I:
		{
			int onlyI = g_value_get_int(value);
			if(onlyI != 0 && onlyI != 1) {
				g_warning("only-Iframe %d exceed range!", onlyI);
			}else{
				venc->bOnlyIFrame = onlyI;
			}
		}
		break;
	case ARG_SUPPORTMULTIINSTANCE:
		{
			int mi = g_value_get_int(value);
			if(mi != 0 && mi != 1 && mi != 2) {
				g_warning("support-multiinstance %d exceeds range!", mi);
			}else{
				venc->SupportMultiinstance = mi;
			}
		}
		break;
	case ARG_HUNGRYSTRATEGY:
		{
			int hs = g_value_get_int(value);
			if(hs != 0 && hs != 1) {
				g_warning("hungry-strategy %d exceeds range!", hs);
			}else{
				venc->HungryStrategy = hs;
			}
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	return;
}

static void 
gst_vmetaenc_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GstVmetaEnc *venc = GST_VMETAENC(object);

	switch (prop_id)
	{
	case ARG_FORCE_STARTTIME:
		g_value_set_int64(value, venc->ForceStartTimestamp);
		break;
	case ARG_VIDEOFORMAT:
		g_value_set_int(value, venc->iVideoStmFormat);
		break;
	case ARG_TARGET_BITRATE:
		g_value_set_int(value, venc->iTargtBitRate);
		break;
	case ARG_DISABLE_RATECTRL:
		g_value_set_int(value, venc->bDisableRateControl);
		break;
	case ARG_QP:
		g_value_set_int(value, venc->iQP);
		break;
	case ARG_ONLY_I:
		g_value_set_int(value, venc->bOnlyIFrame);
		break;
	case ARG_SUPPORTMULTIINSTANCE:
		g_value_set_int(value, venc->SupportMultiinstance);
		break;
	case ARG_HUNGRYSTRATEGY:
		g_value_set_int(value, venc->HungryStrategy);
		break;

#ifdef DEBUG_PERFORMANCE
	case ARG_TOTALFRAME:
		g_value_set_int(value, venc->iEncProducedCompressedFrameCnt);
		break;
	case ARG_CODECTIME:
		g_value_set_int64(value, venc->codec_time);
		break;
#endif
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
	return;
}


static void
gst_vmetaenc_finalize(GObject * object)
{
	GstVmetaEnc* venc = GST_VMETAENC(object);
	
	if(venc->pProbeData) {
		g_free(venc->pProbeData);
		venc->pProbeData = NULL;
	}

	G_OBJECT_CLASS(parent_class)->finalize(object);

#if 0
	assist_myecho("GstVmetaEnc instance(0x%x) is finalized!!!\n", (unsigned int)object);
#else
	printf("GstVmetaEnc instance(0x%x) is finalized!!!\n", (unsigned int)object);
#endif
	return;
}


static void 
gst_vmetaenc_base_init (gpointer g_klass)
{
	static GstElementDetails vmetaenc_details = {
		"vMeta Video Encoder", 
		"Codec/Encoder/Video", 
		"vMeta Video Encoder based-on IPP vMeta codec", 
		""
	};
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_klass);

	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&src_factory));
	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&sink_factory));

	gst_element_class_set_details (element_class, &vmetaenc_details);

	return;
}

static void 
gst_vmetaenc_class_init(GstVmetaEncClass *klass)
{
	GObjectClass *gobject_class  = (GObjectClass*) klass;
	GstElementClass *gstelement_class = (GstElementClass*) klass;

	gobject_class->set_property = gst_vmetaenc_set_property;
	gobject_class->get_property = gst_vmetaenc_get_property;

	g_object_class_install_property (gobject_class, ARG_FORCE_STARTTIME, \
		g_param_spec_int64 ("force-starttime", "The first frame's timestamp", \
		"Set the first compressed frame's timestamp(unit is nanoseconds). If this value is -1, adopt input frame's timestamp. Otherwise, ignore all timestamp attached on input frame buffer and produce following timestamps according to frame rate.", \
		-1/* range_MIN */, G_MAXINT64/* range_MAX */, -1/* default_INIT */, G_PARAM_READWRITE));

#if GSTVMETAENC_PLATFORM == GSTVMETAENC_PLATFORM_HASMJPEG
#define GSTVMETAENC_PROP_VFORMAT_STATEMENT_STRING	"The encoder output video format. Could be 1<h264>, 2<mpeg4>, 3<h263>, 4<jpeg>, 5<mjpeg>."
#else
#define GSTVMETAENC_PROP_VFORMAT_STATEMENT_STRING	"The encoder output video format. Could be 1<h264>, 2<mpeg4>, 3<h263>."
#endif

	g_object_class_install_property (gobject_class, ARG_VIDEOFORMAT, \
		g_param_spec_int ("videoformat", "The compressed video format", \
		GSTVMETAENC_PROP_VFORMAT_STATEMENT_STRING, \
		IPPGSTVMETA_STMFORMAT_MIN/* range_MIN */, IPPGSTVMETA_STMFORMAT_MAX/* range_MAX */, IPPGSTVMETA_STMFORMAT_H264/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_TARGET_BITRATE, \
		g_param_spec_int ("bitrate", "The bit rate of encoded stream", \
		"Only valid when disable-ratecontrol==0. The target bit rate of the encoded stream(bit/second), it's not the real encoded stream's bitrate, only wanted bitrate.", \
		1/* range_MIN */, G_MAXINT/* range_MAX */, IPPGST_DEFAULT_TARGETBITRATE/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_DISABLE_RATECTRL, \
		g_param_spec_int ("disable-ratecontrol", "disable rate control", \
		"Disable encoder rate control mechanism.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_QP, \
		g_param_spec_int ("qp", "qp of encoder", \
		"Only valid when disable-ratecontrol==1. QP of encoder.", \
		1/* range_MIN */, G_MAXINT/* range_MAX */, IPPGST_ENCODER_DEFAULTQP/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_ONLY_I, \
		g_param_spec_int ("only-Iframe", "only produce I frame", \
		"Let encoder produce only I frame.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_SUPPORTMULTIINSTANCE, \
		g_param_spec_int ("support-multiinstance", "support multiple instance", \
		"To support multiple instance usage. Don't access it unless you know what you're doing very clear. It could be 0<not support>, 1<support> or 2<support and reinit>.", \
		0/* range_MIN */, 2/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_HUNGRYSTRATEGY, \
		g_param_spec_int ("hungry-strategy", "vMeta hungry strategy", \
		"Let vMeta always hungry or not. Don't access it unless you know what you're doing very clear. It could be 0<not always let vMeta hungry>, 1<always let vMeta hungry>.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

#ifdef DEBUG_PERFORMANCE
	g_object_class_install_property (gobject_class, ARG_TOTALFRAME,
		g_param_spec_int ("totalframes", "Totalframe of compressed stream",
		"Number of total frames encoder produced. Under some case, number of downstream plug-in received frames is probably less than this value.", 0, G_MAXINT, 0, G_PARAM_READABLE));
	g_object_class_install_property (gobject_class, ARG_CODECTIME,
		g_param_spec_int64 ("codectime", "encoder spent time",
		"Total pure encoder spend system time(microsecond).", 0, G_MAXINT64, 0, G_PARAM_READABLE));
#endif

	gobject_class->finalize = gst_vmetaenc_finalize;
	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_vmetaenc_change_state);


	GST_DEBUG_CATEGORY_INIT (vmetaenc_debug, "vmetaenc", 0, "vMeta Encode Element");

	return;
}


static __inline void
gst_vmetaenc_init_members(GstVmetaEnc* venc)
{
#if defined(IPPGSTVMETAENC_ECHO) || defined(DEBUG_PERFORMANCE)
	venc->pProbeData = g_malloc0(sizeof(VmetaProbeData));
#else
	venc->pProbeData = NULL;
#endif

	venc->ExtHoldingResWatcher = NULL;

	venc->driver_init_ret = -1;

	venc->vMetaEnc_Handle = NULL;

	memset(&venc->vMetaEnc_Par, 0, sizeof(venc->vMetaEnc_Par));
	venc->vMetaEnc_Par.eInputYUVFmt = IPP_YCbCr422I;		//always should be IPP_YCbCr422I
	memset(&venc->vMetaEnc_Info, 0, sizeof(venc->vMetaEnc_Info));

	venc->vMetaStmBufRepo = NULL;
	venc->vMetaFrameBufRepo = NULL;

	g_queue_init(&venc->vMetaFrameCandidateQueue);
	g_queue_init(&venc->vMetaStmTSCandidateQueue);

	venc->AotoIncTimestamp = GST_CLOCK_TIME_NONE;

	venc->FrameBufferForFileSrc = NULL;

	venc->iEncW = 0;
	venc->iEncH = 0;
	venc->iEncFrameRate = 0;
	venc->i1SrcFrameDataLen = 0;
	venc->iWantedStmBufSz = IPPGST_DEFAULT_STEAMBUFSZMIN;
	
	venc->iFixedFRateDuration = GST_CLOCK_TIME_NONE;
	venc->iWholeFrameInputModeFlag = -1;	//-1 means undecided

	venc->ForceStartTimestamp = -1;
	venc->iVideoStmFormat = IPPGSTVMETA_STMFORMAT_H264;
	set_encoderpar_streamformat(&venc->vMetaEnc_Par, venc->iVideoStmFormat);

	venc->iTargtBitRate = IPPGST_DEFAULT_TARGETBITRATE;
	venc->bDisableRateControl = 0;
	venc->iQP = IPPGST_ENCODER_DEFAULTQP;
	venc->bOnlyIFrame = 0;

	venc->iEncProducedByteCnt = 0;
	venc->iEncProducedCompressedFrameCnt = 0;

	venc->SupportMultiinstance = 1;

	venc->HungryStrategy = 0;

#ifdef DEBUG_PERFORMANCE
	venc->codec_time = 0;
#endif

	return;
}

static GstFlowReturn
gst_vmetaenc_sinkpad_allocbuf(GstPad *pad, guint64 offset, guint size, GstCaps *caps, GstBuffer **buf)
{
	assist_myechomore("call gst_vmetaenc_sinkpad_allocbuf(), pad(0x%x), offset(%lld), size(%d)\n",(unsigned int)pad, offset, size);
	GstVmetaEnc* venc = GST_VMETAENC(GST_OBJECT_PARENT(pad));
	if(venc->driver_init_ret < 0) {
		g_warning("vMeta driver isn't inited %d when upstream require buffer, GST will allocate common buffer", venc->driver_init_ret);
		*buf = NULL;
		return GST_FLOW_OK;
	}
	int iBufSz = ((int)size) > venc->i1SrcFrameDataLen ? ((int)size) : venc->i1SrcFrameDataLen;
	if(iBufSz < (int)(venc->vMetaEnc_Info.dis_buf_size)) {
		iBufSz = venc->vMetaEnc_Info.dis_buf_size; 
	}
	IppVmetaPicture* pFrame = IPP_RentFrameBufferFromRepo(venc, iBufSz);
	if(pFrame == NULL) {
		g_warning("Couldn't allocate frame buffer(sz %d) when upstream require buffer, GST will allocate common buffer", iBufSz);
		*buf = NULL;
		return GST_FLOW_OK;
	}
	GstVmetaEncFrameBuffer * vMetaEncBuf = gst_vMetaEncBuffer_new();
	vMetaEncBuf->pVmetaFrame = pFrame;
	vMetaEncBuf->watcher = RES_HOLD_BY_EXTUSER_WATCHMAN_REF(venc->ExtHoldingResWatcher);
	//IPPVMETAFRAME_ATTCHEDGSTBUF(pFrame) = (void*)vMetaEncBuf;
	GST_BUFFER_DATA(vMetaEncBuf) = pFrame->pBuf;
	GST_BUFFER_SIZE(vMetaEncBuf) = size;
	GST_BUFFER_OFFSET(vMetaEncBuf) = offset;
	*buf = (GstBuffer*)vMetaEncBuf;
	gst_buffer_set_caps(*buf, caps);
	return GST_FLOW_OK;
}


static gboolean
gst_vmetaenc_sinkpad_event(GstPad *pad, GstEvent *event)
{
	GstVmetaEnc* venc = GST_VMETAENC(GST_OBJECT_PARENT(pad));
	gboolean eventRet;
	assist_myechomore("receive event %d on vmetaenc sinkpad(0x%x).\n", GST_EVENT_TYPE(event), (unsigned int)pad);

	switch(GST_EVENT_TYPE (event))
	{
	case GST_EVENT_EOS:
		assist_myecho("GST_EVENT_EOS is received!\n");
		if(venc->vMetaEnc_Handle) {
			assist_myecho("call IPP_LoopVmeta(venc, 0, 1, 0) when EOS!\n");
			int loopret = IPP_LoopVmeta(venc, 0, 1, 0);
			if(loopret & LOOPVMETA_ERR_FATAL) {
				g_warning("call IPP_LoopVmeta(venc, 0, 1, 0) when EOS fail, return %d", loopret);
				GST_ERROR_OBJECT(venc, "call IPP_LoopVmeta(venc, 0, 1, 0) when EOS fail, return %d", loopret);
			}
			assist_myecho("terminate encoder handle when receiving EOS: call EncoderFree_Vmeta()\n");
			IppCodecStatus codecRet = EncoderFree_Vmeta(&(venc->vMetaEnc_Handle));
			if(codecRet != IPP_STATUS_NOERR) {
				g_warning("EncoderFree_Vmeta() fail, return %d", codecRet);
				GST_ERROR_OBJECT(venc, "EncoderFree_Vmeta() fail, return %d", codecRet);
			}
			venc->vMetaEnc_Handle = NULL;
		}

		eventRet = gst_pad_push_event(venc->srcpad, event);
		break;
	default:
		eventRet = gst_pad_event_default(pad, event);
		break;
	}

	return eventRet;
}

static void 
gst_vmetaenc_init(GstVmetaEnc *venc, GstVmetaEncClass *venc_klass)
{
	GstElementClass *klass = GST_ELEMENT_CLASS (venc_klass);

	venc->sinkpad = gst_pad_new_from_template(gst_element_class_get_pad_template (klass, "sink"), "sink");

	gst_pad_set_setcaps_function(venc->sinkpad, GST_DEBUG_FUNCPTR(gst_vmetaenc_sinkpad_setcaps));
	gst_pad_set_chain_function(venc->sinkpad, GST_DEBUG_FUNCPTR(gst_vmetaenc_chain));
	gst_pad_set_event_function(venc->sinkpad, GST_DEBUG_FUNCPTR(gst_vmetaenc_sinkpad_event));
	gst_pad_set_bufferalloc_function(venc->sinkpad, GST_DEBUG_FUNCPTR(gst_vmetaenc_sinkpad_allocbuf));

	gst_element_add_pad (GST_ELEMENT(venc), venc->sinkpad);


	venc->srcpad = gst_pad_new_from_template (gst_element_class_get_pad_template (klass, "src"), "src");

	gst_element_add_pad(GST_ELEMENT (venc), venc->srcpad);

	gst_vmetaenc_init_members(venc);

#if 0
	assist_myecho("GstVmetaEnc instance(0x%x) is inited!!!\n", (unsigned int)venc);
#else
	printf("GstVmetaEnc instance(0x%x) is inited!!!\n", (unsigned int)venc);
#endif

	return;

}


static gboolean 
plugin_init (GstPlugin *plugin)
{
	return gst_element_register (plugin, "vmetaenc", GST_RANK_MARGINAL, GST_TYPE_VMETAENC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, 
                   GST_VERSION_MINOR, 
                   "mvl_vmetaenc", 
                   "vMeta encoder based on IPP vMeta, "__DATE__, 
                   plugin_init, 
                   VERSION, 
                   "LGPL", 
                   "", 
                   "");


/* EOF */
