/* GStreamer
 * Copyright (C) <2009> Prajnashi S <prajnashi@gmail.com>
 *                      Steve Guo <letsgoustc@gmail.com>
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

#ifndef _GST_METADATA_RETRIEVER_PIPELINE_H_
#define _GST_METADATA_RETRIEVER_PIPELINE_H_
#include <utils/Errors.h>
#include <media/mediametadataretriever.h>
#include <private/media/VideoFrame.h>
#include "GstPlayer.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gstappsrc.h>

using namespace android;


// The class to handle gst pipeline
class GstMetadataRetrieverPipeline
{
public:
    GstMetadataRetrieverPipeline();
    ~GstMetadataRetrieverPipeline();

    bool setDataSource(const char *url);
    bool setDataSource(int fd, int64_t offset, int64_t length);
    status_t setMode(int mode);
    status_t getMode(int* mode) const;
    VideoFrame* captureFrame();
    MediaAlbumArt* extractAlbumArt();
    const char* extractMetadata(int keyCode);
   
    // looping play 
    gint     mLoopQuit;
    GThread* mLoopingThread;

private:
    // static apis
    static gboolean bus_callback (GstBus *bus, GstMessage *msg, gpointer data);
    static void playbin2_found_source(GObject * object, GObject * orig, 
            GParamSpec * pspec, gpointer player_pipeline);
    static void appsrc_need_data(GstAppSrc *src, guint length, 
            gpointer user_data);
    static void appsrc_enough_data(GstAppSrc *src, gpointer user_data);
    static gboolean appsrc_seek_data(GstAppSrc *src, guint64 offset,
            gpointer user_data);
    static void taglist_foreach(const GstTagList *list, const gchar *tag, 
            gpointer user_data);
    static bool can_get_video_frames(GstElement *player);
    
    // private apis
    GstBuffer* getCurrentVideoFrame();
    
    bool prepare();
    bool prepareAsync();
    bool start();
    bool stop();
    bool seekTo(int msec);
    bool getDuration(int *msec);
    bool getVideoSize(int *w, int *h); 
	void dump();
    bool create_pipeline();
    void delete_pipeline();  

    void handleEos(GstMessage* p_msg);
    void handleError(GstMessage* p_msg);
    void handleTag(GstMessage* p_msg);
    void handleBuffering(GstMessage* p_msg);
    void handleStateChanged(GstMessage* p_msg);
    void handleAsyncDone(GstMessage* p_msg);
    void handleSegmentDone(GstMessage* p_msg);
    void handleDuration(GstMessage* p_msg);
    void handleElement(GstMessage* p_msg);
    void handleApplication(GstMessage* p_msg);

    //The following const should match with MediaMetadataRetriever.java.
    static const int GET_METADATA_ONLY    = 0x01;
    static const int GET_FRAME_ONLY       = 0x02;

    static const int METADATA_KEY_CD_TRACK_NUMBER = 0;
    static const int METADATA_KEY_ALBUM           = 1;
    static const int METADATA_KEY_ARTIST          = 2;
    static const int METADATA_KEY_AUTHOR          = 3;
    static const int METADATA_KEY_COMPOSER        = 4;
    static const int METADATA_KEY_DATE            = 5;
    static const int METADATA_KEY_GENRE           = 6;
    static const int METADATA_KEY_TITLE           = 7;
    static const int METADATA_KEY_YEAR            = 8;
    static const int METADATA_KEY_DURATION        = 9;
    static const int METADATA_KEY_NUM_TRACKS      = 10;
    static const int METADATA_KEY_IS_DRM_CRIPPLED = 11;
    static const int METADATA_KEY_CODEC           = 12;
    static const int METADATA_KEY_RATING          = 13;
    static const int METADATA_KEY_COMMENT         = 14;
    static const int METADATA_KEY_COPYRIGHT       = 15;
    static const int METADATA_KEY_BIT_RATE        = 16;
    static const int METADATA_KEY_FRAME_RATE      = 17;
    static const int METADATA_KEY_VIDEO_FORMAT    = 18;
    static const int METADATA_KEY_VIDEO_HEIGHT    = 19;
    static const int METADATA_KEY_VIDEO_WIDTH     = 20;

    static const int NUM_METADATA_KEYS            = 21;
    static const int MAX_METADATA_STR_LENGTH      = 512;

    int mMode;
    VideoFrame *mVideoFrame;
    //bool mStarted;
    int mPipelineRunResult;		//0: hasn't try to run pipeline; 1: tried to prepare pipeline, succeed; 2: tried to start pipeline, succeed; -1: tried to prepare pipeline, fail; -2: tried to start pipeline, fail
    char mMetadataValues[NUM_METADATA_KEYS][MAX_METADATA_STR_LENGTH];

    GstPlayer*  mSavedGstPlayer;
    GstPlayer*  mGstPlayer;
	bool mAsynchPreparePending;
	bool mIsLooping;
	bool mIsPlaying;
	bool mIsLiveStreaming;

    // gst elements
    GstElement* mPlayBin;
    GstElement* mAudioSink;
    GstElement* mVideoSink;
    GstAppSrc* mAppSource;
    // app source 
    int mFd;
    guint64  mLength;
    guint64  mOffset;
    guint64  mBaseOffset; //Used for media file embeded in APK.
    guint8*  mMemBase;
    // seek
    bool     mSeeking;
    GstState mSeekState;
    // main loop
    GMainLoop*  mMainLoop;
    GThread*      mBusMessageThread;
    pthread_mutex_t  mActionMutex;

    int mAACDuration;	//unit is millisecond. -1 means couldn't get duration because we think it's not aac adts stream.
    int mWMADuration;	//unit is millisecond. -1 means couldn't get duration because we think it's not wma stream, or meet some error when parse stream
}; 

#endif   /*_GST_METADATA_RETRIEVER_PIPELINE_H_*/
