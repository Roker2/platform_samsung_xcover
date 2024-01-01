/* GStreamer
 * Copyright (C) <2009> Prajnashi S <prajnashi@gmail.com>
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
//#define ENABLE_GST_PLAYER_LOG
#if PLATFORM_SDK_VERSION >=8
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#else
#include <ui/ISurface.h>
#include <ui/Surface.h>
#include <ui/SurfaceComposerClient.h>
#endif
#if PLATFORM_SDK_VERSION <= 4
#include <utils/MemoryBase.h>
#include <utils/MemoryHeapBase.h>
#include <utils/MemoryHeapPmem.h>
#else
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryHeapPmem.h>
#endif
#include <cutils/log.h>
#include <ui/Overlay.h>
#include "surfaceflinger_wrap.h"
#include <GstLog.h>
#include "cutils/properties.h"

#ifdef HAVE_MARVELL_GCU
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <linux/android_pmem.h>
#include "gcu.h"
#include "pmem_helper_lib.h"
#endif

using namespace android;
/* max frame buffers */
#ifdef HAVE_MARVELL_GCU
#define  MAX_FRAME_BUFFERS     5
#else
#define  MAX_FRAME_BUFFERS     2
#endif

#define PMEM_FOR_SW_DEC_CNT	3	//at least need 2, if wait gcu finish work at next frame

//#define DUMP_FILE
//#define DUMP_FILE_OUT
//#define DUMP_GC_OUT
typedef struct 
{
    sp<OverlayRef> overlay_ref;
    sp<Overlay>    overlay;
    sp<MemoryHeapBase> frame_heap;	
    sp<ISurface> isurface;
#if PLATFORM_SDK_VERSION <= 4
    sp<Surface> surface;
#else
    sp<SurfaceControl> surface;
#endif
    Mutex surface_lock;
    int32_t hor_stride;
    int32_t ver_stride;
    uint32_t width;
    uint32_t height;
    PixelFormat format;
    VIDEO_FLINGER_PIXEL_FORMAT vf_format;
    int frame_offset[MAX_FRAME_BUFFERS];
    int buf_index;
    ACCELERATE_HW accelerate_hw;
    int frame_count;
    timeval create_clk;

    VIDEO_FLINGER_SOURCE_FORMAT source_format;    

#ifdef HAVE_MARVELL_GCU
    // gcu staff
    GCUContext gcu_context;
    //GCUFence   gcu_fence[MAX_FRAME_BUFFERS];
    GCUSurface gcu_src_surface[MAX_FRAME_BUFFERS];
    GCUSurface gcu_dst_surface[MAX_FRAME_BUFFERS];
    void* gcu_codec_buffer[MAX_FRAME_BUFFERS];
    void (*free_codec_buf)(void*);

    uint32_t gcu_width;
    uint32_t gcu_height;
    uint32_t gcu_hor_stride;
    uint32_t gcu_ver_stride;
    GCU_FORMAT gcu_src_format;

    GCU_INIT_DATA gcu_initdata;
    GCU_CONTEXT_DATA gcu_contextdata;
    GCU_BLT_DATA gcu_bltdata;

    //int gcu_is_firstframe;

    GCU_RECT gcu_src_roi;
    PMEM_HANDLE_MRVL* pmem_for_sw_dec[PMEM_FOR_SW_DEC_CNT];
    int idx_pmem_for_sw_dec;
#endif

} VideoFlingerDevice;


/* pmem dev node */
#define DEV_PMEM_ADSP "/dev/pmem_adsp"

#ifdef ENABLE_FAST_OVERLAY
#include "overlay.marvell.h"

static void *get_buf_paddr( void *user_priv, void *buf_vaddr ) {
	IPPGSTDecDownBufSideInfo *ippExt = (IPPGSTDecDownBufSideInfo *)IPPGST_BUFFER_CUSTOMDATA(user_priv);
    if( ippExt ) {
        return (void*)ippExt->phyAddr;
    }else {
        return NULL;
    }
}

#endif

static int videoflinger_device_create_new_surface(VideoFlingerDevice* videodev);

/* 
 * The only purpose of class "MediaPlayer" is to call Surface::getISurface()
 * in frameworks/base/include/ui/Surface.h, which is private function and accessed
 * by friend class MediaPlayer.
 *
 * We define a fake one to cheat compiler
 */
namespace android {
class MediaPlayer
{
public:
#if PLATFORM_SDK_VERSION <= 4
    static sp<ISurface> getSurface(const Surface* surface)
#else
    static sp<ISurface> getSurface(const SurfaceControl* surface)
#endif
    {
        return surface->getISurface();
    };
};
};

ACCELERATE_HW videoflinger_get_accelerate_hw(const char* videolayer)
{
    char value[PROPERTY_VALUE_MAX];
    struct stat stbuf;
    if (videolayer)
       strcpy(value, videolayer);
    else
        property_get("video.accelerate.hw", value, "");
    if(strcmp(value, "overlay") == 0)
    {
        return OVERLAY;
    }
#ifdef HAVE_MARVELL_GCU    
    if(strcmp(value, "gc") == 0)
    {
        /* gcu needs physical continuous memory */
        if (0 != stat(DEV_PMEM_ADSP, &stbuf))
        {
            return NONE;
        }
        return GCU;
    }
#else
    if(strcmp(value, "gc") == 0)
    {
        /* Copybit needs physical continuous memory */
        if (0 != stat(DEV_PMEM_ADSP, &stbuf))
        {
            return NONE;
        }
        return COPYBIT;
    }
#endif
    return NONE;
}

VideoFlingerDeviceHandle videoflinger_device_create(void* isurface, const char* videolayer)
{
    VideoFlingerDevice *videodev = NULL;

    GST_PLAYER_INFO("Enter\n");
    videodev = new VideoFlingerDevice;
    if (videodev == NULL)
    {
        return NULL;
    }
    videodev->frame_heap.clear();
    videodev->isurface = (ISurface*)isurface;
    videodev->surface.clear();
    videodev->format = -1;
    videodev->width = 0;
    videodev->height = 0;
    videodev->hor_stride = 0;
    videodev->ver_stride = 0;
    videodev->buf_index = 0;
    for ( int i = 0; i<MAX_FRAME_BUFFERS; i++)
    {
        videodev->frame_offset[i] = 0;
    }


    videodev->accelerate_hw = videoflinger_get_accelerate_hw(videolayer);
    GST_PLAYER_INFO("accelerate_hw is selected as %d\n", videodev->accelerate_hw);
    videodev->frame_count = 0;

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.performance.tunning", value, "0");
    if(strcmp(value, "1") == 0)
    {
        gettimeofday(&(videodev->create_clk), NULL);
        //LOGI("VideoFlingerSink created at %d:%d", videodev->create_clk.tv_sec, videodev->create_clk.tv_usec);
    }
#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
        // initialize gcu stuff
        GST_PLAYER_INFO("gcu staff initialize");

        videodev->free_codec_buf = NULL;
        for ( int i = 0; i<MAX_FRAME_BUFFERS; i++)
        {
	    videodev->gcu_codec_buffer[i] = NULL;
        }
        videodev->idx_pmem_for_sw_dec = 0;
        memset( &(videodev->pmem_for_sw_dec[0]), 0, sizeof(videodev->pmem_for_sw_dec) );

        memset( &(videodev->gcu_initdata), 0 , sizeof(videodev->gcu_initdata) );
        videodev->gcu_initdata.debug = 0; 
        gcuInitialize(&(videodev->gcu_initdata));

        memset( &(videodev->gcu_contextdata), 0, sizeof(videodev->gcu_contextdata) );
        videodev->gcu_context = gcuCreateContext(&(videodev->gcu_contextdata));
        if( videodev->gcu_context == NULL ) {
	 	    GST_PLAYER_ERROR("gcuCreateContext failed");
            return NULL;	
        }

        //for( int i = 0; i < MAX_FRAME_BUFFERS; i++ ) {
            //videodev->gcu_fence[i] = gcuCreateFence(videodev->gcu_context);
        //}

        memset( &(videodev->gcu_bltdata), 0 , sizeof(videodev->gcu_bltdata) );
        //videodev->gcu_is_firstframe = 1;
        GST_PLAYER_INFO("gcu staff initialize done");
    }
#endif
    GST_PLAYER_INFO("Leave\n");
    return (VideoFlingerDeviceHandle)videodev;    
}



int videoflinger_device_create_new_surface(VideoFlingerDevice* videodev)
{
    status_t state;
    int pid = getpid();

    GST_PLAYER_INFO("Enter\n");

    /* Create a new Surface object with 320x240
     * TODO: Create surface according to device's screen size and rotate it
     * 90, but not a pre-defined value.
     */
    sp<SurfaceComposerClient> videoClient = new SurfaceComposerClient;
    if (videoClient.get() == NULL)
    {
        GST_PLAYER_ERROR("Fail to create SurfaceComposerClient\n");
        return -1;
    }

    /* release privious surface */
    videodev->surface.clear();
    videodev->isurface.clear();

    int width = videodev->width;
    int height = videodev->height;

#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
        width = videodev->gcu_width;
        height = videodev->gcu_height;
    }
#endif

    videodev->surface = videoClient->createSurface(
            pid, 
            0, 
            width, 
            height, 
            HAL_PIXEL_FORMAT_RGBA_8888,//, PIXEL_FORMAT_RGB_565,//
            ISurfaceComposer::eFXSurfaceNormal|ISurfaceComposer::ePushBuffers);
    if (videodev->surface.get() == NULL)
    {
        GST_PLAYER_ERROR("Fail to create Surface\n");
        return -1;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.surface_layer", value, "500");

    videoClient->openTransaction();

    /* set Surface toppest z-order, this will bypass all isurface created 
     * in java side and make sure this surface displaied in toppest */
    state =  videodev->surface->setLayer(atoi(value));
    if (state != NO_ERROR)
    {
        GST_PLAYER_INFO("videoSurface->setLayer(), state = %d", state);
        videodev->surface.clear();
        return -1;
    }

    /* show surface */
    state =  videodev->surface->show();
    if (state != NO_ERROR)
    {
        GST_PLAYER_INFO("videoSurface->show(), state = %d", state);
        videodev->surface.clear();
        return -1;
    }

    videoClient->closeTransaction();

    /* get ISurface interface */
    videodev->isurface = MediaPlayer::getSurface(videodev->surface.get());

    /* Smart pointer videoClient shall be deleted automatically
     * when function exists
     */
    GST_PLAYER_INFO("Leave\n");
    return 0;
}

int videoflinger_device_release(VideoFlingerDeviceHandle handle)
{
    GST_PLAYER_INFO("Enter");
    
    if (handle == NULL)
    {
        return -1;
    }
    
    /* unregister frame buffer */
    videoflinger_device_unregister_framebuffers(handle); 

    /* release ISurface & Surface */
    VideoFlingerDevice *videodev = (VideoFlingerDevice*)handle;
    videodev->isurface.clear();
    videodev->surface.clear();

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.performance.tunning", value, "0");
    if(strcmp(value, "1") == 0)
    {
        int duration_msec, fps_int=0, fps_dot=0;
        timeval tv;
        gettimeofday(&tv, NULL);
        //LOGE("VideoFlingerSink released at %d:%d", tv.tv_sec, tv.tv_usec);
        //LOGE("VideoFlingerSink played total %d frames", videodev->frame_count);
	 duration_msec = (tv.tv_sec - videodev->create_clk.tv_sec) * 1000 + (tv.tv_usec - videodev->create_clk.tv_usec) / 1000;
	 if(duration_msec != 0) {
			//use integer division instead of float division, because on some platform, toolchain probably has some issue for float operation
			int fps_100 = videodev->frame_count*1000*100/duration_msec;
			fps_int = fps_100/100;
			fps_dot = fps_100 - fps_int*100;
	 }
	 LOGE("-------++++++ VideoFlingerSink played %d frames, in %d msec, fps is %d.%02d.", videodev->frame_count, duration_msec, fps_int, fps_dot);
    }

#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
	if(videodev->free_codec_buf) {
		for(int i=0;i<MAX_FRAME_BUFFERS;i++) {
			if(videodev->gcu_codec_buffer[i]) {
				videodev->free_codec_buf(videodev->gcu_codec_buffer[i]);
				videodev->gcu_codec_buffer[i] = NULL;
			}
		}
	}
	videodev->free_codec_buf = NULL;
	for(int i=0;i<PMEM_FOR_SW_DEC_CNT;i++) {
		if(videodev->pmem_for_sw_dec[i]) {
			pmem_free(videodev->pmem_for_sw_dec[i]);
			videodev->pmem_for_sw_dec[i] = NULL;
		}
	}
	videodev->idx_pmem_for_sw_dec = 0;
        gcuDestroyContext(videodev->gcu_context);
        gcuTerminate();
    }
#endif	     

    /* delete device */
    delete videodev;

    GST_PLAYER_INFO("Leave");
    return 0;
}

int videoflinger_device_register_framebuffers(VideoFlingerDeviceHandle handle, 
    int w, int h, VIDEO_FLINGER_PIXEL_FORMAT format)
{
    int surface_format = 0;
    GST_PLAYER_INFO("Enter");
    if (handle == NULL)
    {
        GST_PLAYER_ERROR("videodev is NULL");
        return -1;
    }
   
    /* TODO: Change here to support more pixel type */
    if ( !((format ==  VIDEO_FLINGER_RGB_565 ) || (format ==  VIDEO_FLINGER_UYVY ) || (format ==  VIDEO_FLINGER_I420 )) )
    {
        GST_PLAYER_ERROR("Unsupported video format: %d", format);
        return -1;
    }
   
    VideoFlingerDevice *videodev = (VideoFlingerDevice*)handle;
    /* unregister previous buffers */
    if (videodev->frame_heap.get())
    {
        videoflinger_device_unregister_framebuffers(handle);
    }

    switch(format)
    {
    case VIDEO_FLINGER_RGB_565:
        surface_format = PIXEL_FORMAT_RGB_565;
        break;
    case VIDEO_FLINGER_UYVY:
        surface_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
        break;
    case VIDEO_FLINGER_I420:
        surface_format = HAL_PIXEL_FORMAT_YCbCr_420_P;
        break;
    default:
	 LOGE("%s, %d: Error! can't support format %d!", __FILE__, __LINE__, format);
	 return -1;
	 break;
    }

#ifdef HAVE_MARVELL_GCU
    if (videodev->accelerate_hw == GCU)
    {
        surface_format = PIXEL_FORMAT_RGBA_8888;// hardcode to 8888 for dithering purpose...
    }
#endif

    /* reset framebuffers */
    videodev->vf_format = format;
    videodev->format = surface_format;
    videodev->width = (w + 1) & -2;
    videodev->height = (h + 1) & -2;
    videodev->hor_stride = videodev->width;
    videodev->ver_stride =  videodev->height;

#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
        // get display device resolution
        videodev->gcu_width = videodev->width;
        videodev->gcu_height = videodev->height;
	 //use gc resize to accellrate the performace.
        struct fb_var_screeninfo var;
        int fd = open("/dev/graphics/fb0", O_RDWR);
        if( fd > 0 ) {
            int ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
            if( ret >= 0 ) {
				//FIX ME: We may need to keep the aspect ratio. Needs more test streams with all kinds of W*H
				if(videodev->width <= var.xres &&  videodev->height <= var.yres){
						//vsrc smaller than screen, do nothing...
						//Not verified, may have problem...
					}
					else if( (bool)(var.xres < var.yres) == (bool)(videodev->width < videodev->height)){
				                videodev->gcu_width		= var.xres;
				                videodev->gcu_height	= var.yres;
					}
					else{
				                videodev->gcu_width		= var.yres;
				                videodev->gcu_height	= var.xres;
					}
                }
            close(fd);
        }
	#define ALIGN16(m) (((m) + 15)& ~15)
	#define ALIGN4(m)  (((m) + 3) & ~3)
	videodev->gcu_width = ALIGN16(videodev->gcu_width);
	videodev->gcu_height = ALIGN4(videodev->gcu_height);
	
        videodev->gcu_hor_stride = videodev->gcu_width;
        videodev->gcu_ver_stride = videodev->gcu_height;

	    switch(format)
	    {
	    case VIDEO_FLINGER_RGB_565:
	        videodev->gcu_src_format = GCU_FORMAT_RGB565;
	        break;
	    case VIDEO_FLINGER_UYVY:
	        videodev->gcu_src_format = GCU_FORMAT_UYVY;
	        break;
	    case VIDEO_FLINGER_I420:
	        videodev->gcu_src_format = GCU_FORMAT_I420;
	        break;
	    default:
		 LOGE("%s, %d: Error! can't support format %d!", __FILE__, __LINE__, format);
		 return -1;
		 break;
	    }

    }
#endif    
    /* create isurface internally, if no ISurface interface input */
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.gstreamer.create_surface", value, "0");
    if( (videodev->isurface.get() == NULL) && (strcmp(value, "1") == 0 ) )
    {
        videoflinger_device_create_new_surface(videodev);
    }

    /* Should be enought for surface_format. Maybe bigger. */
    int frameSize = videodev->width * videodev->height * 2;
#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
	 if(surface_format==PIXEL_FORMAT_RGBA_8888)
        	frameSize = videodev->gcu_width * videodev->gcu_height * 4; // 4: GCU_FORMAT_ARGB8888 // 2:  GCU_FORMAT_RGB565 
        else
		LOGE("%s, %d: Error! Only RGBA_8888 enabled for Gst Surface Sink!", __FILE__, __LINE__);
    }
#endif
    if (frameSize & 0x3F)
    {
        /* GCU hardware needs the buffer must be 64 byte aligned. */
        frameSize = (frameSize + 0x3F) & (~0x3F);
    }

    GST_PLAYER_INFO( 
        "format=%d, width=%d, height=%d, hor_stride=%d, ver_stride=%d, frameSize=%d",
        videodev->format,
        videodev->width,
        videodev->height,
        videodev->hor_stride,
        videodev->ver_stride,
        frameSize);

    if((videodev->accelerate_hw == OVERLAY) && (videodev->isurface.get()))
    {
#ifdef OVERLAY_FORMAT
        if(format == VIDEO_FLINGER_UYVY)
        {
#if PLATFORM_SDK_VERSION >= 8
	    videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbYCr_422_I,0);
#elif PLATFORM_SDK_VERSION >= 5
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbYCr_422_I);
#else
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_422_I);
#endif
        }
        else
        {
#if PLATFORM_SDK_VERSION >= 8
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P, 0);
#else
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P);
#endif
        }
#else

#if PLATFORM_SDK_VERSION >= 8
        videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P, 0);
#else
	videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P);
#endif

#endif
        if( videodev->overlay_ref.get() == NULL ) {
            GST_PLAYER_ERROR("isurface->createOverlay() failed.");
            return -1;
        }
        videodev->overlay = new Overlay(videodev->overlay_ref);
        return 0;
    }
    else if(videodev->accelerate_hw == COPYBIT)
    {
        /* create frame buffer pmem heap */
        sp<MemoryHeapBase> base = new MemoryHeapBase(DEV_PMEM_ADSP, frameSize * MAX_FRAME_BUFFERS);
        if (base->heapID() < 0)
        {
           GST_PLAYER_ERROR("Can not create PMEM frame buffers. size = %d.\n", frameSize * MAX_FRAME_BUFFERS);
           return -1;
        }
        sp<MemoryHeapPmem> frameBuffersPMEM = new MemoryHeapPmem(base, 0);
        frameBuffersPMEM->slap();
        base.clear();
        videodev->frame_heap = frameBuffersPMEM;
    }
#ifdef HAVE_MARVELL_GCU
    else if( videodev->accelerate_hw == GCU )
    {
        /* create frame buffer pmem heap */
        sp<MemoryHeapBase> base = new MemoryHeapBase(DEV_PMEM_ADSP, frameSize * MAX_FRAME_BUFFERS);
        if (base->heapID() < 0)
        {
           GST_PLAYER_ERROR("Can not create PMEM frame buffers. size = %d.\n", frameSize * MAX_FRAME_BUFFERS);
           return -1;
        }
        sp<MemoryHeapPmem> frameBuffersPMEM = new MemoryHeapPmem(base, 0);
        frameBuffersPMEM->slap();
        base.clear();
        videodev->frame_heap = frameBuffersPMEM;
        struct pmem_region region;
        if( ioctl(videodev->frame_heap->getHeapID(), PMEM_GET_PHYS, &region)  < 0 ) {
            GST_PLAYER_ERROR("ioctl PMEM_GET_PHYS failed");
            return -1;
        }
        unsigned long pmem_phybase = region.offset;
        unsigned char* pmem_virbase = static_cast<unsigned char *>(videodev->frame_heap->base());
        for( int i =0; i < MAX_FRAME_BUFFERS; i++ ) {
            unsigned long p = pmem_phybase + i * frameSize;
            unsigned char* v = pmem_virbase + i * frameSize;            
           videodev->gcu_dst_surface[i] = _gcuCreatePreAllocBuffer(videodev->gcu_context,
                 videodev->gcu_width, videodev->gcu_height, GCU_FORMAT_ARGB8888, //GCU_FORMAT_RGB565, //
                 GCU_TRUE, v, GCU_TRUE, p);
            videodev->gcu_src_surface[i] = NULL; 
        }
    }
#endif
    else
    {
        /* create frame buffer heap base */
        videodev->frame_heap = new MemoryHeapBase(frameSize * MAX_FRAME_BUFFERS);
        if (videodev->frame_heap->heapID() < 0) 
        {
            GST_PLAYER_ERROR("Error creating frame buffer heap!");
            return -1;
        }
    }

    /* create frame buffer heap and register with surfaceflinger */
    int width = videodev->width;
    int height = videodev->height;
    int hstride = videodev->hor_stride;
    int vstride = videodev->ver_stride;
#ifdef HAVE_MARVELL_GCU
    if( videodev->accelerate_hw == GCU ) {
        width = videodev->gcu_width;
        height = videodev->gcu_height;
        hstride = videodev->gcu_hor_stride;
        vstride = videodev->gcu_ver_stride;
    }
	LOGE("create BufferHeap width = %d, height = %d", width, height);
#endif
    ISurface::BufferHeap buffers(
        width,
        height,
        hstride,
        vstride,
        videodev->format,
        videodev->frame_heap);

    if (videodev->isurface.get() && videodev->isurface->registerBuffers(buffers) < 0 )
    {
        GST_PLAYER_ERROR("Cannot register frame buffer!");
        videodev->frame_heap.clear();
        return -1;
    }

    for ( int i = 0; i<MAX_FRAME_BUFFERS; i++)
    {
        videodev->frame_offset[i] = i*frameSize;
    }
    videodev->buf_index = 0;
    GST_PLAYER_INFO("Leave");

    return 0;
}

void videoflinger_device_unregister_framebuffers(VideoFlingerDeviceHandle handle)
{
    GST_PLAYER_INFO("Enter");

    if (handle == NULL)
    {
        return;
    }

    VideoFlingerDevice* videodev = (VideoFlingerDevice*)handle;
    if (videodev->frame_heap.get())
    {
        GST_PLAYER_INFO("Unregister frame buffers.  videodev->isurface = %p", videodev->isurface.get());
        
        /* release MemoryHeapBase */
        GST_PLAYER_INFO("Clear frame buffers.");
        videodev->frame_heap.clear();

        /* reset offset */
        for (int i = 0; i<MAX_FRAME_BUFFERS; i++)
        {
            videodev->frame_offset[i] = 0;
        }
            
        videodev->format = -1;
        videodev->width = 0;
        videodev->height = 0;
        videodev->hor_stride = 0;
        videodev->ver_stride = 0;
        videodev->buf_index = 0;
#ifdef HAVE_MARVELL_GCU
        if( videodev->accelerate_hw == GCU ) {
            videodev->gcu_width = 0;
            videodev->gcu_height = 0;
            videodev->gcu_hor_stride = 0;
            videodev->gcu_ver_stride = 0;
            //videodev->gcu_is_firstframe = 1;
            for( int i = 0; i < MAX_FRAME_BUFFERS; i++ ) {
                if( videodev->gcu_dst_surface[i] != NULL ) {
                    _gcuDestroyBuffer(videodev->gcu_context, videodev->gcu_dst_surface[i]);
                    videodev->gcu_dst_surface[i] = NULL;
                }
                if( videodev->gcu_src_surface[i] != NULL ) {
                    _gcuDestroyBuffer(videodev->gcu_context, videodev->gcu_src_surface[i]);
                    videodev->gcu_src_surface[i] = NULL;
                }
                //if( videodev->gcu_fence[i] != NULL ) {
                    //gcuDestroyFence(videodev->gcu_context, videodev->gcu_fence[i]);
                    //videodev->gcu_fence[i] = NULL;
                //}
            }
        }
#endif
    }

    if(videodev->isurface.get())
    {
        videodev->isurface->unregisterBuffers();
        videodev->isurface.clear();
    }

    if(videodev->overlay.get())
    {
        videodev->overlay->destroy();
        videodev->overlay.clear();
    }

    if(videodev->overlay_ref.get())
    {
        videodev->overlay_ref.clear();
    }
    GST_PLAYER_INFO("Leave");
}

void videoflinger_device_post(VideoFlingerDeviceHandle handle, void * buf, int bufsize, void (*gfree)(void*), void *user)
{
    GST_PLAYER_INFO("Enter");
    
    if (handle == NULL)
    {
        return;
    }


	char prop_value[64];

	//dump input yuv if required.
	{
		memset(prop_value, 0, sizeof(prop_value));
		property_get("video.sink_dump.inputyuv", prop_value, "0");
		static  FILE *fp_yuv = NULL;
		if(strcmp(prop_value, "1") == 0)
		{
	    		if( fp_yuv == NULL )
	        		fp_yuv = fopen("/data/surfaceflinger.yuv", "w+");
		        fwrite(buf, 1, bufsize, fp_yuv);
	       	 fflush(fp_yuv);
		}else{
			if( fp_yuv != NULL ){
			        fclose(fp_yuv);
			        fp_yuv = NULL;
			}
	    	}
	}

    VideoFlingerDevice* videodev = (VideoFlingerDevice*)handle;
    AutoMutex _l(videodev->surface_lock);
    if( videodev->isurface.get() == 0 ) {
        if(gfree)
        {
            gfree(user);
        }
        return;
    }

    if(videodev->accelerate_hw == OVERLAY)
    {
		/* Get the subrectangle info from GSTBUF */
	IPPGSTDecDownBufSideInfo *subrect = (IPPGSTDecDownBufSideInfo *)IPPGST_BUFFER_CUSTOMDATA(user);
   
		if(videodev->overlay.get())
        {
#ifdef ENABLE_FAST_OVERLAY
            if(get_buf_paddr(user, buf))
            {
                /* It's physical continous buffer. */
                overlay_buffer_header_t *buffer;
 
                videodev->overlay->dequeueBuffer((overlay_buffer_t*)&buffer);
 		   		if(subrect){
 		   			GST_PLAYER_INFO("%s x_off = %d, y_off = %d, x_stride = %d, y_stride = %d",
 		   				__FUNCTION__, subrect->x_off, subrect->y_off, subrect->x_stride, subrect->y_stride );
    
 		   			/* Pass the subrectangle info to overlay hal for the 16byte non-alignment support */
                       buffer->x_off		= subrect->x_off ;
                       buffer->y_off		= subrect->y_off;

					   //stride information is in byte, while overlay hal requires this para to be in pixel.
					   if( HAL_PIXEL_FORMAT_YCbCr_422_I == videodev->format || PIXEL_FORMAT_RGB_565 == videodev->format )
                       	buffer->stride_x	= subrect->x_stride/2;
					   else
                       	buffer->stride_x	= subrect->x_stride;

                       buffer->stride_y	= subrect->y_stride;
 		   		}else{
 		   			GST_PLAYER_INFO("%s There is no stride information passed, so use the video width and height as the stride information",
 		   				__FUNCTION__);
 		   			buffer->x_off		= 0 ;
 		   			buffer->y_off		= 0;
 		   			buffer->stride_x	= videodev->width;
 		   			buffer->stride_y	= videodev->height;
 		   			GST_PLAYER_INFO("%s x_off = %d, y_off = %d, x_stride = %d, y_stride = %d",
 		   				__FUNCTION__, buffer->x_off, buffer->y_off, buffer->stride_x, buffer->stride_y );
 		   		}

				buffer->flag = 2;
                buffer->len = bufsize;
                buffer->paddr = get_buf_paddr(user, buf);
                buffer->vaddr = buf;
                buffer->user = user;
                buffer->free = gfree;
                buffer->deinterlace = subrect->NeedDeinterlace;

                videodev->frame_count++;
                videodev->overlay->queueBuffer((overlay_buffer_t)buffer);
            }
            else
#endif
            {
                overlay_buffer_header_t *buffer;
                videodev->overlay->dequeueBuffer((overlay_buffer_t*)&buffer);

				if(subrect){
					GST_PLAYER_INFO("%s x_off = %d, y_off = %d, x_stride = %d, y_stride = %d",
						__FUNCTION__, subrect->x_off, subrect->y_off, subrect->x_stride, subrect->y_stride );

					/* Pass the subrectangle info to overlay hal for the 16byte non-alignment support */
                    buffer->x_off		= subrect->x_off ;
                    buffer->y_off		= subrect->y_off;

					//stride information is in byte, while overlay hal requires this para to be in pixel.
				   if( HAL_PIXEL_FORMAT_YCbCr_422_I == videodev->format || PIXEL_FORMAT_RGB_565 == videodev->format )
                    	buffer->stride_x	= subrect->x_stride/2;
					else
                       	buffer->stride_x	= subrect->x_stride;

                    buffer->stride_y	= subrect->y_stride;
				}else{
					GST_PLAYER_INFO("%s There is no stride information passed, so use the video width and height as the stride information",
						__FUNCTION__);
					buffer->x_off		= 0 ;
					buffer->y_off		= 0;
					buffer->stride_x	= videodev->width;
					buffer->stride_y	= videodev->height;
					GST_PLAYER_INFO("%s x_off = %d, y_off = %d, x_stride = %d, y_stride = %d",
						__FUNCTION__, buffer->x_off, buffer->y_off, buffer->stride_x, buffer->stride_y );
				}

				void* address = videodev->overlay->getBufferAddress((overlay_buffer_t)buffer);
				GST_PLAYER_INFO("%s go through non-fastoverlay path, buffer address get from overlay is %p",__FUNCTION__,address);
                if(address)
                {
                    memcpy( address, buf, bufsize);
                    videodev->frame_count++;
					videodev->overlay->queueBuffer((overlay_buffer_t)buffer);
                }
                if(gfree)
                {
                    gfree(user);
                }
            }
        } else {
	        if(gfree)
	        {
	            gfree(user);
	        }
	}
    }
#ifdef HAVE_MARVELL_GCU
    else if( videodev->accelerate_hw == GCU ) {
      GCU_RECT gcu_src_subrect;
      if (++videodev->buf_index == MAX_FRAME_BUFFERS)
            videodev->buf_index = 0;
      //videodev->buf_index = (videodev->buf_index+1) % MAX_FRAME_BUFFERS;
      videodev->gcu_codec_buffer[videodev->buf_index] = user;
      videodev->free_codec_buf = gfree;

#ifdef ENABLE_FAST_OVERLAY
        // create source gcu surface
        int align_w, align_h;
        unsigned long align_Uoffset, align_Voffset;
        void* paddr = get_buf_paddr(user, buf);
        if( paddr != NULL ) {
            IPPGSTDecDownBufSideInfo *subrect = (IPPGSTDecDownBufSideInfo *)IPPGST_BUFFER_CUSTOMDATA(user);
            align_w = videodev->gcu_src_format == GCU_FORMAT_I420 ? subrect->x_stride : (subrect->x_stride>>1);	//subrect->x_stride's unit is byte, align_w's unit is pixel
            align_h = subrect->y_stride;
            gcu_src_subrect.left = subrect->x_off;
            gcu_src_subrect.top = subrect->y_off;
            gcu_src_subrect.right = gcu_src_subrect.left + videodev->width;
            gcu_src_subrect.bottom = gcu_src_subrect.top + videodev->height;
            if( videodev->gcu_src_surface[videodev->buf_index] == NULL ) {
                videodev->gcu_src_surface[videodev->buf_index] = _gcuCreatePreAllocBuffer(videodev->gcu_context,
                     align_w, align_h, videodev->gcu_src_format,
                     GCU_TRUE, buf, GCU_TRUE, (unsigned long)paddr);
            } else {
			GCU_ALLOC_INFO allocInfo[3];
			GCU_SURFACE_UPDATE_DATA surfaceUpdate;
			surfaceUpdate.pSurface = videodev->gcu_src_surface[videodev->buf_index];
			surfaceUpdate.flag.bits.preAllocVirtual = 1;
			surfaceUpdate.flag.bits.preAllocPhysical = 1;
			if (videodev->gcu_src_format == GCU_FORMAT_I420){
				align_Uoffset = align_w * align_h;
				align_Voffset = align_Uoffset + (align_Uoffset>>2);
				allocInfo[0].virtualAddr            = (GCUVirtualAddr)buf;
				allocInfo[0].physicalAddr           = (GCUPhysicalAddr)paddr;

				allocInfo[1].virtualAddr            = (GCUVirtualAddr)((unsigned int)buf + align_Uoffset);
				allocInfo[1].physicalAddr           = (GCUPhysicalAddr)((unsigned int)paddr + align_Uoffset);

				allocInfo[2].virtualAddr            = (GCUVirtualAddr)((unsigned int)buf + align_Voffset);
				allocInfo[2].physicalAddr           = (GCUPhysicalAddr)((unsigned int)paddr + align_Voffset);

				surfaceUpdate.arraySize = 3;
			}else{//videodev->gcu_src_format == GCU_FORMAT_UYVY or GCU_FORMAT_RGB565
				allocInfo[0].virtualAddr            = (GCUVirtualAddr)buf;
				allocInfo[0].physicalAddr           = (GCUPhysicalAddr)paddr;
				
				surfaceUpdate.arraySize = 1;
			}
			surfaceUpdate.pAllocInfos = allocInfo;
			gcuUpdateSurface(videodev->gcu_context, &surfaceUpdate);
            }
        } else {
            void* copyed_paddr = NULL;
            void* copyed_vaddr = NULL;
            align_w = GST_ROUND_UP_16(videodev->width);
            align_h = GST_ROUND_UP_4(videodev->height);
            align_Uoffset = align_w * align_h;
            align_Voffset = align_Uoffset + (align_Uoffset>>2);
            gcu_src_subrect.left = 0;
            gcu_src_subrect.top = 0;
            gcu_src_subrect.right = gcu_src_subrect.left + videodev->width;
            gcu_src_subrect.bottom = gcu_src_subrect.top + videodev->height;
            //if( ((unsigned int)buf & 63) || (videodev->width & 15) || (videodev->height & 3) ) {//if software decoder frame isn't 64 byte align or width isn't 16 pixel align or height isn't 4 pixel align, need memory copy
            if(1) { //must do memory copy for all software frame, because address of software frame probably 64 align, probably not, and changed frame by frame. Therefore, if we don't do memory copy for each frame, the frame to gcuUpdateSurface() probably phsical continue, probably not.  gcuUpdateSurface() couldn't accept that case.
                if(videodev->pmem_for_sw_dec[videodev->idx_pmem_for_sw_dec] == NULL) {
                    videodev->pmem_for_sw_dec[videodev->idx_pmem_for_sw_dec] = pmem_malloc(videodev->gcu_src_format == GCU_FORMAT_I420?(align_w*align_h*3>>1):(align_w*align_h<<1), MARVELL_PMEMDEV_NAME_NONCACHED);	//allocate non-cached buffer, therefore, no need to flush after memcpy
                }
                if(videodev->pmem_for_sw_dec[videodev->idx_pmem_for_sw_dec] == NULL) {
                    LOGE(" +++++ Allocate pmem for software frame fail!!! +++++");
                }
                copyed_paddr = videodev->pmem_for_sw_dec[videodev->idx_pmem_for_sw_dec]->pa;
                copyed_vaddr = videodev->pmem_for_sw_dec[videodev->idx_pmem_for_sw_dec]->va;
                //copy sw frame to pmem
                if(videodev->gcu_src_format == GCU_FORMAT_I420) {
                    if(align_w == (int)videodev->width && align_h == (int)videodev->height) {
                        //only buf isn't 64 byte align
                        memcpy(copyed_vaddr, buf, align_w*align_h*3>>1);
                    }else if(align_w == (int)videodev->width) {
                        unsigned int non_align_Uoffset = align_w*videodev->height;
                        memcpy(copyed_vaddr, buf, non_align_Uoffset);
                        memcpy((unsigned char*)copyed_vaddr + align_Uoffset, (unsigned char*)buf + non_align_Uoffset, non_align_Uoffset>>2);
                        memcpy((unsigned char*)copyed_vaddr + align_Voffset, (unsigned char*)buf + non_align_Uoffset + (non_align_Uoffset>>2), non_align_Uoffset>>2);
                    }else{
                        unsigned char* tgt = (unsigned char*)copyed_vaddr;
                        unsigned char* src = (unsigned char*)buf;
                        int src_stride = videodev->width;
                        int src_stride_uv = videodev->width>>1;
                        if((videodev->width & 7) && (uint32_t)bufsize == (GST_ROUND_UP_4(videodev->width)+GST_ROUND_UP_4(videodev->width>>1))*videodev->height) {//the software frame layout is GST standard I420, not common I420
                            src_stride = GST_ROUND_UP_4(videodev->width);
                            src_stride_uv = GST_ROUND_UP_4(videodev->width>>1);
                        }
                        unsigned int i;
                        //copy Y
                        for(i=0;i<videodev->height;i++) {
                            memcpy(tgt, src, videodev->width);
                            tgt += align_w;
                            src += src_stride;
                        }
                        //copy U
                        tgt = (unsigned char*)copyed_vaddr + align_Uoffset;
                        for(i=0;i<videodev->height>>1;i++) {
                            memcpy(tgt, src, videodev->width>>1);
                            tgt += align_w>>1;
                            src += src_stride_uv;
                        }
                        //copy V
                        tgt = (unsigned char*)copyed_vaddr + align_Voffset;
                        for(i=0;i<videodev->height>>1;i++) {
                            memcpy(tgt, src, videodev->width>>1);
                            tgt += align_w>>1;
                            src += src_stride_uv;
                        }
                    }
                }else{	//for RGB565 or UYVY
                    if(align_w == (int)videodev->width) {
                        memcpy(copyed_vaddr, buf, align_w*videodev->height<<1);
                    }else{
                        unsigned char* tgt = (unsigned char*)copyed_vaddr;
                        unsigned char* src = (unsigned char*)buf;
                        int src_stride = videodev->width<<1;
                        unsigned int i;
                        for(i=0;i<videodev->height;i++) {
                            memcpy(tgt, src, src_stride);
                            tgt += align_w<<1;
                            src += src_stride;
                        }
                    }
                }
                videodev->idx_pmem_for_sw_dec++;
                if(videodev->idx_pmem_for_sw_dec == PMEM_FOR_SW_DEC_CNT) {
                    videodev->idx_pmem_for_sw_dec = 0;
                }
            }
            if( videodev->gcu_src_surface[videodev->buf_index] == NULL ) {
                videodev->gcu_src_surface[videodev->buf_index] = _gcuCreatePreAllocBuffer(videodev->gcu_context,
                     align_w, align_h, videodev->gcu_src_format,
                     GCU_TRUE, copyed_vaddr==NULL?buf:copyed_vaddr, copyed_paddr==NULL?GCU_FALSE:GCU_TRUE, (unsigned long)copyed_paddr);
				if( videodev->gcu_src_surface[videodev->buf_index] == NULL ) {
					LOGE("_gcuCreatePreAllocaBuffer failed----------------");
                }
            } else {
			GCU_ALLOC_INFO allocInfo[3];
			GCU_SURFACE_UPDATE_DATA surfaceUpdate;
			surfaceUpdate.pSurface = videodev->gcu_src_surface[videodev->buf_index];
			surfaceUpdate.flag.bits.preAllocVirtual = 1;
			unsigned long vbase = (unsigned long)(copyed_vaddr==NULL?buf:copyed_vaddr);
			unsigned long pbase = (unsigned long)(copyed_paddr==NULL?0:copyed_paddr);
			surfaceUpdate.flag.bits.preAllocPhysical = pbase==0?0:1;
			if (videodev->gcu_src_format == GCU_FORMAT_I420){
				allocInfo[0].virtualAddr            = (GCUVirtualAddr)vbase;
				allocInfo[0].physicalAddr           = (GCUPhysicalAddr)pbase;

				allocInfo[1].virtualAddr            = (GCUVirtualAddr)(vbase + align_Uoffset);
				allocInfo[1].physicalAddr           = (GCUPhysicalAddr)(pbase == 0 ? 0 : pbase + align_Uoffset);

				allocInfo[2].virtualAddr            = (GCUVirtualAddr)(vbase + align_Voffset);
				allocInfo[2].physicalAddr           = (GCUPhysicalAddr)(pbase == 0 ? 0 : pbase + align_Voffset);

				surfaceUpdate.arraySize = 3;
			}else{//videodev->gcu_src_format == GCU_FORMAT_UYVY or GCU_FORMAT_RGB565
				allocInfo[0].virtualAddr            = (GCUVirtualAddr)vbase;
				allocInfo[0].physicalAddr           = (GCUPhysicalAddr)pbase;
				
				surfaceUpdate.arraySize = 1;
			}
			surfaceUpdate.pAllocInfos = allocInfo;
			gcuUpdateSurface(videodev->gcu_context, &surfaceUpdate);
            }
        }
#else
#error "Not support currently, 2011.05.20"
        if( videodev->gcu_src_surface[videodev->buf_index] == NULL ) {
             videodev->gcu_src_surface[videodev->buf_index] = _gcuCreatePreAllocBuffer(videodev->gcu_context,
                 videodev->width, videodev->height, videodev->gcu_src_format,
                 GCU_TRUE, buf, GCU_FALSE, 0);
        } else {
		GCU_ALLOC_INFO allocInfo[3];
		GCU_SURFACE_UPDATE_DATA surfaceUpdate;
		surfaceUpdate.pSurface = videodev->gcu_src_surface[videodev->buf_index];
		surfaceUpdate.flag.bits.preAllocVirtual = 1;
		surfaceUpdate.flag.bits.preAllocPhysical = 0;
		if (videodev->gcu_src_format == GCU_FORMAT_I420){
			allocInfo[0].virtualAddr            = (GCUVirtualAddr)buf;
			allocInfo[0].physicalAddr           = (GCUPhysicalAddr)0;

			allocInfo[1].virtualAddr            = (GCUVirtualAddr)((unsigned int)buf + videodev->width * videodev->height);
			allocInfo[1].physicalAddr           = (GCUPhysicalAddr)0;

			allocInfo[2].virtualAddr            = (GCUVirtualAddr)((unsigned int)buf + videodev->width * videodev->height*5/4);
			allocInfo[2].physicalAddr           = (GCUPhysicalAddr)0;

			surfaceUpdate.arraySize = 3;
		}else{//videodev->gcu_src_format == GCU_FORMAT_UYVY or GCU_FORMAT_RGB565
			allocInfo[0].virtualAddr            = (GCUVirtualAddr)buf;
			allocInfo[0].physicalAddr           = (GCUPhysicalAddr)0;
			
			surfaceUpdate.arraySize = 1;
		}
		surfaceUpdate.pAllocInfos = allocInfo;
		gcuUpdateSurface(videodev->gcu_context, &surfaceUpdate);
        }
#endif
//marked, non-blocking calling changed to blocking calling
/*        if( videodev->gcu_is_firstframe == 1 ) {
            videodev->gcu_is_firstframe = 0;
        } else {
            int flip_index = videodev->buf_index -1;
            if(flip_index < 0 ) {
                flip_index = MAX_FRAME_BUFFERS -1;
            }
            gcuWaitFence(videodev->gcu_context, 
                videodev->gcu_fence[flip_index], GCU_INFINITE);
            if( gfree && videodev->gcu_codec_buffer[flip_index] ) {
                gfree(videodev->gcu_codec_buffer[flip_index]);
                videodev->gcu_codec_buffer[flip_index] = NULL;
            }


            videodev->isurface->postBuffer(videodev->frame_offset[flip_index]);

        }*/
        memset(&(videodev->gcu_bltdata), 0, sizeof(videodev->gcu_bltdata));
        videodev->gcu_bltdata.pSrcSurface = videodev->gcu_src_surface[videodev->buf_index];
        videodev->gcu_src_roi = gcu_src_subrect;
        videodev->gcu_bltdata.pSrcRect = &videodev->gcu_src_roi;
        videodev->gcu_bltdata.pDstSurface = videodev->gcu_dst_surface[videodev->buf_index];
        videodev->gcu_bltdata.pDstRect = 0;
        videodev->gcu_bltdata.rotation = GCU_ROTATION_0;
        gcuBlit(videodev->gcu_context, &(videodev->gcu_bltdata));
        //gcuSendFence(videodev->gcu_context, videodev->gcu_fence[videodev->buf_index]);
        //gcuFlush(videodev->gcu_context);
        gcuFinish(videodev->gcu_context);
        if( gfree && videodev->gcu_codec_buffer[videodev->buf_index] ) {
            gfree(videodev->gcu_codec_buffer[videodev->buf_index]);
            videodev->gcu_codec_buffer[videodev->buf_index] = NULL;
        }
        videodev->isurface->postBuffer(videodev->frame_offset[videodev->buf_index]);

        {  //dump rgb data...
                    memset(prop_value, 0, sizeof(prop_value));
                    property_get("video.sink_dump.outputrgb", prop_value, "0");
                       static FILE *fp_rgb = NULL;
                    if(strcmp(prop_value, "1") == 0)
                    {
                            if( fp_rgb == NULL ) {
                                fp_rgb = fopen("/data/surfaceflinger.rgb", "w+");
                            }
                           unsigned char* rgbbuff = static_cast<unsigned char *>(videodev->frame_heap->base());
                            rgbbuff += videodev->frame_offset[videodev->buf_index];
                            fwrite(rgbbuff, 1, videodev->gcu_width * videodev->gcu_height * 4, fp_rgb);
                            fflush(fp_rgb);
                        } else{
                           if( fp_rgb != NULL ) {
                            fclose(fp_rgb);
                            fp_rgb = NULL;
                           }
                        }
         }

       videodev->frame_count++;

	{//dump bmp with gcudump...
		memset(prop_value, 0, sizeof(prop_value));
		property_get("video.sink_dump.outputgc", prop_value, "0");
		if(strcmp(prop_value, "1") == 0)
		{
			  	_gcuDumpSurface(videodev->gcu_context, videodev->gcu_bltdata.pDstSurface, "/data/sink_dst_");
		}
	}


    }
#endif
    else
    {
        if (++videodev->buf_index == MAX_FRAME_BUFFERS) 
            videodev->buf_index = 0;
   
        memcpy (static_cast<unsigned char *>(videodev->frame_heap->base()) + videodev->frame_offset[videodev->buf_index],  buf, bufsize);

        GST_PLAYER_INFO ("Post buffer[%d].\n", videodev->buf_index);
        videodev->frame_count++;
        videodev->isurface->postBuffer(videodev->frame_offset[videodev->buf_index]);
        if(gfree)
        {
            gfree(user);
        }
    }
    GST_PLAYER_INFO("Leave");
}


int videoflinger_device_changeVideoSurface(VideoFlingerDevice* videodev, void* isurface) {
    // release original surface
    if(videodev->isurface.get())
    {
        if( videodev->accelerate_hw != OVERLAY ) {
            videodev->isurface->unregisterBuffers();
        }
        videodev->isurface.clear();
    }

    if(videodev->overlay.get())
    {
        videodev->overlay->destroy();
        videodev->overlay.clear();
    }

    if(videodev->overlay_ref.get())
    {
        videodev->overlay_ref.clear();
    }

    // init net surface
    videodev->isurface = (ISurface*)isurface;
    if( videodev->accelerate_hw != OVERLAY ) {
	int w = videodev->width;
	int h = videodev->height;
	int hor_stride = videodev->hor_stride;
	int ver_stride = videodev->ver_stride;
#ifdef HAVE_MARVELL_GCU
	if(videodev->accelerate_hw == GCU) {
		w = videodev->gcu_width;
		h = videodev->gcu_height;
		hor_stride = videodev->gcu_hor_stride;
		ver_stride = videodev->gcu_ver_stride;
	}
#endif
        ISurface::BufferHeap buffers(
            w,
            h,
            hor_stride,
            ver_stride,
            videodev->format,
            videodev->frame_heap);

        if (videodev->isurface.get() && videodev->isurface->registerBuffers(buffers) < 0 )
        {
            GST_PLAYER_ERROR("Cannot register frame buffer!");
            videodev->frame_heap.clear();
        }
    }
    if((videodev->accelerate_hw == OVERLAY) && videodev->isurface.get())
    {
#ifdef OVERLAY_FORMAT
        if(videodev->vf_format == VIDEO_FLINGER_UYVY)
        {
#if PLATFORM_SDK_VERSION >= 8
	    videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbYCr_422_I,0);
#elif PLATFORM_SDK_VERSION >= 5
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbYCr_422_I);
#else
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_422_I);
#endif
        }
        else
        {
#if PLATFORM_SDK_VERSION >= 8
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P, 0);
#else
            videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P);
#endif
        }
#else

#if PLATFORM_SDK_VERSION >= 8
        videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P, 0);
#else
	videodev->overlay_ref = videodev->isurface->createOverlay(videodev->width, videodev->height, OVERLAY_FORMAT_YCbCr_420_P);
#endif

#endif
        if( videodev->overlay_ref.get() == NULL ) {
            GST_PLAYER_ERROR("isurface->createOverlay() failed.");
            return -1;
        }
        videodev->overlay = new Overlay(videodev->overlay_ref);
    }
    return 0;
}

int videoflinger_device_setVideoSurface(VideoFlingerDeviceHandle handle, void* isurface){
    VideoFlingerDevice *videodev = (VideoFlingerDevice*)handle;
    AutoMutex _l(videodev->surface_lock);
    return videoflinger_device_changeVideoSurface(videodev, isurface);
}

