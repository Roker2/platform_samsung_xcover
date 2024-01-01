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



#ifndef __GST_VMETAENC_H__
#define __GST_VMETAENC_H__


#define GSTVMETAENC_PLATFORM_NOMJPEG			0
#define GSTVMETAENC_PLATFORM_HASMJPEG			1
#define GSTVMETAENC_PLATFORM					GSTVMETAENC_PLATFORM_NOMJPEG


#include <gst/gst.h>

#include "codecVC.h"


G_BEGIN_DECLS

#define GST_TYPE_VMETAENC				(gst_vmetaenc_get_type())
#define GST_VMETAENC(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VMETAENC, GstVmetaEnc))
#define GST_VMETAENC_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VMETAENC, GstVmetaEncClass))
#define GST_IS_VMETAENC(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_VMETAENC))
#define GST_IS_VMETAENC_CLASS(obj)		(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_VMETAENC))

typedef void*											RES_HOLD_BY_EXTUSER_WATCHMAN;		//The reason using an individual object instead of the instance of GstVmetaEnc, is gst_buffer_ref/unref is more light calling than gst_object_ref/unref. And sometimes if application developper forget to unref the GstVmetaEnc, RES_HOLD_BY_EXTUSER_WATCHMAN still could be finalized.
#define RES_HOLD_BY_EXTUSER_WATCHMAN_CREATE()			((RES_HOLD_BY_EXTUSER_WATCHMAN)gst_buffer_new())
#define RES_HOLD_BY_EXTUSER_WATCHMAN_REF(watchman)		gst_buffer_ref(watchman)
#define RES_HOLD_BY_EXTUSER_WATCHMAN_UNREF(watchman)	gst_buffer_unref(watchman)


typedef struct _GstVmetaEnc GstVmetaEnc;
typedef struct _GstVmetaEncClass GstVmetaEncClass;

struct _GstVmetaEnc
{
	GstElement					element;
	GstPad						*sinkpad;
	GstPad						*srcpad;

	//IPP vmetaenc
	RES_HOLD_BY_EXTUSER_WATCHMAN	ExtHoldingResWatcher;

	int							driver_init_ret;

	void*						vMetaEnc_Handle;
	IppVmetaEncParSet			vMetaEnc_Par;
	IppVmetaEncInfo				vMetaEnc_Info;

	GSList*						vMetaStmBufRepo;
	GSList*						vMetaFrameBufRepo;


	GQueue						vMetaFrameCandidateQueue;
	GQueue						vMetaStmTSCandidateQueue;
	
	GstClockTime				AotoIncTimestamp;
	
	IppVmetaPicture*			FrameBufferForFileSrc;


	int							iEncW;
	int							iEncH;
	int							iEncFrameRate;

	int							i1SrcFrameDataLen;
	int							iWantedStmBufSz;

	guint64						iFixedFRateDuration;

	int							iWholeFrameInputModeFlag;
	

	//property
	int							iTargtBitRate;
	gint64						ForceStartTimestamp;
	int							iVideoStmFormat;
	int							bDisableRateControl;
	int							iQP;
	int							bOnlyIFrame;
	int							SupportMultiinstance;
	int							HungryStrategy;

	//for log
	guint64						iEncProducedByteCnt;
	int							iEncProducedCompressedFrameCnt;

#ifdef DEBUG_PERFORMANCE
	gint64						codec_time;
#endif

	gpointer					pProbeData;
};

struct _GstVmetaEncClass 
{
	GstElementClass parent_class;
};


G_END_DECLS


#endif /* __GST_VMETAENC_H__ */

/* EOF */
