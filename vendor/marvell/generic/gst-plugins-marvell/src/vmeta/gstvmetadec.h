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

/* gstvmetadec.h */

#ifndef __GST_VMETADEC_H__
#define __GST_VMETADEC_H__


/*****************************************************************************/
/* IPP codec interface */
 #include "codecVC.h"
/*****************************************************************************/


#define VMETADEC_GST_ERR_GENERICFATAL   	1
#define VMETADEC_GST_ERR_LACKFRAMEBUF   	2
#define VMETADEC_GST_ERR_LACKSTREAMBUF  	4
#define VMETADEC_GST_ERR_INPUTDATACORRUPT   8
#define VMETADEC_GST_ERR_OSRESINITFAIL  	0x80000000

#define GSTVMETA_PLATFORM_DOVE  			0
#define GSTVMETA_PLATFORM_MMP2  			1
#define GSTVMETA_PLATFORM_MG1   			2
#define GSTVMETA_PLATFORM   				GSTVMETA_PLATFORM_MG1


//currently only MG1 gingerbread platform support this feature
#define PROFILER_EN
//#define POWER_OPT_DEBUG

#define VMETA_MAX_FRM_TIME_CNT  		60
#define DEFAULT_FRM_TIME_WIN_SIZE   	12
#define ENABLE_ADVANAVSYNC_1080P		0x1
#define ENABLE_ADVANAVSYNC_720P 		0x2
#define ENABLE_ADVANAVSYNC_480  		0x3
#define ENABLE_ADVANAVSYNC_SMALL		0x4
#define D_SKIP  						0x0
#define D_CPUFREQ   					0x1
#define VMETA_MAX_OP					15
#define VMETA_MIN_OP					0

#include <gst/gst.h>


G_BEGIN_DECLS

#define GST_TYPE_VMETADEC   (gst_vmetadec_get_type())
#define GST_VMETADEC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VMETADEC,Gstvmetadec))
#define GST_VMETADEC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VMETADEC,GstvmetadecClass))
#define GST_IS_VMETADEC(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VMETADEC))
#define GST_IS_VMETADEC_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VMETADEC))


typedef struct _Gstvmetadec Gstvmetadec;
typedef struct _GstvmetadecClass GstvmetadecClass;

typedef void*   								OSFRAMERES_WATCHMAN;
#define OSFRAMERES_WATCHMAN_CREATE()			((OSFRAMERES_WATCHMAN)gst_buffer_new())
#define OSFRAMERES_WATCHMAN_REF(finalizer)  	gst_buffer_ref(finalizer)
#define OSFRAMERES_WATCHMAN_UNREF(finalizer)	gst_buffer_unref(finalizer)

typedef struct _VmetaDecFrameRepo {
	int Count;
	IppVmetaPicture* pEntryNode;	//use circle list to implement this data collection
	GMutex* RepoMtx;				//use to prevent rent_frame_from_repo()/frames_repo_recycle() conflict with sink plug-in "return" frame buffer
	int NotNeedFMemAnyMore;
	int TotalSz;
}VmetaDecFrameRepo;

#define STREAM_VDECBUF_SIZE 	(128*1024)  //must equal to or greater than 64k and multiple of 128, because of vMeta limitted
#define STREAM_VDECBUF_NUM  	32

typedef struct _VmetaDecStreamRepo {
	int Count;
	IppVmetaBitstream   Array0[STREAM_VDECBUF_NUM]; //use array to implement this data collection
	IppVmetaBitstream   Array1[STREAM_VDECBUF_NUM]; //use array to implement this data collection
	IppVmetaBitstream*  Array;
	IppVmetaBitstream   ArrayCodec[STREAM_VDECBUF_NUM];
	int nWaitFillCandidateIdx;
	int nNotPushedStreamIdx;
	int nPushReadyCnt;
	int TotalBufSz;
	int TotalSpareSz;
}VmetaDecStreamRepo;

typedef struct _VmetaDecDyncStreamRepo {
	int Count;
	IppVmetaBitstream*  pEntryNode;
	int TotalBufSz;
	IppVmetaBitstream*  pPushCandidate;
}VmetaDecDyncStreamRepo;

typedef struct _VmetaTSManager {
	GSList* 		TSDUList;
	int 			iTDListLen;
	int 			iTSDelayLimit;
	int 			mode;   	//0: input&output TS sync mode; 1: output TS auto increase mode
	int 			bHavePopTs; //1 means the value in TsLatestOut had been used, then TsLatestOut is real lastest output TS
	GstClockTime	TsLatestOut;
	GstClockTime	DurationLatestOut;
	GstClockTime	TsLatestIn;
	guint64 		FrameFixDuration;
	gint64  		DiffTS_Criterion;   // 5/8 of one frame duration, it's used to judge whether the TS is belong to one frame or different frame
	GstClockTime	inputSampeleDuration;
	gint64  		SegmentEndTs;
	gint64  		SegmentStartTs;
}VmetaTSManager;

typedef struct _VmetaDecH264Obj {
	int 			StreamLayout;
	int 			nal_length_size;
	int 			bSeqHdrReceivedForNonAVCC;
}VmetaDecH264Obj;

typedef struct _VmetaDecMPEG2Obj {
	GstBuffer*  	LeftData;
	int 			bSeqHdrReceived;
	int 			bSeekForPicEnd;
	int 			iNorminalBitRate;
	int 			iNorminalW;
	int 			iNorminalH;
}VmetaDecMPEG2Obj;

typedef struct _VmetaDecMPEG4Obj {
	int 			iTimeIncBits;
	int 			low_delay;  //-1:undecided, 0:probably has BVOP, 1:no BVOP
	int 			CodecSpecies;   //-1:undecided, 0:common mpeg4(no packed bitstream), 1: divx version 4(divx version 4 has no BVOP, therefore no packed bitstream), 2: divx version 5(divx version 5 probably has BVOP, therefore probably packed bitstream),	3:	xvid(xvid	probably	has	BVOP,	therefore	probably	packed	bitstream)
	int			bIsDivx; /* Marvell patch : simplicity 449764 */
}VmetaDecMPEG4Obj;


typedef struct _VmetaDecVC1MPObj {
	int 			iContentWidth;
	int 			iContentHeight;
}VmetaDecVC1MPObj;

typedef struct {
	long int tv_sec;
	long int tv_usec;
}local_time_t;

typedef struct _TSDUPair {
	GstClockTime	TS;
	GstClockTime	DU;
}TSDUPair;

typedef enum
{
	VMETA_LESS_VGA = 5, //because For VSmall size, Vmeta is too quick for this , fps >1XX ~ 2XX ,then target fps or more lower;
	VMETA_VGA      = 5,
	VMETA_480p     = 7,
	VMETA_720p     = 10,
	VMETA_1080p    = 7,
} VSIZE_ENUM;

typedef struct _FPSWin {
	gboolean	bAdvanSync;
	int 		nFrmDecTimeCnt;
	int 		nFrmDecTime[VMETA_MAX_FRM_TIME_CNT];
	int 		nFrmDecTimeWinSize;
	int 		nThreOffset;
	int 		nPrevFps;
	uint		nExtAdvanSync;
	VSIZE_ENUM  nVSize;
}FPSWin;

#if 0
typedef struct _FPSWinPolicy{
	int 		nFpsTargetIndex;
	FPSWin  	sFpsTargetWin;
}FPSWinPolicy;
#endif

struct _Gstvmetadec
{
	GstElement  				element;
	GstPad  					*sinkpad;
	GstPad  					*srcpad;

	OSFRAMERES_WATCHMAN 		osframeres_finalizer;
	VmetaDecStreamRepo  		StmRepo;
	VmetaDecDyncStreamRepo  	DyncStmRepo;
	VmetaDecFrameRepo   		FrameRepo;
	GQueue  					OutFrameQueue;

	IppVmetaDecParSet   		VDecParSet;
	IppVmetaDecInfo 			VDecInfo;
	void						*pVDecoderObj;

	IppVideoStreamFormat		StmCategory;
	int (*pfun_digest_inbuf)(Gstvmetadec *vmetadec, GstBuffer* buf);

	MiscGeneralCallbackTable	*pCbTable;

	gchar*  					ChkSumFileName;
	FILE*   					ChkSumFile;

	int 						bVmetadecRetAtWaitEvent;

	int 						iFrameContentWidth; //in pixel
	int 						iFrameContentHeight;
	int 						iFrameCompactSize;  //in byte
	int 						iFrameStride;   //in byte
	IppiRect					vMetaDecFrameROI;
	gint						iFRateNum;
	gint						iFRateDen;

	GstBuffer*  				gbuf_cdata;

	int 						DecErrOccured;

	VmetaDecH264Obj 			DecH264Obj;
	VmetaDecMPEG2Obj			DecMPEG2Obj;
	VmetaDecVC1MPObj			DecVC1MPObj;
	VmetaDecMPEG4Obj			DecMPEG4Obj;

	int 						bAppendStuffbytes;

	int 						iSegmentSerialNum;
	VmetaTSManager  			TSDUManager;
	int 						UpAdjacentNonQueueEle;  //ignore queue element
	int 						DownAdjacentNonQueueEle;	//ignore queue element

	int 						bNotDispFrameInNewSeg;

	int 						DisableMpeg2Packing;
	int 						NotDispPBBeforeI_MPEG2;
	int 						KeepDecFrameLayout;

	int 						iInFrameCumulated;
	int 						iInFrameCumulated2;

	int 						bStoppedCodecInEos;
	int 						bVMetaIsInDream;	//it must be protected by DecMutex
	GMutex* 					DecMutex;

	int 						iNewSegPushedFrameCnt;

	int 						SupportMultiinstance;
	int 						HungryStrategy;
	int 						CodecReorder;
	int 						CumulateThreshold;
	int 						StmBufCacheable;
	int 						EnableVmetaSleepInPause;
	int 						bJudgeFpsForFieldH264;
	int 						PushDummyFrame;

	int 						iWidth_InSinkCap;   //it's a blurry description of video resolution
	int 						iHeight_InSinkCap;

	int 						totalFrames;	//only count the decoded frame for all streams
	gint64  					codec_time;

	gpointer					pProbeData;
	gint						bPlaying;

	gboolean					ProfilerEn;
#ifdef PROFILER_EN
	gboolean					bAdvanAVSync;
	unsigned long long  		nCurTime;
	unsigned long long  		nPreTime;
	int 						nFrameRate; //videoSourceFrameRate
	int 						nSkippedFrames;
	IppVmetaFastMode			eFastMode;
	int 						nPrevSkippedFrames;
	gboolean					bFieldStrm;
	FPSWin  					sFpsWin[2];
	signed int  				nTargeFps;
	signed int  				nCurOP;
	gboolean					TagFps_ctl;
#endif
};

struct _GstvmetadecClass
{
	GstElementClass parent_class;
};

G_END_DECLS


#endif

/* EOF */
