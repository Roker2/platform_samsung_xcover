/* Small helper element for format conversion
 * (c) 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
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

#include <gst/gst.h>
#include <string.h>
#include <gst/video/video.h>
#include "ippGST_sideinfo.h"

#include "gstscreenshot.h"

#include <utils/Log.h>
#define GST_DEBUG LOGD
#define g_warning LOGW

static void
feed_fakesrc (GstElement * src, GstBuffer * buf, GstPad * pad, gpointer data)
{
  GstBuffer *in_buf = GST_BUFFER (data);

  g_assert (GST_BUFFER_SIZE (buf) >= GST_BUFFER_SIZE (in_buf));
  g_assert (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_READONLY));

  gst_buffer_set_caps (buf, GST_BUFFER_CAPS (in_buf));

  memcpy (GST_BUFFER_DATA (buf), GST_BUFFER_DATA (in_buf),
      GST_BUFFER_SIZE (in_buf));

  GST_BUFFER_SIZE (buf) = GST_BUFFER_SIZE (in_buf);

  GST_DEBUG ("feeding buffer %p, size %u, caps %" GST_PTR_FORMAT,
      buf, GST_BUFFER_SIZE (buf), GST_BUFFER_CAPS (buf));
}

static void
save_result (GstElement * sink, GstBuffer * buf, GstPad * pad, gpointer data)
{
  GstBuffer **p_buf = (GstBuffer **) data;

  *p_buf = gst_buffer_ref (buf);

  GST_DEBUG ("received converted buffer %p with caps %" GST_PTR_FORMAT,
      *p_buf, GST_BUFFER_CAPS (*p_buf));
}

static gboolean
create_element (const gchar *factory_name, GstElement **element, GError **err)
{
  *element = gst_element_factory_make (factory_name, NULL);
  if (*element)
    return TRUE;

  if (err && *err == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "cannot create element '%s' - please check your GStreamer installation",
        factory_name);
  }

  return FALSE;
}


static GstBuffer* convert_gstframe_layout(GstBuffer* tgtGBuf, GstBuffer* srcGBuf, int wid, int hei, int change_type)
{
	GST_BUFFER_TIMESTAMP(tgtGBuf) = GST_BUFFER_TIMESTAMP(srcGBuf);	//in fact, timestamp and duration is useless for yuv2rgb conversion
	GST_BUFFER_DURATION(tgtGBuf) = GST_BUFFER_DURATION(srcGBuf);
	int i;
	if(change_type == 3) {
		//vMeta UYVY
		IPPGSTDecDownBufSideInfo* downbufsideinfo = (IPPGSTDecDownBufSideInfo*)(IPPGST_BUFFER_CUSTOMDATA(srcGBuf));
		int srcStride = downbufsideinfo->x_stride;
		int tgtStride = wid<<1;	//same as gst_video_format_get_row_stride(GST_VIDEO_FORMAT_UYVY, 0, wid)
		unsigned char* pSrc = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + downbufsideinfo->y_off * srcStride + (downbufsideinfo->x_off<<1);
		unsigned char* pTgt = (unsigned char*)(GST_BUFFER_DATA(tgtGBuf));
		for(i=0;i<hei;i++) {
			memcpy(pTgt, pSrc, wid<<1);
			pTgt += tgtStride;
			pSrc += srcStride;
		}
	}else{
		int tgtStride_Y = gst_video_format_get_row_stride(GST_VIDEO_FORMAT_I420, 0, wid);
		int tgtStride_UV = gst_video_format_get_row_stride(GST_VIDEO_FORMAT_I420, 1, wid);
		unsigned char* pTgt_Y = (unsigned char*)(GST_BUFFER_DATA(tgtGBuf));
		unsigned char* pTgt_U = pTgt_Y + gst_video_format_get_component_offset(GST_VIDEO_FORMAT_I420, 1, wid, hei);
		unsigned char* pTgt_V = pTgt_Y + gst_video_format_get_component_offset(GST_VIDEO_FORMAT_I420, 2, wid, hei);
		int srcStride_Y, srcStride_UV;
		unsigned char* pSrc_Y, * pSrc_U, * pSrc_V;
		if(change_type == 2) {
			//C&M I420
			IPPGSTDecDownBufSideInfo* downbufsideinfo = (IPPGSTDecDownBufSideInfo*)(IPPGST_BUFFER_CUSTOMDATA(srcGBuf));
			srcStride_Y = downbufsideinfo->x_stride;
			srcStride_UV = downbufsideinfo->x_stride >> 1;
			int tmp = downbufsideinfo->x_stride * downbufsideinfo->y_stride;
			pSrc_Y = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + downbufsideinfo->y_off * srcStride_Y + downbufsideinfo->x_off;
			pSrc_U = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + tmp + (downbufsideinfo->y_off>>1) * srcStride_UV + (downbufsideinfo->x_off>>1);
			pSrc_V = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + tmp + (tmp>>2) + (downbufsideinfo->y_off>>1) * srcStride_UV + (downbufsideinfo->x_off>>1);
		}else{
			//IPP software compact I420
			srcStride_Y = wid;
			srcStride_UV = wid >> 1;
			int tmp = wid * hei;
			pSrc_Y = (unsigned char*)(GST_BUFFER_DATA(srcGBuf));
			pSrc_U = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + tmp;
			pSrc_V = (unsigned char*)(GST_BUFFER_DATA(srcGBuf)) + tmp + (tmp>>2);
		}
		//copy Y plannar
		if(srcStride_Y == tgtStride_Y) {
			memcpy(pTgt_Y, pSrc_Y, tgtStride_Y*hei);
		}else{
			for(i=0;i<hei;i++) {
				memcpy(pTgt_Y, pSrc_Y, wid);
				pTgt_Y += tgtStride_Y;
				pSrc_Y += srcStride_Y;
			}
		}
		//copy U,V plannar
		if(srcStride_UV == tgtStride_UV) {
			memcpy(pTgt_U, pSrc_U, tgtStride_UV*(hei>>1));
			memcpy(pTgt_V, pSrc_V, tgtStride_UV*(hei>>1));
		}else{
			for(i=0;i<(hei>>1);i++) {
				memcpy(pTgt_U, pSrc_U, wid>>1);
				pTgt_U += tgtStride_UV;
				pSrc_U += srcStride_UV;
			}
			for(i=0;i<(hei>>1);i++) {
				memcpy(pTgt_V, pSrc_V, wid>>1);
				pTgt_V += tgtStride_UV;
				pSrc_V += srcStride_UV;
			}
		}
	}
	return tgtGBuf;
}

#define I420_standardLayout_sameas_compactLayout(w, h)  (((w)&7)==0 && ((h)&1)==0)  //if width is 8 align and height is 2 align, the GST standard I420 layout is the same as marvell compact I420 layout
//convert the frame layout from marvell layout to Gst standard layout
static GstBuffer* standardizeGstFrameLayout(GstBuffer* GstFrame)
{
	GstStructure *s = NULL;
	guint32 fourcc = 0;
	int wid = 0, hei = 0;
	int change_type = 0;
	int standardSize;
	GstBuffer* standardBuf = NULL;
	GstCaps* tgtCap = NULL;

	if(GstFrame == NULL) {
		goto standardizeGstFrameLayout_donothing;
	}
	s = gst_caps_get_structure(GST_BUFFER_CAPS(GstFrame), 0);
	if(s == NULL) {
		goto standardizeGstFrameLayout_donothing;
	}
	
	gst_structure_get_fourcc(s, "format", &fourcc);
	if(fourcc != GST_STR_FOURCC("I420") && fourcc != GST_STR_FOURCC("UYVY")) {
		//marvell GST plug-in only produce I420 or UYVY format frame
		goto standardizeGstFrameLayout_donothing;
	}
	gst_structure_get_int(s, "width", &wid);
	gst_structure_get_int(s, "height", &hei);
	if(wid == 0 || hei == 0 || (wid & 1) || (hei & 1)) {
		//we don't handle odd yuv data
		goto standardizeGstFrameLayout_donothing;
	}

	if(fourcc == GST_STR_FOURCC("I420")) {
		if(!I420_standardLayout_sameas_compactLayout(wid, hei) && (int)GST_BUFFER_SIZE(GstFrame) == (wid*hei*3 >>1)) {
			//decoder plug-in is IPP SW plug-in, provided frame is using marvell compact I420 format, without ROI
			change_type = 1;
		}else if(IPPGST_BUFFER_CUSTOMDATA(GstFrame) != NULL && (int)GST_BUFFER_SIZE(GstFrame) > gst_video_format_get_size(GST_VIDEO_FORMAT_I420, wid, hei)) {
			//decoder plug-in is c&m, the frame is using marvell ROI I420 format
			change_type = 2;
		}
	}else{
		//UYVY format
		if(IPPGST_BUFFER_CUSTOMDATA(GstFrame) != NULL && (int)GST_BUFFER_SIZE(GstFrame) > gst_video_format_get_size(GST_VIDEO_FORMAT_UYVY, wid, hei)) {
			//decoder plug-in is vMeta, the frame is using marvell ROI UYVY format
			change_type = 3;
		}
	}
	
	if(change_type == 0) {
		goto standardizeGstFrameLayout_donothing;
	}

	standardSize = change_type == 3 ? gst_video_format_get_size(GST_VIDEO_FORMAT_UYVY, wid, hei) : gst_video_format_get_size(GST_VIDEO_FORMAT_I420, wid, hei);
	standardBuf = gst_buffer_try_new_and_alloc(standardSize);
	if(standardBuf == NULL) {
		goto standardizeGstFrameLayout_donothing;
	}
	
	tgtCap = gst_caps_copy(GST_BUFFER_CAPS(GstFrame));
	gst_buffer_set_caps(standardBuf, tgtCap);
	gst_caps_unref(tgtCap);	//when create cap, the refcount is 1, after gst_buffer_set_caps, ref count is 2, therefore, should unref it.
	convert_gstframe_layout(standardBuf, GstFrame, wid, hei, change_type);

	return standardBuf;

standardizeGstFrameLayout_donothing:
	return GstFrame;
}

static int calculate_downscaled_w_h(GstBuffer* buf, int* pW, int* pH)
{
#define DOWNSCALE_TARGET_SIZE	320
#define DOWNSCALE_ACTIVE_THRESHOULD		(DOWNSCALE_TARGET_SIZE*DOWNSCALE_TARGET_SIZE*2 > 720*480 ? DOWNSCALE_TARGET_SIZE*DOWNSCALE_TARGET_SIZE*2 : 720*480)
	int src_w = 0, src_h = 0;
	int dst_w, dst_h;
	GstStructure* str = gst_caps_get_structure(GST_BUFFER_CAPS(buf), 0);
	*pW = 0;	// = 0 means no scale
	*pH = 0;
	if(str)
	{
		gst_structure_get_int(str, "width", &src_w);
		gst_structure_get_int(str, "height", &src_h);
	}
	if(src_w <= 0 || src_h <= 0 || (src_w & 1) || (src_h & 1) ) {
		return -1;
	}
	if(src_w * src_h <= DOWNSCALE_ACTIVE_THRESHOULD)
	{
		//if source resolution isn't big, no scale
		return -2;
	}
	dst_w = (DOWNSCALE_TARGET_SIZE + 15) & (~15); //let it 16 align, gcu and jpeg encoder prefer 16 align
	dst_h = dst_w*src_h/src_w;
	dst_h = (dst_h+3) & (~3);	//let it 4 align, gcu prefer 4 align
	
	if(dst_h < 16)
	{
		return -3;	//for too strange shape, no scale
	}

	if(dst_w*dst_h >= src_w*src_h)
	{
		return -4;	//no scale
	}

	*pW = dst_w;
	*pH = dst_h;
	return 0;
}


/* takes ownership of the input buffer */
GstBuffer *
bvw_frame_conv_convert (GstBuffer * buf, GstCaps * to_caps)
{
  GstElement *src, *csp, *filter1, *vscale, *filter2, *sink, *pipeline;
  GstMessage *msg;
  GstBuffer *result = NULL;
  GError *error = NULL;
  GstBus *bus;
  GstCaps *to_caps_no_par;
  int downscaled_w = 0, downscaled_h = 0;
  GstBuffer* oldbuf = buf;

  g_return_val_if_fail (GST_BUFFER_CAPS (buf) != NULL, NULL);

  buf = standardizeGstFrameLayout(oldbuf);
  calculate_downscaled_w_h(buf, &downscaled_w, &downscaled_h);

  /* videoscale is here to correct for the pixel-aspect-ratio for us */
  GST_DEBUG ("creating elements");
  if (!create_element ("fakesrc", &src, &error) ||
      !create_element ("ffmpegcolorspace", &csp, &error) ||
      !create_element ("videoscale", &vscale, &error) ||
      !create_element ("capsfilter", &filter1, &error) ||
      !create_element ("capsfilter", &filter2, &error) ||
      !create_element ("fakesink", &sink, &error)) {
    g_warning ("Could not take screenshot: %s", error->message);
    g_error_free (error);
    result = NULL;
    goto bvw_frame_conv_convert_return;
  }

  pipeline = gst_pipeline_new ("screenshot-pipeline");
  if (pipeline == NULL) {
    g_warning ("Could not take screenshot: %s", "no pipeline (unknown error)");
    result = NULL;
    goto bvw_frame_conv_convert_return;
  }

  GST_DEBUG ("adding elements");
  gst_bin_add_many (GST_BIN (pipeline), src, csp, filter1, vscale, filter2,
      sink, NULL);

  g_signal_connect (src, "handoff", G_CALLBACK (feed_fakesrc), buf);

  /* set to 'fixed' sizetype */
  g_object_set (src, "sizemax", GST_BUFFER_SIZE (buf), "sizetype", 2,
      "num-buffers", 1, "signal-handoffs", TRUE, NULL);

  /* adding this superfluous capsfilter makes linking cheaper */
  to_caps_no_par = gst_caps_copy (to_caps);
  gst_structure_remove_field (gst_caps_get_structure (to_caps_no_par, 0),
      "pixel-aspect-ratio");
  g_object_set (filter1, "caps", to_caps_no_par, NULL);
  gst_caps_unref (to_caps_no_par);

  if(downscaled_w > 0 && downscaled_h > 0)
  {
  	//if do downscale, the pipeline is src->vscale->csp->sink, therefore, the cap after vscale should has same format as input buffer instead of RGB format
	to_caps = gst_caps_copy(GST_BUFFER_CAPS(buf));
	gst_caps_set_simple(to_caps, "width", G_TYPE_INT, downscaled_w, NULL);
	gst_caps_set_simple(to_caps, "height", G_TYPE_INT, downscaled_h, NULL);
  }

  g_object_set (filter2, "caps", to_caps, NULL);
  gst_caps_unref(to_caps);

  g_signal_connect (sink, "handoff", G_CALLBACK (save_result), &result);

  g_object_set (sink, "preroll-queue-len", 1, "signal-handoffs", TRUE, NULL);

  /* FIXME: linking is still way too expensive, profile this properly */
  if(downscaled_w == 0 || downscaled_h == 0)
  {
	  GST_DEBUG ("linking src->csp");
	  if (!gst_element_link_pads (src, "src", csp, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }

	  GST_DEBUG ("linking csp->filter1");
	  if (!gst_element_link_pads (csp, "src", filter1, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }

	  GST_DEBUG ("linking filter1->vscale");
	  if (!gst_element_link_pads (filter1, "src", vscale, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }

	  GST_DEBUG ("linking vscale->capsfilter");
	  if (!gst_element_link_pads (vscale, "src", filter2, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }

	  GST_DEBUG ("linking capsfilter->sink");
	  if (!gst_element_link_pads (filter2, "src", sink, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
  }else{
	  if (!gst_element_link_pads (src, "src", vscale, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
	  if (!gst_element_link_pads (vscale, "src", filter2, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
	  if (!gst_element_link_pads (filter2, "src", csp, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
	  if (!gst_element_link_pads (csp, "src", filter1, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
	  if (!gst_element_link_pads (filter1, "src", sink, "sink"))
	  {
		result = NULL;
		goto bvw_frame_conv_convert_return;
	  }
  }


  GST_DEBUG ("running conversion pipeline");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_poll (bus, GST_MESSAGE_ERROR | GST_MESSAGE_EOS, 25*GST_SECOND);

  if (msg) {
    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_EOS: {
        if (result) {
          GST_DEBUG ("conversion successful: result = %p", result);
        } else {
          GST_WARNING ("EOS but no result frame?!");
        }
        break;
      }
      case GST_MESSAGE_ERROR: {
        gchar *dbg = NULL;

        gst_message_parse_error (msg, &error, &dbg);
        if (error) {
          g_warning ("Could not take screenshot: %s", error->message);
          GST_DEBUG ("%s [debug: %s]", error->message, GST_STR_NULL (dbg));
          g_error_free (error);
        } else {
          g_warning ("Could not take screenshot (and NULL error!)");
        }
        g_free (dbg);
        result = NULL;
        break;
      }
      default: {
        g_return_val_if_reached (NULL);
      }
    }
  } else {
    g_warning ("Could not take screenshot: %s", "timeout during conversion");
    result = NULL;
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

bvw_frame_conv_convert_return:
  if(oldbuf != buf)	//the buf is standardized buffer
  {
    gst_buffer_unref(buf);
  }
  return result;
}

