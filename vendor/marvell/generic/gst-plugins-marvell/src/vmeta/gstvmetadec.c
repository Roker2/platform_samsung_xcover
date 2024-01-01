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

/* gstvmetadec.c */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h> //to include memcpy, memset()
#include <strings.h>	//to include strcasecmp
#include <unistd.h> //to include usleep()
#include <sys/time.h>
#include <time.h>

#include "gstvmetadec.h"
#include "vdec_os_api.h"
#include "misc.h"
#include "ippGSTdefs.h"
#include "ippGST_sideinfo.h"

#if 0
#define assist_myecho(...)  printf(__VA_ARGS__)
#else
#define assist_myecho(...)
#endif

#if 0
#define assist_myechomore(...)  printf(__VA_ARGS__)
#else
#define assist_myechomore(...)
#endif

#if !defined(PROFILER_EN) && defined(POWER_OPT_DEBUG)
#undef POWER_OPT_DEBUG  //if no def PROFILER_EN, should not define POWER_OPT_DEBUG
#endif

//Current profiler using itself's fps timing measure method.
//If you want to using unify time measurement in this file
//Please #define MEASURE_CODEC_SYST_FORPUSH

//#define DEBUG_LOG_SOMEINFO
//#define MEASURE_CODEC_THREADT 	//to measure the vMeta codec API cost cpu thread time
//#define GATHER_GSTVMETA_TIME  	//to measure the system/process/thread time of vMeta GST plug-in whole life

//#define MEASURE_WAITPUSH_SYST //to measure the time cost on waiting next push coming

//#define MEASURE_CHAIN_SYST		//to measure time cost in _chain() function
#if defined MEASURE_CHAIN_SYST && ! defined MEASURE_WAITPUSH_SYST   //MEASURE_CHAIN_SYST is depended on MEASURE_WAITPUSH_SYST
#define MEASURE_WAITPUSH_SYST
#endif

//#define MEASURE_CODEC_SYST_FORPUSH	//to measure time cost in vMeta codec API

//#define MEASURE_CODEC_SYST_FORDECOUT  //to measure time cost in vMeta codec API and use division, it's not very high efficient
#if defined MEASURE_CODEC_SYST_FORDECOUT && ! defined MEASURE_CODEC_SYST_FORPUSH	//MEASURE_CODEC_SYST_FORDECOUT is depended on MEASURE_CODEC_SYST_FORPUSH
#define MEASURE_CODEC_SYST_FORPUSH
#endif


#if defined MEASURE_CODEC_THREADT || defined GATHER_GSTVMETA_TIME
#include <sys/syscall.h>	//to support struct rusage
#include <sys/resource.h>
#endif

typedef struct{
	//for DEBUG_LOG_SOMEINFO
	int _MaxFrameCnt;
	int _VDECedFrameCnt;
	int _VDECPushedFrameCnt;
	int _AskFrameFailCnt;
	int _MaxDStmCnt;
	int _MaxDStmBufTSz;

#ifdef MEASURE_CODEC_THREADT
	struct rusage codecThrClk0;
	struct rusage codecThrClk1;
	long long codec_cputime;	//in usec
#endif

#ifdef GATHER_GSTVMETA_TIME
	struct timeval sysClk0;
	struct timeval sysClk1;
	struct timespec proClk0;
	struct timespec proClk1;
	struct rusage thrClk0;
	struct rusage thrClk1;
#endif

#ifdef MEASURE_WAITPUSH_SYST
	struct timeval pushClk0;
	struct timeval pushClk1;
	int waitpush_t_for1frame;   //in usec, store the interval between gst_pad_push finish moment and next gst_pad_push begin moment
#endif

#ifdef MEASURE_CHAIN_SYST
	struct timeval chainClk0;
	struct timeval chainClk1;
	long long chain_tick;   	//in usec
	long long chain_tick_last;
	int t_pushframes_in1chain;
	int chain_t_in1chain;   	//store last _chain() calling cost time, excluding gst_pad_push
	int chain_t_for1frame;  	//store the increased chain_tick time between twice gst_pad_push calling.
#endif

#ifdef MEASURE_CODEC_SYST_FORPUSH
	long long codec_tick;   	//in usec
	long long codec_tick_last;
	int codec_t_forPush1frame;
#endif

#ifdef MEASURE_CODEC_SYST_FORDECOUT 	//define it will use division, it's not very high efficient
	long long codec_tick_snapshot;
	int codec_t_forDecOut1frame;		//if vMeta codec API always output 1 frame each time, codec_t_forDecOut1frame will be equal to codec_t_forPush1frame
#endif
}VmetaDecProbeData;

#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD   1   	//Our toolchain's linux header may be not the latest. So define it by our self. From linux 2.6.26, the kernel should support RUSAGE_THREAD to get thread ticket
#endif

static __inline long long diff_systemclock(struct timeval* pStart, struct timeval* pStop)   //in usec
{
	long long interval = (pStop->tv_sec - pStart->tv_sec) * (long long)1000000 + (pStop->tv_usec - pStart->tv_usec);
	return interval;
}

static __inline int diff_systemclock_short(struct timeval* pStart, struct timeval* pStop)   //in usec, just for interval which is shorter than 0.5 hour
{
	int interval = (pStop->tv_sec - pStart->tv_sec) * 1000000 + (pStop->tv_usec - pStart->tv_usec);
	return interval;
}

static __inline long long diff_processclock(struct timespec* pStart, struct timespec* pStop)	//in usec
{
	long long interval = (pStop->tv_sec - pStart->tv_sec) * (long long)1000000 + (pStop->tv_nsec - pStart->tv_nsec)/1000;   //because exist division, it's better not to call this function too much times
	return interval;
}

static __inline int diff_processclock_short(struct timespec* pStart, struct timespec* pStop)	//in usec, just for interval which is shorter than 0.5 hour
{
	int interval = (pStop->tv_sec - pStart->tv_sec) * 1000000 + (pStop->tv_nsec - pStart->tv_nsec)/1000;	//because exist division, it's better not to call this function too much times
	return interval;
}


#if defined MEASURE_CODEC_THREADT || defined GATHER_GSTVMETA_TIME
static __inline long long diff_threadclock(struct rusage * pStart, struct rusage* pStop)	//in usec
{
	long long interval = (pStop->ru_utime.tv_sec - pStart->ru_utime.tv_sec + pStop->ru_stime.tv_sec - pStart->ru_stime.tv_sec)*(long long)1000000 + (pStop->ru_utime.tv_usec - pStart->ru_utime.tv_usec + pStop->ru_stime.tv_usec - pStart->ru_stime.tv_usec);
	return interval;
}

static __inline int diff_threadclock_short(struct rusage * pStart, struct rusage* pStop)	//in usec, just for interval which is shorter than 0.5 hour
{
	int interval = (pStop->ru_utime.tv_sec - pStart->ru_utime.tv_sec + pStop->ru_stime.tv_sec - pStart->ru_stime.tv_sec)*1000000 + (pStop->ru_utime.tv_usec - pStart->ru_utime.tv_usec + pStop->ru_stime.tv_usec - pStart->ru_stime.tv_usec);
	return interval;
}
#endif

GST_DEBUG_CATEGORY_STATIC (vmetadec_debug);
#define GST_CAT_DEFAULT vmetadec_debug


static __inline void measure_pushtime0(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_WAITPUSH_SYST
	gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk0, NULL);
	if(vmetadec->iNewSegPushedFrameCnt > 1) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->waitpush_t_for1frame = diff_systemclock_short(&((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk1, &((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk0);
	}
#endif
	return;
}

static __inline void measure_pushtime1(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_WAITPUSH_SYST
	gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk1, NULL);
#endif
#ifdef MEASURE_CHAIN_SYST
	((VmetaDecProbeData*)vmetadec->pProbeData)->t_pushframes_in1chain += diff_systemclock_short(&((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->pushClk1);
#endif
	return;
}

static __inline void measure_chaintime0(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CHAIN_SYST
	gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->chainClk0, NULL);
	((VmetaDecProbeData*)vmetadec->pProbeData)->t_pushframes_in1chain = 0;  //clear the gst_pad_push cost time in _chain()
#endif
	return;
}

static __inline void measure_chaintime1(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CHAIN_SYST
	gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->chainClk1, NULL);
	//chain time = chain stop clock - chain begin clock - push cost time
	((VmetaDecProbeData*)vmetadec->pProbeData)->chain_t_in1chain = diff_systemclock_short(&((VmetaDecProbeData*)vmetadec->pProbeData)->chainClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->chainClk1) - ((VmetaDecProbeData*)vmetadec->pProbeData)->t_pushframes_in1chain;
	((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick += ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_t_in1chain;
#endif
	return;
}

static __inline void measure_chaintime_for1frame(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CHAIN_SYST
	if( ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick_last != 0 ) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->chain_t_for1frame = ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick - ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick_last;
	}
	((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick_last = ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick;
#endif
	return;
}

static __inline void measure_codectime0(struct timeval * pclk0)
{
#if defined DEBUG_PERFORMANCE || defined MEASURE_CODEC_SYST_FORPUSH
	gettimeofday(pclk0, NULL);
#endif
	return;
}

static __inline void measure_codectime1(Gstvmetadec* vmetadec, struct timeval * pclk0, struct timeval * pclk1)
{
#if defined DEBUG_PERFORMANCE || defined MEASURE_CODEC_SYST_FORPUSH
	int diff;
	gettimeofday(pclk1, NULL);
	diff = diff_systemclock_short(pclk0, pclk1);
	vmetadec->codec_time += diff;
#ifdef MEASURE_CODEC_SYST_FORPUSH
	((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick += diff;
#endif
#endif
	return;
}

static __inline void measure_codectime_push1frame(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CODEC_SYST_FORPUSH
	if( ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_last != 0 ) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->codec_t_forPush1frame = ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick - ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_last;
	}
	((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_last = ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick;
#endif
	return;
}

static __inline void measure_codectime_decout1frame(Gstvmetadec* vmetadec, int framecnt)
{
#ifdef MEASURE_CODEC_SYST_FORDECOUT
	if(framecnt > 0) {
		if( ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_snapshot != 0 ) {
			((VmetaDecProbeData*)vmetadec->pProbeData)->codec_t_forDecOut1frame = (int)(((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick - ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_snapshot)/framecnt; //integer division isn't very high efficient
		}
		((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick_snapshot = ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_tick;
	}
#endif
	return;
}

static __inline int get_waitpushtime_for1frame(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_WAITPUSH_SYST
	if(vmetadec->iNewSegPushedFrameCnt > 5 && vmetadec->bPlaying == 1) {
		return ((VmetaDecProbeData*)vmetadec->pProbeData)->waitpush_t_for1frame;
	}else{
		//at the beginning stage of new segment, the time measurement probably isn't accurate
		return 0;
	}
#else
	return 0;
#endif
}

static __inline int get_chaintime_for1frame(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CHAIN_SYST
	if(vmetadec->iNewSegPushedFrameCnt > 5 && vmetadec->bPlaying == 1) {
		return ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_t_for1frame;
	}else{
		//at the beginning stage of new segment, the time measurement probably isn't accurate
		return 0;
	}
#else
	return 0;
#endif
}

static __inline int get_codectime_push1frame(Gstvmetadec* vmetadec)
{
#if defined MEASURE_CODEC_SYST_FORPUSH
	if(vmetadec->iNewSegPushedFrameCnt > 5) {
		return ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_t_forPush1frame;
	}else{
		//at the beginning stage of new segment, the time measurement probably isn't accurate
		return 0;
	}
#else
	return 0;
#endif
}

static __inline int get_codectime_decout1frame(Gstvmetadec* vmetadec)
{
#ifdef MEASURE_CODEC_SYST_FORDECOUT
	if(vmetadec->iNewSegPushedFrameCnt > 2) {
		return ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_t_forDecOut1frame;
	}else{
		//at the beginning stage of new segment, the time measurement probably isn't accurate
		return 0;
	}
#else
	return 0;
#endif
}

//static void* myfp=0;
//static void* myfp2=0;
//static void* myfp3=0;

enum {

	/* fill above with user defined signals */
	LAST_SIGNAL
};

enum {
	ARG_0,
	/* fill below with user defined arguments */
	ARG_CHECKSUMFILE,
	ARG_DISABLEMPEG2PACKING,
	ARG_NOTDISPBEFOREI_MPEG2,
	ARG_KEEPDECFRAMELAYOUT,
	ARG_SUPPORTMULTIINSTANCE,
	ARG_HUNGRYSTRATEGY,
	ARG_CODECREORDER,
	ARG_CUMULATETHRESHOLD,
	ARG_STMBUFCACHEABLE,
	ARG_ENABLEVMETASLEEPWHENPAUSE,
	ARG_JUDGEFPSFORFIELDH264,
	ARG_PUSHDUMMYFRAME,

#ifdef DEBUG_PERFORMANCE
	ARG_TOTALFRAME,
	ARG_CODECTIME,
#endif
};

#if GSTVMETA_PLATFORM == GSTVMETA_PLATFORM_DOVE
static GstStaticPadTemplate sink_factory =
	GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
	GST_STATIC_CAPS ("video/x-h264, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/mpeg, mpegversion = (int) {2,4}, systemstream = (boolean) FALSE, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-divx, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]" /* Marvell patch : simplicity 449764*/
					";video/x-xvid, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-wmv, wmvversion = (int) 3, format=(fourcc){WVC1,WMV3}, fourcc=(fourcc){WVC1,WMV3}, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";image/jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ]"
					/*";video/x-h263, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"*/));  //dove vMeta not support h263
#else
static GstStaticPadTemplate sink_factory =
	GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
	GST_STATIC_CAPS ("video/x-h264, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/mpeg, mpegversion = (int) {2,4}, systemstream = (boolean) FALSE, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-divx, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]" /* Marvell patch : simplicity 449764*/
					";video/x-xvid, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-wmv, wmvversion = (int) 3, format=(fourcc){WVC1,WMV3}, fourcc=(fourcc){WVC1,WMV3}, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";video/x-jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"
					";image/jpeg, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ]"
					";video/x-h263, width = (int) [ 16, 2048 ], height = (int) [ 16, 2048 ], framerate = (fraction) [ 0, MAX ]"));  //dove vMeta not support h263
#endif

static GstStaticPadTemplate src_factory =
	GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
	GST_STATIC_CAPS ("video/x-raw-yuv,"
					 "format = (fourcc) UYVY, "
					 "width = (int) [ 16, 2048 ], "
					 "height = (int) [ 16, 2048 ], "
					 "framerate = (fraction) [ 0, MAX ]"));


GST_BOILERPLATE (Gstvmetadec, gst_vmetadec, GstElement, GST_TYPE_ELEMENT);


static const gint mpeg2fpss[][2] = {
{0, 1},
{24000, 1001},
{24, 1},
{25, 1},
{30000, 1001},
{30, 1},
{50, 1},
{60000, 1001},
{60, 1}
};

/*this eospad is expired 2010.07.01
static const unsigned char eospad[] = {
				0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x04, 0xff,
				0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};
*/
static const unsigned char eospad[] = {
				0x00, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x04,
				0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};


#define VMETA_FRAME_CODEDTYPE_ENABLED   		//codec provide coded_type information to indicate the frame type in codecVC.h

#define H264STM_LAYOUT_SYNCCODENAL  	0
#define H264STM_LAYOUT_AVCC 			1

static const unsigned char H264NALSyncCode[4] = {0,0,0,1};
static const unsigned char VC1FrameStartCode[4] = {0,0,1,0xd};
static const unsigned char VC1EndSeqStartCode[4] = {0,0,1,0xa};

//#define BIG_CONTINUOUS_STREAMBUF
#ifdef BIG_CONTINUOUS_STREAMBUF
#error "BIG_CONTINUOUS_STREAMBUF based an assumption: the decoder contain the stream buffer in FIFO order. Now(2010.09.16), this rule is broken by decoder update, therefore, BIG_CONTINUOUS_STREAMBUF shouldn't be valid any more"
#endif

//#define VPROINTERNALLIMIT_DOMORECHECK
//#define VDECLOOP_DOMORECHECK
//#define GETVDECGST_TIMESPEND

//#define ENABLE_VDEC_NONBLOCK_CALLING  //this macro should not be set from 2010.08.27
//#define NOTEXITLOOP_FOR_FRAMESCOMPLETE	//this macro should not be set from 2010.08.27
#define NOTEXITLOOP_FOR_NEEDOUTBUF
#define NOTEXITLOOP_FOR_NEEDOUTBUF
#define NOTEXITLOOP_FOR_NEWSEQ

//#define VMETAAUTOFILL_EOS
//#define DISABLE_FAKEFRAME

#define FRAMEBUF_INREPO_CNT_THRESHOLD   	50  			//if too much frame buffer in Repo, not allocate memory any more but wait sink release those frame buffer
#if GSTVMETA_PLATFORM == GSTVMETA_PLATFORM_DOVE
#define FRAMEBUF_INREPO_TOTALSZ_THRESHOLD   (96*1024*1024)  //if too much frame buffer in Repo, not allocate memory any more but wait sink release those frame buffer
#else
//on mg1 and mmp, not many memory
#define FRAMEBUF_INREPO_TOTALSZ_THRESHOLD   (70*1024*1024)
#endif

#define BIGVIDEO_DIM						(1280*720)

#define WORKAROUND_FORVMETAJPEG_PARSEISSUE  	//2010.11.23 Just workaround, after fixed vMeta jpeg parsing performance issue, should remove this macro.

#define TS_MAXDELAY_COMMON  		(15+2+4)
#define TS_MAXDELAY_MPEG4   		(15+2+4)		//for each vop, we append stuffing bytes to drive vMeta. so vMeta decoding delay should be 0. 2 is for reorder delay(in theory, 1 is enough for mpeg2 reorder delay, but vMeta need 2). 4 is additional delay	for	bit	error	and	others.	Now(2010.09.16),	codec	introduce	some	other	delay.	And	non-hungry	strategy	also	introduce	delay.
#define TS_MAXDELAY_H264			(15+16+4)
#define TS_MAXDELAY_MPEG2   		(15+2+4)	//sometimes(big mpeg2 video resolution in PS), we don't append stuffing bytes after picture. so 15 is an approximate number of decoding delay.
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
#define TS_MAXDELAY_JPEG			(1024)
#endif


//#define MANUAL_FILL_ENDOFUNITPATTERN  		//manualfill has been verified, autofill hasn't been verified for long time. autofill probably has better compatibility
#ifdef MANUAL_FILL_ENDOFUNITPATTERN
#define FILL_ENDOFUNIT_PATTERN(pStmRepo, vmetadec)  	manualfill_stuffpattern_to_stream2(pStmRepo, 0x88)
#else
#define FILL_ENDOFUNIT_PATTERN(pStmRepo, vmetadec)  	autofill_endpattern_to_stream(pStmRepo, IPP_VMETA_STRM_BUF_END_OF_UNIT, vmetadec)
#endif


static __inline void
empty_framebuf_res(IppVmetaPicture* pF, VmetaDecFrameRepo* Repo)
{
	Repo->TotalSz -= pF->nBufSize;
	vdec_os_api_dma_free(pF->pBuf);
	free(pF->pUsrData2);
	pF->pBuf = NULL;
	pF->nBufSize = 0;
	return;
}

#define VMETA_PICBUF_ISIDLE(VmetaPic)   (((IppVmetaPicture*)(VmetaPic))->pUsrData0 == (void*)0)
#define VMETA_PICBUF_SETIDLE(VmetaPic)  (((IppVmetaPicture*)(VmetaPic))->pUsrData0 = (void*)0)
#define VMETA_PICBUF_SETBUSY(VmetaPic)  (((IppVmetaPicture*)(VmetaPic))->pUsrData0 = (void*)1)

#define VMETA_DYNCSTMBUF_ISIDLE(VmetaDStm)  (((IppVmetaBitstream*)(VmetaDStm))->pUsrData2 == (void*)0)
#define VMETA_DYNCSTMBUF_SETIDLE(VmetaDStm) (((IppVmetaBitstream*)(VmetaDStm))->pUsrData2 = (void*)0)
#define VMETA_DYNCSTMBUF_SETBUSY(VmetaDStm) (((IppVmetaBitstream*)(VmetaDStm))->pUsrData2 = (void*)1)

#define VMETA_STMBUF_ISUNITEND(VmetaStm)		( 0 != ((unsigned int)((IppVmetaBitstream*)(VmetaStm))->pUsrData3 & 2) )
#define VMETA_STMBUF_SETISUNITEND(VmetaStm) ( ((IppVmetaBitstream*)(VmetaStm))->pUsrData3 = (void*)((unsigned int)((IppVmetaBitstream*)(VmetaStm))->pUsrData3 | 2) )
#define VMETA_STMBUF_SETISNOTUNITEND(VmetaStm)  ( ((IppVmetaBitstream*)(VmetaStm))->pUsrData3 = (void*)((unsigned int)((IppVmetaBitstream*)(VmetaStm))->pUsrData3 & (~2)) )

#define VMETA_STMBUF_ISSTATICSTM(VmetaStm)  	(((unsigned int)(((IppVmetaBitstream*)(VmetaStm))->pUsrData3) & 1) == 0)

#ifdef PROFILER_EN
static unsigned long long TimeGetTickCountL();
static int CalFrameRate_VmetaDec(Gstvmetadec *vmetadec);
static int CheckAdvanAVSync_VmetaDec(Gstvmetadec *vmetadec);
static int CheckCPUFreqDown_VmetaDec(Gstvmetadec *vmetadec);
static int AdvanSync_VmetaDec(Gstvmetadec *vmetadec,signed long long chaintime,unsigned long long PushTimeOmit);
static int AVSyncSkipExec_VmetaDec(Gstvmetadec *vmetadec, unsigned long nAvgFps);
static int CPUFreqDownExec_VmetaDec(Gstvmetadec *vmetadec, unsigned long nAvgFps);
static unsigned long long TimeGetTickCountL()
{
	struct timeval g_tv;
	gettimeofday(&g_tv, NULL);
	return (unsigned long long)g_tv.tv_sec * 1000000 + g_tv.tv_usec;
}
static int CalFrameRate_VmetaDec(Gstvmetadec *vmetadec/*, IppVmetaPicture* pFrame*/)
{
	Gstvmetadec 		*this = (Gstvmetadec*)vmetadec;
	long long nDelta;
	VmetaTSManager* TSDUManager = &(vmetadec->TSDUManager);
	FPSWin* pFpsWin = &(vmetadec->sFpsWin[0]);

	nDelta = 0;
	if(TSDUManager->iTDListLen > 1) {
		TSDUPair* first = TSDUManager->TSDUList->data;
		TSDUPair* end = g_slist_nth_data(TSDUManager->TSDUList, TSDUManager->iTDListLen-1);
		if(end->TS != GST_CLOCK_TIME_NONE && first->TS != GST_CLOCK_TIME_NONE) {
			nDelta = end->TS - first->TS;
		}
	}

	if (0 < nDelta) {
		this->nFrameRate = ((TSDUManager->iTDListLen - 1) * (long long)1000000000 + (nDelta>>1)) / nDelta;
	} else {
		this->nFrameRate		= 0;
		(pFpsWin+D_SKIP)->nExtAdvanSync= FALSE;
		(pFpsWin+D_SKIP)->nFrmDecTimeCnt= -1;
		(pFpsWin+D_SKIP)->bAdvanSync = FALSE;
		(pFpsWin+D_CPUFREQ)->nExtAdvanSync= FALSE;
		(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt= -1;
		(pFpsWin+D_CPUFREQ)->bAdvanSync = FALSE;
		return 0;
	}
	return 0;
}
static int AVSyncSkipExec_VmetaDec(Gstvmetadec *vmetadec, unsigned long nAvgFps)
{
	Gstvmetadec 		   *this = (Gstvmetadec*)vmetadec;
	FPSWin* 			   pFpsWin = &(vmetadec->sFpsWin[0]);
	unsigned long   	   nTargeFps;
	IppCodecStatus  	   codecRt = -1;
	(pFpsWin+D_SKIP)->nFrmDecTimeCnt = 0;
	(pFpsWin+D_SKIP)->nPrevFps = nAvgFps;
	this->nPrevSkippedFrames = this->nSkippedFrames;
	this->nSkippedFrames = 0;
	nTargeFps = this->nFrameRate + 3/*(pFpsWin+D_SKIP)->nThreOffset*/;
	if (nAvgFps < nTargeFps) {
		this->eFastMode = IPP_VMETA_FASTMODE_100;
		codecRt=DecodeSendCmd_Vmeta(IPPVC_SET_FASTMODE, &(this->eFastMode), NULL, this->pVDecoderObj);
	} else if(nAvgFps > nTargeFps) {
		switch (this->eFastMode) {
			case IPP_VMETA_FASTMODE_25:
				this->eFastMode = IPP_VMETA_FASTMODE_DISABLE;
			break;
			case IPP_VMETA_FASTMODE_50:
				this->eFastMode = IPP_VMETA_FASTMODE_25;
			break;
			case IPP_VMETA_FASTMODE_75:
				this->eFastMode = IPP_VMETA_FASTMODE_50;
			break;
			case IPP_VMETA_FASTMODE_100:
				this->eFastMode = IPP_VMETA_FASTMODE_75;
			break;
			case IPP_VMETA_FASTMODE_DISABLE:
			default:
			break;
		}
		codecRt = DecodeSendCmd_Vmeta(IPPVC_SET_FASTMODE, &(this->eFastMode), NULL, this->pVDecoderObj);
	}
	if(codecRt != IPP_STATUS_NOERR) {
		g_warning("DecodeSendCmd_Vmeta(IPPVC_SET_FASTMODE) fail, ret %d\n", codecRt);
		return -2;
	}
	return 0;
}
static int CPUFreqDownExec_VmetaDec(Gstvmetadec *vmetadec,unsigned long nAvgFps)
{
	Gstvmetadec 		*this = (Gstvmetadec*)vmetadec;
	FPSWin* 			pFpsWin = &(vmetadec->sFpsWin[0]);
	IppCodecStatus  codecRt = -1;
	signed int cpufreq=0;
	(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt = 0;
	(pFpsWin+D_CPUFREQ)->nPrevFps = nAvgFps;
	if(this->TagFps_ctl){
		if(this->nFrameRate > 24){
			this->nTargeFps = this->nFrameRate+ (pFpsWin+D_CPUFREQ)->nThreOffset + (pFpsWin+D_CPUFREQ)->nVSize;
		}else{
			/*this branch means stream fps calcultate wrong*/
			this->nTargeFps = 35;
		}
#ifdef POWER_OPT_DEBUG
		{
			IPP_FILE *f;
			int Read_bound;
			f = IPP_Fopen("/data/poweropt_gst.cfg", "r");
			if (f) {
				IPP_Fscanf(f, "%d", &Read_bound);
				if (25 < Read_bound) {
					this->nTargeFps = Read_bound;
				}
				IPP_Fclose(f);
			}
		}
#endif
		this->TagFps_ctl = FALSE;
	}
	if (nAvgFps < this->nTargeFps){
		if(VMETA_MAX_OP != this->nCurOP) {
			cpufreq = 1;
		}else{
			return 0;
		}
	}
	else if (nAvgFps > this->nTargeFps + 8) {
		if(VMETA_MIN_OP != this->nCurOP) {
			cpufreq = -1;
		}else{
			return 0;
		}
	}
	else
		return 0;
	codecRt = DecodeSendCmd_Vmeta(IPPVC_SET_PERF_REQ, &cpufreq/*-99,-1,0,+1,+99*/, &(this->nCurOP), this->pVDecoderObj);
	if(codecRt != IPP_STATUS_NOERR) {
		g_warning("DecodeSendCmd_Vmeta(IPPVC_SET_PERF_REQ) fail, ret %d\n", codecRt);
		return -2;
	}
	return 0;
}
static int AdvanSync_VmetaDec(Gstvmetadec *vmetadec,signed long long chaintime,unsigned long long PushTimeOmit)
{
	Gstvmetadec 		  *this = (Gstvmetadec*)vmetadec;
	FPSWin* 			  pFpsWin = &(vmetadec->sFpsWin[0]);
	unsigned long  int    nCurFrmDecTime;
	unsigned long  int    nAvgFrmDecTime,nAvgFrmDecTimeCPU;
	unsigned long   	  nAvgFps,nAvgFpsCPU;
	int 				  i;

	//Step 1 start of time measure mechanism
	if(chaintime <= 0){
		//local 	 :time measure mechanism
		this->nCurTime = TimeGetTickCountL();
		if ((0 > (pFpsWin+D_SKIP)->nFrmDecTimeCnt)&&(0 > (pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt)) {
			this->nPreTime = this->nCurTime;
			if((pFpsWin+D_SKIP)->bAdvanSync)
				(pFpsWin+D_SKIP)->nFrmDecTimeCnt = 0;
			if((pFpsWin+D_CPUFREQ)->bAdvanSync)
				(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt = 0;
			return 0;
		}
		nCurFrmDecTime = this->nCurTime - this->nPreTime;
		this->nPreTime = this->nCurTime;
		if(PushTimeOmit){
			nCurFrmDecTime-=PushTimeOmit;
		}
	}else{
		//Whole file :time measure mechanism
		nCurFrmDecTime = chaintime ;
	}

	//Step 2 start of time window statistic for FPS
	if((pFpsWin+D_SKIP)->bAdvanSync){
		(pFpsWin+D_SKIP)->nFrmDecTime[(pFpsWin+D_SKIP)->nFrmDecTimeCnt++] = nCurFrmDecTime;
		if ((pFpsWin+D_SKIP)->nFrmDecTimeCnt < (pFpsWin+D_SKIP)->nFrmDecTimeWinSize) {
			goto CAL_CPUFREQ_WIN;
		}
		if ((pFpsWin+D_SKIP)->nFrmDecTimeCnt >= (pFpsWin+D_SKIP)->nFrmDecTimeWinSize) {
			(pFpsWin+D_SKIP)->nFrmDecTimeCnt = (pFpsWin+D_SKIP)->nFrmDecTimeWinSize;
		}
		nAvgFrmDecTime= 0;
		for (i = 0; i < (pFpsWin+D_SKIP)->nFrmDecTimeCnt; i++) {
			nAvgFrmDecTime += (pFpsWin+D_SKIP)->nFrmDecTime[i];
		}
		nAvgFrmDecTime = (nAvgFrmDecTime + ((pFpsWin+D_SKIP)->nFrmDecTimeCnt>>1)) / (pFpsWin+D_SKIP)->nFrmDecTimeCnt;
		nAvgFps = (1000000 + (nAvgFrmDecTime>>1)) / nAvgFrmDecTime;
	//Step 3 start of skip accord to FPS
		AVSyncSkipExec_VmetaDec(vmetadec,nAvgFps);
		goto CAL_CPUFREQ_WIN;
	}
CAL_CPUFREQ_WIN:

	//Step 2 start of time window statistic for CPUfreq
	if((pFpsWin+D_CPUFREQ)->bAdvanSync){
		(pFpsWin+D_CPUFREQ)->nFrmDecTime[(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt++] = nCurFrmDecTime;
		if ((pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt < (pFpsWin+D_CPUFREQ)->nFrmDecTimeWinSize) {
			return 0;
		}
		if ((pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt >= (pFpsWin+D_CPUFREQ)->nFrmDecTimeWinSize) {
			(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt = (pFpsWin+D_CPUFREQ)->nFrmDecTimeWinSize;
		}
		nAvgFrmDecTimeCPU = 0;
		for (i = 0; i < (pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt; i++) {
			nAvgFrmDecTimeCPU += (pFpsWin+D_CPUFREQ)->nFrmDecTime[i];
		}
		nAvgFrmDecTimeCPU = (nAvgFrmDecTimeCPU + ((pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt>>1)) / (pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt;
		nAvgFpsCPU = (1000000 + (nAvgFrmDecTimeCPU>>1)) / nAvgFrmDecTimeCPU;
	//Step 3 start of cpufreq control accord to FPS
		return CPUFreqDownExec_VmetaDec(vmetadec, nAvgFpsCPU);
	}
	return 0;
}
static int CheckAdvanAVSync_VmetaDec(Gstvmetadec *vmetadec)
{
	Gstvmetadec 		*this = (Gstvmetadec*)vmetadec;
	FPSWin* 			pFpsWin = &(vmetadec->sFpsWin[0]);
	//Close Skip function.
	(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
	return 0;	//currently(2011.05.30), we disabled skip function
	if (this->iFRateNum && this->iFRateDen) {
		this->nFrameRate =
			(this->iFRateNum + (this->iFRateDen>>1)) / this->iFRateDen;
	}else if(this->VDecInfo.seq_info.frame_rate_num && this->VDecInfo.seq_info.frame_rate_den) {
		this->nFrameRate =
			(this->VDecInfo.seq_info.frame_rate_num + (this->VDecInfo.seq_info.frame_rate_den>>1)) / this->VDecInfo.seq_info.frame_rate_den;
	}
	if (0 >= this->nFrameRate){
		CalFrameRate_VmetaDec(vmetadec);
		if (30 > this->nFrameRate) {
			(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
			(pFpsWin+D_SKIP)->nFrmDecTimeCnt  = -1;
			return 0;
		}
	}
	if ((pFpsWin+D_SKIP)->nExtAdvanSync) {
		if ((1920*1080) <= (this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height)) {
			if ((pFpsWin+D_SKIP)->nExtAdvanSync & ENABLE_ADVANAVSYNC_1080P) {
				if ((30 <= this->nFrameRate) || (0 == this->nFrameRate)) {
					(pFpsWin+D_SKIP)->bAdvanSync	  = TRUE;
				} else {
					(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
				}
			} else {
				(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
			}
		} else if ((1280*720) <= (this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height)) {
			if ((pFpsWin+D_SKIP)->nExtAdvanSync& ENABLE_ADVANAVSYNC_720P) {
				if ((30 <= this->nFrameRate) || (0 == this->nFrameRate)) {
					(pFpsWin+D_SKIP)->bAdvanSync	  = TRUE;
				} else {
					(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
				}
			} else {
				(pFpsWin+D_SKIP)->bAdvanSync	  = FALSE;
			}
		} else {
			(pFpsWin+D_SKIP)->bAdvanSync = FALSE;
		}
	} else {
		(pFpsWin+D_SKIP)->bAdvanSync = FALSE;
	}
	this->bFieldStrm = FALSE; /*currently we don't know whether is field stream or not, so just set it as non-field stream*/
	return 0;
}
static int CheckCPUFreqDown_VmetaDec(Gstvmetadec *vmetadec)
{
	Gstvmetadec 		*this = (Gstvmetadec*)vmetadec;
	FPSWin* 			pFpsWin = &(vmetadec->sFpsWin[0]);

	(pFpsWin+D_CPUFREQ)->bAdvanSync = TRUE;
	if (this->iFRateNum && this->iFRateDen) {
		this->nFrameRate =
			(this->iFRateNum + (this->iFRateDen>>1)) / this->iFRateDen;
	}else if(this->VDecInfo.seq_info.frame_rate_num && this->VDecInfo.seq_info.frame_rate_den) {
		this->nFrameRate =
			(this->VDecInfo.seq_info.frame_rate_num + (this->VDecInfo.seq_info.frame_rate_den>>1)) / this->VDecInfo.seq_info.frame_rate_den;
	}
	if (0 >= this->nFrameRate){
		CalFrameRate_VmetaDec(vmetadec);
		if (this->nFrameRate <= 0){
			(pFpsWin+D_CPUFREQ)->bAdvanSync = FALSE;
			return 0;
		}
	}
	if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height >= (1920*1080)){
		(pFpsWin+D_CPUFREQ)->nVSize = VMETA_1080p;
		(pFpsWin+D_CPUFREQ)->bAdvanSync = FALSE;
	}
	else if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height >= (1280*720)){
		(pFpsWin+D_CPUFREQ)->nVSize = VMETA_720p;
	}
	else if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height >= (720*480)){
		(pFpsWin+D_CPUFREQ)->nVSize = VMETA_480p;
	}
	else if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height >= (640*480)){
		(pFpsWin+D_CPUFREQ)->nVSize = VMETA_VGA;
	}
	else if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height == 0){
		(pFpsWin+D_CPUFREQ)->nVSize = 0;
		(pFpsWin+D_CPUFREQ)->bAdvanSync = FALSE;
	}
	else if(this->VDecInfo.seq_info.max_width * this->VDecInfo.seq_info.max_height > 0) {
		(pFpsWin+D_CPUFREQ)->nVSize = VMETA_LESS_VGA;
	}else{
		(pFpsWin+D_CPUFREQ)->nVSize = 0;
		(pFpsWin+D_CPUFREQ)->bAdvanSync = FALSE;
	}
#ifdef POWER_OPT_DEBUG
	{
		IPP_FILE *f;
		int nFrmDecTimeWinSize;
		f = IPP_Fopen("/data/poweropt.cfg", "r");
		if (f) {
			IPP_Fscanf(f, "%d", &nFrmDecTimeWinSize);
			if (0 < nFrmDecTimeWinSize) {
				(pFpsWin+D_CPUFREQ)->nFrmDecTimeWinSize = nFrmDecTimeWinSize;
			}
			IPP_Fclose(f);
		}
	}
#endif
	return 0;
}
#endif


static int
frames_repo_recycle(VmetaDecFrameRepo* FrameRepo)
{
	int i=0,j=0,k=0;
	IppVmetaPicture* pCurNode = FrameRepo->pEntryNode, *pNext;
	IppVmetaPicture* pNewListHead = NULL, *pNewListTail = NULL;
	for(;i<FrameRepo->Count;i++, pCurNode=pNext) {
		pNext = (IppVmetaPicture*)pCurNode->pUsrData1;

		if( VMETA_PICBUF_ISIDLE(pCurNode) ) {
			empty_framebuf_res(pCurNode, FrameRepo);
			g_free(pCurNode);
			k++;
		}else{
			if(pNewListTail != NULL) {
				pNewListTail->pUsrData1 = pCurNode;
				pNewListTail = pCurNode;
			}else{
				pNewListHead = pNewListTail = pCurNode;
			}
			j++;
		}

	}

	FrameRepo->Count = j;
	if(j != 0) {
		FrameRepo->pEntryNode = pNewListHead;
		pNewListTail->pUsrData1 = (void*)pNewListHead;
	}else{
		FrameRepo->pEntryNode = NULL;
	}
	return k;
}


static void
destroy_allframes_repo(VmetaDecFrameRepo* FrameRepo)
{
	IppVmetaPicture* pTmp, * pCurNode = FrameRepo->pEntryNode;

	for(; FrameRepo->Count != 0; FrameRepo->Count--) {
		empty_framebuf_res(pCurNode, FrameRepo);
		pTmp = pCurNode;
		pCurNode = (IppVmetaPicture*)pCurNode->pUsrData1;
		g_free(pTmp);
	}
	FrameRepo->pEntryNode = NULL;
	return;
}


static __inline int
allocate_framebuf_res(int OneFrameBufSz, IppVmetaPicture* pFrame, GstPad* pad, VmetaDecFrameRepo* Repo)
{
	unsigned char* pTmp = NULL;
	unsigned int nPhyAddr;

	pFrame->pUsrData2 = malloc(sizeof(IPPGSTDecDownBufSideInfo));
	if(pFrame->pUsrData2 == NULL) {
		return -2;
	}

	if(pad == NULL) {
		pTmp = (Ipp8u*)vdec_os_api_dma_alloc(OneFrameBufSz, VMETA_DIS_BUF_ALIGN, &nPhyAddr);
		if(G_UNLIKELY(pTmp == NULL)) {
			return -1;
		}
	}else{
		g_warning("Allocating vMeta pic buffer from downstream hasn't been implemented till now.");
		return -1;
	}

	assist_myecho("new a frame buffer, sz %d, vaddr 0x%x, paddr 0x%x\n", OneFrameBufSz, (unsigned int)pTmp, nPhyAddr);
	//printf("new a frame buffer, sz %d, vaddr 0x%x, paddr 0x%x\n", OneFrameBufSz, (unsigned int)pTmp, nPhyAddr);

	pFrame->pBuf = pTmp;
	pFrame->nPhyAddr = nPhyAddr;
	pFrame->nBufSize = OneFrameBufSz;
	pFrame->nDataLen = 0;
	pFrame->nOffset = 0;
	pFrame->nBufProp = 0;
	memset(&(pFrame->pic), 0, sizeof(pFrame->pic));

	Repo->TotalSz += OneFrameBufSz;

	return 0;

}

static IppVmetaPicture*
new_frame_in_repo(VmetaDecFrameRepo* FrameRepo, int OneFrameBufSz, GstPad* pad, Gstvmetadec* vmetadec)
{
	IppVmetaPicture* pTmpNode;

	pTmpNode = g_try_new(IppVmetaPicture,1);
	if(G_UNLIKELY(pTmpNode == NULL)) {
		g_warning("No memory to new frame node in repository.");
		return NULL;
	}

	if( 0 != allocate_framebuf_res(OneFrameBufSz, pTmpNode, pad, FrameRepo) ) {
		g_warning("Allocate frame buffer resource for new frame fail.");
		g_free(pTmpNode);
		return NULL;
	}

	VMETA_PICBUF_SETIDLE(pTmpNode);

	//add the node into repo
	if(FrameRepo->Count == 0) {
		pTmpNode->pUsrData1 = (void*)pTmpNode;
		FrameRepo->pEntryNode = pTmpNode;
	}else{
		pTmpNode->pUsrData1 = FrameRepo->pEntryNode->pUsrData1; 	//insert the new node at the downstream of pEntryNode
		FrameRepo->pEntryNode->pUsrData1 = (void*)pTmpNode;
	}

	FrameRepo->Count++;
#ifdef DEBUG_LOG_SOMEINFO
	if(FrameRepo->Count > ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxFrameCnt) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxFrameCnt = FrameRepo->Count;
	}
#endif
	return pTmpNode;
}

static IppVmetaPicture*
seek_idleframe_in_repo(VmetaDecFrameRepo* FrameRepo, int wantframebufsz)
{

	if(FrameRepo->Count != 0) {
		int i;
		IppVmetaPicture* pFNode = (IppVmetaPicture*)FrameRepo->pEntryNode->pUsrData1;   //FrameRepo->pEntryNode point to the last rented frame, so search from the next

		for(i=0; i<FrameRepo->Count; i++,pFNode = (IppVmetaPicture*)pFNode->pUsrData1) {
			if( VMETA_PICBUF_ISIDLE(pFNode) && (int)pFNode->nBufSize == wantframebufsz ) {
				return pFNode;
			}
		}
	}
	return NULL;
}

static IppVmetaPicture*
rent_frame_from_repo(VmetaDecFrameRepo* FrameRepo, int OneFrameBufSz, int* pNotTooMuchFrame, Gstvmetadec* vmetadec)
{
	IppVmetaPicture* pFNode;
	GstPad* pad = NULL; //Allocating vMeta pic buffer from downstream hasn't been implemented till now.

	if(pNotTooMuchFrame) {
		*pNotTooMuchFrame = 0;
	}
	g_mutex_lock(FrameRepo->RepoMtx);

#if 0
	if(FrameRepo->Count < 5) {
		goto FORCE_NEWFRAME_IN_REPO;
	}
#endif

	pFNode = seek_idleframe_in_repo(FrameRepo, OneFrameBufSz);
	if(pFNode != NULL) {
		FrameRepo->pEntryNode = pFNode; //let entrynode point to the rented node, it's not must to have, but better to have
		VMETA_PICBUF_SETBUSY(pFNode);   //mark this buffer is using
		goto Rent_Frame_From_Repo_Ret;
	}else{
		if(pNotTooMuchFrame != NULL && (FrameRepo->Count >= FRAMEBUF_INREPO_CNT_THRESHOLD || FrameRepo->TotalSz >= FRAMEBUF_INREPO_TOTALSZ_THRESHOLD)) {
			if(frames_repo_recycle(FrameRepo) <= 0) {
				//if too much frame buffer in repo and recycling hasn't free any memory, return NULL
				*pNotTooMuchFrame = 1;
				goto Rent_Frame_From_Repo_Ret;
			}
		}
	}


//FORCE_NEWFRAME_IN_REPO:
	//no idle node in repo, new one
	pFNode = new_frame_in_repo(FrameRepo, OneFrameBufSz, pad, vmetadec);

	//do recycle and try to new again
	if(pFNode == NULL && pad == NULL && frames_repo_recycle(FrameRepo) > 0) {
		pFNode = new_frame_in_repo(FrameRepo, OneFrameBufSz, pad, vmetadec);
	}

	if(G_LIKELY(pFNode != NULL)) {
		FrameRepo->pEntryNode = pFNode; 	//let entrynode point to the rented node, it's not must to have, but better to have
		VMETA_PICBUF_SETBUSY(pFNode);   	//mark this buffer is using
		assist_myecho("frame repo cnt %d\n",FrameRepo->Count);
	}else{
		g_warning("New frame in repository fail.");
	}

Rent_Frame_From_Repo_Ret:
	g_mutex_unlock(FrameRepo->RepoMtx);
	return pFNode;
}



static __inline void
destroy_allstreams_repo(VmetaDecStreamRepo* StmRepo)
{
#ifdef BIG_CONTINUOUS_STREAMBUF
	if(StmRepo->Count != 0) {
		vdec_os_api_dma_free(StmRepo->Array[0].pBuf);
		StmRepo->Count = 0;
	}
#else
	if(StmRepo->Count != 0) {
		int i;
		for(i=0;i<StmRepo->Count;i++) {
			vdec_os_api_dma_free(StmRepo->Array[i].pBuf);
		}
		StmRepo->Count = 0;
	}
#endif
	return;
}

//reset the stream repository state
static __inline void
reset_streamrepo_context(VmetaDecStreamRepo* StmRepo, int OneBufSz, int BufCnt, Gstvmetadec* vmetadec)
{
	int i;
	StmRepo->Count = BufCnt;
	StmRepo->nWaitFillCandidateIdx = 0;
	StmRepo->nNotPushedStreamIdx = 0;
	StmRepo->nPushReadyCnt = 0;
	StmRepo->TotalBufSz = OneBufSz*BufCnt;
	StmRepo->TotalSpareSz = StmRepo->TotalBufSz;
	for(i=0;i<BufCnt;i++) {
		StmRepo->Array0[i].nDataLen = 0;		//DecodeFrame_Vmeta() will change this member under some case, it won't change pUsrData0
		StmRepo->Array0[i].nOffset = 0;
		StmRepo->Array0[i].nBufProp = vmetadec->StmBufCacheable==0 ? 0 : VMETA_BUF_PROP_CACHEABLE;
		StmRepo->Array0[i].nFlag = 0;
		StmRepo->Array0[i].pUsrData0 = (void*)0;
		StmRepo->Array0[i].pUsrData1 = (void*)0;
		StmRepo->Array0[i].pUsrData2 = (void*)0;
		StmRepo->Array0[i].pUsrData3 = (void*)0;	//indicate it's static stream buffer, the lsb bit indicates whether it's static stream buffer
	}

	memcpy(StmRepo->Array1, StmRepo->Array0, sizeof(StmRepo->Array0));

	for(i=0;i<BufCnt;i++) {
		if(StmRepo->ArrayCodec[i].pUsrData0 != (void*)0) {
			assist_myecho("Still some static stream buffer not popepd from codec %d, 0x%x.\n", i, (unsigned int)&StmRepo->ArrayCodec[i]);
			g_warning("Still some static stream buffer not popepd from codec %d, 0x%x.", i, (unsigned int)&StmRepo->ArrayCodec[i]);
		}
	}
	memset(StmRepo->ArrayCodec, 0, sizeof(StmRepo->ArrayCodec));
	return;
}

static __inline int
alloc_streambuf_in_repo(VmetaDecStreamRepo* StmRepo, int OneBufSz, int BufCnt, Gstvmetadec* vmetadec)
{
#ifdef BIG_CONTINUOUS_STREAMBUF
	//dma allocate a big uninterrupted buffer to hold all stream buffers
	int i;
	unsigned int nPhyAddr;
	unsigned char* pTmp;
	if(vmetadec->StmBufCacheable == 0) {
		pTmp = (Ipp8u*)vdec_os_api_dma_alloc(OneBufSz*BufCnt, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
	}else{
		pTmp = (Ipp8u*)vdec_os_api_dma_alloc_cached(OneBufSz*BufCnt, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
	}
	if(pTmp == NULL) {
		return -1;
	}

	for(i=0;i<BufCnt;i++) {
		StmRepo->Array[i].pBuf = pTmp;
		StmRepo->Array[i].nPhyAddr = nPhyAddr;
		pTmp += OneBufSz;
		nPhyAddr += OneBufSz;
		StmRepo->Array[i].nBufSize = OneBufSz;
	}
#else
	//dma allocate a serious of small buffers
	int i;
	unsigned int nPhyAddr;
	for(i=0; i<BufCnt; i++) {
		//init
		StmRepo->Array[i].pBuf = 0;
		StmRepo->Array[i].nPhyAddr = 0;
		StmRepo->Array[i].nBufSize = 0;

		//allocate
		if(vmetadec->StmBufCacheable == 0) {
			StmRepo->Array[i].pBuf = (Ipp8u*)vdec_os_api_dma_alloc(OneBufSz, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
		}else{
			StmRepo->Array[i].pBuf = (Ipp8u*)vdec_os_api_dma_alloc_cached(OneBufSz, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
		}
		if(StmRepo->Array[i].pBuf == NULL) {
			break;
		}
		StmRepo->Array[i].nPhyAddr = nPhyAddr;
		StmRepo->Array[i].nBufSize = OneBufSz;
	}
	if(i != BufCnt) {
		//some vdec_os_api_dma_alloc fail, free all stream buffers
		for(i--; i>=0; i--) {
			vdec_os_api_dma_free(StmRepo->Array[i].pBuf);
			StmRepo->Array[i].pBuf = 0;
			StmRepo->Array[i].nPhyAddr = 0;
			StmRepo->Array[i].nBufSize = 0;
		}
		return -1;
	}
#endif
	return 0;
}

static int
init_streams_in_repo(VmetaDecStreamRepo* StmRepo, Gstvmetadec* vmetadec)
{
	//let active Array point to Array0, in alloc_streambuf_in_repo(), it work on Array
	StmRepo->Array = StmRepo->Array0;

	if(alloc_streambuf_in_repo(StmRepo, STREAM_VDECBUF_SIZE, STREAM_VDECBUF_NUM, vmetadec) != 0) {
		g_warning("No memory to new dma stream buffer.");
		return -1;
	}
	reset_streamrepo_context(StmRepo, STREAM_VDECBUF_SIZE, STREAM_VDECBUF_NUM, vmetadec);
	return 0;
}


static int
dyncstm_repo_recycle(VmetaDecDyncStreamRepo* DStmRepo)
{
	int i=0,j=0,k=0;
	IppVmetaBitstream* pCurNode = DStmRepo->pEntryNode, *pNext;
	IppVmetaBitstream* pNewListHead = NULL, *pNewListTail = NULL;
	for(; i<DStmRepo->Count; i++,pCurNode=pNext) {
		pNext = (IppVmetaBitstream*)pCurNode->pUsrData1;

		if( VMETA_DYNCSTMBUF_ISIDLE(pCurNode) ) {
			vdec_os_api_dma_free(pCurNode->pBuf);
			DStmRepo->TotalBufSz -= pCurNode->nBufSize;
			g_free(pCurNode);
			k++;
		}else{
			if(pNewListTail != NULL) {
				pNewListTail->pUsrData1 = pCurNode;
				pNewListTail = pCurNode;
			}else{
				pNewListHead = pNewListTail = pCurNode;
			}
			j++;
		}

	}

	DStmRepo->Count = j;
	if(j != 0) {
		DStmRepo->pEntryNode = pNewListHead;
		pNewListTail->pUsrData1 = (void*)pNewListHead;
	}else{
		DStmRepo->pEntryNode = NULL;
	}
	return k;
}

static void
destroy_allstreams_dync_repo(VmetaDecDyncStreamRepo* DStmRepo)
{
	IppVmetaBitstream* pTmp, *pNode = DStmRepo->pEntryNode;
	for(; DStmRepo->Count > 0; DStmRepo->Count--) {
		vdec_os_api_dma_free(pNode->pBuf);
		pTmp = pNode;
		pNode = (IppVmetaBitstream*)(pNode->pUsrData1);
		g_free(pTmp);
	}

	DStmRepo->pEntryNode = NULL;
	DStmRepo->TotalBufSz = 0;
	DStmRepo->pPushCandidate = NULL;
}

static __inline int
allocate_dynstmbuf_res(int OneBufSz, IppVmetaBitstream* pStm, Gstvmetadec* vmetadec)
{
	unsigned char* pTmp = NULL;
	unsigned int nPhyAddr;

	if(vmetadec->StmBufCacheable == 0) {
		pTmp = (Ipp8u*)vdec_os_api_dma_alloc(OneBufSz, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
	}else{
		pTmp = (Ipp8u*)vdec_os_api_dma_alloc_cached(OneBufSz, VMETA_STRM_BUF_ALIGN, &nPhyAddr);
	}
	if(G_UNLIKELY(pTmp == NULL)) {
		return -1;
	}

	pStm->pBuf = pTmp;
	pStm->nPhyAddr = nPhyAddr;
	pStm->nBufSize = OneBufSz;
	pStm->nDataLen = 0;
	pStm->nOffset   = 0;
	pStm->nBufProp = vmetadec->StmBufCacheable==0 ? 0 : VMETA_BUF_PROP_CACHEABLE;
	pStm->nFlag = 0;
	pStm->pUsrData3 = (void*)1; //indicate it's dynamic stream buffer, the lsb bit indicates whether it's static stream buffer
	return 0;
}

static IppVmetaBitstream*
new_stm_in_dyncstm_repo(VmetaDecDyncStreamRepo* DStmRepo, int OneBufSz, Gstvmetadec *vmetadec)
{
	IppVmetaBitstream* pTmpNode;

	pTmpNode = g_try_new(IppVmetaBitstream, 1);
	if(G_UNLIKELY(pTmpNode == NULL)) {
		g_warning("No memory to new dync stream node in repository.");
		return NULL;
	}

	if( 0 != allocate_dynstmbuf_res(OneBufSz, pTmpNode, vmetadec) ) {
		g_warning("Allocate dync stream buffer resource fail.");
		g_free(pTmpNode);
		return NULL;
	}

	VMETA_DYNCSTMBUF_SETIDLE(pTmpNode);
	DStmRepo->TotalBufSz += OneBufSz;

	//add the node into repo
	if(DStmRepo->Count == 0) {
		pTmpNode->pUsrData1 = (void*)pTmpNode;
		DStmRepo->pEntryNode = pTmpNode;
	}else{
		pTmpNode->pUsrData1 = DStmRepo->pEntryNode->pUsrData1;  	//insert the new node at the downstream of pEntryNode
		DStmRepo->pEntryNode->pUsrData1 = (void*)pTmpNode;
	}

	DStmRepo->Count++;
#ifdef DEBUG_LOG_SOMEINFO
	if(DStmRepo->Count > ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmCnt) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmCnt = DStmRepo->Count;
	}
	if(DStmRepo->TotalBufSz > ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmBufTSz) {
		((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmBufTSz = DStmRepo->TotalBufSz;
	}
#endif
	return pTmpNode;
}


static IppVmetaBitstream*
seek_idlestm_in_dyncstm_repo(VmetaDecDyncStreamRepo* DStmRepo, int wantbufsz)
{

	if(DStmRepo->Count != 0) {
		int i;
		IppVmetaBitstream* pFNode = (IppVmetaBitstream*)DStmRepo->pEntryNode->pUsrData1;	//DStmRepo->pEntryNode point to the last rented stm, so search from the next

		for(i=0; i<DStmRepo->Count; i++,pFNode = (IppVmetaBitstream*)pFNode->pUsrData1) {
			if( VMETA_DYNCSTMBUF_ISIDLE(pFNode) && (int)pFNode->nBufSize >= wantbufsz ) {
				return pFNode;
			}
		}
	}
	return NULL;
}


static IppVmetaBitstream*
rent_stm_from_dyncstm_repo(VmetaDecDyncStreamRepo* DStmRepo, int OneBufSz, Gstvmetadec *vmetadec)
{
	IppVmetaBitstream* pFNode;

	pFNode = seek_idlestm_in_dyncstm_repo(DStmRepo, OneBufSz);
	if(pFNode != NULL) {
		DStmRepo->pEntryNode = pFNode;  //let entrynode point to the rented node, it's not must to have, but better to have
		VMETA_DYNCSTMBUF_SETBUSY(pFNode);   //mark this buffer is using
		return pFNode;
	}

	if(DStmRepo->Count > 8 || DStmRepo->TotalBufSz >= 2*1024*1024) {
		assist_myecho("Do dync stream repo recycle, cnt %d, totalsz %d\n", DStmRepo->Count, DStmRepo->TotalBufSz);
		dyncstm_repo_recycle(DStmRepo);
		assist_myecho("dync stream repo recycle done, cnt %d, totalsz %d\n", DStmRepo->Count, DStmRepo->TotalBufSz);
	}

	//no idle node in repo, new one
	pFNode = new_stm_in_dyncstm_repo(DStmRepo, OneBufSz, vmetadec);

	if(G_LIKELY(pFNode != NULL)) {
		DStmRepo->pEntryNode = pFNode;  	//let entrynode point to the rented node, it's not must to have, but better to have
		VMETA_DYNCSTMBUF_SETBUSY(pFNode);   //mark this buffer is using
		assist_myecho("dync stream repo cnt %d, new buf sz %d\n", DStmRepo->Count, pFNode->nBufSize);
		return pFNode;
	}else{
		g_warning("New dync stream in repository fail.");
		return NULL;
	}
}


//only fill it, it don't care whether the stream could be filled or modify the correlative information in repo, it only modify the information in this stream
static __inline int
stuff_onestream(IppVmetaBitstream* pStm, int StuffingByte)
{
	int filled = pStm->nBufSize - (int)pStm->pUsrData0;
	memset(pStm->pBuf + (int)pStm->pUsrData0, StuffingByte, filled);
	pStm->nDataLen = pStm->nBufSize;
	pStm->pUsrData0 = (void*)pStm->nBufSize;
	return filled;
}

static int
fill_streams_in_repo(VmetaDecStreamRepo* StmRepo, const Ipp8u* pData, int WantedSz)
{
#ifdef BIG_CONTINUOUS_STREAMBUF
	IppVmetaBitstream* NodeArr = StmRepo->Array;
	int LeftSz = WantedSz;
	int CollapseFillSz=0, SpareSz;
	int CurNodeWaitFill = StmRepo->nWaitFillCandidateIdx;
	int PushReadyCnt = 0;
	unsigned char* pDst = NodeArr[StmRepo->nWaitFillCandidateIdx].pBuf + (int)(NodeArr[StmRepo->nWaitFillCandidateIdx].pUsrData0);


	for(;LeftSz>0;) {
		if(NodeArr[CurNodeWaitFill].pUsrData0 == (void*)NodeArr[CurNodeWaitFill].nBufSize) {
			//all nodes are filled, no space to fill
			break;
		}
		SpareSz = NodeArr[CurNodeWaitFill].nBufSize - (int)NodeArr[CurNodeWaitFill].pUsrData0;
		if(LeftSz < SpareSz) {
			//current node won't be filled full
			NodeArr[CurNodeWaitFill].pUsrData0 = (void*)((int)NodeArr[CurNodeWaitFill].pUsrData0 + LeftSz);
			NodeArr[CurNodeWaitFill].nDataLen += LeftSz;
			CollapseFillSz += LeftSz;
			LeftSz = 0;
			break;
		}else{
			//current node will be filled full
			NodeArr[CurNodeWaitFill].pUsrData0 = (void*)NodeArr[CurNodeWaitFill].nBufSize;
			NodeArr[CurNodeWaitFill].nDataLen = NodeArr[CurNodeWaitFill].nBufSize;
			PushReadyCnt++;
			CurNodeWaitFill++;
			CollapseFillSz += SpareSz;
			LeftSz -= SpareSz;
			if(CurNodeWaitFill == StmRepo->Count) {
				memcpy(pDst, pData, CollapseFillSz);
				pData += CollapseFillSz;
				pDst = NodeArr[0].pBuf;
				CurNodeWaitFill = 0;
				CollapseFillSz = 0;
			}
		}
	}

	//real consume all continuous data
	if(CollapseFillSz > 0) {
		memcpy(pDst, pData, CollapseFillSz);
	}

	StmRepo->nWaitFillCandidateIdx = CurNodeWaitFill;
	StmRepo->nPushReadyCnt += PushReadyCnt;
	StmRepo->TotalSpareSz -= WantedSz-LeftSz;
	return WantedSz-LeftSz;

#else
	IppVmetaBitstream* NodeArr = StmRepo->Array;
	int LeftSz = WantedSz;
	int SpareSz;
	int CurNodeWaitFill = StmRepo->nWaitFillCandidateIdx;
	int PushReadyCnt = 0;
	unsigned char* pDst = NodeArr[StmRepo->nWaitFillCandidateIdx].pBuf + (int)(NodeArr[StmRepo->nWaitFillCandidateIdx].pUsrData0);


	for(;LeftSz>0;) {
		if(NodeArr[CurNodeWaitFill].pUsrData0 == (void*)NodeArr[CurNodeWaitFill].nBufSize) {
			//all nodes are filled, no space to fill
			break;
		}
		SpareSz = NodeArr[CurNodeWaitFill].nBufSize - (int)NodeArr[CurNodeWaitFill].pUsrData0;

		if(LeftSz < SpareSz) {
			//current node won't be filled full
			NodeArr[CurNodeWaitFill].pUsrData0 = (void*)((int)NodeArr[CurNodeWaitFill].pUsrData0 + LeftSz);
			NodeArr[CurNodeWaitFill].nDataLen += LeftSz;
			memcpy(pDst, pData, LeftSz);
			LeftSz = 0;
			break;
		}else{
			//current node will be filled full
			NodeArr[CurNodeWaitFill].pUsrData0 = (void*)NodeArr[CurNodeWaitFill].nBufSize;
			NodeArr[CurNodeWaitFill].nDataLen = NodeArr[CurNodeWaitFill].nBufSize;
			PushReadyCnt++;
			LeftSz -= SpareSz;

			memcpy(pDst, pData, SpareSz);
			pData += SpareSz;
			if(CurNodeWaitFill+1 == StmRepo->Count) {
				CurNodeWaitFill = 0;
				pDst = NodeArr[0].pBuf;
			}else{
				CurNodeWaitFill++;
				pDst = NodeArr[CurNodeWaitFill].pBuf;
			}
		}
	}

	StmRepo->nWaitFillCandidateIdx = CurNodeWaitFill;
	StmRepo->nPushReadyCnt += PushReadyCnt;
	StmRepo->TotalSpareSz -= WantedSz-LeftSz;
	return WantedSz-LeftSz;

#endif
}

static int autofill_endpattern_to_stream(VmetaDecStreamRepo* pStmRepo, IppVmetaBitstreamFlag flag, Gstvmetadec* vmetadec);
static __inline int
fill_1frameIn1stm_inStaticRepo(VmetaDecStreamRepo* StmRepo, const Ipp8u* pData, int WantedSz, int bSealStm, Gstvmetadec* vmetadec)
{
	int filled = fill_streams_in_repo(StmRepo, pData, WantedSz);
	if(bSealStm) {
		autofill_endpattern_to_stream(StmRepo, IPP_VMETA_STRM_BUF_END_OF_UNIT, vmetadec);
	}
	return filled;
}

static __inline int
fill_1frameIn1stm_inDyncRepo(IppVmetaBitstream* pStm, const Ipp8u* pData, int WantedSz, int bSealStm)
{
	memcpy(pStm->pBuf + pStm->nOffset + pStm->nDataLen, pData, WantedSz);
	pStm->nDataLen += WantedSz;
	if(bSealStm) {
		pStm->nFlag = IPP_VMETA_STRM_BUF_END_OF_UNIT;
	}
	return WantedSz;
}


static int
push_staticstreams_to_vdec(void* pVDecObj, VmetaDecStreamRepo* StmRepo, int PushNodeCnt)
{
	IppCodecStatus VDecRtCode;
	int i=0;
	for(; PushNodeCnt>0; PushNodeCnt--) {
		//Note: the IppVmetaBitstream what is filled data is from Array0/1, but IppVmetaBitstream that is pushed to codec is from ArrayCodec
		for(;i<StmRepo->Count;i++) {
			if(StmRepo->ArrayCodec[i].pUsrData0 == (void*)0) {
				break;
			}
		}
		if(i==StmRepo->Count) {
			g_warning("Couldn't found idle stream node in %s line %d", __FUNCTION__, __LINE__);
			return -2;
		}

		StmRepo->ArrayCodec[i] = StmRepo->Array[StmRepo->nNotPushedStreamIdx];
		StmRepo->ArrayCodec[i].pUsrData0 = (void*)1;
		StmRepo->ArrayCodec[i].pUsrData1 = (void*)&StmRepo->Array[StmRepo->nNotPushedStreamIdx];
		StmRepo->Array[StmRepo->nNotPushedStreamIdx].pUsrData1 = (void*)&StmRepo->ArrayCodec[i];	//let Node in Array could track push Node
		VDecRtCode = DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, &StmRepo->ArrayCodec[i], pVDecObj);
		if(IPP_STATUS_NOERR != VDecRtCode) {
			g_warning("Push stream to vMeta Decoder fail, %d!", VDecRtCode);
			return -1;
		}
		assist_myechomore("Push stream to vMeta Decoder ok\n");
		assist_myechomore("-- Push STM spare left %f\n", StmRepo->TotalSpareSz/(float)StmRepo->TotalBufSz);
		StmRepo->nPushReadyCnt--;
		StmRepo->nNotPushedStreamIdx++;
		if(StmRepo->nNotPushedStreamIdx == StmRepo->Count) {
			StmRepo->nNotPushedStreamIdx = 0;
		}
		i++;
	}
	return 0;
}


static void
remove_unwanted_TS(VmetaTSManager* TSDUManager, int maxlen)
{
	while(TSDUManager->iTDListLen > maxlen) {
		g_slice_free(TSDUPair, (TSDUPair*)TSDUManager->TSDUList->data);
		TSDUManager->TSDUList = g_slist_delete_link(TSDUManager->TSDUList, TSDUManager->TSDUList);
		TSDUManager->iTDListLen--;
		assist_myecho("Removed one unwanted TS %d %d.\n", TSDUManager->iTDListLen, TSDUManager->iTSDelayLimit);
	}
	return;
}

static __inline void
pop_TS(VmetaTSManager* TSDUManager, int bAutoTSWhenLackTSinSyncMode)
{
	//remove unwanted TS to avoid list overflow
	remove_unwanted_TS(TSDUManager, TSDUManager->iTSDelayLimit);

	if(TSDUManager->mode == 0) {

		if(G_LIKELY(TSDUManager->TSDUList != NULL)) {
			TSDUPair* pTSDU = (TSDUPair*)TSDUManager->TSDUList->data;
			TSDUManager->TsLatestOut = pTSDU->TS;
			TSDUManager->DurationLatestOut = pTSDU->DU;
			TSDUManager->TSDUList = g_slist_delete_link(TSDUManager->TSDUList, TSDUManager->TSDUList);
			TSDUManager->iTDListLen--;
			g_slice_free(TSDUPair, pTSDU);
		}else{
			if(bAutoTSWhenLackTSinSyncMode == 0) {
				//when there is no TS in TS list, output the GST_CLOCK_TIME_NONE, which leads to sink show this gstbuffer immediately. If output the last TS, the sink probably drops this gstbuffer, because the last TS has lapsed.
				TSDUManager->TsLatestOut = GST_CLOCK_TIME_NONE;
				TSDUManager->DurationLatestOut = GST_CLOCK_TIME_NONE;
			}else{
				//output an automatic increasement TS. Limitation: the automatic increasement TS may exceed the next TS.
				if(TSDUManager->TsLatestOut != GST_CLOCK_TIME_NONE) {
					TSDUManager->TsLatestOut += TSDUManager->FrameFixDuration;
				}
			}
		}
	}else{
		if(TSDUManager->TsLatestOut != GST_CLOCK_TIME_NONE) {
			TSDUManager->TsLatestOut += TSDUManager->FrameFixDuration;
			TSDUManager->DurationLatestOut = TSDUManager->FrameFixDuration;
		}
	}
	TSDUManager->bHavePopTs = 1;

	return;
}

static gint
compareTSDU(gconstpointer a, gconstpointer b)
{
	if(((TSDUPair*)a)->TS >= ((TSDUPair*)b)->TS) {
		return 1;
	}else{
		return -1;
	}
}

static __inline void
push_TS(VmetaTSManager* TSDUManager, GstClockTime* TS, GstClockTime* DU)
{
	gint64 diff;
	//*TS shouldn't be GST_CLOCK_TIME_NONE
	if(*TS == GST_CLOCK_TIME_NONE) {
		return;
	}
	//if(TSDUManager->inputSampeleDuration == GST_CLOCK_TIME_NONE) {
		TSDUManager->inputSampeleDuration = *DU;	//always refresh inputSampeleDuration, let it is the latest
	//}
	//at the begin of each segment, TSDUManager->TsLatestOut is cleared to GST_CLOCK_TIME_NONE. Therefore, it must be reinit
	if(TSDUManager->TsLatestOut == GST_CLOCK_TIME_NONE) {
		TSDUManager->TsLatestOut = *TS;
	}
	if(TSDUManager->mode == 0) {
		//if input TS early than latest out TS, ignore the input TS
		if(TSDUManager->TsLatestOut != GST_CLOCK_TIME_NONE && TSDUManager->bHavePopTs != 0 && *TS <= TSDUManager->TsLatestOut) {
			return;
		}
		if(TSDUManager->TsLatestIn != GST_CLOCK_TIME_NONE) {
			if(TSDUManager->TsLatestIn > (*TS)) {
				diff = TSDUManager->TsLatestIn - *TS;
			}else{
				diff = *TS - TSDUManager->TsLatestIn;
			}
			if(diff <= TSDUManager->DiffTS_Criterion) {
				//we only store the timestamp belong to different frame
				//if we couldn't get frame duration information correctly, DiffTS_Criterion is set to 0. Under that case, only the equivalent TSs are considered to belong to one frame
				return;
			}
		}

		TSDUPair* pTSDU = g_slice_new(TSDUPair);
		pTSDU->TS = *TS;
		pTSDU->DU = *DU;
		TSDUManager->TsLatestIn = *TS;
		TSDUManager->TSDUList = g_slist_insert_sorted(TSDUManager->TSDUList, pTSDU, compareTSDU);
		TSDUManager->iTDListLen++;
	}
	return;
}

static __inline void
clear_TSManager(VmetaTSManager* TSDUManager)
{
	TSDUManager->TsLatestIn = GST_CLOCK_TIME_NONE;
	TSDUManager->TsLatestOut = GST_CLOCK_TIME_NONE;
	TSDUManager->bHavePopTs = 0;
	TSDUManager->DurationLatestOut = GST_CLOCK_TIME_NONE;
	while(TSDUManager->TSDUList != NULL) {
		g_slice_free(TSDUPair, (TSDUPair*)TSDUManager->TSDUList->data);
		TSDUManager->TSDUList = g_slist_delete_link(TSDUManager->TSDUList, TSDUManager->TSDUList);
		TSDUManager->iTDListLen--;
	}
	TSDUManager->SegmentEndTs = -1;
	TSDUManager->SegmentStartTs = -1;
	return;
}


#define MPEG2_SCID_SEQ  	0xb3
#define MPEG2_SCID_SEQEND   0xb7
#define MPEG2_SCID_PIC  	0x00
#define MPEG2_SCID_GOP  	0xb8
#define MPEG4_SCID_VOP  	0xb6
static unsigned char*
Seek4bytesCode(unsigned char* pData, int len, unsigned int n4byteCode)
{
	if(len >= 4) {
		unsigned int code = (pData[0]<<16) | (pData[1]<<8) | (pData[2]);
		len -= 3;
		pData += 3;
		while(len > 0) {
			code = (code<<8) | *pData++ ;
			len--;
			if(code == n4byteCode) {
				return pData-4;
			}
		}
	}
	return NULL;
}



static unsigned char*
SeekMPEG2LatestPicEnd(unsigned char* pData, int len, int* bCurSeekPicEnd, unsigned char** ppLatestSC)
{
	unsigned int code;
	unsigned char *pByte = pData;
	unsigned char *pLatestPicEnd = NULL;

	*ppLatestSC = NULL;
	len -= 4;

	for (;;) {
		// Matching string "0x000001xx"
		code = ((pByte[0])<<16) | ((pByte[1])<<8) | (pByte[2]);
		for (;;) {
			if ( (int)(pByte - pData) > len ) {
				return pLatestPicEnd;
			}
			code <<= 8;
			if(code == 0x00000100) {
				break;
			}else{
				code |= pByte[3];
				pByte++;
			}

		}

		// Matching the start code
		*ppLatestSC = pByte;
		if(*bCurSeekPicEnd == 1) {
			if(pByte[3]==MPEG2_SCID_PIC||pByte[3]==MPEG2_SCID_GOP||pByte[3]==MPEG2_SCID_SEQ||pByte[3]==MPEG2_SCID_SEQEND) {
				//find pic end
				pLatestPicEnd = pByte;
				//if pic start code occur, it's not only the end of last picture but also the start of next picture.
				if(pByte[3]!=MPEG2_SCID_PIC) {
					*bCurSeekPicEnd = 0;
				}
			}
		}else{
			if(pByte[3]==MPEG2_SCID_PIC) {
				*bCurSeekPicEnd = 1;
			}
		}
		//bofang: For error stream, the stream like 0x000001000001 maybe occur.
		//We accept the former 00000100 is the start code of picture, and ignore the later 000001. Therefore, skipping 4 bytes instead of 3 bytes.
		//pByte += 3;
		pByte += 4;
	}
}

static __inline int
get_seqinfos_from_mpeg2stream(Gstvmetadec* vmetadec, Ipp8u* pData, int DataLen)
{
	unsigned int FrIdx;
	int w, h;
	unsigned int BR_low18_value;
	if(DataLen < 11) {
		return -1;
	}
	w = (((unsigned int)pData[4])<<4) | (((unsigned int)pData[5])>>4);
	h = (((unsigned int)pData[5]&0x0f)<<8) | (((unsigned int)pData[6]));
	if(w == 0 || h == 0) {
		return -1;
	}
	FrIdx = (pData[7] & 0x0f);
	if(FrIdx != 0 && FrIdx < sizeof(mpeg2fpss)/sizeof(mpeg2fpss[0]) ) {
		vmetadec->iFRateNum = mpeg2fpss[FrIdx][0];
		vmetadec->iFRateDen = mpeg2fpss[FrIdx][1];
		assist_myecho("Parse framerate in raw mpeg2 video stream is %d/%d\n", vmetadec->iFRateNum, vmetadec->iFRateDen);
		vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
		vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
	}else{
		return -1;
	}

	BR_low18_value = ((unsigned int)pData[8]<<10) | ((unsigned int)pData[9]<<2) | ((unsigned int)pData[10]>>6);
	//we don't need accurate bit rate, so we low 18bit is enough and we don't check whether it is zero
	vmetadec->DecMPEG2Obj.iNorminalBitRate = BR_low18_value * 400;
	assist_myecho("Bit rate in raw mpeg2 video stream is %d\n", vmetadec->DecMPEG2Obj.iNorminalBitRate);

	vmetadec->DecMPEG2Obj.iNorminalW = w;
	vmetadec->DecMPEG2Obj.iNorminalH = h;
	assist_myecho("Parse display size in raw mpeg2 video stream is w:%d, h:%d\n", vmetadec->DecMPEG2Obj.iNorminalW, vmetadec->DecMPEG2Obj.iNorminalH);

	return 0;
}

static int
parse_mpeg2seqinfo_fromstrem(Gstvmetadec* vmetadec, Ipp8u* pData, int nDataLen)
{
	unsigned char* pSeqHead = Seek4bytesCode(pData, nDataLen, 0x00000100|MPEG2_SCID_SEQ);
	if(pSeqHead == NULL) {
		return -1;
	}
	if( 0 != get_seqinfos_from_mpeg2stream(vmetadec, pSeqHead, nDataLen-(pSeqHead-pData)) ) {
		return -1;
	}
	return 0;
}

static __inline unsigned
peek_unsigned_from_LEstream(Ipp8u* pData, int len)
{
	int i;
	unsigned int val = 0;
	for(i=0;i<len;i++) {
		val = val<<8 | pData[i];
	}
	return val;
}

static __inline int
ThreeBytesIsMPEG2SCPrefix(unsigned char* pBytes)
{
	unsigned int code = (pBytes[0]<<16) || (pBytes[1]<<8) || (pBytes[2]);
	if(code == 1 || (code&0xff) == 0) {
		return 1;
	}else{
		return 0;
	}
}

static int
parse_mpeg4_TIB(unsigned char* p, int len, int* plow_delay)
{
#define __GETnBITS_InByte(B, Bitoff, N, Code)   {Code = (B<<(24+Bitoff))>>(32-N); Bitoff += N;}
	assist_myecho("parse_mpeg4_TIB is called.\n");
	unsigned int SCode;
	if(len < 9) {
		//at least, 4byte VOL startcode and 34 bits contain vop_time_increment_resolution
		return -1;
	}

	//ISO 14496-2, sec 6.2.3
	//seek video object layer startcode
	SCode = ((unsigned int)p[0]<<16) | ((unsigned int)p[1]<<8) | ((unsigned int)p[2]);
	len -= 3;
	p += 3;
	while(len > 0) {
		SCode = (SCode<<8) | *p++ ;
		len--;
		if((SCode>>4) == (0x00000120>>4)) {
			break;
		}
	}

	if(len < 5) {   //at least, should have 34 bits to contain vop_time_increment_resolution
		return -1;
	}

	if((p[0]&127) == 5 && (p[1]&128) == 0) {	//video_object_type_indication
		return -1;
	}else{
		unsigned int vtir, code, Byte = *++p;
		int time_inc_bits;;
		int bitoff;
		len--;
		if(Byte & 0x40) {   //is_object_layer_identifier
			len--;
			if(len<=0) {return -1;}
			Byte = *++p;
			bitoff = 1;
		}else{
			bitoff = 2;
		}

		__GETnBITS_InByte(Byte, bitoff, 4, code);   //aspect_ratio_info
		if(code == 0xf) {
			len -= 2;
			if(len<=0) {return -1;}
			p += 2;
			Byte = *p;
		}

		__GETnBITS_InByte(Byte, bitoff, 1, code);   //vol_control_parameters
		if(len<3) {return -1;}  //video_object_layer_shape+marker_bit+vop_time_increment_resolution have 19bits at least
		if(code) {
			len--;
			Byte = *++p;
			bitoff = bitoff + 2 - 8;
			__GETnBITS_InByte(Byte, bitoff, 1, code);//low_delay
			*plow_delay = code;
			assist_myecho("found mpeg4 stream low_delay %d\n", code);
			__GETnBITS_InByte(Byte, bitoff, 1, code);//vbv_parameters
			if(code) {
				len -= 10;
				if(len<=0) {return -1;}
				p += 10;
				bitoff -= 1;
				Byte = *p;
			}
		}

		//video_object_layer_shape
		code = (((Byte<<8)|p[1])<<(16+bitoff)) >> 30;
		bitoff += 2;
		if(bitoff >= 8) {
			bitoff -= 8;
			len--;
			p++;
		}
		if(code != 0) { //only support video_object_layer_shape==rectangular
			return -2;
		}

		//vop_time_increment_resolution
		if(len < 3) {
			return -1;
		}
		vtir = (((unsigned int)p[0]<<16)|((unsigned int)p[1]<<8)|(unsigned int)p[2])<<(8+1+bitoff);

		if((vtir>>16) == 0) {
			return -3;
		}
		assist_myecho("parse_mpeg4_TIB parsed vtir %d\n", vtir>>16);
		vtir -= 0x10000;
		for(time_inc_bits = 16; time_inc_bits>0; time_inc_bits--) {
			if(((int)vtir) < 0) {
				break;
			}
			vtir <<= 1;
		}
		if(time_inc_bits == 0) {
			time_inc_bits = 1;
		}

		assist_myecho("parse_mpeg4_TIB() parsed time_inc_bits %d\n", time_inc_bits);
		return time_inc_bits;
	}
}

static void
store_codecdata(Gstvmetadec* vmetadec, GstBuffer* buf)
{
	if(vmetadec->gbuf_cdata) {
		gst_buffer_unref(vmetadec->gbuf_cdata);
		vmetadec->gbuf_cdata = NULL;
	}
	vmetadec->gbuf_cdata = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf));
	memcpy(GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
	return;
}

static int
consume_h264_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
	VmetaDecH264Obj* h264obj = &vmetadec->DecH264Obj;
	if(csize < 15) {	//each sync code 3 bytes, SPS NAL>= 6 bytes, PPS NAL>= 3 bytes
		g_warning("codec_data length is %d, not have enough data in code_data(at least SPS+PPS) for h264.", csize);
		GST_WARNING_OBJECT(vmetadec, "codec_data length is %d, not have enough data in code_data(at least SPS+PPS) for h264.", csize);
		//return FALSE;
		if(csize > 0) {
			fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
		}
		return 0;
	}

	//ISO 14496-15: sec 5.2.4.1, sec 5.3.4.1.2
	if(G_LIKELY(csize >= 20 && cdata && 1 == *cdata)){
		unsigned int AVCProfileIndication = cdata[1];
		int offset = 4;
		int cnt_sps;
		int cnt_pps;
		unsigned int len;
		int lengthSizeMinusOne;
		int i;
		h264obj->StreamLayout = H264STM_LAYOUT_AVCC;
		assist_myecho("according to codec_data, it's AVCC format h264.\n");

		if(AVCProfileIndication != 77) {
			assist_myecho("AVCProfileIndication in avcC is %d, not 77(Main Profile)\n", AVCProfileIndication);
			//g_warning("AVCProfileIndication in avcC is %d, not 77(Main Profile)", AVCProfileIndication);
			//return -1;
		}

		/*parse sps from avcC*/
		cdata += offset;					/* start form 4 */
		lengthSizeMinusOne = (*(cdata++) & 0x03);
		if(lengthSizeMinusOne == 2) {
			g_warning("lengthSizeMinusOne in avcC shouldn't equal to 2 according to ISO 14496-15: sec 5.2.4.1.2");
			//return -2;
			lengthSizeMinusOne = 3;
		}
		h264obj->nal_length_size = lengthSizeMinusOne+1;

		cnt_sps = *(cdata++) & 0x1f;		/* sps cnt */
		/* some stream may be parsed with cnt_sps = 0, we force it as 1 to init codec */
		cnt_sps = cnt_sps > 0 ? cnt_sps : 1;
		for (i = 0; i < cnt_sps; i++) {
			len = *(cdata++)<<8;
			len |= *(cdata++);  	/* one sps len 2 bytes*/

			/* fill sps to vmeta stream buffer */
			fill_streams_in_repo(&vmetadec->StmRepo, H264NALSyncCode, sizeof(H264NALSyncCode));
			fill_streams_in_repo(&vmetadec->StmRepo, cdata, len);
			cdata += len;
		}

		/*parse pps from avcC*/
		cnt_pps= *(cdata++);				/* pps cnt*/
		/* some stream may be parsed with cnt_pps = 0, we force it as 1 to init codec */
		cnt_pps = cnt_pps > 0 ? cnt_pps : 1;
		for (i = 0; i < cnt_pps; i++) {
			len = *(cdata++)<<8;
			len |= *(cdata++);  		/*one pps len */
			/* fill pps to vmeta stream buffer */
			fill_streams_in_repo(&vmetadec->StmRepo, H264NALSyncCode, sizeof(H264NALSyncCode));
			fill_streams_in_repo(&vmetadec->StmRepo, cdata, len);
			cdata += len;
		}

	}else{
		//usually, for h264 in avi file, it's layout isn't avcC but h.264 byte stream format(h.264 spec. annex B) picture by picture
		h264obj->StreamLayout = H264STM_LAYOUT_SYNCCODENAL;
		fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
	}
	return 0;
}

static gboolean
handle_264_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	VmetaDecH264Obj* h264obj = &vmetadec->DecH264Obj;
	//the frame dimension is postponed to init after frame decoded, because we only output the ROI of h264 frame
	if(G_UNLIKELY(FALSE == gst_structure_get_fraction(str, "framerate", &vmetadec->iFRateNum, &vmetadec->iFRateDen))) {
		vmetadec->iFRateNum = 0;		//if this information isn't in caps, we assume it is 0
		vmetadec->iFRateDen = 1;
	}
	if(vmetadec->iFRateNum != 0) {
		vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
		vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
		assist_myecho("FrameFixDuration is calculated %lld ns, DiffTS_Criterion %lld ns according to srcpad cap fps\n", vmetadec->TSDUManager.FrameFixDuration, vmetadec->TSDUManager.DiffTS_Criterion);
	}

	h264obj->StreamLayout = H264STM_LAYOUT_SYNCCODENAL; //set a default value
	if(G_LIKELY(TRUE == gst_structure_has_field(str, "codec_data"))) {
		const GValue *value = gst_structure_get_value(str, "codec_data");
		GstBuffer* buf = gst_value_get_buffer(value);
		store_codecdata(vmetadec, buf);
	}else{
		h264obj->StreamLayout = H264STM_LAYOUT_SYNCCODENAL;
		//g_warning("No codec_data provided by demuxer, suppose the stream is h.264 byte stream format(h.264 spec. annex B), and sps+pps are combined into element stream");
		GST_INFO_OBJECT(vmetadec, "No codec_data provided by demuxer, suppose the stream is h.264 byte stream format(h.264 spec. annex B), and sps+pps are combined into element stream");
	}
	return TRUE;
}

static __inline int
consume_vc1wmv_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
	fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
	return 0;
}

static gboolean
handle_vc1wmv_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	gint ver;
	guint32 f4cc;
	//the frame dimension is postponed to init after frame decoded
	if(G_UNLIKELY(FALSE == gst_structure_get_fraction(str, "framerate", &vmetadec->iFRateNum, &vmetadec->iFRateDen))) {
		vmetadec->iFRateNum = 0;		//if this information isn't in caps, we assume it is 0
		vmetadec->iFRateDen = 1;
	}
	if(vmetadec->iFRateNum != 0) {
		vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
		vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
	}

	//check wmvversion
	if(G_LIKELY(TRUE == gst_structure_get_int(str, "wmvversion", &ver))) {
		if(ver != 3) {
			g_warning("The wmv version is %d in mime type, we don't support it.", ver);
			return FALSE;
		}
	}else{
		g_warning("There is no wmv version information in MIME");
	}

	//check fourcc
	if(FALSE == gst_structure_get_fourcc(str, "fourcc", &f4cc)) {
		if(FALSE == gst_structure_get_fourcc(str, "format", &f4cc)) {
			f4cc = 0;
		}
	}
	if(f4cc != GST_STR_FOURCC("WVC1")) {
		g_warning("There is no fourcc information in MIME or fourcc isn't WVC1 but 0x%x", f4cc);
	}

	if(G_LIKELY(TRUE == gst_structure_has_field(str, "codec_data"))) {
		const GValue *value = gst_structure_get_value(str, "codec_data");
		GstBuffer* buf = gst_value_get_buffer(value);
		store_codecdata(vmetadec, buf);
	}
	return TRUE;
}

static int
consume_vc1wmvSPMP_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
#define IPPCODEC_SUPPORT_STRUCTC_MAXLEN		5
	//SMPTE 421M, Annex L. The last bit in STRUCT_C[3] stand for different encoder version(latest spec doesn't claim that). 0 means beta version, 1 means RTM version.
	//In lastest spec, STRUCT_C len should be 4, but for some early version, STRUCT_C probably is 5 even 6. But the data after STRUCT_C[3] doesn't affect decoding process.
	//vmeta decoder could accept IPPCODEC_SUPPORT_STRUCTC_MAXLEN bytes for STRUCT_C at most. Therefore, trunc it.
	if(csize > IPPCODEC_SUPPORT_STRUCTC_MAXLEN) {
		GST_DEBUG_OBJECT(vmetadec, "trunc STRUCT_C data from %d to "G_STRINGIFY(IPPCODEC_SUPPORT_STRUCTC_MAXLEN)" in %s()", csize, __FUNCTION__);
		assist_myecho("trunc STRUCT_C data from %d to "G_STRINGIFY(IPPCODEC_SUPPORT_STRUCTC_MAXLEN)" in %s()\n", csize, __FUNCTION__);
		csize = IPPCODEC_SUPPORT_STRUCTC_MAXLEN;
	}

#if 0
	unsigned int seqMetaLen;
	unsigned char seqMeta[37] = {0};
	//SMPTE 421M, Annex L
	seqMeta[0] = seqMeta[1] = seqMeta[2] = 0xff;	//NUMFRAMES = 0xffffff
	seqMeta[3] = 0xC5;
	seqMeta[4] = csize;
	memcpy(&seqMeta[8], cdata, csize);
	seqMetaLen = 8+csize;
	seqMeta[seqMetaLen++] = vmetadec->DecVC1MPObj.iContentHeight & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentHeight)>>8) & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentHeight)>>16) & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentHeight)>>24) & 0xff;
	seqMeta[seqMetaLen++] = vmetadec->DecVC1MPObj.iContentWidth & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentWidth)>>8) & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentWidth)>>16) & 0xff;
	seqMeta[seqMetaLen++] = ((vmetadec->DecVC1MPObj.iContentWidth)>>24) & 0xff;
	seqMeta[seqMetaLen] = 0x0C;
	seqMetaLen += 4;

	seqMeta[seqMetaLen++] = 0xff;   //HRD_BUFFER = 0x007fff
	seqMeta[seqMetaLen++] = 0x7f;
	seqMeta[seqMetaLen++] = 0;

	if((cdata[0]>>4) == 4) {	//main profile
		seqMeta[seqMetaLen++] = ((4<<1)|1)<<4;  //high level, CBR=1
	}else{  //simple profile
		seqMeta[seqMetaLen++] = ((2<<1)|1)<<4;  //mail level, CBR=1
	}

	seqMeta[seqMetaLen++] = 0xff;   //HRD_RATE = 0x00007fff
	seqMeta[seqMetaLen++] = 0x7f;
	seqMetaLen+=2;

	seqMeta[seqMetaLen++] = 0xff;   //FRAMERATE = 0xffffffff
	seqMeta[seqMetaLen++] = 0xff;
	seqMeta[seqMetaLen++] = 0xff;
	seqMeta[seqMetaLen++] = 0xff;

	assist_myecho("vc1 SPMP seq meta data len %d\n", seqMetaLen);
	fill_1frameIn1stm_inStaticRepo(&vmetadec->StmRepo, seqMeta, seqMetaLen, 1, vmetadec);
#else
	vc1m_seq_header SeqMet;
	IppCodecStatus rt;
	SeqMet.num_frames = 0xffffff;
	SeqMet.vert_size = vmetadec->DecVC1MPObj.iContentHeight;
	SeqMet.horiz_size = vmetadec->DecVC1MPObj.iContentWidth;
	SeqMet.level = ((cdata[0]>>4) == 4) ? 4 : 2;
	SeqMet.cbr = 1;
	SeqMet.hrd_buffer = 0x007fff;
	SeqMet.hrd_rate = 0x00007fff;
	SeqMet.frame_rate = 0xffffffff;
	memcpy(SeqMet.exthdr, cdata, csize);
	SeqMet.exthdrsize = csize;
	rt = DecodeSendCmd_Vmeta(IPPVC_SET_VC1M_SEQ_INFO, &SeqMet, NULL, vmetadec->pVDecoderObj);
	if(rt != IPP_STATUS_NOERR) {
		g_warning("DecodeSendCmd_Vmeta(IPPVC_SET_VC1M_SEQ_INFO) fail, ret %d\n", rt);
		return -2;
	}
#endif
	return 0;
}


static gboolean
handle_vc1wmvSPMP_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	gint ver, w, h;
	guint32 f4cc;
	//the frame dimension is postponed to init after frame decoded
	if(G_UNLIKELY(FALSE == gst_structure_get_fraction(str, "framerate", &vmetadec->iFRateNum, &vmetadec->iFRateDen))) {
		vmetadec->iFRateNum = 0;		//if this information isn't in caps, we assume it is 0
		vmetadec->iFRateDen = 1;
	}
	if(vmetadec->iFRateNum != 0) {
		vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
		vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
	}

	//check wmvversion
	if(G_LIKELY(TRUE == gst_structure_get_int(str, "wmvversion", &ver))) {
		if(ver != 3) {
			g_warning("The wmv version is %d in mime type, we don't support it.", ver);
			return FALSE;
		}
	}else{
		g_warning("There is no wmv version information in MIME");
	}

	//check fourcc
	if(FALSE == gst_structure_get_fourcc(str, "fourcc", &f4cc)) {
		if(FALSE == gst_structure_get_fourcc(str, "format", &f4cc)) {
			f4cc = 0;
		}
	}
	if(f4cc != GST_STR_FOURCC("WMV3")) {
		GST_DEBUG_OBJECT(vmetadec, "No WMV3 fourcc in caps, probably is avidemux");
		assist_myecho("No WMV3 fourcc in caps, probably is avidemux\n");
	}

	if(FALSE == gst_structure_get_int(str, "width", &w)) {
		g_warning("Couldn't get width in mime type!");
		return FALSE;
	}
	vmetadec->DecVC1MPObj.iContentWidth = w;

	if(FALSE == gst_structure_get_int(str, "height", &h)) {
		g_warning("Couldn't get height in mime type!");
		return FALSE;
	}
	vmetadec->DecVC1MPObj.iContentHeight = h;

	if(G_LIKELY(TRUE == gst_structure_has_field(str, "codec_data"))) {
		const GValue *value = gst_structure_get_value(str, "codec_data");
		GstBuffer* buf = gst_value_get_buffer(value);
		unsigned char* pData = GST_BUFFER_DATA(buf);
		unsigned int sz = GST_BUFFER_SIZE(buf);
		if(sz < 4) {
			GST_ERROR_OBJECT(vmetadec, "codec_data size %d too small for VC1 SPMP", sz);
			assist_myecho("codec_data size %d too small for VC1 SPMP\n", sz);
			return FALSE;
		}
		if((pData[0]>>4) != 0 && (pData[0]>>4) != 4) {
			GST_ERROR_OBJECT(vmetadec, "Err: codec_data[0] is 0x%x, neither VC1 simple profile nor main profile, not supported", pData[0]);
			assist_myecho("Err: codec_data[0] is 0x%x, neither VC1 simple profile nor main profile, not supported\n", pData[0]);
			return FALSE;
		}
		store_codecdata(vmetadec, buf);
	}
	return TRUE;
}


static __inline void update_mpeg2_rapiddecodemode(Gstvmetadec *vmetadec);
static __inline int
consume_mpeg2_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
	if(vmetadec->DecMPEG2Obj.bSeqHdrReceived == 0 && 0 == parse_mpeg2seqinfo_fromstrem(vmetadec, cdata, csize)) {
		vmetadec->DecMPEG2Obj.bSeqHdrReceived = 1;
		update_mpeg2_rapiddecodemode(vmetadec);
	}
	fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
	return 0;
}

static gboolean
handle_common_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	if(G_UNLIKELY(FALSE == gst_structure_get_fraction(str, "framerate", &vmetadec->iFRateNum, &vmetadec->iFRateDen))) {
		vmetadec->iFRateNum = 0;
		vmetadec->iFRateDen = 1;
	}
	if(vmetadec->iFRateNum != 0) {
		vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
		vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
	}

	if(TRUE == gst_structure_has_field(str, "codec_data")) {
		const GValue *value = gst_structure_get_value(str, "codec_data");
		GstBuffer* buf = gst_value_get_buffer(value);
		store_codecdata(vmetadec, buf);
	}
	return TRUE;
}


static gboolean
handle_mpeg2_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	return handle_common_capinfo(str, vmetadec);
}

static __inline int
consume_mpeg4_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
	vmetadec->DecMPEG4Obj.iTimeIncBits = parse_mpeg4_TIB(cdata, csize, &vmetadec->DecMPEG4Obj.low_delay);
	fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
	return 0;
}

static gboolean
handle_mpeg4_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	return handle_common_capinfo(str, vmetadec);
}

static gboolean
handle_h263_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	return handle_common_capinfo(str, vmetadec);
}

static gboolean
handle_jpeg_capinfo(GstStructure* str, Gstvmetadec* vmetadec)
{
	return handle_common_capinfo(str, vmetadec);
}

static __inline int
consume_h263_codecdata(Gstvmetadec* vmetadec, unsigned char* cdata, unsigned int csize)
{
	fill_streams_in_repo(&vmetadec->StmRepo, cdata, csize);
	return 0;
}

static int digest_h264_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_mpeg2_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_mpeg4_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_vc1_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_vc1spmp_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_h263_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);
static int digest_mjpeg_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf);


static gboolean
gst_vmetadec_sinkpad_setcaps(GstPad *pad, GstCaps *caps)
{
	Gstvmetadec * vmetadec = GST_VMETADEC (GST_OBJECT_PARENT (pad));

	int i = 0;
	const gchar *name;
	GstStructure *str;
	int stru_num = gst_caps_get_size (caps);

	assist_myecho("gst_vmetadec_sinkpad_setcaps stru_num =%d\n", stru_num);
	if(stru_num != 1) {
		gchar* sstr;
		g_warning("Multiple MIME stream type in sinkpad caps, only select the first item.");
		for(i=0; i<stru_num; i++) {
			str = gst_caps_get_structure(caps, i);
			sstr = gst_structure_to_string(str);
			g_free(sstr);
		}
	}

	str = gst_caps_get_structure(caps, 0);

	//We won't use iWidth_InSinkCap/iHeight_InSinkCap to allocate memory, but use the information provided by vMeta codec. Because iWidth_InSinkCap/iHeight_InSinkCap probably don't exist, and probably isn't same as vMeta codec reported.
	//But iWidth_InSinkCap/iHeight_InSinkCap could be used to indicate the video resolution, though sometimes it's not very accurate.
	if(gst_structure_get_int(str, "width", &vmetadec->iWidth_InSinkCap) != TRUE) {
		vmetadec->iWidth_InSinkCap = 0;
	}
	if(gst_structure_get_int(str, "height", &vmetadec->iHeight_InSinkCap) != TRUE) {
		vmetadec->iHeight_InSinkCap = 0;
	}
	GST_DEBUG_OBJECT(vmetadec, "Get width %d, height %d in sinkpad caps", vmetadec->iWidth_InSinkCap, vmetadec->iHeight_InSinkCap);

	name = gst_structure_get_name(str);
	if(0 == strcmp(name, "video/x-h264")) {
		vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_H264;
		vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_H264;
		vmetadec->pfun_digest_inbuf = digest_h264_inbuf;
		return handle_264_capinfo(str, vmetadec);
	}else if(0 == strcmp(name, "video/mpeg")) {
		gint mpegversion;
		if(G_LIKELY(TRUE == gst_structure_get_int(str, "mpegversion", &mpegversion))) {
			switch(mpegversion) {
				case 2:
					vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_MPEG2;
					vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_MPG2;
					vmetadec->pfun_digest_inbuf = digest_mpeg2_inbuf;
					return handle_mpeg2_capinfo(str, vmetadec);
				case 4:
					vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_MPEG4;
					vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_MPG4;
					vmetadec->pfun_digest_inbuf = digest_mpeg4_inbuf;
					vmetadec->DecMPEG4Obj.CodecSpecies = 0;
					return handle_mpeg4_capinfo(str, vmetadec);
				case 1:
				default:
					g_warning("Unsupported mpeg version %d.", mpegversion);
					return FALSE;
			}
		}else{
			g_warning("No mpeg version information in sink pad caps.");
			return FALSE;
		}
	}else if(0 == strcmp(name, "video/x-divx") || 0 == strcmp(name, "video/x-xvid")) {
		vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_MPEG4;
		vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_MPG4;
		vmetadec->pfun_digest_inbuf = digest_mpeg4_inbuf;
		if(0 == strcmp(name, "video/x-xvid")) {
			vmetadec->DecMPEG4Obj.CodecSpecies = 3;
		}else{
			gint divxversion;
			if(TRUE != gst_structure_get_int(str, "divxversion", &divxversion)) {
				vmetadec->DecMPEG4Obj.CodecSpecies = 0;
			}else{
				vmetadec->DecMPEG4Obj.CodecSpecies = divxversion-3;
			}

		}
		return handle_mpeg4_capinfo(str, vmetadec);
	}else if(0 == strcmp(name, "video/x-wmv")) {
		guint32 f4cc;
		if(FALSE == gst_structure_get_fourcc(str, "fourcc", &f4cc)) {
			if(FALSE == gst_structure_get_fourcc(str, "format", &f4cc)) {
				f4cc = 0;
			}
		}

		if(f4cc == GST_STR_FOURCC("WVC1")) {
			vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_VC1;
			vmetadec->pfun_digest_inbuf = digest_vc1_inbuf;
			return handle_vc1wmv_capinfo(str, vmetadec);
		}else if(f4cc == GST_STR_FOURCC("WMV3")) {
			vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_VC1M;
			vmetadec->pfun_digest_inbuf = digest_vc1spmp_inbuf;
			return handle_vc1wmvSPMP_capinfo(str, vmetadec);
		}else{
			GST_DEBUG_OBJECT(vmetadec, "Neither WVC1 nor WMV3 in fourcc, probably it's avidemux, assume it's VC1 SPMP");
			assist_myecho("Neither WVC1 nor WMV3 in fourcc, probably it's avidemux, assume it's VC1 SPMP\n");
			vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_VC1M;
			vmetadec->pfun_digest_inbuf = digest_vc1spmp_inbuf;
			return handle_vc1wmvSPMP_capinfo(str, vmetadec);
		}
	}else if(0 == strcmp(name, "video/x-h263")) {
		vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_H263;
		vmetadec->pfun_digest_inbuf = digest_h263_inbuf;
		return handle_h263_capinfo(str, vmetadec);
	}else if(0 == strcmp(name, "video/x-jpeg")) {
		assist_myecho("sinkpad video caps is video/x-jpeg\n");
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
		vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_JPEG;
#endif
		vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_MJPG;
		vmetadec->pfun_digest_inbuf = digest_mjpeg_inbuf;
		return handle_jpeg_capinfo(str, vmetadec);
	}else if(0 == strcmp(name, "image/jpeg")) {
		assist_myecho("sinkpad video caps is image/jpeg\n");
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
		vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_JPEG;
#endif
		vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_MJPG;
		vmetadec->pfun_digest_inbuf = digest_mjpeg_inbuf;
		return handle_jpeg_capinfo(str, vmetadec);
	}else{
		g_warning("Unsupported stream MIME type %s.", name);
		return FALSE;
	}

	return FALSE;
}

static __inline void
uyvy_stride_compact(Ipp8u* pDst, Ipp8u* pSrc, int stride, int w, int h)
{
	int i;
	w<<=1;
	for(i=0;i<h;i++) {
		memcpy(pDst, pSrc, w);
		pSrc += stride;
		pDst += w;
	}
	return;
}



//LSB 8 bits indicate container format
#define GVMETA_FILE_MPG 				(0x1)
#define GVMETA_FILE_MPGTS   			(GVMETA_FILE_MPG | (0x1<<1))
#define GVMETA_FILE_MPGPS   			(GVMETA_FILE_MPG | (0x2<<1))
#define GVMETA_FILE_ASF 				(0x10)
#define GVMETA_FILE_AVI 				(0x20)
//16-8 bits indicate demuxer
#define GVMETA_DEMUXER_FF   			(0x1<<16)
#define GVMETA_DEMUXER_FLU  			(0x2<<16)
#define GVMETA_DEMUXER_QT   			(0x4<<16)


//define demuxer character id, the character id indicates this demuxer's special feature
#define ELEMENT_CHARID_UNDECIDED	0
#define ELEMENT_CHARID_UNKNOWN  	0x80000000  	//UNKNOWN means the species of element is unknown. The element still probably has some feature.
#define DEMUX_CHARID_FFMPEGTS   	(GVMETA_FILE_MPGTS | GVMETA_DEMUXER_FF)
#define DEMUX_CHARID_FFMPEGPS   	(GVMETA_FILE_MPGPS | GVMETA_DEMUXER_FF)
#define DEMUX_CHARID_FLUTS  		(GVMETA_FILE_MPGTS | GVMETA_DEMUXER_FLU)
#define DEMUX_CHARID_FLUPS  		(GVMETA_FILE_MPGPS | GVMETA_DEMUXER_FLU)
#define DEMUX_CHARID_MPEGTS 		DEMUX_CHARID_FLUTS  								//flutsdemux and mpegtsdemux have same behaivor, therefore, we think thay are the same demuxer and has same character id
#define DEMUX_CHARID_MPEGPS 		DEMUX_CHARID_FLUPS  								//flupsdemux and mpegpsdemux have same behaivor, therefore, we think thay are the same demuxer and has same character id
#define DEMUX_CHARID_FFMPEGASF  	(GVMETA_FILE_ASF   | GVMETA_DEMUXER_FF)
#define DEMUX_CHARID_AVI			(GVMETA_FILE_AVI   | 0)
#define DEMUX_CHARID_FFMPEGAVI  	(GVMETA_FILE_AVI   | GVMETA_DEMUXER_FF)
#define DEMUX_CHARID_QT 			(0  			   | GVMETA_DEMUXER_QT)


#define GVMETA_FILEIS_AVI(vdec) 			((GVMETA_FILE_AVI & query_adjacent_nonqueue_element_species(vdec, 0)) == GVMETA_FILE_AVI)
#define GVMETA_FILEIS_TS(vdec)  			((GVMETA_FILE_MPGTS & query_adjacent_nonqueue_element_species(vdec, 0)) == GVMETA_FILE_MPGTS)
#define GVMETA_FILEIS_PS(vdec)  			((GVMETA_FILE_MPGPS & query_adjacent_nonqueue_element_species(vdec, 0)) == GVMETA_FILE_MPGPS)
#define GVMETA_DEMUXERIS_FF(vdec)   		((GVMETA_DEMUXER_FF & query_adjacent_nonqueue_element_species(vdec, 0)) == GVMETA_DEMUXER_FF)
#define GVMETA_DEMUXERIS_FLUTS(vdec)		(query_adjacent_nonqueue_element_species(vdec, 0) == DEMUX_CHARID_FLUTS)
#define GVMETA_DEMUXERIS_FFMPEGASF(vdec)	(query_adjacent_nonqueue_element_species(vdec, 0) == DEMUX_CHARID_FFMPEGASF)
#define GVMETA_DEMUXERIS_QT(vdec)   		(query_adjacent_nonqueue_element_species(vdec, 0) == DEMUX_CHARID_QT)


#define FLUPSDEMUX_TYPENAME 		"GstFluPSDemux"
#define FLUTSDEMUX_TYPENAME 		"GstFluTSDemux"
#define MPEGPSDEMUX_TYPENAME		"GstMpegPSDemux"
#define MPEGTSDEMUX_TYPENAME		"GstMpegTSDemux"
#define FFMPEGPSDEMUX_TYPENAME  	"ffdemux_mpeg"
#define FFMPEGTSDEMUX_TYPENAME  	"ffdemux_mpegts"
#define FFMPEGASFDEMUX_TYPENAME 	"ffdemux_asf"
#define FFMPEGAVIDEMUX_TYPENAME 	"ffdemux_avi"
#define AVIDEMUX_TYPENAME   		"GstAviDemux"
#define QTDEMUX_TYPENAME			"GstQTDemux"

static int
query_adjacent_nonqueue_element_species(Gstvmetadec* vmetadec, int direction)   //direction=0 upstream, direction=1 downstream
{
	int* species = direction==0 ?  &vmetadec->UpAdjacentNonQueueEle : &vmetadec->DownAdjacentNonQueueEle;

	if(*species != ELEMENT_CHARID_UNDECIDED) {
		return *species;
	}else{
		char* padname = direction==0 ? "sink" : "src";
		GstElement* ele = GST_ELEMENT_CAST(vmetadec);
		GstPad* pad;
		GstPad* peerPad;
		GstObject* peerEleObj;
		GstElement* peerEle;
		const gchar* peerEleTname;
		int eleIsQueue = 0;
		GstIterator *iter = NULL;
		gpointer pTmpEle;

		do{
			if(eleIsQueue != 2) {
				pad = gst_element_get_static_pad(ele, padname);
				peerPad = gst_pad_get_peer(pad);
				gst_object_unref(pad);
			}else{
				if(direction == 0) {
					iter = gst_element_iterate_sink_pads(ele);
				}else{
					iter = gst_element_iterate_src_pads(ele);
				}
				if(GST_ITERATOR_OK != gst_iterator_next(iter, &pTmpEle)) {
					gst_iterator_free(iter);
					*species = ELEMENT_CHARID_UNKNOWN;
					return ELEMENT_CHARID_UNKNOWN;
				}
				pad = (GstPad*)pTmpEle;
				peerPad = gst_pad_get_peer(pad);
				gst_object_unref(pad);
				gst_iterator_free(iter);
			}
			if(peerPad == NULL) {
				*species = ELEMENT_CHARID_UNKNOWN;
				return ELEMENT_CHARID_UNKNOWN;
			}
			peerEleObj = GST_OBJECT_PARENT(peerPad);
			if(! GST_IS_ELEMENT(peerEleObj)) {  //sometimes, the peerPad's parent isn't an element, it may be ghostpad or something else.
				gst_object_unref(peerPad);
				*species = ELEMENT_CHARID_UNKNOWN;
				return ELEMENT_CHARID_UNKNOWN;
			}
			peerEle = GST_ELEMENT(peerEleObj);
			gst_object_unref(peerPad);
			peerEleTname = G_OBJECT_TYPE_NAME(peerEle);
			ele = peerEle;
			assist_myecho("direct %d, %s\n",direction, peerEleTname);
			if(strcmp(peerEleTname, "GstQueue") == 0) {
				eleIsQueue = 1;
			}else if(strcmp(peerEleTname, "GstMultiQueue") == 0) {
				eleIsQueue = 2;
			}else{
				eleIsQueue = 0;
			}
		}while(eleIsQueue != 0);

		if( strcmp(peerEleTname, FFMPEGPSDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_FFMPEGPS;
		}else if( strcmp(peerEleTname, FFMPEGTSDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_FFMPEGTS;
		}else if( strcmp(peerEleTname, FLUPSDEMUX_TYPENAME) == 0) {
			*species = DEMUX_CHARID_FLUPS;
		}else if( strcmp(peerEleTname, FLUTSDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_FLUTS;
		}else if( strcmp(peerEleTname, MPEGPSDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_MPEGPS;
		}else if( strcmp(peerEleTname, MPEGTSDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_MPEGTS;
		}else if( strcmp(peerEleTname, FFMPEGASFDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_FFMPEGASF;
		}else if( strcmp(peerEleTname, AVIDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_AVI;
		}else if( strcmp(peerEleTname, FFMPEGAVIDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_FFMPEGAVI;
		}else if( strcmp(peerEleTname, QTDEMUX_TYPENAME) == 0 ) {
			*species = DEMUX_CHARID_QT;
		}else{
			*species = ELEMENT_CHARID_UNKNOWN;
		}
		assist_myecho("Adjacent element at direction %s is %s(id 0x%x).\n", direction==0?"up":"down", peerEleTname, *species);
		return *species;
	}

}

static void
framebuf_consumed_by_downstream(gpointer data)
{
	Gstvmetadec* vmetadec = (Gstvmetadec*)(((IppVmetaPicture*)data)->pUsrData3);
	g_mutex_lock( vmetadec->FrameRepo.RepoMtx );
	VMETA_PICBUF_SETIDLE(data); //mark the buffer is idle
	if(vmetadec->FrameRepo.NotNeedFMemAnyMore == 1) {
		frames_repo_recycle(&vmetadec->FrameRepo);  //if not need frame buffer any more, we free the buffer as early as possible
	}
	g_mutex_unlock( vmetadec->FrameRepo.RepoMtx );

	//finally, below operation cause OSFRAMERES_WATCHMAN's finalization and vmetadec's finalization
	OSFRAMERES_WATCHMAN_UNREF(vmetadec->osframeres_finalizer);
	return;
}

#define IPPGST_VMETA_IS_INTERLACEFRAME(pPicDataInfo)		((pPicDataInfo)->pic_type==1 || (pPicDataInfo)->pic_type==2)
#ifdef VMETA_FRAME_CODEDTYPE_ENABLED
#define IPPGST_VMETA_IS_IFRAME(pPicDataInfo)				((pPicDataInfo)->coded_type[0] == 0 || (pPicDataInfo)->coded_type[1] == 0)
#else
#define IPPGST_VMETA_IS_IFRAME(pPicDataInfo)				(1)
#endif
static GstFlowReturn
vmetadec_push_frame_to_downElement(Gstvmetadec *vmetadec, IppVmetaPicture* pFrame, GstClockTime* ts, GstClockTime* duration)
{
	GstFlowReturn ret = GST_FLOW_OK;
	GstBuffer *down_buf = NULL;
	unsigned char* pDisplayStart = ((Ipp8u*)pFrame->pic.ppPicPlane[0]) + (pFrame->pic.picROI.y)*(pFrame->pic.picPlaneStep[0]) + ((pFrame->pic.picROI.x)<<1);

	//printf("push frame to down ++++++++++++++\n");

	if((0 >= pFrame->nDataLen) && (pFrame->PicDataInfo.is_skipped) ) {
		VMETA_PICBUF_SETIDLE(pFrame);
#ifdef PROFILER_EN
			vmetadec->nSkippedFrames++;
			unsigned long long  tPrePadpush = 0, tAfPadpush ,tChain4frame;

			measure_chaintime_for1frame(vmetadec);
			measure_codectime_push1frame(vmetadec);
			measure_pushtime0(vmetadec);
			//start win stastic
			if(vmetadec->ProfilerEn){
				tPrePadpush = TimeGetTickCountL();
			}

			//fake push, gst_pad_push...

			measure_pushtime1(vmetadec);
			if(vmetadec->ProfilerEn){
				tAfPadpush = TimeGetTickCountL();
				tChain4frame = get_codectime_push1frame(vmetadec);
				if (vmetadec->sFpsWin[D_SKIP].bAdvanSync || vmetadec->sFpsWin[D_CPUFREQ].bAdvanSync){
					AdvanSync_VmetaDec(vmetadec,tChain4frame,(tAfPadpush-tPrePadpush));
				}
			}
			//end  win stastic
#endif
			return GST_FLOW_OK;
		}


	if(! vmetadec->KeepDecFrameLayout) {
		if(pFrame->pic.picPlaneStep[0]==(vmetadec->iFrameContentWidth<<1)) {

		}else{//stride change is needed
			assist_myechomore("stride compact occur\n");
			IppVmetaPicture* pDownFrame = rent_frame_from_repo(&vmetadec->FrameRepo, vmetadec->iFrameCompactSize, NULL, vmetadec);
			if(pDownFrame == NULL) {
				VMETA_PICBUF_SETIDLE(pFrame);   //give up this frame. Not use RepoMtx, because rent_frame_from_repo() won't compete with this operation.
				g_warning("Couldn't allocate memory to do compact!");
				GST_WARNING_OBJECT(vmetadec, "Couldn't allocate memory to do compact!");
				return ret;
			}
			uyvy_stride_compact(pDownFrame->pBuf, pDisplayStart, pFrame->pic.picPlaneStep[0], vmetadec->iFrameContentWidth, vmetadec->iFrameContentHeight);
			pDownFrame->pic.picPlaneStep[0] = vmetadec->iFrameContentWidth<<1;
			pDownFrame->pic.picWidth = vmetadec->iFrameContentWidth;
			pDownFrame->pic.picHeight = vmetadec->iFrameContentHeight;
			pDownFrame->pic.ppPicPlane[0] = pDownFrame->pBuf;
			pDownFrame->pic.picROI.x = 0;
			pDownFrame->pic.picROI.y = 0;
			pDownFrame->pic.picROI.width = vmetadec->iFrameContentWidth;
			pDownFrame->pic.picROI.height = vmetadec->iFrameContentHeight;
			//copy pic data information
			memcpy(&pDownFrame->PicDataInfo, &pFrame->PicDataInfo, sizeof(IppPicDataInfo));

			pDisplayStart = pDownFrame->pBuf;
			VMETA_PICBUF_SETIDLE(pFrame);   //Not use RepoMtx, because rent_frame_from_repo() won't compete with this operation.
			pFrame = pDownFrame;
		}
	}

	down_buf = gst_buffer_new();
	if(vmetadec->KeepDecFrameLayout) {
		//for mmp2,mg1
		IPPGSTDecDownBufSideInfo* downbufsideinfo = (IPPGSTDecDownBufSideInfo*)(pFrame->pUsrData2);
		downbufsideinfo->x_off = pFrame->pic.picROI.x;
		downbufsideinfo->y_off = pFrame->pic.picROI.y;
		downbufsideinfo->x_stride = pFrame->pic.picPlaneStep[0];
		downbufsideinfo->y_stride = GST_ROUND_UP_4(pFrame->pic.picROI.height + pFrame->pic.picROI.y);
		downbufsideinfo->NeedDeinterlace = (int)IPPGST_VMETA_IS_INTERLACEFRAME(&(pFrame->PicDataInfo));
		downbufsideinfo->phyAddr = (unsigned int)(pFrame->pic.ppPicPlane[0]) - (unsigned int)(pFrame->pBuf) + (unsigned int)(pFrame->nPhyAddr);
		downbufsideinfo->virAddr = pFrame->pic.ppPicPlane[0];
		GST_BUFFER_DATA(down_buf) = pFrame->pic.ppPicPlane[0];
		GST_BUFFER_SIZE(down_buf) = downbufsideinfo->x_stride*downbufsideinfo->y_stride;
		IPPGST_BUFFER_CUSTOMDATA(down_buf) = (gpointer) downbufsideinfo;
	}else{
		//only for dove ubuntu
		GST_BUFFER_DATA(down_buf) = pDisplayStart;
		GST_BUFFER_SIZE(down_buf) = vmetadec->iFrameCompactSize;
		IPPGST_BUFFER_CUSTOMDATA(down_buf) = (gpointer)IPPGST_VMETA_IS_INTERLACEFRAME(&(pFrame->PicDataInfo));
	}
	assist_myechomore("----pad_pushed frame pictype %d,%d, interlace %d\n", pFrame->PicDataInfo.coded_type[0], pFrame->PicDataInfo.coded_type[1], IPPGST_VMETA_IS_INTERLACEFRAME(&(pFrame->PicDataInfo)));

	gst_buffer_set_caps(down_buf, GST_PAD_CAPS(vmetadec->srcpad));
	GST_BUFFER_MALLOCDATA(down_buf) = (guint8*)pFrame;
	GST_BUFFER_FREE_FUNC(down_buf) = (GFreeFunc)framebuf_consumed_by_downstream;
	pFrame->pUsrData3 = (void*)vmetadec;	//NOTE: pUsrData3 act as different role at different time.
	OSFRAMERES_WATCHMAN_REF(vmetadec->osframeres_finalizer);

	GST_BUFFER_TIMESTAMP(down_buf) = *ts;
	GST_BUFFER_DURATION(down_buf) = *duration;


#ifdef DEBUG_LOG_SOMEINFO
	((VmetaDecProbeData*)vmetadec->pProbeData)->_VDECPushedFrameCnt++;
#endif
	//fprintf(myfp3, "ts %lld\n", GST_BUFFER_TIMESTAMP(down_buf));
	//printf("++++++++++++++\n");
	vmetadec->iNewSegPushedFrameCnt++;
	assist_myechomore("out ts %lld, du %lld\n", GST_BUFFER_TIMESTAMP(down_buf), GST_BUFFER_DURATION(down_buf));

	//start win stastic
	unsigned long long  tPrePadpush = 0, tAfPadpush ,tChain4frame;
	measure_chaintime_for1frame(vmetadec);
	measure_codectime_push1frame(vmetadec);
	measure_pushtime0(vmetadec);
#ifdef PROFILER_EN
	if(vmetadec->ProfilerEn){
		tPrePadpush = TimeGetTickCountL();
	}
#endif
	ret = gst_pad_push(vmetadec->srcpad, down_buf);

	measure_pushtime1(vmetadec);
#ifdef PROFILER_EN
	if(vmetadec->ProfilerEn){
		tAfPadpush = TimeGetTickCountL();
		tChain4frame = get_codectime_push1frame(vmetadec);
		if (vmetadec->sFpsWin[D_SKIP].bAdvanSync || vmetadec->sFpsWin[D_CPUFREQ].bAdvanSync){
			AdvanSync_VmetaDec(vmetadec,tChain4frame,(tAfPadpush-tPrePadpush));
		}
	}
#endif
	//end win stastic

	if (GST_FLOW_OK != ret) {
		if(GST_FLOW_WRONG_STATE != ret) {//during seeking, sink element often return GST_FLOW_WRONG_STATUS. No need to echo this message
			g_warning("The decoded frame did not successfully push out to downstream element, ret %d", ret);
			GST_ERROR_OBJECT (vmetadec, "The decoded frame did not successfully push out to downstream element, ret %d", ret);
		}
		return ret;
	}


	return GST_FLOW_OK;
}

static __inline void
update_mpeg2_rapiddecodemode(Gstvmetadec *vmetadec)
{
	//rapiddecoding could reduce the decoding delay, but cost more time to scan bits if demuxer doesn't provide data picture by picture. For mpeg2 PS, if demuxer couldn't provide data picture by picture, scanning bitstream will cost additional time.
	if(GVMETA_DEMUXERIS_FF(vmetadec) || GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
		vmetadec->bAppendStuffbytes = 1;
	}else{
		if(vmetadec->DisableMpeg2Packing == 0) {
			vmetadec->bAppendStuffbytes = vmetadec->DecMPEG2Obj.iNorminalW*vmetadec->DecMPEG2Obj.iNorminalH <= BIGVIDEO_DIM || vmetadec->DecMPEG2Obj.iNorminalBitRate <= STREAM_VDECBUF_SIZE*8*25 ? 1 : 0;
			//in fact, it should be STREAM_VDECBUF_SIZE/vmetadec->DecMPEG2Obj.iNorminalBitRate >= some duration, it means there is too much compressed frames in on stream buffer

			//because no (GVMETA_DEMUXERIS_FF(vmetadec) || GVMETA_DEMUXERIS_FLUTS(vmetadec)), must be mpegpsdemux/flupsdemux
			if(vmetadec->DecMPEG2Obj.iNorminalW*vmetadec->DecMPEG2Obj.iNorminalH >= 1920*1080 && vmetadec->DecMPEG2Obj.iNorminalBitRate >= 20000000) {
				vmetadec->bAppendStuffbytes = 0;
				vmetadec->HungryStrategy = 1;   //for those high bps big mpeg2 stream in flu PS, always use hungry strategy
				assist_myecho("because it's %d x %d, norminal bps %d mpeg2 PS demuxed by flu stream, disable parse&enable hungry.\n", vmetadec->DecMPEG2Obj.iNorminalW, vmetadec->DecMPEG2Obj.iNorminalH, vmetadec->DecMPEG2Obj.iNorminalBitRate);
			}
		}else{
			vmetadec->bAppendStuffbytes = 0;
		}
	}

	assist_myecho("Appending stuff bytes to force vMeta decode rapiddly is %s\n", vmetadec->bAppendStuffbytes==0? "Disabled":"Enabled");
	return;
}




static void
update_vmetadec_srcpad_cap(Gstvmetadec *vmetadec)
{
	GstCaps *Tmp;
	Tmp= gst_caps_new_simple ("video/x-raw-yuv",
			"format", GST_TYPE_FOURCC, GST_STR_FOURCC ("UYVY"),
			"width", G_TYPE_INT, vmetadec->iFrameContentWidth,
			"height", G_TYPE_INT, vmetadec->iFrameContentHeight,
			"framerate", GST_TYPE_FRACTION, vmetadec->iFRateNum, vmetadec->iFRateDen,
			NULL);


	gst_pad_set_caps(vmetadec->srcpad, Tmp);
	gst_caps_unref(Tmp);
	assist_myecho("update vmetadec srcpad's cap, framerate num %d, framerate den %d, w %d, h %d\n", vmetadec->iFRateNum, vmetadec->iFRateDen,vmetadec->iFrameContentWidth,vmetadec->iFrameContentHeight);
	return;
}




static void
output_checksum(IppVmetaPicture *pPic, FILE *f)
{
	 if (1 == pPic->PicDataInfo.pic_type) {
		/*top field*/
		fprintf(f, "CHKSUM>> [T] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
				pPic->PicDataInfo.chksum_data[0][0],
				pPic->PicDataInfo.chksum_data[0][1],
				pPic->PicDataInfo.chksum_data[0][2],
				pPic->PicDataInfo.chksum_data[0][3],
				pPic->PicDataInfo.chksum_data[0][4],
				pPic->PicDataInfo.chksum_data[0][5],
				pPic->PicDataInfo.chksum_data[0][6],
				pPic->PicDataInfo.chksum_data[0][7]);

		/*bottom field*/
		fprintf(f, "CHKSUM>> [B] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
				pPic->PicDataInfo.chksum_data[1][0],
				pPic->PicDataInfo.chksum_data[1][1],
				pPic->PicDataInfo.chksum_data[1][2],
				pPic->PicDataInfo.chksum_data[1][3],
				pPic->PicDataInfo.chksum_data[1][4],
				pPic->PicDataInfo.chksum_data[1][5],
				pPic->PicDataInfo.chksum_data[1][6],
				pPic->PicDataInfo.chksum_data[1][7]);
	} else {
		/*non-interlaced frame, or interlaced frame, or non-paired field*/
		fprintf(f, "CHKSUM>> [F] %08x-%08x-%08x-%08x-%08x-%08x-%08x-%08x\n",
				pPic->PicDataInfo.chksum_data[0][0],
				pPic->PicDataInfo.chksum_data[0][1],
				pPic->PicDataInfo.chksum_data[0][2],
				pPic->PicDataInfo.chksum_data[0][3],
				pPic->PicDataInfo.chksum_data[0][4],
				pPic->PicDataInfo.chksum_data[0][5],
				pPic->PicDataInfo.chksum_data[0][6],
				pPic->PicDataInfo.chksum_data[0][7]);
	}
	return;

}

static void
clear_candidate_outputframes(Gstvmetadec *vmetadec, int bDoPopTs)
{
	IppVmetaPicture* pFrame;
	for(;;){
		pFrame=g_queue_pop_tail(&vmetadec->OutFrameQueue);
		if(pFrame == NULL) {
			return;
		}
		if(vmetadec->ChkSumFile != NULL){
			output_checksum(pFrame,vmetadec->ChkSumFile);
		}

		//If upstream demuxer is ffdemux_asf, don't adopt TS auto increase method when Lacking TS in TS sync mode.
		//Because the ffdemux_asf's framerate are totally wrong, and it provide TS as 0, -1, -1, ...
		if(bDoPopTs != 0) {
			pop_TS(&vmetadec->TSDUManager, !GVMETA_DEMUXERIS_FFMPEGASF(vmetadec));
		}
		VMETA_PICBUF_SETIDLE(pFrame);   //give up this frame
	}
	return;
}


static GstFlowReturn
output_frames_to_downstream(Gstvmetadec *vmetadec, int outputcnt, GstFlowReturn PadPushRet, int bEos)   //this function shouldn't be in lock state of DecMutex, because gst_pad_push is in it
{
	IppVmetaPicture* pFrame;
	int i;
	for(i=0;i<outputcnt;i++){
		pFrame=g_queue_pop_tail(&vmetadec->OutFrameQueue);
		if(pFrame == NULL) {
			break;
		}

		if(vmetadec->ChkSumFile != NULL){
			output_checksum(pFrame,vmetadec->ChkSumFile);
		}

		if(G_UNLIKELY((int)pFrame->pUsrData3 != vmetadec->iSegmentSerialNum)) {
			VMETA_PICBUF_SETIDLE(pFrame);   //give up this frame
			assist_myecho("One frame is given up because it doesn't belong to current segment!\n");
			continue;
		}

		//for mpeg2 in .mpg, sometimes, demux only provide TS for some frame not for all frames. Therefore, when want to get output TS, probably no TS. Because decoding delay exists, when some frame is outputting, it will adopt subsequent frame's TS but not	its	TS(demuxer	missing	its	TS).	To	avoid	this	case,	adopt	TS	auto	increasing	method.
		if(vmetadec->TSDUManager.mode == 0 && vmetadec->TSDUManager.iTDListLen == 0 && vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_MPG2 && GVMETA_FILEIS_PS(vmetadec)) {
			assist_myecho("TS mode changed to 1 because mpeg2 in .mpg TS gap found\n");
			vmetadec->TSDUManager.mode = 1;
		}
		//If upstream demuxer is ffdemux_asf, don't adopt TS auto increase method when Lacking TS in TS sync mode.
		//Because the ffdemux_asf's framerate are totally wrong, and it provide TS as 0, -1, -1, ...
		pop_TS(&vmetadec->TSDUManager, !GVMETA_DEMUXERIS_FFMPEGASF(vmetadec));



		if(vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_MPG2 && vmetadec->NotDispPBBeforeI_MPEG2 && pFrame->nDataLen > 0) {
			if(vmetadec->bNotDispFrameInNewSeg) {
				if(IPPGST_VMETA_IS_IFRAME(&(pFrame->PicDataInfo))) {
					vmetadec->bNotDispFrameInNewSeg = 0;
				}else{
					VMETA_PICBUF_SETIDLE(pFrame);   //give up this frame
#ifdef VMETA_FRAME_CODEDTYPE_ENABLED
					assist_myecho("One frame is given up because it isn't I frame(%d, %d) in a new segment!\n", pFrame->PicDataInfo.coded_type[0], pFrame->PicDataInfo.coded_type[1]);
#endif
					continue;
				}
			}

		}


#ifdef VDECLOOP_DOMORECHECK
		if(G_UNLIKELY((pFrame->pic.picROI.width<<1) > pFrame->pic.picPlaneStep[0])) {
			VMETA_PICBUF_SETIDLE(pFrame);   //give up this frame
			g_warning("The ROI's width is larger than the stride, give up this frame.");
			continue;
		}
#endif

		if(G_UNLIKELY(PadPushRet !=  GST_FLOW_OK)) {
			VMETA_PICBUF_SETIDLE(pFrame);   //if previous push not successful, give up this frame
			continue;
		}

		if((vmetadec->iFrameContentWidth != pFrame->pic.picROI.width || vmetadec->iFrameContentHeight != pFrame->pic.picROI.height || vmetadec->iFrameStride != pFrame->pic.picPlaneStep[0]) && pFrame->nDataLen > 0) {
			vmetadec->iFrameContentWidth = pFrame->pic.picROI.width;
			vmetadec->iFrameContentHeight = pFrame->pic.picROI.height;
			vmetadec->iFrameCompactSize = ((pFrame->pic.picROI.width * pFrame->pic.picROI.height)<<1);  //only support UYVY
			vmetadec->iFrameStride = pFrame->pic.picPlaneStep[0];
			vmetadec->vMetaDecFrameROI.x = pFrame->pic.picROI.x;
			vmetadec->vMetaDecFrameROI.y = pFrame->pic.picROI.y;
			vmetadec->vMetaDecFrameROI.width = pFrame->pic.picROI.width;
			vmetadec->vMetaDecFrameROI.height = pFrame->pic.picROI.height;

			update_vmetadec_srcpad_cap(vmetadec);
			assist_myecho("!!!!Pop frame's size from vMeta Decoder is changed!!!\n");
		}

#define WORKROUND_QTDEMUX_BFRAME_LASTFRAME
#ifdef WORKROUND_QTDEMUX_BFRAME_LASTFRAME
		//For B frame h264 stream, the last frame's timestamp provided by qtdemuxer isn't less than the stop time of current segment data provided by qtdemxer. Therefore, the basesink thinks the last frame exceed the current segment data's range, and ignore	the	last	frame.
		//To fix it, decrease the last frame's timestamp a little. 2009.09.25, decrease a little is useless for some stream, for those streams, the last frame's timestamp exceed the segment's duration a lot(those stream is edited by some edit tool, therefore,	before	the	last	frame,	some	frame	are	eliminated.	however,	the	segment's	duration	is	calculated	through	the	frame	count	in	the	segment).	To	work	round,	force	the	last	frame's	timestamp	to	GST_CLOCK_TIME_NONE.
		//Qtdemuxer 0.10.14 has this issue. ffdemuxer hasn't this issue. But ffdemuxer(0.10.6.2) couldn't provide right framerate, it always provides field rate.
		//And for some wrong muxed .mp4 file, has this issue.
		//This workround hasn't side effect for other format stream(like mpeg2, vc1). Therefore, we don't distinguish stream format and demuxer type here.
		if(vmetadec->TSDUManager.SegmentEndTs > 0 && vmetadec->TSDUManager.TsLatestOut != GST_CLOCK_TIME_NONE) {
			if((gint64)vmetadec->TSDUManager.TsLatestOut >= vmetadec->TSDUManager.SegmentEndTs) {
				assist_myecho("------------- found some TS(%lld) exceed segment duration(stop %lld), force it to %lld\n", vmetadec->TSDUManager.TsLatestOut, vmetadec->TSDUManager.SegmentEndTs, GST_CLOCK_TIME_NONE);
				vmetadec->TSDUManager.TsLatestOut = GST_CLOCK_TIME_NONE;
			}
		}else if(bEos && vmetadec->OutFrameQueue.length == 0) {
			assist_myecho("---------- last push down frame origin ts %lld\n", vmetadec->TSDUManager.TsLatestOut);
			//The last frame for this stream.
			if(vmetadec->TSDUManager.TsLatestOut != GST_CLOCK_TIME_NONE) {
				//vmetadec->TSDUManager.TsLatestOut = vmetadec->TSDUManager.TsLatestOut - 100;
				vmetadec->TSDUManager.TsLatestOut = GST_CLOCK_TIME_NONE;
				assist_myecho("---------- last push down frame changed TS is %lld\n", vmetadec->TSDUManager.TsLatestOut);
			}
		}
#endif
		PadPushRet = vmetadec_push_frame_to_downElement(vmetadec, pFrame, &vmetadec->TSDUManager.TsLatestOut, &vmetadec->TSDUManager.DurationLatestOut);
	}
	return PadPushRet;
}

//rearrange_staticstmrepo() let full-filled stream on the begin of Array, let empty stream on the end of Array.
static int
rearrange_staticstmrepo(VmetaDecStreamRepo* Repo)
{
	int i, j, k;
	IppVmetaBitstream* Array_idle = Repo->Array == Repo->Array0 ? Repo->Array1 : Repo->Array0;

	j = 0;
	k = Repo->Count;
	for(i=0;i<Repo->Count;i++) {
		if(Repo->Array[i].pUsrData0 == (void*)Repo->Array[i].nBufSize) {
			Array_idle[j] = Repo->Array[i];
			if(Repo->Array[i].pUsrData1) {
				((IppVmetaBitstream*)Repo->Array[i].pUsrData1)->pUsrData1 = &Array_idle[j]; //update the pushed Node
			}
			if(Repo->nNotPushedStreamIdx == i) {
				Repo->nNotPushedStreamIdx = j;
			}
			j++;
		}else if(Repo->Array[i].pUsrData0 == (void*)0) {
			k--;
			Array_idle[k] = Repo->Array[i];
			if(Repo->Array[i].pUsrData1) {
				((IppVmetaBitstream*)Repo->Array[i].pUsrData1)->pUsrData1 = &Array_idle[k]; //update the pushed Node
			}
		}
	}
	if(Repo->Array[Repo->nWaitFillCandidateIdx].pUsrData0 == (void*)0 || Repo->Array[Repo->nWaitFillCandidateIdx].pUsrData0 == (void*)Repo->Array[Repo->nWaitFillCandidateIdx].nBufSize) {
		Repo->nWaitFillCandidateIdx = k;
	}else{
		Array_idle[k-1] = Repo->Array[Repo->nWaitFillCandidateIdx];
		Repo->nWaitFillCandidateIdx = k-1;
	}
	if(Repo->nPushReadyCnt == 0) {
		Repo->nNotPushedStreamIdx = Repo->nWaitFillCandidateIdx;
	}
	//flip Array
	Repo->Array = Array_idle;
	return 0;
}

static __inline int
pop_streambuf_from_codec(Gstvmetadec* vmetadec, void * pVdecObj)
{
	void* pPopTmp;
	IppVmetaBitstream*  pStream;
	IppVmetaBitstream*  pStmA;
	int bPopedStaticStm = 0;
	int iPopedStmCnt = 0;
	for(;;) {
		DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, &pPopTmp, pVdecObj);
		pStream = (IppVmetaBitstream*)pPopTmp;
		if(pStream == NULL) {
			//all stream are popped
			break;
		}
		iPopedStmCnt++;
		assist_myechomore("-- Pop STM static/dync(%d)++++++++++++++++++++\n", VMETA_STMBUF_ISSTATICSTM(pStream));

		if(VMETA_STMBUF_ISUNITEND(pStream)) {
			VMETA_STMBUF_SETISNOTUNITEND(pStream);
			if(vmetadec->CumulateThreshold > 0) {
				vmetadec->iInFrameCumulated2--;
			}
		}

		if(VMETA_STMBUF_ISSTATICSTM(pStream)) {
			//the stream buffer come from static stream repo
			bPopedStaticStm = 1;

			pStmA = pStream->pUsrData1;
			pStream->pUsrData0 = (void*)0;
			pStream->pUsrData1 = (void*)0;
			if(pStmA) {
				pStmA->pUsrData0 = (void*)0;
				pStmA->pUsrData1 = (void*)0;
				VMETA_STMBUF_SETISNOTUNITEND(pStmA);
				pStmA->nOffset = 0;
				pStmA->nDataLen = 0;
				pStmA->nFlag = 0;
			}
			vmetadec->StmRepo.TotalSpareSz += pStream->nBufSize;
			assist_myechomore("-- Pop STM spare left %f\n", vmetadec->StmRepo.TotalSpareSz/(float)vmetadec->StmRepo.TotalBufSz);
		}else{
			//the stream buffer come from dynamic stream repo
			pStream->nDataLen = 0;
			pStream->nOffset = 0;
			pStream->nFlag = 0;
			VMETA_DYNCSTMBUF_SETIDLE(pStream);
		}
	}

	if(bPopedStaticStm) {
		rearrange_staticstmrepo(&vmetadec->StmRepo);
	}
	return iPopedStmCnt;
}



static IppCodecStatus
loop_vdec_until(Gstvmetadec *vmetadec, IppCodecStatus StopCond, int* pbWantFlushFrame, int ClearOutFrame)
{
	IppCodecStatus VDecRtCode;
	void * pVdecObj = vmetadec->pVDecoderObj;
	int iPopStmCnt = 0;
	int iPopFrameCnt = 0;

	struct timeval clk0, clk1;  //only be used when defined DEBUG_PERFORMANCE or MEASURE_CODEC_SYST_FORPUSH

	*pbWantFlushFrame = 0;

#ifdef ENABLE_VDEC_NONBLOCK_CALLING
	if((StopCond!=IPP_STATUS_WAIT_FOR_EVENT && vmetadec->bVmetadecRetAtWaitEvent!=0) || (StopCond==IPP_STATUS_WAIT_FOR_EVENT && vmetadec->bVmetadecRetAtWaitEvent==0)) {
		vmetadec->bVmetadecRetAtWaitEvent = ! vmetadec->bVmetadecRetAtWaitEvent;
		assist_myechomore("VMETADEC whether to wait event change, bVmetadecRetAtWaitEvent == %d\n", vmetadec->bVmetadecRetAtWaitEvent);
		DecodeSendCmd_Vmeta(IPPVC_SET_INTWAITEVENTDISABLE, &(vmetadec->bVmetadecRetAtWaitEvent), NULL, pVdecObj);
	}
#endif

	for(;;) {

		measure_codectime0(&clk0);

#ifdef MEASURE_CODEC_THREADT
		syscall(__NR_getrusage, RUSAGE_THREAD, &((VmetaDecProbeData*)vmetadec->pProbeData)->codecThrClk0);
#endif
		assist_myechomore("Call DecodeFrame_Vmeta begin 0x%x\n", (unsigned int)pVdecObj);
		VDecRtCode = DecodeFrame_Vmeta(&(vmetadec->VDecInfo), pVdecObj);
		assist_myechomore("Call DecodeFrame_Vmeta end 0x%x, ret %d\n", (unsigned int)pVdecObj, VDecRtCode);

#ifdef MEASURE_CODEC_THREADT
		syscall(__NR_getrusage, RUSAGE_THREAD, &((VmetaDecProbeData*)vmetadec->pProbeData)->codecThrClk1);
		((VmetaDecProbeData*)vmetadec->pProbeData)->codec_cputime += diff_threadclock_short(&((VmetaDecProbeData*)vmetadec->pProbeData)->codecThrClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->codecThrClk1);
#endif
		measure_codectime1(vmetadec, &clk0, &clk1);

		switch(VDecRtCode) {
		case IPP_STATUS_WAIT_FOR_EVENT:
		case IPP_STATUS_NEED_INPUT:
		case IPP_STATUS_END_OF_STREAM:
			return VDecRtCode;

		case IPP_STATUS_NEED_OUTPUT_BUF:
			{
				IppVmetaPicture* pFrame;
				IppCodecStatus DecAPIret;
				//GstPad* pad_character = NULL; //if pad_character==NULL, means the vMeta frame buffer is allocated in this plug-in and share to downstream. Otherwise, the vMeta frame buffer is from downstream.
				int NotAllocateTooMuchFrame;

#ifdef VDECLOOP_DOMORECHECK
				if(vmetadec->VDecInfo.seq_info.dis_buf_size == 0) {
					return IPP_STATUS_ERR;  //decoder status error
				}
#endif

				//if(1) {
				//  pad_character = NULL;
				//}else{
				//  pad_character = vmetadec->srcpad;
				//}

				for(;;) {
					pFrame = rent_frame_from_repo(&vmetadec->FrameRepo, vmetadec->VDecInfo.seq_info.dis_buf_size, &NotAllocateTooMuchFrame, vmetadec);
					if(G_LIKELY(pFrame != NULL)) {
						break;
					}
#ifdef DEBUG_LOG_SOMEINFO
					((VmetaDecProbeData*)vmetadec->pProbeData)->_AskFrameFailCnt++;
#endif
					g_warning("Rent frame fail");
					/*
					IppVmetaDecStatus DecStatus;
					DecodeSendCmd_Vmeta(IPPVC_QUERY_STATUS, &DecStatus, NULL, pVdecObj);
					printf("nUsingInputBufNum = %d\n", DecStatus.nUsingInputBufNum);
					printf("nUsedInputBufNum= %d\n", DecStatus.nUsedInputBufNum);
					printf("nUsingOutputBufNum= %d\n", DecStatus.nUsingOutputBufNum);
					printf("nUsedOutputBufNum= %d\n", DecStatus.nUsedOutputBufNum);
					*/

					if(NotAllocateTooMuchFrame == 0) {
						vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKFRAMEBUF;
						if(vmetadec->StmRepo.TotalSpareSz >  (vmetadec->StmRepo.TotalBufSz>>2)) {
							//still enough space to fill data from demuxer
							return IPP_STATUS_BUFFER_UNDERRUN;
						}
					}

					g_warning("waiting downstream plug-in to release frame buffer...\n");
					usleep(8000);   //sleep moment
				}

				pFrame->pUsrData3 = (void*)vmetadec->iSegmentSerialNum;

				DecAPIret = DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, pFrame, pVdecObj);

				if(DecAPIret != IPP_STATUS_NOERR) {
					g_warning("DecoderPushBuffer_Vmeta() return %d, fail!!!!!", DecAPIret);
					return IPP_STATUS_ERR;
				}
			}

#ifndef NOTEXITLOOP_FOR_NEEDOUTBUF
			if(StopCond == IPP_STATUS_NEED_OUTPUT_BUF) {
				return StopCond;
			}
#endif
			break;
		case IPP_STATUS_RETURN_INPUT_BUF:
			iPopStmCnt += pop_streambuf_from_codec(vmetadec, pVdecObj);
#ifndef NOTEXITLOOP_FOR_RETURNINBUF
			if(StopCond == IPP_STATUS_RETURN_INPUT_BUF) {
				return StopCond;
			}
#endif
			break;
		case IPP_STATUS_FRAME_COMPLETE:
			//for new codec, when return IPP_STATUS_FRAME_COMPLETE, could pop stream buffer
			iPopStmCnt += pop_streambuf_from_codec(vmetadec, pVdecObj);

			{
				int frame_complete_cnt;
				void* pPopTmp;
				IppVmetaPicture* pFrame;
#if GSTVMETA_PLATFORM == GSTVMETA_PLATFORM_DOVE //only for dove suspend
				IppCodecStatus RtvMetaDec;
				if(vdec_os_api_suspend_check()) {   //in codec, vdec_os_driver_init()/vdec_os_driver_clean() is called, no need to call them again
					RtvMetaDec = DecodeSendCmd_Vmeta(IPPVC_PAUSE, NULL, NULL, pVdecObj);
					if(RtvMetaDec == IPP_STATUS_NOERR) {
						vdec_os_api_suspend_ready();
						RtvMetaDec = DecodeSendCmd_Vmeta(IPPVC_RESUME, NULL, NULL, pVdecObj);
						if(RtvMetaDec != IPP_STATUS_NOERR) {
							g_warning("Call DecodeSendCmd_Vmeta(IPPVC_RESUME) fail, ret %d\n", RtvMetaDec);
						}
					}else{
						g_warning("Call DecodeSendCmd_Vmeta(IPPVC_PAUSE) fail, ret %d\n", RtvMetaDec);
					}
				}
#endif
				frame_complete_cnt = 0;
				for(;;) {
					DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, &pPopTmp, pVdecObj);
					pFrame = (IppVmetaPicture*)pPopTmp;
					if(pFrame == NULL) {
						//all frames are popped
						break;
					}
					if(pFrame->nDataLen == 0 && pFrame->PicDataInfo.is_skipped == 0) {
						//sometimes, decoder popup an empty frame which isn't used by decoder, we should ignore this frame.
						//Not use RepoMtx, because rent_frame_from_repo() won't compete with this operation, rent_frame_from_repo() and this function are in same thread
						assist_myechomore("Pop up an empty unused Frame buffer\n");
						VMETA_PICBUF_SETIDLE(pFrame);
						continue;
					}

					frame_complete_cnt++;
					iPopFrameCnt++;
					assist_myechomore("Pop Frame buffer cnt %d\n", iPopFrameCnt);

#ifdef DEBUG_PERFORMANCE
					vmetadec->totalFrames++;
#endif

#ifdef DEBUG_LOG_SOMEINFO
					((VmetaDecProbeData*)vmetadec->pProbeData)->_VDECedFrameCnt++;
#endif


					//for h264, in .avi and .mp4, video compressed data are stored (and demuxer output data) picture by picture not frame by frame
					//for vc1, in .wmv, video are stored in frame by frame
					//for mpeg2, in .mpg(PS), video are stored in PES by PES
					if(vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_H264 && 0 == vmetadec->TSDUManager.mode && 1 == pFrame->PicDataInfo.pic_type && pFrame->nDataLen > 0) {
						//paired field occur
						if(!GVMETA_FILEIS_TS(vmetadec)) {
							vmetadec->TSDUManager.mode = 1;

							//for field h264 in .mp4/.avi, qtdemux/ffdemux_mov_mp4_m4a_3gp_3g2_mj2/avidemux/ffdemux_avi provided frame rate are different(sometimes, are fieldrate, sometimes, are 2*fieldrate), but the duration for each input fragment/field is	the	same	for	above	demuxers	except	some	specially	muxed	file
							//therefore, we calculate the framerate according to sample duration at first
							if(vmetadec->TSDUManager.inputSampeleDuration != GST_CLOCK_TIME_NONE) {
								if(vmetadec->bJudgeFpsForFieldH264) {
									//only fps > 40, will cut fps to half
									if(gst_util_uint64_scale_int(GST_SECOND, 1, vmetadec->TSDUManager.inputSampeleDuration) > 40) {
										vmetadec->TSDUManager.FrameFixDuration = (vmetadec->TSDUManager.inputSampeleDuration<<1);
										assist_myecho("cut fps to half\n");
									}else{
										vmetadec->TSDUManager.FrameFixDuration = vmetadec->TSDUManager.inputSampeleDuration;
									}
								}else{
									vmetadec->TSDUManager.FrameFixDuration = (vmetadec->TSDUManager.inputSampeleDuration<<1);
									assist_myecho("cut fps to half\n");
								}
								vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
								vmetadec->iFRateDen = 1000000;
								vmetadec->iFRateNum = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->TSDUManager.FrameFixDuration);
							}else{
								if(vmetadec->bJudgeFpsForFieldH264) {
									//only fps > 40, will cut fps to half
									if(vmetadec->iFRateNum > 40*vmetadec->iFRateDen) {
										vmetadec->iFRateNum = vmetadec->iFRateNum>>1;
										assist_myecho("cut fps to half\n");
									}
								}else{
									vmetadec->iFRateNum = vmetadec->iFRateNum>>1;
									assist_myecho("cut fps to half\n");
								}
								if(vmetadec->iFRateNum != 0) {
									vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
									vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
								}
							}
						}
					}

					//put the frame into our output queue
					g_queue_push_head(&vmetadec->OutFrameQueue, pFrame);

				}

				measure_codectime_decout1frame(vmetadec, frame_complete_cnt);

#if 0
#error "couldn't gst_pad_push during DecMutex lock"
				//if there are more than 1 frames in one stream segment data, push some of frames to downstream to free some frame buffer.
				if(output_frames_to_downstream(vmetadec, 1, GST_FLOW_OK, 0) != GST_FLOW_OK) {
					return IPP_STATUS_FRAME_ERR;
				}
#endif
				if(ClearOutFrame == 1) {
					clear_candidate_outputframes(vmetadec, 0);  //it will release frame buffer immediately
				}

				if(StopCond == IPP_STATUS_END_OF_STREAM) {
					assist_myecho("will push down frame (line %d), frame account is %d\n", __LINE__, g_queue_get_length(&vmetadec->OutFrameQueue));
					output_frames_to_downstream(vmetadec, 0x7fffffff, GST_FLOW_OK, 1);// in EOS, should no pause/play occur, therefore, not care lock/suspend in fact.
				}


#ifndef NOTEXITLOOP_FOR_FRAMESCOMPLETE
				if(StopCond == IPP_STATUS_FRAME_COMPLETE) {
					return StopCond;
				}
#endif
			}
			break;
		case IPP_STATUS_NEW_VIDEO_SEQ:
			assist_myecho("!!!!!new seq found interlaced=%d, m_wid=%d, m_hei=%d, bufsz=%d, stride=%d, framerate=%d/%d\n", \
				vmetadec->VDecInfo.seq_info.is_intl_seq, vmetadec->VDecInfo.seq_info.max_width, vmetadec->VDecInfo.seq_info.max_height, \
				vmetadec->VDecInfo.seq_info.dis_buf_size, vmetadec->VDecInfo.seq_info.dis_stride, vmetadec->VDecInfo.seq_info.frame_rate_num, vmetadec->VDecInfo.seq_info.frame_rate_den);

                        /* Marvell patch : simplicity 449764 +*/
			//forbid big resolution video
			if(vmetadec->VDecInfo.seq_info.max_width > 800 || vmetadec->VDecInfo.seq_info.max_height > 480) {
				GST_ERROR_OBJECT(vmetadec, "big resolution video (%dx%d), forbid to support", vmetadec->VDecInfo.seq_info.max_width, vmetadec->VDecInfo.seq_info.max_height);
				return IPP_STATUS_INIT_ERR;
			}            
                        /* Marvell patch : simplicity 449764 -*/            

			/*
			printf("!!!!!new seq found interlaced=%d, m_wid=%d, m_hei=%d, bufsz=%d, stride=%d, framerate=%d/%d\n", \
				vmetadec->VDecInfo.seq_info.is_intl_seq, vmetadec->VDecInfo.seq_info.max_width, vmetadec->VDecInfo.seq_info.max_height, \
				vmetadec->VDecInfo.seq_info.dis_buf_size, vmetadec->VDecInfo.seq_info.dis_stride, vmetadec->VDecInfo.seq_info.frame_rate_num, vmetadec->VDecInfo.seq_info.frame_rate_den);
				*/

			if(GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
				//flutsdemuxer couldn't provide frame rate information, update framerate according to vmeta, sometimes, vmeta couldn't provide framerate too
				if(vmetadec->iFRateNum == 0 && vmetadec->iFRateDen == 1 && vmetadec->VDecInfo.seq_info.frame_rate_num != 0 && vmetadec->VDecInfo.seq_info.frame_rate_den != 0) {
					vmetadec->iFRateNum = vmetadec->VDecInfo.seq_info.frame_rate_num;
					vmetadec->iFRateDen = vmetadec->VDecInfo.seq_info.frame_rate_den;
					vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
					vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
				}
			}else if(GVMETA_DEMUXERIS_QT(vmetadec) && vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_H264) {
				//for some .mov with progress h264, 0.10.21 version qtdemux couldn't provideo correct fps, do workaround
				if(vmetadec->VDecInfo.seq_info.is_intl_seq == 0 && vmetadec->iFRateNum !=0 && vmetadec->iFRateNum < 18*vmetadec->iFRateDen) {
					//if fps < 20, we think this fps isn't correct, we use other information to calculate fps
					if(vmetadec->VDecInfo.seq_info.frame_rate_num != 0 && vmetadec->VDecInfo.seq_info.frame_rate_den != 0 && vmetadec->VDecInfo.seq_info.frame_rate_num >= 18*vmetadec->VDecInfo.seq_info.frame_rate_den) {
						vmetadec->iFRateNum = vmetadec->VDecInfo.seq_info.frame_rate_num;
						vmetadec->iFRateDen = vmetadec->VDecInfo.seq_info.frame_rate_den;
						vmetadec->TSDUManager.FrameFixDuration = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->iFRateNum);
						vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
						assist_myecho("!!! Consider qtdemux fps isn't correct, modify it to %d/%d according to vMeta codec\n", vmetadec->iFRateNum, vmetadec->iFRateDen);
					}else if(vmetadec->TSDUManager.inputSampeleDuration != GST_CLOCK_TIME_NONE && vmetadec->TSDUManager.inputSampeleDuration <= 55555555 && vmetadec->TSDUManager.inputSampeleDuration >= 20000000) {
						//55555555 corresponding to 1/55.5ms = 18fps, 20000000 corresponding to 1/20ms = 50 fps
						vmetadec->TSDUManager.FrameFixDuration = vmetadec->TSDUManager.inputSampeleDuration;
						vmetadec->TSDUManager.DiffTS_Criterion = (vmetadec->TSDUManager.FrameFixDuration>>1)+(vmetadec->TSDUManager.FrameFixDuration>>3);
						vmetadec->iFRateDen = 1000000;
						vmetadec->iFRateNum = gst_util_uint64_scale_int(GST_SECOND, vmetadec->iFRateDen, vmetadec->TSDUManager.FrameFixDuration);
						assist_myecho("!!! Consider qtdemux fps isn't correct, modify it to %d/%d according to input data duration\n", vmetadec->iFRateNum, vmetadec->iFRateDen);
					}
				}
			}
#ifdef PROFILER_EN
			if(vmetadec->ProfilerEn){
				CheckAdvanAVSync_VmetaDec(vmetadec);
				CheckCPUFreqDown_VmetaDec(vmetadec);
			}
#endif
			assist_myecho("When vmeta meet new seq, ts cnt is %d\n", vmetadec->TSDUManager.iTDListLen);

			if(vmetadec->bAppendStuffbytes) {
#define VMETACODEC_NEWSEQ_DELAY 4
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
				remove_unwanted_TS(&vmetadec->TSDUManager, vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_MJPG ? TS_MAXDELAY_JPEG:VMETACODEC_NEWSEQ_DELAY);
#else
				remove_unwanted_TS(&vmetadec->TSDUManager, VMETACODEC_NEWSEQ_DELAY);	//keep multiple timestamp not 1 timestamp, because the appendstuffbytes action is delayed until next frame is arrived for flutsdemux h264 and mpeg2 stream. At the same time,	decoder	delay	increased	with	codec	update.
#endif
			}

#ifndef NOTEXITLOOP_FOR_NEWSEQ
			if(StopCond == IPP_STATUS_NEW_VIDEO_SEQ) {
				return StopCond;
			}
#endif
			break;

		default:
			//other vdec return isn't acceptable
			g_warning("DecodeFrame_Vmeta() return unrecognized code %d, fail!!!!!", VDecRtCode);
			return IPP_STATUS_ERR;
		}//switch end
	}//loop end

	return IPP_STATUS_ERR;
}


static void
stop_vmetadec_whendecoding(Gstvmetadec* vmetadec, int bFireStopCmd)
{
	IppCodecStatus cdRet;
	void* pTmp;
	IppVmetaPicture* pFrame;
	//IppVmetaBitstream*	pStream;
	assist_myecho("stop_vmetadec_whendecoding() is calling, bFireStopCmd %d\n", bFireStopCmd);
	if(bFireStopCmd) {
		cdRet = DecodeSendCmd_Vmeta(IPPVC_STOP_DECODE_STREAM, NULL, NULL, vmetadec->pVDecoderObj);//if any buffer in VDEC, pop them and unref them
		assist_myecho("DecodeSendCmd_Vmeta(IPPVC_STOP_DECODE_STREAM) ret %d\n", cdRet);
	}
	g_mutex_lock(vmetadec->FrameRepo.RepoMtx);  //stop_vmetadec_whendecoding() probably is called in gst_vmetadec_dec1stream_pause2ready() which isn't in the thread that _chain
	for(;;) {
		DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_PIC, &pTmp, vmetadec->pVDecoderObj);
		pFrame = (IppVmetaPicture*)pTmp;
		if(pFrame != NULL) {
			assist_myecho("!!!After Force stop VMETADEC, some frame buffer poped up!!!\n");
			VMETA_PICBUF_SETIDLE(pFrame);   //mark the buffer is idle
		}else{
			break;
		}
	}
	g_mutex_unlock(vmetadec->FrameRepo.RepoMtx);

	/*
	for(;;) {
		DecoderPopBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, &pTmp, vmetadec->pVDecoderObj);
		pStream = (IppVmetaBitstream*)pTmp;
		if(pStream == NULL) {
			break;
		}
	}
	*/
	pop_streambuf_from_codec(vmetadec, vmetadec->pVDecoderObj);

	reset_streamrepo_context(&vmetadec->StmRepo, STREAM_VDECBUF_SIZE, STREAM_VDECBUF_NUM, vmetadec);

	return;
}

#define ENDSTUFFING_LEN 	(65*1024)
static int
manualfill_endpattern_to_stream(VmetaDecStreamRepo* pStmRepo, IppVmetaBitstreamFlag flag)
{
	IppVmetaBitstream* pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	assist_myechomore("manual fill EOF/EOU\n");
	if(G_UNLIKELY((int)pStm->pUsrData0 == 0)) {
		assist_myecho("little probability occur, one compressed frame ended at the border of vMeta stream buffer!\n");
#if STREAM_VDECBUF_SIZE < ENDSTUFFING_LEN
#error  !!!Only the buffer whose size is larger 65K could drive vMeta to consume all former streams!!!
#endif
		//manual fill an individual end pattern stream
		if(G_LIKELY(pStmRepo->TotalSpareSz >= STREAM_VDECBUF_SIZE)) {//at least, there is one total spare stream buffer
#if 0
			pStmRepo->TotalSpareSz -= stuff_onestream(pStm, 0x88);
#else
			memset(pStm->pBuf, 0x88, ENDSTUFFING_LEN);
			pStm->nDataLen = ENDSTUFFING_LEN;
			pStm->pUsrData0 = (void*)pStm->nBufSize;
			pStmRepo->TotalSpareSz -= pStm->nBufSize;
#endif
			pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
			pStmRepo->nPushReadyCnt ++;
		}else{
			return -1;
		}
	}else{

		//manual fill an individual end pattern stream
#if STREAM_VDECBUF_SIZE < ENDSTUFFING_LEN
#error  !!!Only the buffer whose size is larger 65K could drive vMeta to consume all former streams!!!
#endif
		if(G_LIKELY(pStmRepo->TotalSpareSz > STREAM_VDECBUF_SIZE)) {//at least, there is one total spare stream buffer left
			pStmRepo->TotalSpareSz -= stuff_onestream(pStm, 0x88);
			pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
			pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
#if 0
			pStmRepo->TotalSpareSz -= stuff_onestream(pStm, 0x88);
#else
			memset(pStm->pBuf, 0x88, ENDSTUFFING_LEN);
			pStm->nDataLen = ENDSTUFFING_LEN;
			pStm->pUsrData0 = (void*)pStm->nBufSize;
			pStmRepo->TotalSpareSz -= pStm->nBufSize;
#endif
			pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
			pStmRepo->nPushReadyCnt += 2;
		}else{
			return -1;
		}
	}
	return 0;
}

static int
manualfill_stuffpattern_to_stream2(VmetaDecStreamRepo* pStmRepo, int stuffbyte)
{
	IppVmetaBitstream* pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	assist_myechomore("call manual fill stuff pattern function\n");
#if STREAM_VDECBUF_SIZE < (ENDSTUFFING_LEN + 128)
#error  !!!Only the buffer whose size is larger 65K could drive vMeta to consume all former streams!!!
#endif
	int needfilllen = ENDSTUFFING_LEN+128-((int)pStm->pUsrData0 & (128-1));
	if((int)pStm->nBufSize - (int)pStm->pUsrData0 >= needfilllen) { //current stream buffer could contain the whole ENDSTUFFING
		unsigned char* pTmp = pStm->pBuf+(int)pStm->pUsrData0;
		memset(pTmp, stuffbyte, needfilllen);
		pStm->nDataLen += needfilllen;  //nDataLen must be multiple of 128
		pStmRepo->TotalSpareSz -= pStm->nBufSize - (int)pStm->pUsrData0;
		pStm->pUsrData0 = (void*)pStm->nBufSize;
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStmRepo->nPushReadyCnt ++;
		return 0;
	}else if(pStmRepo->TotalSpareSz > STREAM_VDECBUF_SIZE) {	//at least there is one totally empty stream buffer
		pStmRepo->TotalSpareSz -= stuff_onestream(pStm, stuffbyte);
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
		memset(pStm->pBuf, stuffbyte, ENDSTUFFING_LEN);
		pStm->nDataLen = ENDSTUFFING_LEN;
		pStm->pUsrData0 = (void*)pStm->nBufSize;
		pStmRepo->TotalSpareSz -= pStm->nBufSize;
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStmRepo->nPushReadyCnt += 2;
		return 0;
	}else{
		return -1;
	}
}


static int
autofill_endpattern_to_stream(VmetaDecStreamRepo* pStmRepo, IppVmetaBitstreamFlag flag, Gstvmetadec* vmetadec)
{
	vmetadec->iInFrameCumulated++;
	vmetadec->iInFrameCumulated2++;
	IppVmetaBitstream* pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	assist_myechomore("auto fill EOF/EOU\n");
	if(G_UNLIKELY((int)pStm->pUsrData0 == 0)) {
		int nLastStmIdx;
		assist_myecho("little probability occur, one compressed frame ended at the border of vMeta stream buffer!\n");
		nLastStmIdx = pStmRepo->nWaitFillCandidateIdx == 0 ? pStmRepo->Count-1 : pStmRepo->nWaitFillCandidateIdx-1;
		//let vmeta IPP code to fill eof/eounit
		pStmRepo->Array[nLastStmIdx].nFlag = flag;
		VMETA_STMBUF_SETISUNITEND(&(pStmRepo->Array[nLastStmIdx]));
	}else{

		//let vmeta IPP code to fill eof
		pStmRepo->TotalSpareSz -= pStm->nBufSize - (int)pStm->pUsrData0;
		pStm->pUsrData0 = (void*)pStm->nBufSize;
		pStm->nFlag = flag;
		VMETA_STMBUF_SETISUNITEND(pStm);
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStmRepo->nPushReadyCnt++;
	}
	return 0;
}

static unsigned char*
seek_bytestream_h264nal(unsigned char* pData, int len, int* pStartCodeLen)
{
	unsigned int code;
	unsigned char* pStartCode = pData;
	if(len < 4) {
		return NULL;
	}
	code = ((unsigned int)pStartCode[0]<<24) | ((unsigned int)pStartCode[1]<<16) | ((unsigned int)pStartCode[2]<<8) | pStartCode[3];
	while((code&0xffffff00) != 0x00000100) {
		len--;
		if(len < 4) {
			return NULL;
		}
		code = (code << 8)| pStartCode[4];
		pStartCode++;
	}

	*pStartCodeLen = 3;
	if(pStartCode>pData) {
		//though 0x000001 is the official startcode for h264, but for most of h264, use 0x00000001 as the startcode.
		//therefore, if 0x00 exist before 0x000001, we think 0x00000001 is the startcode
		if(pStartCode[-1] == 0) {
			pStartCode--;
			*pStartCodeLen = 4;
		}
	}
	return pStartCode;
}

#define H264_NALU_TYPE_SPS  	7
#define H264_NALU_TYPE_PPS  	8
static unsigned char*
found_h264_bytestream_sps(unsigned char* pData, int len)
{
	int SClen;
	unsigned char* pNAL;
	unsigned char* pSPS = NULL;
	//seek SPS
	for(;;) {
		pNAL = seek_bytestream_h264nal(pData, len, &SClen);
		if(pNAL == NULL) {
			break;
		}
		len -= pNAL+SClen+1-pData;
		pData = pNAL+SClen+1;
		if((pNAL[SClen]&0x1f) == H264_NALU_TYPE_SPS) {
			pSPS = pNAL;
			break;
		}
	}
	return pSPS;
}

static __inline int
check_bytestreamNal_is_spspps(GstBuffer* buf, int iMaxLenLimit)
{
	int len = GST_BUFFER_SIZE(buf);
	unsigned char* pData = GST_BUFFER_DATA(buf);
	unsigned char* pFirstNal;
	int iSCLen;
	if(len >= iMaxLenLimit) {
		return 0;
	}else{
		pFirstNal = seek_bytestream_h264nal(pData, len, &iSCLen);
		if(pFirstNal) {
			if((pFirstNal[iSCLen]&0x1f) == H264_NALU_TYPE_SPS) {
				return 1;
			}else if((pFirstNal[iSCLen]&0x1f) == H264_NALU_TYPE_PPS) {
				return 2;
			}
		}
	}
	return 0;
}

static int
digest_h264_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);


	int ret = 0;
	if(vmetadec->DecH264Obj.StreamLayout == H264STM_LAYOUT_AVCC) {
		int notAlldataFilled = 0;
		int nalLen;
		int l = nInDataLen;
		unsigned char* pNal = pInData;
		int zeroNalcnt = 0;
		int realfilled = 0;
		while(l>vmetadec->DecH264Obj.nal_length_size) {
			nalLen = peek_unsigned_from_LEstream(pNal, vmetadec->DecH264Obj.nal_length_size);
			if((unsigned int)nalLen > (unsigned int)(l-vmetadec->DecH264Obj.nal_length_size)) {
				//error bitstream
				nalLen = l-vmetadec->DecH264Obj.nal_length_size;
			}
			l -= vmetadec->DecH264Obj.nal_length_size + nalLen;
			pNal += vmetadec->DecH264Obj.nal_length_size;
			if(nalLen == 0) {
				//in some mov, there are zero stuffing data in AVCC format h264 data
				zeroNalcnt++;
				GST_LOG_OBJECT(vmetadec, "Found zero NAL in AVCC format h264 stream data");
				if(zeroNalcnt > 20) {
					//if occur continuous zero NALs, we consider there won't be real data following those zero NALs, and give up following data
					GST_DEBUG_OBJECT(vmetadec, "Found too much continuous zero NALs in AVCC format h264 stream data, give up following data");
					assist_myecho("Found too much continuous zero NALs in AVCC format h264 stream data, give up following data\n");
					break;
				}else{
					continue;
				}
			}
			if(G_LIKELY(vmetadec->StmRepo.TotalSpareSz >= (int)sizeof(H264NALSyncCode)+nalLen)) {
				fill_streams_in_repo(&vmetadec->StmRepo, H264NALSyncCode, sizeof(H264NALSyncCode));
				fill_streams_in_repo(&vmetadec->StmRepo, pNal, nalLen);
				pNal += nalLen;
				realfilled += sizeof(H264NALSyncCode) + nalLen;
			}else{
				notAlldataFilled = 1;
				vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
				ret |= 1;
				g_warning("Not enough space to fill compressed H264 data, some data are discarded, spare left %d", vmetadec->StmRepo.TotalSpareSz);
				break;
			}
		}
		if(notAlldataFilled == 0 && realfilled > 0) {
			if(vmetadec->bAppendStuffbytes) {
				//14496-15 said h264 in mp4 should picture by picture. However, one picture maybe one field or frame.
				//Therefore, for interlace h264, qtdemuxer provide data field by field, not frame by frame. And it provides timestamp for each filed, and the duration between those timestamp is field duration for most of file, (if the file isn't muxed very	correctly,	probably	the	duraiton	between	picture	is	frame	duration).
				if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
					ret |= 2;
					g_warning("No space left in stream buffer to fill end of unit flag!");
				}
			}

			push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));
		}
	}else{
		if(vmetadec->DecH264Obj.bSeqHdrReceivedForNonAVCC == 0) {
			if(found_h264_bytestream_sps(pInData, nInDataLen) != NULL) {
				vmetadec->DecH264Obj.bSeqHdrReceivedForNonAVCC = 1;
			}else{
				gst_buffer_unref(buf);
				ret |= 0x80000000;
				assist_myecho("Couldn't found sps in current byte stream data\n");
				return ret;
			}
		}

		if(vmetadec->bAppendStuffbytes && GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
			//for ts/m2ts/mts flutsdemuxer, if the timestamp != none, it means current data is a new frame/field begin(the first byte in current input data is the first byte of new frame/field).
			if(GST_BUFFER_TIMESTAMP(buf) != GST_CLOCK_TIME_NONE) {
				if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
					ret |= 2;
					g_warning("No space left in stream buffer to fill end of unit flag!");
				}
			}
		}

		if(G_LIKELY(nInDataLen == fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
			push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));
		}else{
			vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
			ret |= 1;
			g_warning("Not enough space to fill compressed H264 data, some data are discarded, line %d", __LINE__);
		}
		if(vmetadec->bAppendStuffbytes && !GVMETA_DEMUXERIS_FLUTS(vmetadec) ) {
			//only in ts/m2ts file and the demuxer is flu, the data from demuxer isn't nal(s) by nal(s)
#define ROUGHLYCHECK_SPSPPS_MAXLEN  		50  //50 is just an approximate number
//#define ROUGHLYCHECK_IS_SPSPPS(len, gstbuf)   	((len) < ROUGHLYCHECK_SPSPPS_MAXLEN && GST_BUFFER_TIMESTAMP(gstbuf) == GST_CLOCK_TIME_NONE) 	//after pps, vmeta couldn't append stuffing bytes, therefore we must check whether the data is sps and pps.
//  		if( !ROUGHLYCHECK_IS_SPSPPS(nInDataLen, buf) ) {
			if( check_bytestreamNal_is_spspps(buf, ROUGHLYCHECK_SPSPPS_MAXLEN) == 0 ) {
				if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
					ret |= 2;
					g_warning("No space left in stream buffer to fill end of unit flag!");
				}
			}
		}
	}
	gst_buffer_unref(buf);
	return ret;
}

static int
digest_vc1_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);

	int ret=0;
	if(vmetadec->bAppendStuffbytes && GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
		//for ts/m2ts/mts flutsdemuxer&mpegtsdemux, if the timestamp != none, it means current data is a new frame/field begin(the first byte in current input data is the first byte of new frame/field).
		if(GST_BUFFER_TIMESTAMP(buf) != GST_CLOCK_TIME_NONE) {
			if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
				ret |= 2;
				g_warning("No space left in stream buffer to fill end of unit flag!");
			}
		}
	}


	//Through SMPTE 421M sec. G.8 said the frame start code is optional, vMeta only could decode the stream with frame start code
	//For some wmv, it lacks VC1 start code, for others, it has VC1 start code for each frame. Therefore, sometime the data from asfdemux contains start code, sometimes, it doesn't.
	//But fortunately, SMPTE 421M said, two fields share one frame header and frame start code. Therefore, asfdemux/ffdemux_asf always provide one whole frame to downstream. But the ffdemux_asf 0.10.6.2 couldn't parse framerate and timestamp for interlace correctly.
	if(!GVMETA_DEMUXERIS_FLUTS(vmetadec) && (nInDataLen < 3 || pInData[0] != 0 || pInData[1] != 0 || pInData[2] != 1)) {
		assist_myechomore("Insert frame start code to VC1 stream\n");
		fill_streams_in_repo(&vmetadec->StmRepo, VC1FrameStartCode, sizeof(VC1FrameStartCode));
	}

	if(G_LIKELY(nInDataLen == fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
		push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));
	}else{
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
		ret |= 1;
		g_warning("Not enough space to fill compressed VC1 data, some data are discarded");
	}
	gst_buffer_unref(buf);
	if(vmetadec->bAppendStuffbytes && !GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
		if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
			ret |= 2;
			g_warning("No space left in stream buffer to fill end of unit flag!");
		}
	}

	return ret;
}

static int
digest_vc1spmp_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);
	unsigned int frameMeta[2] = {0};
	int ret = 0;
	IppVmetaBitstream* pStm;

	if(nInDataLen > 0xffffff) {
		nInDataLen = 0xffffff;
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_INPUTDATACORRUPT;
		ret |= 2;
	}

	//SMPTE 421M: table 266
	frameMeta[0] = nInDataLen;  //little-endian

#if 0
	{
		//for vmeta decoder, timestamp information isn't important for decoding process
		unsigned int TS;
		//TS = (GST_BUFFER_TIMESTAMP(buf))/1000000;
		TS = (GST_BUFFER_TIMESTAMP(buf))>>20;
		frameMeta[1] = TS;  //little-endian
	}
#endif

#define OPTIMIZE_OFFSET_FOR_VMETAVC1MP  	16

	//always use dynamic stream buffer, make thing simple
	//if( OPTIMIZE_OFFSET_FOR_VMETAVC1MP + 8 + nInDataLen <= STREAM_VDECBUF_SIZE && vmetadec->StmRepo.TotalSpareSz >= STREAM_VDECBUF_SIZE) {
	if(0) {
		//use static stream buffer

#if OPTIMIZE_OFFSET_FOR_VMETAVC1MP == 16
		unsigned char tmp[OPTIMIZE_OFFSET_FOR_VMETAVC1MP];
		pStm = &(vmetadec->StmRepo.Array[vmetadec->StmRepo.nWaitFillCandidateIdx]);
		fill_1frameIn1stm_inStaticRepo(&vmetadec->StmRepo, tmp, OPTIMIZE_OFFSET_FOR_VMETAVC1MP, 0, vmetadec);
#endif
		fill_1frameIn1stm_inStaticRepo(&vmetadec->StmRepo, (unsigned char*)frameMeta, 8, 0, vmetadec);
		fill_1frameIn1stm_inStaticRepo(&vmetadec->StmRepo, pInData, nInDataLen, 1, vmetadec);
#if OPTIMIZE_OFFSET_FOR_VMETAVC1MP == 16
		pStm->nOffset = OPTIMIZE_OFFSET_FOR_VMETAVC1MP;
		pStm->nDataLen = 8 + nInDataLen;
#endif

	}else{
		//use dynamic stream buffer
		int wantbufsz = OPTIMIZE_OFFSET_FOR_VMETAVC1MP + 8 + nInDataLen;
#if 1
		if(wantbufsz < 64*1024) {
			wantbufsz = 64*1024;	//to aviod data copy, in codec, if buffer size < 64k, it will memcpy the data into one >=64K buffer
		}
#endif
		wantbufsz = (wantbufsz + 127) & ~127;   //size must 128 align
		pStm = rent_stm_from_dyncstm_repo(&vmetadec->DyncStmRepo, wantbufsz, vmetadec);
		if(G_UNLIKELY(pStm == NULL)) {
			gst_buffer_unref(buf);
			vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
			ret |= 1;
			g_warning("One VC1 MP frame is too long(sz %d,%d), couldn't found dync stream(%d) to hold it", GST_BUFFER_SIZE(buf), nInDataLen, wantbufsz);
			return ret;
		}
#if OPTIMIZE_OFFSET_FOR_VMETAVC1MP == 16
		pStm->nOffset = OPTIMIZE_OFFSET_FOR_VMETAVC1MP;
#endif
		fill_1frameIn1stm_inDyncRepo(pStm, (unsigned char*)frameMeta, 8, 0);
		fill_1frameIn1stm_inDyncRepo(pStm, pInData, nInDataLen, 1);
		vmetadec->DyncStmRepo.pPushCandidate = pStm;
	}

	push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));
	gst_buffer_unref(buf);
	return ret;
}


static int
digest_mpeg2_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);
	int ret = 0;

	unsigned char* pLatestPicEnd,* pLatestSC;
	unsigned char* left;
	int leftlen;
	GstClockTime Ts,Du;

	//static int myoff = 0;

	Ts = GST_BUFFER_TIMESTAMP(buf);
	//Du = GST_BUFFER_DURATION(buf);
	Du = vmetadec->TSDUManager.FrameFixDuration;

	if(vmetadec->DecMPEG2Obj.LeftData != NULL) {
		buf = gst_buffer_join(vmetadec->DecMPEG2Obj.LeftData, buf);
		vmetadec->DecMPEG2Obj.LeftData = NULL;
		pInData = GST_BUFFER_DATA(buf);
		nInDataLen = GST_BUFFER_SIZE(buf);
	}

	//parse seq header
	if(vmetadec->DecMPEG2Obj.bSeqHdrReceived == 0) {
		if(G_LIKELY( 0 == parse_mpeg2seqinfo_fromstrem(vmetadec, pInData, nInDataLen) )) {
			vmetadec->DecMPEG2Obj.bSeqHdrReceived = 1;
			Du = vmetadec->TSDUManager.FrameFixDuration;
			update_mpeg2_rapiddecodemode(vmetadec);
		}else{
			//if not found seq header or parse the seqheader uncorrectly, ignore the input stream
			gst_buffer_unref(buf);
			ret |= 0x80000000;
			return ret;
		}
	}


	if( GVMETA_FILEIS_PS(vmetadec) ) {  //some .mpg file which contain mpeg2 video are not well muxed, for those file, demuxer always provide TS as GST_CLOCK_TIME_NONE. Therefore, did workaround here, adopt segment start time as first TS.
		if(vmetadec->TSDUManager.TsLatestOut == GST_CLOCK_TIME_NONE && Ts == GST_CLOCK_TIME_NONE && vmetadec->TSDUManager.SegmentStartTs != -1) {
			vmetadec->TSDUManager.TsLatestOut = vmetadec->TSDUManager.SegmentStartTs;
			assist_myecho("Adopt segment start time as first TS %lld\n", vmetadec->TSDUManager.TsLatestOut);
		}
	}
	push_TS(&vmetadec->TSDUManager, &Ts, &Du);

	if(vmetadec->bAppendStuffbytes) {
		if(GVMETA_DEMUXERIS_FLUTS(vmetadec)) {
			//for ts/m2ts/mts flutsdemuxer, if the timestamp != none, it means current data is a new frame/field begin
			if(Ts != GST_CLOCK_TIME_NONE) {
				if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
					ret |= 2;
					g_warning("No space left in stream buffer to fill end of unit flag!");
				}
			}
			if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
				vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
				ret |= 1;
				g_warning("Not enough space to fill compressed MPEG2 data, some data are discarded");
			}
		}else if(GVMETA_DEMUXERIS_FF(vmetadec)) {
			//the demuxer is ffdemux, the data is always picture by picture
			if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
				vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
				ret |= 1;
				g_warning("Not enough space to fill compressed MPEG2 data, some data are discarded");
			}
			if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
				ret |= 2;
				g_warning("No space left in stream buffer to fill end of unit flag!");
			}
		}else{
			//for low bps mpeg2 and demuxer isn't ffdemux or fluts, each input data probably contains multiple frames. Therefore, if stop vMeta once frame_complete, probably much of stream data hasn't been consumed. So should use hungry strategy though hungry	strategy	is	harmful	for	multi-instance.
			if(vmetadec->HungryStrategy == 0) {
				vmetadec->HungryStrategy = 1;
			}

			//ffdemux_mpeg always provide whole picture each time for PS, while other demuxers like mpegpsdemux don't.
			//Therefore, we need to split the compressed data to picture by picture. Note: ffdemux_mpeg still has some bugs, like not support little stream(<3Mbyte) and interlace mpeg2. 2009.7.22
			pLatestPicEnd = SeekMPEG2LatestPicEnd(pInData, nInDataLen, &vmetadec->DecMPEG2Obj.bSeekForPicEnd, &pLatestSC);
			if(pLatestPicEnd != NULL) {
				//static int myid = 0;
				//printf("off 0x%x, id %d\n", pLatestPicEnd-pInData+myoff, myid++);
				//myoff += pLatestPicEnd-pInData;

				if(G_LIKELY(pLatestPicEnd-pInData == fill_streams_in_repo(&vmetadec->StmRepo, pInData, pLatestPicEnd-pInData))) {
					if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
						ret |= 2;
						g_warning("No space left in stream buffer to fill end of unit flag!");
					}
				}else{
					vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
					ret |= 1;
					g_warning("Not enough space to fill compressed MPEG2 data, some data are discarded");
				}
				nInDataLen -= pLatestPicEnd-pInData;
				pInData = pLatestPicEnd;
			}

			//ensure left data not contain any start code
			if(pLatestSC != NULL) {
				left = pLatestSC+4;
				leftlen = nInDataLen - (pLatestSC+4-pInData);
			}else{
				left = pInData;
				leftlen = nInDataLen;
			}
			if(G_LIKELY(leftlen>=3)) {
				if( !ThreeBytesIsMPEG2SCPrefix(left+leftlen-3) )	{
					left += leftlen;
					leftlen = 0;
				}else{
					left += leftlen-3;
					leftlen = 3;
				}
			}
			//store left data for next _chain()
			if(leftlen!=0) {
				vmetadec->DecMPEG2Obj.LeftData = gst_buffer_create_sub(buf, left-GST_BUFFER_DATA(buf), leftlen);
			}
			//myoff += left-pInData;
			if(G_UNLIKELY(left-pInData != fill_streams_in_repo(&vmetadec->StmRepo, pInData, left-pInData))) {
				vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
				ret |= 1;
				g_warning("Not enough space to fill compressed MPEG2 data, some data are discarded");
			}
		}
	}else{
		if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
			vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
			ret |= 1;
			g_warning("Not enough space to fill compressed MPEG2 data, some data are discarded");
		}
	}
	gst_buffer_unref(buf);
	return ret;
}


#define PACKSTM_SKIPVOP_MAXLEN  	15  //15 is just a rough estimation
static int
is_MPEG4_SkipVop(unsigned char* pData, int len, int iTimeIncBitsLen)
{
	if(len > 4 && len <= PACKSTM_SKIPVOP_MAXLEN && iTimeIncBitsLen > 0) {
		//probably, we needn't to check the frame coding type, check the data length is enough
		unsigned char* p = Seek4bytesCode(pData, len, 0x00000100 | MPEG4_SCID_VOP);
		if(p != NULL) {
			p += 4;
			len -= (p-pData);
			if(len > 0 && (p[0] >> 6) == 1) {
				//the skip frame for packed stream is always P frame
				//iso 14496-2: sec 6.2.5
				int bitoffset = 2;
				unsigned int modulo_time_base;
				do{
					modulo_time_base = (p[0]<<bitoffset)&0x80;
					bitoffset = (bitoffset+1) & 7;
					if(bitoffset == 0) {
						len--;
						p++;
					}
				}while(modulo_time_base!=0 && len>0);
				bitoffset = bitoffset + 2 + iTimeIncBitsLen;
				if(bitoffset >= (len<<3)) { //if((len<<3)<bitoffset+1)
					return 0;
				}else{
					return ((p[bitoffset>>3]<<(bitoffset&7))&0x80) == 0;
				}
			}
		}
	}
	return 0;
}


static unsigned char*
seek2ndVop(unsigned char* pData, int len)
{
	unsigned char* p = Seek4bytesCode(pData, len, 0x00000100 | MPEG4_SCID_VOP);
	unsigned char* p2 = NULL;
	if(p!=NULL) {
		p2 = Seek4bytesCode(p+4, len-(p+4-pData), 0x00000100 | MPEG4_SCID_VOP);
	}
	return p2;
}

static int
digest_mpeg4_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);
	int ret = 0;
	unsigned char* p2ndVop = NULL;
	if(nInDataLen == 1 && *pInData == 0x7f) {
		//stuffing bits, spec 14496-2: sec 5.2.3, 5.2.4
		return 0x80000000;
	}

	push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));

	if(vmetadec->DecMPEG4Obj.iTimeIncBits < 0) {
		vmetadec->DecMPEG4Obj.iTimeIncBits = parse_mpeg4_TIB(pInData, nInDataLen, &vmetadec->DecMPEG4Obj.low_delay);
	}
	assist_myechomore("low_delay is %d, mpeg4.CodecSpecies is %d\n", vmetadec->DecMPEG4Obj.low_delay, vmetadec->DecMPEG4Obj.CodecSpecies);
	if(GVMETA_FILEIS_AVI(vmetadec) && vmetadec->DecMPEG4Obj.low_delay != 1 && vmetadec->DecMPEG4Obj.CodecSpecies >=2) {
		//in avi file, xvid & divx probably use "packed bitstream"(if no bvop, impossible to use "packed bitstream"), those skip vop should be ignored
		if(is_MPEG4_SkipVop(pInData, nInDataLen, vmetadec->DecMPEG4Obj.iTimeIncBits)) {
			assist_myechomore("skip packed bistream skip frame\n");
			//printf("skip packed bistream skip frame\n");
			return 0x40000000;
		}
		//p2ndVop = seek2ndVop(pInData, nInDataLen);
	}

	if(p2ndVop != NULL) {
		if(G_UNLIKELY((p2ndVop-pInData) != fill_streams_in_repo(&vmetadec->StmRepo, pInData, (p2ndVop-pInData)))) {
			vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
			ret |= 1;
			g_warning("Not enough space to fill compressed mpeg4 data, some data are discarded");
		}
		if(vmetadec->bAppendStuffbytes) {
			if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
				ret |= 2;
				g_warning("No space left in stream buffer to fill end of unit flag!");
			}
		}
		nInDataLen -= (p2ndVop-pInData);
		pInData = p2ndVop;
	}

	if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
		ret |= 1;
		g_warning("Not enough space to fill compressed mpeg4 data, some data are discarded");
	}
	gst_buffer_unref(buf);
	if(vmetadec->bAppendStuffbytes) {
		if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
			ret |= 2;
			g_warning("No space left in stream buffer to fill end of unit flag!");
		}
	}

	return ret;
}

static int
digest_h263_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);
	int ret = 0;


	push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));

	if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
		ret |= 1;
		g_warning("Not enough space to fill compressed h263 data, some data are discarded");
	}
	gst_buffer_unref(buf);
	if(vmetadec->bAppendStuffbytes) {
		if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
			ret |= 2;
			g_warning("No space left in stream buffer to fill end of unit flag!");
		}
	}

	return ret;
}

static int
digest_mjpeg_inbuf(Gstvmetadec *vmetadec, GstBuffer* buf)
{
	int nInDataLen = GST_BUFFER_SIZE(buf);
	unsigned char* pInData = GST_BUFFER_DATA(buf);
	int ret = 0;

	push_TS(&vmetadec->TSDUManager, &(GST_BUFFER_TIMESTAMP(buf)), &(GST_BUFFER_DURATION(buf)));
	{
#define MAX_PADDEDBYTE_LENGTH   128*1024	//for qtdemuxer and mov file, there are probably a lot of padding bytes. so define MAX_PADDEDBYTE_LENGTH as a big value
		unsigned short eoiFlag = 0;
		int iPaddedBytes = 0;
		while(0xd9ff !=eoiFlag && iPaddedBytes < nInDataLen){
			eoiFlag = (eoiFlag << 8) | (pInData[nInDataLen-iPaddedBytes-1]);
			iPaddedBytes++;
		}
		if(G_LIKELY(iPaddedBytes <= MAX_PADDEDBYTE_LENGTH && iPaddedBytes-2 < nInDataLen)) {
			nInDataLen -= iPaddedBytes-2;
		}else{
			//if too much padded bytes exist, it means this stream is error stream
			g_warning("Couldn't found 0xd9ff in end of %d bytes, probably an error stream", MAX_PADDEDBYTE_LENGTH);
		}
		//printf("garbadge bytes at end of jpeg len %d\n", iPaddedBytes-2);
	}

	if(G_UNLIKELY(nInDataLen != fill_streams_in_repo(&vmetadec->StmRepo, pInData, nInDataLen))) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
		ret |= 1;
		g_warning("Not enough space to fill compressed jpeg data, some data are discarded");
	}
	gst_buffer_unref(buf);
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
	ret |= 0x40000000;  //still drive vMeta
#else
	if(vmetadec->bAppendStuffbytes) {
		if( FILL_ENDOFUNIT_PATTERN(&vmetadec->StmRepo, vmetadec) != 0 ) {
			ret |= 2;
			g_warning("No space left in stream buffer to fill end of unit flag!");
		}
	}
#endif
	return ret;

}

static int
consume_codecdata(Gstvmetadec *vmetadec)
{
	if(vmetadec->gbuf_cdata != NULL) {
		switch(vmetadec->StmCategory) {
		case IPP_VIDEO_STRM_FMT_H264:
			return consume_h264_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_MPG2:
			return consume_mpeg2_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_MPG4:
			return consume_mpeg4_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_VC1:
			return consume_vc1wmv_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_VC1M:
			return consume_vc1wmvSPMP_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_H263:
			return consume_h263_codecdata(vmetadec, GST_BUFFER_DATA(vmetadec->gbuf_cdata), GST_BUFFER_SIZE(vmetadec->gbuf_cdata));
		case IPP_VIDEO_STRM_FMT_MJPG:
			return 0;   //for motion jpeg, codec_data is useless, all needed data is stored in input buffer of _chain
		default:
			return -1;
		}
	}
	return 0;
}

static __inline IppCodecStatus
SuspendResumeVmeta(Gstvmetadec* vmetadec, int bSuspend)
{
	IppCodecStatus cdRet;
	cdRet = DecodeSendCmd_Vmeta(bSuspend==0 ? IPPVC_RESUME:IPPVC_PAUSE, NULL, NULL, vmetadec->pVDecoderObj);
	vmetadec->bVMetaIsInDream = bSuspend;
	return cdRet;
}


static GstFlowReturn
gst_vmetadec_chain(GstPad *pad, GstBuffer *buf)
{
	int tmp;
        int ErrSideInfo = 0;    
	int locked = 0;
	IppCodecStatus VDecLoopRt, cdRet;
	GstFlowReturn PadPushException = GST_FLOW_OK;
	Gstvmetadec* vmetadec = GST_VMETADEC(GST_OBJECT_PARENT(pad));
	int vMetaDreamStateAtLocked;
	int oldPushReady0;
	IppVmetaBitstream* oldPushReady1;
	int bReachStmfireline = 0;
	int bWantFlushFrame = 0;

        /* Marvell patch : simplicity 449764 +*/  
	if(vmetadec->DecMPEG4Obj.bIsDivx) {
		gst_buffer_unref(buf);
		GST_ELEMENT_ERROR(vmetadec, STREAM, DECODE, (NULL), ("%s() exit because not support dixv", __FUNCTION__));
		return GST_FLOW_ERROR;
	}

	//forbid big resolution video
	if(vmetadec->iWidth_InSinkCap > 800 || vmetadec->iHeight_InSinkCap > 480 || vmetadec->DecMPEG2Obj.iNorminalW > 800 || vmetadec->DecMPEG2Obj.iNorminalH > 480) {
		gst_buffer_unref(buf);
		GST_ELEMENT_ERROR(vmetadec, STREAM, DECODE, (NULL), ("%s() exit because not support big resolution %dx%d > 800x480", __FUNCTION__, vmetadec->iWidth_InSinkCap>vmetadec->DecMPEG2Obj.iNorminalW ? vmetadec->iWidth_InSinkCap:vmetadec->DecMPEG2Obj.iNorminalW, vmetadec->iHeight_InSinkCap>vmetadec->DecMPEG2Obj.iNorminalH ? vmetadec->iHeight_InSinkCap:vmetadec->DecMPEG2Obj.iNorminalH));
		return GST_FLOW_ERROR;
	}
        /* Marvell patch : simplicity 449764 -*/      

	measure_chaintime0(vmetadec);

	//fprintf(myfp2, "ts %lld\n", GST_BUFFER_TIMESTAMP(buf));
	//printf("------------------------\n");
	//fprintf(myfp2, "%d\n", GST_BUFFER_SIZE(buf));
	//printf("in sz %d\n",GST_BUFFER_SIZE(buf));
	//fwrite(GST_BUFFER_DATA(buf),1,GST_BUFFER_SIZE(buf),myfp);
	//if(GST_BUFFER_TIMESTAMP(buf)!=GST_CLOCK_TIME_NONE) printf("in ts %lld\n", GST_BUFFER_TIMESTAMP(buf));
	assist_myechomore("in ts %lld, du %lld, sz %d\n", GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_DURATION(buf), GST_BUFFER_SIZE(buf));
	//printf("in ts %lld, du %lld, sz %d\n", GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_DURATION(buf), GST_BUFFER_SIZE(buf));


	if(vmetadec->StmCategory != IPP_VIDEO_STRM_FMT_H264) {
		//cumulate strategy only fit for h264 currently
		vmetadec->CumulateThreshold = 0;
	}

	if((vmetadec->DecErrOccured & (VMETADEC_GST_ERR_GENERICFATAL|VMETADEC_GST_ERR_OSRESINITFAIL)) != 0){
		gst_buffer_unref(buf);
		goto VMETADEC_GSTCHAINFUN_FATALERR;
	}

	//init decoder according current stream category
	if(G_UNLIKELY(vmetadec->pVDecoderObj == NULL)) {

		g_mutex_lock(vmetadec->DecMutex);
		locked = 1;
		assist_myechomore("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);


		vmetadec->VDecParSet.strm_fmt = vmetadec->StmCategory;
		vmetadec->VDecParSet.bMultiIns = vmetadec->SupportMultiinstance == 0 ? 0 : 1;
		vmetadec->VDecParSet.bFirstUser = vmetadec->SupportMultiinstance != 2 ? 0 : 1;
		//vmetadec->VDecParSet.opt_fmt = IPP_YCbCr422I; 	//always this value, no need to change
		vmetadec->VDecParSet.no_reordering = vmetadec->CodecReorder==1? 0 : 1;
		vmetadec->DecH264Obj.bSeqHdrReceivedForNonAVCC = 0;
		vmetadec->iInFrameCumulated = 0;
		vmetadec->iInFrameCumulated2 = 0;
		vmetadec->bVMetaIsInDream = 0;
		memset(&vmetadec->VDecInfo, 0, sizeof(vmetadec->VDecInfo));
		assist_myecho("---call DecoderInitAlloc_Vmeta(), bMultiIns %d, bFirstUser %d\n", vmetadec->VDecParSet.bMultiIns, vmetadec->VDecParSet.bFirstUser);
		cdRet = DecoderInitAlloc_Vmeta(&(vmetadec->VDecParSet), vmetadec->pCbTable, &(vmetadec->pVDecoderObj));
		assist_myecho("---DecoderInitAlloc_Vmeta() is called, ret %d\n", cdRet);

		//in consume_codecdata(), probably need vmetadec->pVDecoderObj. Therefore, call consume_codecdata() after DecoderInitAlloc_Vmeta()
		if(IPP_STATUS_NOERR == cdRet) {
			consume_codecdata(vmetadec);
		}

		assist_myechomore("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
		g_mutex_unlock(vmetadec->DecMutex);
		locked = 0;


		if( IPP_STATUS_NOERR != cdRet) {
			GST_ERROR_OBJECT (vmetadec, "vmetadec init decoder error, ret %d", cdRet);
			vmetadec->pVDecoderObj = NULL;
			goto VMETADEC_GSTCHAINFUN_FATALERR;
		}

		vmetadec->bNotDispFrameInNewSeg = 0;	//if it's the first segment of stream, disable bNotDispFrameInNewSeg
	}


	//digest input data
	oldPushReady0 = vmetadec->StmRepo.nPushReadyCnt;
	oldPushReady1 = vmetadec->DyncStmRepo.pPushCandidate;
	tmp = vmetadec->pfun_digest_inbuf(vmetadec, buf);

	if((0x80000000 & tmp) != 0) {
		assist_myecho("Not found sequence header or meet garbage data, ignore the data/TS and return\n");
		goto VMETADEC_GSTCHAINFUN_COMMON_RET;
	}

	if((0x40000000 & tmp) != 0) {
		//only for mpeg4 packed bitstream, it means meet a skip vop, and still need to drive vmeta, let vmeta return stream buffer and pop frame buffer, because in previous one fragment, P/B are combined in one fragment.
		assist_myechomore("xvid/divx skip VOP, thought needn't push it to vMeta, still need drive vMeta\n");
	}else{
		if(oldPushReady0 == vmetadec->StmRepo.nPushReadyCnt && oldPushReady1 == vmetadec->DyncStmRepo.pPushCandidate) {
			assist_myechomore("No new stream buffer full filled, needn't to kick off vMeta\n");
			goto VMETADEC_GSTCHAINFUN_COMMON_RET;
		}
	}

	//sometimes, even after pause the player, the gst_vmetadec_chain() still will be called. For example, seek during pause
	g_mutex_lock(vmetadec->DecMutex);
	locked = 1;
	assist_myechomore("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);
	vMetaDreamStateAtLocked = vmetadec->bVMetaIsInDream;
	if(vMetaDreamStateAtLocked) {
		//wake up vmeta
		SuspendResumeVmeta(vmetadec, 0);
	}

	if(vmetadec->StmRepo.nPushReadyCnt > 0) {
		if(0 != push_staticstreams_to_vdec(vmetadec->pVDecoderObj, &vmetadec->StmRepo, vmetadec->StmRepo.nPushReadyCnt)) {
			goto VMETADEC_GSTCHAINFUN_FATALERR;
		}
	}

	if(vmetadec->DyncStmRepo.pPushCandidate) {
		IppCodecStatus rt = DecoderPushBuffer_Vmeta(IPP_VMETA_BUF_TYPE_STRM, vmetadec->DyncStmRepo.pPushCandidate, vmetadec->pVDecoderObj);
		vmetadec->DyncStmRepo.pPushCandidate = NULL;
		if(rt != IPP_STATUS_NOERR) {
			g_warning("Call DecoderPushBuffer_Vmeta(stm) for dync stream buf 0x%x fail, ret %d", (unsigned int)(vmetadec->DyncStmRepo.pPushCandidate), rt);
			goto VMETADEC_GSTCHAINFUN_FATALERR;
		}
	}

#define STREAM_VDECBUF_CUMULATESTRTEGY_FIRELINE 	(3*STREAM_VDECBUF_SIZE)
	if(vmetadec->CumulateThreshold > 0 && vmetadec->iInFrameCumulated < vmetadec->CumulateThreshold && vmetadec->iInFrameCumulated2 < vmetadec->CumulateThreshold && vmetadec->StmRepo.TotalSpareSz > STREAM_VDECBUF_CUMULATESTRTEGY_FIRELINE) {
		assist_myechomore("cumulated frame cnt %d(cnt2 %d) < threshold %d, not loop vmeta.\n", vmetadec->iInFrameCumulated, vmetadec->iInFrameCumulated2, vmetadec->CumulateThreshold);
		//printf("cumulated frame cnt %d(cnt2 %d) < threshold %d, not loop vmeta.\n", vmetadec->iInFrameCumulated, vmetadec->iInFrameCumulated2, vmetadec->CumulateThreshold);

		if(vMetaDreamStateAtLocked != vmetadec->bVMetaIsInDream) {
			SuspendResumeVmeta(vmetadec, vMetaDreamStateAtLocked);
		}
		assist_myechomore("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
		g_mutex_unlock(vmetadec->DecMutex);
		locked = 0;
		goto VMETADEC_GSTCHAINFUN_COMMON_RET;
	}


#ifdef ENABLE_VDEC_NONBLOCK_CALLING
#error "not support ENABLE_VDEC_NONBLOCK_CALLING now"
#define STMREPOSPACE_FIRELINE   ((STREAM_VDECBUF_SIZE*STREAM_VDECBUF_NUM*3)/8)
	if(vmetadec->StmRepo.TotalSpareSz > STMREPOSPACE_FIRELINE) {
		VDecLoopRt = loop_vdec_until(vmetadec, IPP_STATUS_WAIT_FOR_EVENT);
	}else{
#endif

	assist_myechomore("begin loop vmeta.\n");
	//clear cumulate counter
	vmetadec->iInFrameCumulated = 0;

	for(;;){
		VDecLoopRt = loop_vdec_until(vmetadec, vmetadec->HungryStrategy ? IPP_STATUS_NEED_INPUT : IPP_STATUS_FRAME_COMPLETE, &bWantFlushFrame, 0);
		assist_myechomore("loop_vdec_until() return %d\n", VDecLoopRt);

		if(VDecLoopRt == IPP_STATUS_FRAME_COMPLETE /* && vmetadec->CumulateThreshold > 0 */) {
			if(vmetadec->StmRepo.TotalSpareSz > STREAM_VDECBUF_CUMULATESTRTEGY_FIRELINE) {
				break;  //if there is still a lot of stream buffer left, got one IPP_STATUS_FRAME_COMPLETE is enough and could continue to fill stream
			}else{
				bReachStmfireline = 1;
				assist_myecho("+++++ stream buffer repo reach the fireline, size %d\n", vmetadec->StmRepo.TotalSpareSz);
				//printf("+++++ stream buffer repo reach the fireline, size %d\n", vmetadec->StmRepo.TotalSpareSz);
			}
		}else{
			break;
		}
	}

#ifdef ENABLE_VDEC_NONBLOCK_CALLING
	}
#endif

	if(vMetaDreamStateAtLocked != vmetadec->bVMetaIsInDream) {
		SuspendResumeVmeta(vmetadec, vMetaDreamStateAtLocked);
	}
	assist_myechomore("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
	g_mutex_unlock(vmetadec->DecMutex);
	locked = 0;

	//printf("-----------loop_vdec_until ret %d\n", VDecLoopRt);
	if(VDecLoopRt == IPP_STATUS_ERR || VDecLoopRt == IPP_STATUS_END_OF_STREAM) {
		g_warning("!!!!!!! loop vMeta Decoder error %d", VDecLoopRt);
		goto VMETADEC_GSTCHAINFUN_FATALERR;
	}else if(VDecLoopRt == IPP_STATUS_FRAME_ERR) {
		goto VMETADEC_GSTCHAINFUN_PUSHERR;
         /* Marvell patch : simplicity 449764 +*/        
	}else if(VDecLoopRt == IPP_STATUS_INIT_ERR) {
		ErrSideInfo = 1;
		goto VMETADEC_GSTCHAINFUN_FATALERR;        
         /* Marvell patch : simplicity 449764 -*/            
	}


	{
		assist_myechomore("Candidate push frame cnt %d\n", g_queue_get_length(&vmetadec->OutFrameQueue));
		int wantpushcnt = 1;
		if(vmetadec->StmCategory==IPP_VIDEO_STRM_FMT_MPG2) {
			//for mpeg2 ps, one PES data from mpegpsdemux may contain multiple frames. for other video, always half frame or one frame in each data segment from demuxer.
			wantpushcnt = 0x7fffffff;
		}
#ifdef WORKAROUND_FORVMETAJPEG_PARSEISSUE
		if(vmetadec->StmCategory==IPP_VIDEO_STRM_FMT_MJPG) {
			wantpushcnt = 0x7fffffff;
		}
#endif
		if(bReachStmfireline == 1) {
			//at this case, probably produce multiple frames
			wantpushcnt = 0x7fffffff;
		}
		if(bWantFlushFrame == 1) {
			wantpushcnt = 0x7fffffff;
		}
		PadPushException = output_frames_to_downstream(vmetadec, wantpushcnt, GST_FLOW_OK, 0);
	}

	if(G_UNLIKELY(PadPushException != GST_FLOW_OK)) {
		goto VMETADEC_GSTCHAINFUN_PUSHERR;
	}

VMETADEC_GSTCHAINFUN_COMMON_RET:
	measure_chaintime1(vmetadec);
	return GST_FLOW_OK;

VMETADEC_GSTCHAINFUN_PUSHERR:
	if(locked) {
		if(vMetaDreamStateAtLocked != vmetadec->bVMetaIsInDream) {
			SuspendResumeVmeta(vmetadec, vMetaDreamStateAtLocked);
		}
		assist_myechomore("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
		g_mutex_unlock(vmetadec->DecMutex);
		locked = 0;
	}

	measure_chaintime1(vmetadec);

	if(PadPushException != GST_FLOW_WRONG_STATE) {//during seeking, sink element often return GST_FLOW_WRONG_STATUS. No need to echo this message
		GST_WARNING_OBJECT(vmetadec, "!!!!! vmetadec in chain() push down on pad error");
	}

	if(PadPushException < GST_FLOW_UNEXPECTED) {
		GST_ELEMENT_ERROR(vmetadec, STREAM, DECODE, (NULL), ("%s() return, error %d occur when pushing", __FUNCTION__, PadPushException));
	}
	return PadPushException;

VMETADEC_GSTCHAINFUN_FATALERR:
	if(locked) {
		if(vMetaDreamStateAtLocked != vmetadec->bVMetaIsInDream) {
			SuspendResumeVmeta(vmetadec, vMetaDreamStateAtLocked);
		}
		assist_myechomore("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
		g_mutex_unlock(vmetadec->DecMutex);
		locked = 0;
	}

	vmetadec->DecErrOccured |= VMETADEC_GST_ERR_GENERICFATAL;

	measure_chaintime1(vmetadec);

	GST_ERROR_OBJECT(vmetadec, "!!!!! vmetadec in chain() fatal error");

        GST_ELEMENT_ERROR(vmetadec, STREAM, DECODE, (NULL), ("%s() exit with un-accepted error %d, error side info %d", __FUNCTION__, GST_FLOW_ERROR, ErrSideInfo)); /* Marvell patch : simplicity 449764 */
	return GST_FLOW_ERROR;
}

static void
osframeres_finalizer_finalize(gpointer data)
{
	assist_myecho("osframeres finalizer is finalizing\n");
	//printf("osframeres finalizer is finalizing\n");

	Gstvmetadec *vmetadec = (Gstvmetadec *)data;

	//release resource
	destroy_allframes_repo(&vmetadec->FrameRepo);

	/*vdec_os_driver_clean();*/

	//since resource is freeed, set osframeres_finalizer = NULL;
	vmetadec->osframeres_finalizer = NULL;

	gst_object_unref(vmetadec); //since the os_driver is cleaned, vmetadec could be finalized.
	assist_myecho("osframeres finalizer is finalized\n");
	//printf("osframeres finalizer is finalized\n");
	return;
}

static gboolean
gst_vmetadec_null2ready(Gstvmetadec *vmetadec)
{
	//myfp = fopen("frame.dat","wb");
	//myfp2 = fopen("framelen.txt","wt");
	//myfp3 = fopen("header.dat","wb");

	/* no need to call it, memory allocate has no relationship with vdec_os_driver_init()
	int os_driver_ret = vdec_os_driver_init();
	if(os_driver_ret < 0) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_OSRESINITFAIL;
		GST_ERROR_OBJECT(vmetadec, "vdec_os_driver_init() fail, ret is %d!!!!!!!", os_driver_ret);
		g_warning("vdec_os_driver_init() fail, ret is %d!!!!!!!", os_driver_ret);
		return FALSE;
	}
	*/


	if( 0 != (vmetadec->DecErrOccured & VMETADEC_GST_ERR_OSRESINITFAIL) || 0 != miscInitGeneralCallbackTable(&(vmetadec->pCbTable)) ) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_OSRESINITFAIL;
		GST_ERROR_OBJECT (vmetadec, "vmetadec init call back table error");
		g_warning("vmetadec init call back table error");
		return FALSE;
	}

	if( 0 != (vmetadec->DecErrOccured & VMETADEC_GST_ERR_OSRESINITFAIL) || 0 != init_streams_in_repo(&(vmetadec->StmRepo), vmetadec) ) {
		vmetadec->DecErrOccured |= VMETADEC_GST_ERR_OSRESINITFAIL;
		GST_ERROR_OBJECT (vmetadec, "vmetadec init decoder stream repository error");
		g_warning("vmetadec init decoder stream repository error");
		return FALSE;
	}

	if( 0 == (vmetadec->DecErrOccured & VMETADEC_GST_ERR_OSRESINITFAIL) && vmetadec->osframeres_finalizer == NULL) {
		vmetadec->osframeres_finalizer = OSFRAMERES_WATCHMAN_CREATE();
		GST_BUFFER_MALLOCDATA(vmetadec->osframeres_finalizer) = (guint8*)gst_object_ref(vmetadec);
		assist_myecho("ref vmetadec object for osframeres_finalizer!!!\n");
		GST_BUFFER_FREE_FUNC(vmetadec->osframeres_finalizer) = (GFreeFunc)osframeres_finalizer_finalize;
	}

	assist_myecho("VMETADEC started, init working is normally finished.\n");
	return TRUE;
}

static gboolean
gst_vmetadec_ready2null(Gstvmetadec *vmetadec)
{
	//fclose(myfp);
	//fclose(myfp2);
	//fclose(myfp3);
#ifdef DEBUG_LOG_SOMEINFO
	printf("VMETADEC begin to null, error occured = %d, %d elements lefts in framerepo, SegmentSid %d, TS cnt %d.\n\n", vmetadec->DecErrOccured, vmetadec->FrameRepo.Count, vmetadec->iSegmentSerialNum, g_slist_length(vmetadec->TSDUManager.TSDUList));
#endif
	clear_candidate_outputframes(vmetadec, 0);
	clear_TSManager(&vmetadec->TSDUManager);

	if(vmetadec->gbuf_cdata) {
		gst_buffer_unref(vmetadec->gbuf_cdata);
		vmetadec->gbuf_cdata = NULL;
	}

	if(vmetadec->DecMPEG2Obj.LeftData != NULL) {
		gst_buffer_unref(vmetadec->DecMPEG2Obj.LeftData);
		vmetadec->DecMPEG2Obj.LeftData = NULL;
	}

	if(vmetadec->StmRepo.Count != 0) {
		destroy_allstreams_repo(&(vmetadec->StmRepo));
	}

	if(vmetadec->DyncStmRepo.Count != 0) {
		destroy_allstreams_dync_repo(&(vmetadec->DyncStmRepo));
	}

	//free those frame buffer which are not owned by sink. The purpose is to free memory as early as possible instead of waiting until sink release all frame buffers.
	g_mutex_lock(vmetadec->FrameRepo.RepoMtx);
	frames_repo_recycle(&vmetadec->FrameRepo);
	vmetadec->FrameRepo.NotNeedFMemAnyMore = 1;
	g_mutex_unlock(vmetadec->FrameRepo.RepoMtx);

	if(vmetadec->pCbTable) {
		miscFreeGeneralCallbackTable(&vmetadec->pCbTable);
		vmetadec->pCbTable = NULL;
	}

	g_free(vmetadec->ChkSumFileName);
	vmetadec->ChkSumFileName = NULL;
	if(vmetadec->ChkSumFile) {
		fclose(vmetadec->ChkSumFile);
		vmetadec->ChkSumFile = NULL;
	}

	//smart release frame resource based on refcount
	if(vmetadec->osframeres_finalizer != NULL) {
		assist_myecho("unref vmetadec's osframeres_finalizer during vmetadec ready2null!\n");
		OSFRAMERES_WATCHMAN_UNREF(vmetadec->osframeres_finalizer);
	}

	assist_myecho("vMeta ready2null is normally finished\n");

#ifdef DEBUG_LOG_SOMEINFO
	printf("vMeta ever has at most %d frames in frame repo.\n", ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxFrameCnt);
	printf("vMeta ever has at most %d dync stream buf, at most %d total size in repo.\n", ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmCnt, ((VmetaDecProbeData*)vmetadec->pProbeData)->_MaxDStmBufTSz);
	printf("vMeta Total decoded frame %d, pushed frame %d, ask frame fail happend %d.\n\n", ((VmetaDecProbeData*)vmetadec->pProbeData)->_VDECedFrameCnt, ((VmetaDecProbeData*)vmetadec->pProbeData)->_VDECPushedFrameCnt, ((VmetaDecProbeData*)vmetadec->pProbeData)->_AskFrameFailCnt);
#endif

	return TRUE;
}

static void
clear_mpeg2_seqdatainfos(VmetaDecMPEG2Obj* mpeg2obj)
{
	if(mpeg2obj->LeftData != NULL) {
		gst_buffer_unref(mpeg2obj->LeftData);
		mpeg2obj->LeftData = NULL;
	}
	mpeg2obj->bSeqHdrReceived = 0;
	mpeg2obj->bSeekForPicEnd = 0;
	mpeg2obj->iNorminalBitRate = 0;
	mpeg2obj->iNorminalW = 0;
	mpeg2obj->iNorminalH = 0;
	return;
}

static gboolean
gst_vmetadec_dec1stream_ready2pause(Gstvmetadec* vmetadec)
{
	vmetadec->UpAdjacentNonQueueEle = ELEMENT_CHARID_UNDECIDED;
	vmetadec->DownAdjacentNonQueueEle = ELEMENT_CHARID_UNDECIDED;

	//because it's to decode a new stream, stream correlative information should be reset
	vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_UNAVAIL;
	vmetadec->pfun_digest_inbuf = NULL;

	if(vmetadec->ChkSumFile) {
		fclose(vmetadec->ChkSumFile);
		vmetadec->ChkSumFile = NULL;
	}
	if(vmetadec->ChkSumFileName && vmetadec->ChkSumFileName[0] != 0) {
		vmetadec->ChkSumFile = fopen(vmetadec->ChkSumFileName, "wt");
	}

	vmetadec->iFrameContentWidth = 0;
	vmetadec->iFrameContentHeight = 0;
	vmetadec->iFrameCompactSize = 0;
	vmetadec->iFrameStride = 0;
	vmetadec->iFRateNum = 0;
	vmetadec->iFRateDen = 1;
	vmetadec->TSDUManager.mode = 0;
	vmetadec->TSDUManager.bHavePopTs = 0;
	vmetadec->TSDUManager.FrameFixDuration = GST_CLOCK_TIME_NONE;
	vmetadec->TSDUManager.DiffTS_Criterion = 0;
	vmetadec->TSDUManager.inputSampeleDuration = GST_CLOCK_TIME_NONE;

	vmetadec->DecErrOccured &= VMETADEC_GST_ERR_OSRESINITFAIL;

	vmetadec->bAppendStuffbytes = 1;

	vmetadec->DecH264Obj.StreamLayout = H264STM_LAYOUT_SYNCCODENAL;
	vmetadec->DecH264Obj.nal_length_size = 0;

	if(vmetadec->gbuf_cdata) {
		gst_buffer_unref(vmetadec->gbuf_cdata);
		vmetadec->gbuf_cdata = NULL;
	}

	clear_mpeg2_seqdatainfos(&vmetadec->DecMPEG2Obj);

	vmetadec->DecMPEG4Obj.iTimeIncBits = -1;
	vmetadec->DecMPEG4Obj.low_delay = -1;

	vmetadec->DecVC1MPObj.iContentWidth = -1;
	vmetadec->DecVC1MPObj.iContentHeight = -1;

	reset_streamrepo_context(&(vmetadec->StmRepo), STREAM_VDECBUF_SIZE, STREAM_VDECBUF_NUM, vmetadec);

	//frame buffers recycle
	g_mutex_lock(vmetadec->FrameRepo.RepoMtx);
	frames_repo_recycle(&vmetadec->FrameRepo);
	g_mutex_unlock(vmetadec->FrameRepo.RepoMtx);

	vmetadec->VDecInfo.seq_info.dis_buf_size = 0;

	return TRUE;
}


static __inline gboolean
gst_vmetadec_dec1stream_pause2ready(Gstvmetadec* vmetadec)
{
	IppCodecStatus cdRet;
	//close the vmeta
	g_mutex_lock(vmetadec->DecMutex);
	assist_myecho("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);

	if(vmetadec->pVDecoderObj) {
		if(vmetadec->EnableVmetaSleepInPause) {
			if(vmetadec->bVMetaIsInDream) {
				cdRet = SuspendResumeVmeta(vmetadec, 0);
				assist_myecho("Call DecodeSendCmd_Vmeta(IPPVC_RESUME...) in %s line %d, ret %d\n", __FUNCTION__, __LINE__, cdRet);
				if(cdRet != IPP_STATUS_NOERR) {
					//g_warning("DecodeSendCmd_Vmeta(IPPVC_RESUME...) fail, ret %d", cdRet);
				}
			}
		}
		stop_vmetadec_whendecoding(vmetadec, vmetadec->bStoppedCodecInEos==1? 0:1);
		cdRet = DecoderFree_Vmeta(&(vmetadec->pVDecoderObj));
		assist_myecho("---DecoderFree_Vmeta() is called in pause2ready, ret %d\n", cdRet);
		vmetadec->pVDecoderObj = NULL;
	}

	assist_myecho("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
	g_mutex_unlock(vmetadec->DecMutex);

	if(vmetadec->ChkSumFile) {
		fclose(vmetadec->ChkSumFile);
		vmetadec->ChkSumFile = NULL;
	}
	return TRUE;
}

static __inline int
gst_vmetadec_pause2play(Gstvmetadec* vmetadec)
{
	IppCodecStatus cdRet = IPP_STATUS_NOERR;
	g_mutex_lock(vmetadec->DecMutex);
	assist_myecho("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);

	if(vmetadec->pVDecoderObj && vmetadec->EnableVmetaSleepInPause && vmetadec->bVMetaIsInDream) {
		cdRet = SuspendResumeVmeta(vmetadec, 0);
		assist_myecho("Call DecodeSendCmd_Vmeta(IPPVC_RESUME...) in %s line %d, ret %d\n", __FUNCTION__, __LINE__, cdRet);

	}
	assist_myecho("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
	g_mutex_unlock(vmetadec->DecMutex);

	if(cdRet != IPP_STATUS_NOERR) {
		g_warning("DecodeSendCmd_Vmeta(IPPVC_RESUME...) fail, ret %d", cdRet);
		return -1;
	}
	return 0;
}

static __inline int
gst_vmetadec_play2pause(Gstvmetadec* vmetadec)
{
	IppCodecStatus cdRet = IPP_STATUS_NOERR;
	g_mutex_lock(vmetadec->DecMutex);
	assist_myecho("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);
	if(vmetadec->pVDecoderObj && vmetadec->EnableVmetaSleepInPause && !vmetadec->bVMetaIsInDream) {
		cdRet = SuspendResumeVmeta(vmetadec, 1);
		assist_myecho("Call DecodeSendCmd_Vmeta(IPPVC_PAUSE...) in %s line %d, ret %d\n", __FUNCTION__, __LINE__, cdRet);
	}
	assist_myecho("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
	g_mutex_unlock(vmetadec->DecMutex);
	if(cdRet != IPP_STATUS_NOERR) {
		g_warning("DecodeSendCmd_Vmeta(IPPVC_PAUSE...) fail, ret %d", cdRet);
		return -1;
	}
#ifdef PROFILER_EN
	vmetadec->sFpsWin[D_SKIP].nFrmDecTimeCnt = -1;
	vmetadec->sFpsWin[D_CPUFREQ].nFrmDecTimeCnt = -1;
	vmetadec->sFpsWin[D_SKIP].nPrevFps  = -1;
	vmetadec->sFpsWin[D_CPUFREQ].nPrevFps = -1;
	vmetadec->nSkippedFrames = 0;
	vmetadec->nPrevSkippedFrames = -1;
#endif
	return 0;
}

static GstStateChangeReturn
gst_vmetadec_change_state (GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstvmetadec *vmetadec = GST_VMETADEC (element);

	assist_myecho("Begin GST vmetadec state change trans from %d to %d\n", transition>>3, transition&7);

	switch (transition)
	{
	case GST_STATE_CHANGE_NULL_TO_READY:
		if(!gst_vmetadec_null2ready(vmetadec)){
			goto VMETADECGST_STATECHANGE_ERR;
		}
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		gst_vmetadec_dec1stream_ready2pause(vmetadec);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		gst_vmetadec_pause2play(vmetadec);
		break;

	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		vmetadec->bPlaying = 0;
		break;
	default:
		break;
	}
	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_warning("GST vmetadec parent class state change fail!");
		return ret;
	}


	switch (transition)
	{
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		gst_vmetadec_play2pause(vmetadec);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		gst_vmetadec_dec1stream_pause2ready(vmetadec);
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		if(!gst_vmetadec_ready2null(vmetadec)){
			goto VMETADECGST_STATECHANGE_ERR;
		}
		break;

	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		vmetadec->bPlaying = 1;
		break;
	default:
		break;
	}

	assist_myecho("End GST vmetadec state change trans from %d to %d\n", transition>>3, transition&7);
	return ret;

	 /* ERRORS */
VMETADECGST_STATECHANGE_ERR:
	{
		g_warning("vmetadec state change failed");
		/* subclass must post a meaningfull error message */
		GST_ERROR_OBJECT (vmetadec, "state change failed");
		return GST_STATE_CHANGE_FAILURE;
	}
}


static int
filleospad_to_streams_and_push(Gstvmetadec* vmetadec)
{
	VmetaDecStreamRepo* pStmRepo = &(vmetadec->StmRepo);
	IppVmetaBitstream* pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	if(G_LIKELY(pStmRepo->TotalSpareSz > 0)) {
		//still some space left in repo
		//It's better that all content of eospad in only one stream node
		if(pStm->nBufSize-(int)pStm->pUsrData0 < sizeof(eospad)) {
			pStmRepo->TotalSpareSz -= stuff_onestream(pStm, 0x88);
			pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
			pStmRepo->nPushReadyCnt++;
		}
		//sizeof(eospad) must smaller than one stream buffer's capacity
		if(G_LIKELY(pStmRepo->TotalSpareSz > 0)) {
#if 1
			//fill only one eospad in one stream buffer
			fill_streams_in_repo(pStmRepo, eospad, sizeof(eospad));
			pStmRepo->TotalSpareSz -= stuff_onestream(&(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]), 0x88);
			pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
			pStmRepo->nPushReadyCnt++;
#else
			//fill multiple eospad in one stream buffer
			{
				int cnt = STREAM_VDECBUF_SIZE / sizeof(eospad), i, all=0;
				for(i=0;i<cnt;i++) {
					all += fill_streams_in_repo(pStmRepo, eospad, sizeof(eospad));
				}
				if(all < STREAM_VDECBUF_SIZE) {
					pStmRepo->TotalSpareSz -= stuff_onestream(&(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]), 0x88);
					pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
					pStmRepo->nPushReadyCnt++;
				}

			}
#endif
		}

		if( 0 != push_staticstreams_to_vdec(vmetadec->pVDecoderObj, pStmRepo, pStmRepo->nPushReadyCnt) ) {
			return -1;
		}
		return 0;
	}else{
		g_warning("!!!!All VDEC stream buffer are filled, no space to fill eospad!!!!!!!!!");
		return -2;
	}
}


//2010.07.01, it's strange. For VC1 AP stream, IPPVC_END_OF_STREAM cmd has no effect.
static int
fire_eos_to_vmeta(Gstvmetadec* vmetadec)
{
	VmetaDecStreamRepo* pStmRepo = &(vmetadec->StmRepo);
	IppVmetaBitstream* pStm = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	if(pStmRepo->TotalSpareSz > 0 && pStm->pUsrData0 != (void*)0) {

#if 1
		//let vmeta IPP code to fill 0x88
		pStm->nFlag = IPP_VMETA_STRM_BUF_END_OF_UNIT;
		pStmRepo->TotalSpareSz -= pStm->nBufSize - (int)pStm->pUsrData0;
		pStm->pUsrData0 = (void*)pStm->nBufSize;
#else
		//manual fill 0x88
		pStmRepo->TotalSpareSz -= stuff_onestream(pStm, 0x88);
#endif

		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStmRepo->nPushReadyCnt++;
		if( 0 != push_staticstreams_to_vdec(vmetadec->pVDecoderObj, pStmRepo, pStmRepo->nPushReadyCnt) ) {
			return -1;
		}
	}

	if(IPP_STATUS_NOERR != DecodeSendCmd_Vmeta(IPPVC_END_OF_STREAM, NULL, NULL, vmetadec->pVDecoderObj)) {
		return -1;
	}

	return 0;
}


static unsigned char eoseg_pattern[] = {0x00, 0x00, 0x01, 0xff, 0xff, 0x00, 0x00, 0x04, 0x7f, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
#define LEN_VMETAEOSEG_PATTERN  	((int)(sizeof(eoseg_pattern)/sizeof(eoseg_pattern[0])))
static int
fill_endofseg_pattern_to_stream(VmetaDecStreamRepo* pStmRepo)
{
	assist_myecho("fill_endofseg_pattern_to_stream is called.\n");
#if STREAM_VDECBUF_SIZE < (ENDSTUFFING_LEN + 128)
#error  !!!Only the buffer whose size is larger 65K could drive vMeta to consume all former streams!!!
#endif
#if 1
	IppVmetaBitstream* pStm0 = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);
	unsigned char* pStm0Beg = pStm0->pBuf + (int)pStm0->pUsrData0;
	int iStm0leftspace = pStm0->nBufSize - (int)pStm0->pUsrData0;

	if(pStmRepo->TotalSpareSz > STREAM_VDECBUF_SIZE) {
		//there is one "half empty" stream buffer and one "total empty" stream buffer
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		IppVmetaBitstream* pStm1 = &(pStmRepo->Array[pStmRepo->nWaitFillCandidateIdx]);

		memset(pStm0Beg, 0x88, iStm0leftspace);
		memset(pStm1->pBuf, 0x88, pStm1->nBufSize);
		pStm0->nDataLen = pStm0->nBufSize;
		pStm0->pUsrData0 = (void*)pStm0->nBufSize;
		pStm1->nDataLen = pStm1->nBufSize;
		pStm1->pUsrData0 = (void*)pStm1->nBufSize;

		pStmRepo->TotalSpareSz -= pStm1->nBufSize + iStm0leftspace;
		pStmRepo->nPushReadyCnt += 2;
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;

		if( iStm0leftspace >=  LEN_VMETAEOSEG_PATTERN || pStm1->pBuf == pStm0->pBuf + pStm0->nBufSize ) {
			memcpy(pStm0Beg, eoseg_pattern, LEN_VMETAEOSEG_PATTERN);
		}else{
			memcpy(pStm0Beg, eoseg_pattern, iStm0leftspace);
			memcpy(pStm1->pBuf, &(eoseg_pattern[iStm0leftspace]), LEN_VMETAEOSEG_PATTERN - iStm0leftspace);
		}
		return 0;
	}else if(pStmRepo->TotalSpareSz >= ENDSTUFFING_LEN) {
		//only one "half empty" stream buffer whose left space >= ENDSTUFFING_LEN
		memset(pStm0Beg, 0x88, iStm0leftspace);
		memcpy(pStm0Beg, eoseg_pattern, LEN_VMETAEOSEG_PATTERN);
		pStmRepo->TotalSpareSz -= iStm0leftspace;;
		pStmRepo->nWaitFillCandidateIdx = (pStmRepo->nWaitFillCandidateIdx+1==pStmRepo->Count)? 0 : pStmRepo->nWaitFillCandidateIdx+1;
		pStmRepo->nPushReadyCnt += 1;

		pStm0->nDataLen = pStm0->nBufSize;
		pStm0->pUsrData0 = (void*)pStm0->nBufSize;
		return 0;
	}else{
		assist_myecho("No space to fill end of segment pattern\n");
		return -1;
	}

#else
	return manualfill_stuffpattern_to_stream2(pStmRepo, 0x88);
#endif
}

static IppVmetaPicture*
create_dummyframe(Gstvmetadec *vmetadec)
{
	IppVmetaPicture* pFakeFrame;
	int len;
	if(vmetadec->iFrameContentWidth == 0) {
		return NULL;
	}
	pFakeFrame = rent_frame_from_repo(&vmetadec->FrameRepo, vmetadec->VDecInfo.seq_info.dis_buf_size, NULL, vmetadec);
	if(pFakeFrame == NULL) {
		return NULL;
	}
	pFakeFrame->pic.picPlaneStep[0] = vmetadec->iFrameStride;
	pFakeFrame->pic.ppPicPlane[0] = pFakeFrame->pBuf;
	memcpy(&pFakeFrame->pic.picROI, &vmetadec->vMetaDecFrameROI, sizeof(IppiRect));
	len = vmetadec->iFrameStride*(vmetadec->vMetaDecFrameROI.height + vmetadec->vMetaDecFrameROI.y);
	memset(pFakeFrame->pBuf, 0, len);   //create a whole green frame
	return pFakeFrame;
}

static gboolean
gst_vmetadec_sinkpad_event (GstPad *pad, GstEvent *event)
{
	Gstvmetadec *vmetadec = GST_VMETADEC(GST_OBJECT_PARENT(pad));
	gboolean eventRet;
	IppCodecStatus cdRet;

	assist_myecho("GST vmetadec get event(%s) from upstream element's pad, thread id 0x%x\n", GST_EVENT_TYPE_NAME(event), (unsigned int)syscall(224));

	switch (GST_EVENT_TYPE (event))
	{
	case GST_EVENT_NEWSEGMENT:
	{
		int bWantFlushFrame;

		gboolean update;
		gdouble rate;
		GstFormat format;
		gint64 start, stop;
		gint64 position;

#ifdef GATHER_GSTVMETA_TIME
		gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->sysClk0, NULL);
		syscall(__NR_clock_gettime, CLOCK_PROCESS_CPUTIME_ID, &((VmetaDecProbeData*)vmetadec->pProbeData)->proClk0);
		syscall(__NR_getrusage, RUSAGE_THREAD, &((VmetaDecProbeData*)vmetadec->pProbeData)->thrClk0);
#endif
		assist_myecho("GST vmetadec !new segment! event received\n");

		vmetadec->bStoppedCodecInEos = 0;

		//bofang, drive vMeta to consume all data. After new vMeta, here should call frame and stream flush
		if(vmetadec->StmCategory != IPP_VIDEO_STRM_FMT_UNAVAIL){
			int vMetaDreamAtLock;
			assist_myecho("New segment, flush frame and stream buffer!\n");
			clear_candidate_outputframes(vmetadec, 0);

			g_mutex_lock(vmetadec->DecMutex);
			assist_myecho("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);
			vMetaDreamAtLock = vmetadec->bVMetaIsInDream;
			if(vMetaDreamAtLock) {
				SuspendResumeVmeta(vmetadec, 0);
			}

			if(vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_MPG2 && vmetadec->DecMPEG2Obj.LeftData != NULL) {
				fill_streams_in_repo(&vmetadec->StmRepo, GST_BUFFER_DATA(vmetadec->DecMPEG2Obj.LeftData), GST_BUFFER_SIZE(vmetadec->DecMPEG2Obj.LeftData));
				gst_buffer_unref(vmetadec->DecMPEG2Obj.LeftData);
				vmetadec->DecMPEG2Obj.LeftData = NULL;
			}
			if(vmetadec->StmRepo.Array[vmetadec->StmRepo.nNotPushedStreamIdx].pUsrData0 != (void*)0) {
				assist_myecho("fill end of seg pattern to stream when new seg occur\n");
				fill_endofseg_pattern_to_stream(&vmetadec->StmRepo);
			}
			if(0 != push_staticstreams_to_vdec(vmetadec->pVDecoderObj, &vmetadec->StmRepo, vmetadec->StmRepo.nPushReadyCnt)) {
				GST_WARNING_OBJECT(vmetadec, "call push_staticstreams_to_vdec fail in %s()", __FUNCTION__);
			}

			vmetadec->iInFrameCumulated = 0;

			if(vmetadec->pVDecoderObj != NULL && (vmetadec->DecErrOccured & (VMETADEC_GST_ERR_GENERICFATAL)) == 0) {
				IppCodecStatus VDecLoopRt;

				VDecLoopRt = loop_vdec_until(vmetadec, IPP_STATUS_NEED_INPUT, &bWantFlushFrame, 1);

				if(VDecLoopRt != IPP_STATUS_NEED_INPUT) {
					vmetadec->DecErrOccured |= VMETADEC_GST_ERR_GENERICFATAL;
					g_warning("!!!!!!! loop vMeta Decoder error %d in NewSeg event", VDecLoopRt);
				}
			}
			if(vMetaDreamAtLock != vmetadec->bVMetaIsInDream) {
				SuspendResumeVmeta(vmetadec, vMetaDreamAtLock);
			}
			assist_myecho("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
			g_mutex_unlock(vmetadec->DecMutex);

		}

		assist_myecho("Candidate push frame cnt %d at newseg\n", g_queue_get_length(&vmetadec->OutFrameQueue));
		clear_candidate_outputframes(vmetadec, 0);
		clear_TSManager(&vmetadec->TSDUManager);

		gst_event_parse_new_segment(event, &update, &rate, &format, &start, &stop, &position);
		if(format == GST_FORMAT_TIME) {
			assist_myecho("At newseg, start %lld ns, stop %lld ns\n", start, stop);
			if(start != -1) {
				if(start >= 0) {
					vmetadec->TSDUManager.SegmentStartTs = start;
				}
				if(stop != -1  && stop >= start) {
					vmetadec->TSDUManager.SegmentEndTs = stop;
				}
			}
		}

		vmetadec->iNewSegPushedFrameCnt = 0;
#ifdef PROFILER_EN
		vmetadec->sFpsWin[D_SKIP].nFrmDecTimeCnt = -1;
		vmetadec->sFpsWin[D_CPUFREQ].nFrmDecTimeCnt = -1;
		vmetadec->sFpsWin[D_SKIP].nPrevFps  = -1;
		vmetadec->sFpsWin[D_CPUFREQ].nPrevFps = -1;
		vmetadec->nSkippedFrames = 0;
		vmetadec->nPrevSkippedFrames = -1;
#endif
		eventRet = gst_pad_push_event(vmetadec->srcpad, event);

		//prepare to decoding new segment
		vmetadec->DecMPEG2Obj.bSeekForPicEnd = 0;
		vmetadec->iSegmentSerialNum++;
		vmetadec->bNotDispFrameInNewSeg = 1;
		break;
	}
	case GST_EVENT_EOS:
	{
		int DreamAtLock;
		int bWantFlushFrame;
		assist_myecho("GST vmetadec !EOS! event received. vMeta Decoder err=%d\n", vmetadec->DecErrOccured);

		if(NULL != vmetadec->pVDecoderObj) {
			IppCodecStatus VDecLoopRt = IPP_STATUS_NOERR;

			if(vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_VC1 && vmetadec->StmRepo.TotalSpareSz >= (int)sizeof(VC1EndSeqStartCode)) {
				fill_streams_in_repo(&vmetadec->StmRepo, VC1EndSeqStartCode, sizeof(VC1EndSeqStartCode));
			}

			g_mutex_lock(vmetadec->DecMutex);
			assist_myecho("g_mutex_lock is called in %s(%d)\n", __FUNCTION__, __LINE__);
			DreamAtLock = vmetadec->bVMetaIsInDream;
			if(DreamAtLock) {
				assist_myecho("vmeta is in dream, wakeup it(line %d)\n", __LINE__);
				SuspendResumeVmeta(vmetadec, 0);
			}

			//push all left ready stream to vmeta
			if(0 != push_staticstreams_to_vdec(vmetadec->pVDecoderObj, &vmetadec->StmRepo, vmetadec->StmRepo.nPushReadyCnt)) {
				vmetadec->DecErrOccured |= VMETADEC_GST_ERR_GENERICFATAL;
			}

			if((vmetadec->DecErrOccured & (VMETADEC_GST_ERR_GENERICFATAL)) == 0) {
				int RetFireEosPad;
				for(;;) {
					if(vmetadec->StmCategory == IPP_VIDEO_STRM_FMT_VC1) {
						//manual fill eospad
						//2010.07.01, it's strange. For VC1 AP stream, IPPVC_END_OF_STREAM cmd has no effect.
						RetFireEosPad = filleospad_to_streams_and_push(vmetadec);
					}else{
						RetFireEosPad = fire_eos_to_vmeta(vmetadec);
					}

					if(RetFireEosPad == -1) {
						vmetadec->DecErrOccured |= VMETADEC_GST_ERR_GENERICFATAL;
						break;  //error happend
					}else if (RetFireEosPad == -2) {
						vmetadec->DecErrOccured |= VMETADEC_GST_ERR_LACKSTREAMBUF;
						break;  //error happend
					}
					assist_myecho("vMeta Decoder push eospad/fire eos cmd into stream\n");
					VDecLoopRt = loop_vdec_until(vmetadec, IPP_STATUS_END_OF_STREAM, &bWantFlushFrame, 0);
					assist_myecho("In EOS loop, loop_vdec_until return %d\n", VDecLoopRt);
					if(VDecLoopRt == IPP_STATUS_END_OF_STREAM) {
						assist_myecho("vMeta Decoder return end of stream!!!!!!\n");
						break;
					}else if(VDecLoopRt == IPP_STATUS_NEED_INPUT || VDecLoopRt == IPP_STATUS_BUFFER_UNDERRUN) {
						continue;
					}else{
						g_warning("vMeta Decoder return error when do EOS decoding %d", VDecLoopRt);
						vmetadec->DecErrOccured |= VMETADEC_GST_ERR_GENERICFATAL;
					}
				}
			}else{
				g_warning("!!!Force stop VMETADEC!!!");
				//DecodeSendCmd_Vmeta(IPPVC_STOP_DECODE_STREAM, NULL, NULL, vmetadec->pVDecoderObj);	//it will be done in stop_vmetadec_whendecoding
			}
			if(VDecLoopRt == IPP_STATUS_END_OF_STREAM) {
				assist_myecho("After vMeta return end of stream, push all frames to downstream!!!!!!\n");
				assist_myecho("will push down frame (line %d), frame account is %d\n", __LINE__, g_queue_get_length(&vmetadec->OutFrameQueue));
				output_frames_to_downstream(vmetadec, 0x7fffffff, GST_FLOW_OK, 1);
			}

			clear_candidate_outputframes(vmetadec, 0);
			clear_TSManager(&vmetadec->TSDUManager);

			if(VDecLoopRt == IPP_STATUS_END_OF_STREAM) {
				//vmeta has been in stop state, needn't fire stop command, but need to pop some buffer
				stop_vmetadec_whendecoding(vmetadec, 0);
				vmetadec->bStoppedCodecInEos = 1;
			}
			//for vMeta decoder, once called sending IPPVC_STOP_DECODE_STREAM in stop_vmetadec_whendecoding(), it should DecoderFree_Vmeta().
			cdRet = DecoderFree_Vmeta(&(vmetadec->pVDecoderObj));
			vmetadec->pVDecoderObj = NULL;
			assist_myecho("---DecoderFree_Vmeta() is called in EOS, ret %d\n", cdRet);
			assist_myecho("g_mutex_unlock will be called in %s(%d)\n", __FUNCTION__, __LINE__);
			g_mutex_unlock(vmetadec->DecMutex);

			clear_mpeg2_seqdatainfos(&vmetadec->DecMPEG2Obj);

			if(vmetadec->PushDummyFrame) {
				if(vmetadec->iNewSegPushedFrameCnt == 0) {
					//Sometimes, after seeking operation, no frame or no I frame is decoded, therefore, probably no frame could be pushed to downstream. Under that case, we push a whole green frame to downstream.
					assist_myecho("----Create and push a dummy frame to downstream (line %d).\n", __LINE__);
					IppVmetaPicture* pDummy = create_dummyframe(vmetadec);
					if(pDummy != NULL) {
						GstClockTime ts = GST_CLOCK_TIME_NONE;
						GstClockTime du = GST_CLOCK_TIME_NONE;
						if(vmetadec->TSDUManager.SegmentEndTs != -1 && vmetadec->TSDUManager.SegmentEndTs > 0) {
							ts = vmetadec->TSDUManager.SegmentEndTs - 1;
						}
						vmetadec_push_frame_to_downElement(vmetadec, pDummy, &ts, &du);
					}
				}
			}
		}
#ifdef GATHER_GSTVMETA_TIME
		gettimeofday(&((VmetaDecProbeData*)vmetadec->pProbeData)->sysClk1, NULL);
		syscall(__NR_clock_gettime, CLOCK_PROCESS_CPUTIME_ID, &((VmetaDecProbeData*)vmetadec->pProbeData)->proClk1);
		syscall(__NR_getrusage, RUSAGE_THREAD, &((VmetaDecProbeData*)vmetadec->pProbeData)->thrClk1);
		{
			long long sysT, proT, thrT;
			sysT = diff_systemclock(&((VmetaDecProbeData*)vmetadec->pProbeData)->sysClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->sysClk1);
			proT = diff_processclock(&((VmetaDecProbeData*)vmetadec->pProbeData)->proClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->proClk1);
			thrT = diff_threadclock(&((VmetaDecProbeData*)vmetadec->pProbeData)->thrClk0, &((VmetaDecProbeData*)vmetadec->pProbeData)->thrClk1);
			//thrT = proT;
			printf("++++++++++++++++ Gather Time information ++++++++++++++++\n");
			printf("From NewSeg to EOS, sys cost %lld usec, process cost %lld usec, thread cost %lld usec\n", sysT, proT, thrT);
			printf("From NewSeg to EOS, sys cost %.2f%%, process cost %.2f%%, thread cost %.2f%%\n", 100., (double)proT*100./sysT, (double)thrT*100./sysT);
			printf("++++++++++++++++++++++++++++++++\n");
		}
#endif

		eventRet = gst_pad_push_event(vmetadec->srcpad, event);
		assist_myecho("GST vmetadec !EOS! event push returned\n");
		break;
	}

	default:
		eventRet = gst_pad_event_default (pad, event);
		break;
	}

	return eventRet;
}


static gboolean
gst_vmetadec_set_checksumfilename(Gstvmetadec* vmetadec, const gchar* location)
{
	if(vmetadec->ChkSumFile) {
		goto CHECKSUMFILE_WAS_OPEN;
	}

	g_free(vmetadec->ChkSumFileName);

	if(G_LIKELY(location != NULL)) {
		vmetadec->ChkSumFileName = g_strdup(location);
	}else{
		vmetadec->ChkSumFileName = NULL;
	}

	return TRUE;

	/* ERRORS */
CHECKSUMFILE_WAS_OPEN:
	g_warning ("Changing the checksumfile property on vmetadec when a checksum file is open not supported.");
	return FALSE;

}

static void
gst_vmetadec_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Gstvmetadec* vmetadec = GST_VMETADEC(object);

	switch (prop_id)
	{
	case ARG_CHECKSUMFILE:
		{
			gst_vmetadec_set_checksumfilename(vmetadec, g_value_get_string(value));
		}
		break;
	case ARG_DISABLEMPEG2PACKING:
		{
			int disable_pack = g_value_get_int(value);
			if(disable_pack != 0 && disable_pack != 1) {
				GST_ERROR_OBJECT(vmetadec, "disable-mpeg2packing %d exceeds range!", disable_pack);
			}else{
				vmetadec->DisableMpeg2Packing = disable_pack;
			}
		}
		break;
	case ARG_NOTDISPBEFOREI_MPEG2:
		{
			int notd = g_value_get_int(value);
			if(notd != 0 && notd != 1) {
				GST_ERROR_OBJECT(vmetadec, "notdisp-beforeI-mpeg2 %d exceeds range!", notd);
			}else{
				vmetadec->NotDispPBBeforeI_MPEG2 = notd;
			}
		}
		break;
	case ARG_KEEPDECFRAMELAYOUT:
		{
			int keep = g_value_get_int(value);
			if(keep != 0 && keep != 1) {
				GST_ERROR_OBJECT(vmetadec, "keep-decframelayout %d exceeds range!", keep);
			}else{
				vmetadec->KeepDecFrameLayout = keep;
			}
		}
		break;
	case ARG_SUPPORTMULTIINSTANCE:
		{
			int mi = g_value_get_int(value);
			if(mi != 0 && mi != 1 && mi != 2) {
				GST_ERROR_OBJECT(vmetadec, "support-multiinstance %d exceeds range!", mi);
			}else{
				vmetadec->SupportMultiinstance = mi;
			}
		}
		break;
	case ARG_HUNGRYSTRATEGY:
		{
			int hs = g_value_get_int(value);
			if(hs != 0 && hs != 1) {
				g_warning("hungry-strategy %d exceeds range!", hs);
			}else{
				vmetadec->HungryStrategy = hs;
			}
		}
		break;
	case ARG_CODECREORDER:
		{
			int cr = g_value_get_int(value);
			if(cr != 0 && cr != 1) {
				g_warning("codec-reorder %d exceeds range!", cr);
			}else{
				vmetadec->CodecReorder = cr;
			}
		}
		break;
	case ARG_CUMULATETHRESHOLD:
		{
			int ct = g_value_get_int(value);
			if(ct < 0 || ct >= STREAM_VDECBUF_NUM) {
				g_warning("cumulate-threshold %d exceeds range!", ct);
			}else{
				vmetadec->CumulateThreshold = ct;
			}
		}
		break;
	case ARG_STMBUFCACHEABLE:
		{
			int sc = g_value_get_int(value);
			if(sc != 0 && sc != 1) {
				g_warning("stmbuf-cacheable %d exceeds range!", sc);
			}else{
				vmetadec->StmBufCacheable = sc;
			}
		}
		break;
	case ARG_ENABLEVMETASLEEPWHENPAUSE:
		{
			int es = g_value_get_int(value);
			if(es != 0 && es != 1) {
				g_warning("vmetasleep-atpause %d exceeds range!", es);
			}else{
				vmetadec->EnableVmetaSleepInPause = es;
			}
		}
		break;
	case ARG_JUDGEFPSFORFIELDH264:
		{
			int a = g_value_get_int(value);
			if(a != 0 && a != 1) {
				g_warning("judgefps-field264 %d exceeds range!", a);
			}else{
				vmetadec->bJudgeFpsForFieldH264 = a;
			}
		}
		break;
	case ARG_PUSHDUMMYFRAME:
		{
			int a = g_value_get_int(value);
			if(a != 0 && a != 1) {
				g_warning("push-dummyframe %d exceeds range!", a);
			}else{
				vmetadec->PushDummyFrame = a;
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
gst_vmetadec_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	Gstvmetadec* vmetadec = GST_VMETADEC(object);
	switch (prop_id)
	{
	case ARG_CHECKSUMFILE:
		g_value_set_string(value, vmetadec->ChkSumFileName);
		break;
	case ARG_DISABLEMPEG2PACKING:
		g_value_set_int(value, vmetadec->DisableMpeg2Packing);
		break;
	case ARG_NOTDISPBEFOREI_MPEG2:
		g_value_set_int(value, vmetadec->NotDispPBBeforeI_MPEG2);
		break;
	case ARG_KEEPDECFRAMELAYOUT:
		g_value_set_int(value, vmetadec->KeepDecFrameLayout);
		break;
	case ARG_SUPPORTMULTIINSTANCE:
		g_value_set_int(value, vmetadec->SupportMultiinstance);
		break;
	case ARG_HUNGRYSTRATEGY:
		g_value_set_int(value, vmetadec->HungryStrategy);
		break;
	case ARG_CODECREORDER:
		g_value_set_int(value, vmetadec->CodecReorder);
		break;
	case ARG_CUMULATETHRESHOLD:
		g_value_set_int(value, vmetadec->CumulateThreshold);
		break;
	case ARG_STMBUFCACHEABLE:
		g_value_set_int(value, vmetadec->StmBufCacheable);
		break;
	case ARG_ENABLEVMETASLEEPWHENPAUSE:
		g_value_set_int(value, vmetadec->EnableVmetaSleepInPause);
		break;
	case ARG_JUDGEFPSFORFIELDH264:
		g_value_set_int(value, vmetadec->bJudgeFpsForFieldH264);
		break;
	case ARG_PUSHDUMMYFRAME:
		g_value_set_int(value, vmetadec->PushDummyFrame);
		break;

#ifdef DEBUG_PERFORMANCE
	case ARG_TOTALFRAME:
		g_value_set_int (value, vmetadec->totalFrames);
		break;
	case ARG_CODECTIME:
		g_value_set_int64 (value, vmetadec->codec_time);
		break;
#endif
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
	return;
}


static void
gst_vmetadec_base_init (gpointer klass)
{
	static GstElementDetails vmetadec_details = {
		"vMeta Decoder",
		"Codec/Decoder/Video",
		"vMeta accerated Video Decoder",
		""
	};

	GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&src_factory));
	gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&sink_factory));

	gst_element_class_set_details (element_class, &vmetadec_details);

	assist_myecho("vmetadec base inited\n");
	return;
}


static void
gst_vmetadec_finalize(GObject * object)
{
	Gstvmetadec* vmetadec = GST_VMETADEC(object);

#ifdef DEBUG_PERFORMANCE
	printf("codec system time %lld usec, frame number %d\n", vmetadec->codec_time, vmetadec->totalFrames);
#endif
#ifdef MEASURE_CODEC_THREADT
	printf("codec cpu time %lld usec\n", ((VmetaDecProbeData*)vmetadec->pProbeData)->codec_cputime);
#endif
#ifdef MEASURE_CHAIN_SYST
	printf("vMeta GST plug-in chain() system time %lld usec, excluding gst_pad_push\n", ((VmetaDecProbeData*)vmetadec->pProbeData)->chain_tick);
#endif

	if(vmetadec->pProbeData) {
		g_free(vmetadec->pProbeData);
	}
	if(vmetadec->DecMutex) {
		g_mutex_free(vmetadec->DecMutex);
	}
	if(vmetadec->FrameRepo.RepoMtx) {
		g_mutex_free(vmetadec->FrameRepo.RepoMtx);
	}
	GST_DEBUG_OBJECT(vmetadec, "At finalizing, Gstvmetadec instance(%p) DecErrOccured is 0x%x", vmetadec, vmetadec->DecErrOccured);
#if 1
	printf("At finalizing, Gstvmetadec instance(0x%x) DecErrOccured is 0x%x.\n", (unsigned int)object, vmetadec->DecErrOccured);
#endif
	G_OBJECT_CLASS(parent_class)->finalize(object);

#if 0
	assist_myecho("Gstvmetadec instance(0x%x) is finalized.\n", (unsigned int)object);
#else
	printf("Gstvmetadec instance(0x%x) is finalized.\n", (unsigned int)object);
#endif
	return;
}


static void
gst_vmetadec_class_init (GstvmetadecClass *klass)
{
	GObjectClass *gobject_class  = (GObjectClass*) klass;
	GstElementClass *gstelement_class = (GstElementClass*) klass;

	//parent_class = g_type_class_peek_parent(klass);   //parent_class has been declared in GST_BOILERPLATE, and this opration has been done in GST_BOILERPLATE_FULL

	gobject_class->set_property = gst_vmetadec_set_property;
	gobject_class->get_property = gst_vmetadec_get_property;

	g_object_class_install_property (gobject_class, ARG_CHECKSUMFILE,
		g_param_spec_string ("checksumfile", "Check Sum File Location",
		"Location of the check sum file to write", NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, ARG_DISABLEMPEG2PACKING, \
		g_param_spec_int ("disable-mpeg2packing", "disable mpeg2 frame packing", \
		"Under some case, decoder plug-in do frame packing for some mpeg2 video stream. It reduces the decoding latency, but probably lower the average decoding speed. Don't access this property unless you are very clear what you are doing. It could be 0<enable packing> or 1<disable packing>.",	\
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_NOTDISPBEFOREI_MPEG2, \
		g_param_spec_int ("notdisp-beforeI-mpeg2", "Ignore P and B frame before I frame for mpeg2", \
		"Not display P/B frames before I frame in one new segment, could be 1<not display> or 0<display>. Only valid for mpeg2", \
		0/* range_MIN */, 1/* range_MAX */, 1/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_KEEPDECFRAMELAYOUT, \
		g_param_spec_int ("keep-decframelayout", "Not change the decoded frame buffer layout", \
		"Keep layout of decoded frame buffer, could be 1<keep> or 0<not keep>. Don't access this property unless you are clear what you are doing.", \
		0/* range_MIN */, 1/* range_MAX */, 1/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_SUPPORTMULTIINSTANCE, \
		g_param_spec_int ("support-multiinstance", "support multiple instance", \
		"To support multiple instance usage. Don't access it unless you know what you're doing very clear. It could be 0<not support>, 1<support> or 2<support and reinit>.", \
		0/* range_MIN */, 2/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_HUNGRYSTRATEGY, \
		g_param_spec_int ("hungry-strategy", "vMeta hungry strategy", \
		"Let vMeta always hungry or not. Don't access it unless you know what you're doing very clear. It could be 0<not always let vMeta hungry>, 1<always let vMeta hungry>. For some mpeg2, it will be 1 automatically.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_CODECREORDER, \
		g_param_spec_int ("codec-reorder", "vMeta codec reorder choice", \
		"Let vMeta codec do reorder or not. Don't access it unless you know what you're doing very clear. It could be 0<codec not reorder>, 1<codec reorder>.", \
		0/* range_MIN */, 1/* range_MAX */, 1/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_CUMULATETHRESHOLD, \
		g_param_spec_int ("cumulate-threshold", "plug-in input frame cumulate threshold", \
		"Decide plug-in input frame cumulate threshold. Only valid for some h264. Don't access it unless you know what you're doing very clear. If it is 0, no cumulate.", \
		0/* range_MIN */, STREAM_VDECBUF_NUM-1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_STMBUFCACHEABLE, \
		g_param_spec_int ("stmbuf-cacheable", "vMeta stream buffer cacheable", \
		"Set vMeta decoder stream buffer cacheable property. Don't access it unless you know what you're doing very clear. It could be 0<not cacheable>, 1<cacheable>.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_ENABLEVMETASLEEPWHENPAUSE, \
		g_param_spec_int ("vmetasleep-atpause", "let vMeta sleep during player pause", \
		"Enable vMeta sleep during player pause. Don't access it unless you know what you're doing very clear. It could be 0<not sleep>, 1<sleep>.", \
		0/* range_MIN */, 1/* range_MAX */, 0/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_JUDGEFPSFORFIELDH264, \
		g_param_spec_int ("judgefps-field264", "for interlace h264 in .mp4 .avi, cut fps if fps > 40", \
		"For interlace h264 in .mp4 or .avi, auto cut demuxer reported fps if it's > 40. Don't access it unless you know what you're doing very clear. It could be 0<not judge>, 1<judge>.", \
		0/* range_MIN */, 1/* range_MAX */, 1/* default_INIT */, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, ARG_PUSHDUMMYFRAME, \
		g_param_spec_int ("push-dummyframe", "push dummy frame when no frame in new segment", \
		"Push a dummy frame when no frame pushed during one segment at EOS. Don't access it unless you know what you're doing very clear. It could be 0<not push>, 1<push>.", \
		0/* range_MIN */, 1/* range_MAX */, 1/* default_INIT */, G_PARAM_READWRITE));

#ifdef DEBUG_PERFORMANCE
	g_object_class_install_property (gobject_class, ARG_TOTALFRAME,
		g_param_spec_int ("totalframes", "Number of frame",
		"Number of total decoded frames for all tracks", 0, G_MAXINT, 0, G_PARAM_READABLE));
	g_object_class_install_property (gobject_class, ARG_CODECTIME,
		g_param_spec_int64 ("codectime", "codec time (microsecond)",
		"Total pure decoding spend system time for all tracks (microsecond)", 0, G_MAXINT64, 0, G_PARAM_READABLE));
#endif

	gobject_class->finalize = gst_vmetadec_finalize;
	gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_vmetadec_change_state);

	GST_DEBUG_CATEGORY_INIT (vmetadec_debug, "vmetadec", 0, "vMeta GST Decoder Element");   //after this, GST_DEBUG(), GST_DEBUG_OBJECT() and ... could work
	assist_myecho("vmetadec class inited\n");
	return;
}

static __inline void
vmetadec_init_members(Gstvmetadec *vmetadec)
{
	vmetadec->pProbeData = g_malloc0(sizeof(VmetaDecProbeData));
	vmetadec->bPlaying = 0;

	vmetadec->osframeres_finalizer = NULL;

	vmetadec->DyncStmRepo.Count = 0;
	vmetadec->DyncStmRepo.pEntryNode = NULL;
	vmetadec->DyncStmRepo.TotalBufSz = 0;
	vmetadec->DyncStmRepo.pPushCandidate = NULL;

	vmetadec->FrameRepo.Count = 0;
	vmetadec->FrameRepo.pEntryNode = NULL;
	vmetadec->FrameRepo.RepoMtx = g_mutex_new();
	if(vmetadec->FrameRepo.RepoMtx == NULL) {
		g_warning("call g_mutex_new for RepoMtx fail.");
	}
	vmetadec->FrameRepo.NotNeedFMemAnyMore = 0;
	vmetadec->FrameRepo.TotalSz = 0;

	g_queue_init(&vmetadec->OutFrameQueue);
	memset(&(vmetadec->VDecParSet), 0, sizeof(vmetadec->VDecParSet));
	vmetadec->VDecParSet.strm_fmt = IPP_VIDEO_STRM_FMT_UNAVAIL;
	vmetadec->VDecParSet.opt_fmt = IPP_YCbCr422I;
	vmetadec->VDecParSet.no_reordering = 0;

	vmetadec->pVDecoderObj = NULL;
	vmetadec->StmCategory = IPP_VIDEO_STRM_FMT_UNAVAIL;
	vmetadec->pfun_digest_inbuf = NULL;
	vmetadec->pCbTable = NULL;

	vmetadec->ChkSumFileName = NULL;
	vmetadec->ChkSumFile = NULL;

	vmetadec->bVmetadecRetAtWaitEvent = 0;

	vmetadec->iFrameContentWidth = 0;
	vmetadec->iFrameContentHeight = 0;
	vmetadec->iFrameCompactSize = 0;
	vmetadec->iFrameStride = 0;
	memset(&vmetadec->vMetaDecFrameROI, 0, sizeof(IppiRect));
	vmetadec->iFRateNum = 0;
	vmetadec->iFRateDen = 1;

	vmetadec->TSDUManager.iTSDelayLimit = TS_MAXDELAY_COMMON;
	vmetadec->TSDUManager.FrameFixDuration = GST_CLOCK_TIME_NONE;
	vmetadec->TSDUManager.DiffTS_Criterion = 0;
	vmetadec->TSDUManager.SegmentEndTs = -1;
	vmetadec->TSDUManager.SegmentStartTs = -1;

	vmetadec->gbuf_cdata = NULL;

	vmetadec->DecErrOccured = 0;

	vmetadec->DecH264Obj.StreamLayout = H264STM_LAYOUT_SYNCCODENAL;
	vmetadec->DecH264Obj.nal_length_size = 0;
	vmetadec->DecH264Obj.bSeqHdrReceivedForNonAVCC = 0;

	vmetadec->DecMPEG2Obj.LeftData = NULL;
	vmetadec->DecMPEG2Obj.bSeqHdrReceived = 0;
	vmetadec->DecMPEG2Obj.bSeekForPicEnd = 0;
	vmetadec->DecMPEG2Obj.iNorminalBitRate = 0;
	vmetadec->DecMPEG2Obj.iNorminalW = 0;
	vmetadec->DecMPEG2Obj.iNorminalH = 0;

	vmetadec->DecMPEG4Obj.iTimeIncBits = -1;
	vmetadec->DecMPEG4Obj.low_delay = -1;
	vmetadec->DecMPEG4Obj.CodecSpecies = -1;
        vmetadec->DecMPEG4Obj.bIsDivx = 0; /* Marvell patch : simplicity 449764 */

	vmetadec->DecVC1MPObj.iContentWidth = -1;
	vmetadec->DecVC1MPObj.iContentHeight = -1;

	vmetadec->bAppendStuffbytes = 1;

	vmetadec->iSegmentSerialNum = -1;
	vmetadec->TSDUManager.TSDUList = NULL;
	vmetadec->TSDUManager.iTDListLen = 0;

	vmetadec->UpAdjacentNonQueueEle = ELEMENT_CHARID_UNDECIDED;
	vmetadec->DownAdjacentNonQueueEle = ELEMENT_CHARID_UNDECIDED;

	vmetadec->bNotDispFrameInNewSeg = 0;

	vmetadec->DisableMpeg2Packing = 0;
#ifdef VMETA_FRAME_CODEDTYPE_ENABLED
	vmetadec->NotDispPBBeforeI_MPEG2 = 1;
#else
	vmetadec->NotDispPBBeforeI_MPEG2 = 0;
#endif

	vmetadec->bStoppedCodecInEos = 0;
	vmetadec->bVMetaIsInDream = 0;
	vmetadec->DecMutex = g_mutex_new();
	if(vmetadec->DecMutex == NULL) {
		g_warning("call g_mutex_new for DecMutex fail.");
	}
	vmetadec->iNewSegPushedFrameCnt = 0;

	vmetadec->CodecReorder = 1;
	vmetadec->StmBufCacheable = 0;

	vmetadec->HungryStrategy = 0;

#if GSTVMETA_PLATFORM == GSTVMETA_PLATFORM_DOVE
	vmetadec->KeepDecFrameLayout = 0;
	vmetadec->SupportMultiinstance = 1;
	vmetadec->CumulateThreshold = 0;
	vmetadec->EnableVmetaSleepInPause = 0;
#elif GSTVMETA_PLATFORM == GSTVMETA_PLATFORM_MMP2
	vmetadec->KeepDecFrameLayout = 1;
	vmetadec->SupportMultiinstance = 1;
	vmetadec->CumulateThreshold = 0;
	vmetadec->EnableVmetaSleepInPause = 1;
#else
	vmetadec->KeepDecFrameLayout = 1;
	vmetadec->SupportMultiinstance = 1;
	vmetadec->CumulateThreshold = 0;
	vmetadec->EnableVmetaSleepInPause = 1;
#endif

	vmetadec->bJudgeFpsForFieldH264 = 1;

	vmetadec->PushDummyFrame = 1;

	vmetadec->iWidth_InSinkCap = 0;
	vmetadec->iHeight_InSinkCap = 0;

	vmetadec->iInFrameCumulated = 0;
	vmetadec->iInFrameCumulated2 = 0;

	vmetadec->totalFrames = 0;
	vmetadec->codec_time = 0;

	vmetadec->ProfilerEn = FALSE;

#ifdef PROFILER_EN
	vmetadec->ProfilerEn = TRUE;
	FPSWin* pFpsWin = &(vmetadec->sFpsWin[0]);
	vmetadec->TagFps_ctl = TRUE;
	vmetadec->nTargeFps  = 0   ;
	vmetadec->nCurOP	 = 8   ;
#ifdef POWER_OPT_DEBUG
	{
		IPP_FILE *f;
		gboolean LocalProfilerEn;
		f = IPP_Fopen("/data/profiler_en.cfg", "r");
		if (f) {
			IPP_Fscanf(f, "%d", &LocalProfilerEn);
			vmetadec->ProfilerEn = LocalProfilerEn;
			IPP_Fclose(f);
		}
	}
#endif
	memset(pFpsWin, 0, sizeof(FPSWin)*2);
	(pFpsWin+D_SKIP)->bAdvanSync=FALSE;
	(pFpsWin+D_SKIP)->nExtAdvanSync|= ENABLE_ADVANAVSYNC_1080P;
	(pFpsWin+D_SKIP)->nFrmDecTimeCnt=-1;
	(pFpsWin+D_SKIP)->nFrmDecTimeWinSize=DEFAULT_FRM_TIME_WIN_SIZE;
	(pFpsWin+D_SKIP)->nPrevFps=-1;
	(pFpsWin+D_SKIP)->nThreOffset= -2;
	(pFpsWin+D_CPUFREQ)->nVSize = 0;

	(pFpsWin+D_CPUFREQ)->bAdvanSync=FALSE;
	(pFpsWin+D_CPUFREQ)->nExtAdvanSync|= ENABLE_ADVANAVSYNC_1080P | ENABLE_ADVANAVSYNC_720P|\
							ENABLE_ADVANAVSYNC_480 | ENABLE_ADVANAVSYNC_SMALL;
	(pFpsWin+D_CPUFREQ)->nFrmDecTimeCnt=-1;
	(pFpsWin+D_CPUFREQ)->nFrmDecTimeWinSize=DEFAULT_FRM_TIME_WIN_SIZE;
	(pFpsWin+D_CPUFREQ)->nPrevFps=-1;
	(pFpsWin+D_CPUFREQ)->nThreOffset= 0;
	(pFpsWin+D_CPUFREQ)->nVSize = 0;
	vmetadec->bFieldStrm	= FALSE;
	vmetadec->eFastMode 	= IPP_VMETA_FASTMODE_DISABLE;
	vmetadec->nFrameRate	= 0;
	vmetadec->nPrevSkippedFrames  = -1;
	vmetadec->nSkippedFrames	  =-1;
#endif

	return;
}

static void
gst_vmetadec_init(Gstvmetadec *vmetadec, GstvmetadecClass *klass)
{
	vmetadec_init_members(vmetadec);

	vmetadec->sinkpad = gst_pad_new_from_template (
		gst_element_class_get_pad_template ((GstElementClass*)klass, "sink"), "sink");


	gst_pad_set_setcaps_function(vmetadec->sinkpad, GST_DEBUG_FUNCPTR (gst_vmetadec_sinkpad_setcaps));
	gst_pad_set_chain_function(vmetadec->sinkpad, GST_DEBUG_FUNCPTR (gst_vmetadec_chain));
	gst_pad_set_event_function(vmetadec->sinkpad, GST_DEBUG_FUNCPTR (gst_vmetadec_sinkpad_event));

	vmetadec->srcpad = gst_pad_new_from_template (
		gst_element_class_get_pad_template ((GstElementClass*)klass, "src"), "src");

	gst_element_add_pad (GST_ELEMENT(vmetadec), vmetadec->sinkpad);
	gst_element_add_pad (GST_ELEMENT(vmetadec), vmetadec->srcpad);

#if 0
	assist_myecho("vmetadec instance(0x%x) inited.\n",(unsigned int)vmetadec);
#else
	printf("vmetadec instance(0x%x) inited.\n",(unsigned int)vmetadec);
#endif
	return;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
	assist_myecho("vmetadec plugin inited\n");
	return gst_element_register(plugin, "vmetadec", GST_RANK_PRIMARY+5, GST_TYPE_VMETADEC);
}

GST_PLUGIN_DEFINE(  GST_VERSION_MAJOR,
					GST_VERSION_MINOR,
					"mvl_vmetadec",
					"video decoder based on vMeta, "__DATE__,
					plugin_init,
					VERSION,
					"LGPL",
					"",
					"");

/* EOF */
