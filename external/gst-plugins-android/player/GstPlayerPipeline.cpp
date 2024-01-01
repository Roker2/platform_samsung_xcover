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
#include "GstLog.h"
#include <utils/Log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "GstPlayerPipeline.h"
#include "cutils/properties.h"
#include "gstdebugutils.h"
#include "gstbaseaudiosink.h"

#define GST_DECBIN2_BUF_MAX_SIZE_BYTES	(8*1024*1024)
extern "C" {
static void element_added_into_uridecbin_notify(GstBin * bin, GstElement * element, gpointer user_data)
{
	if(G_OBJECT_TYPE(element) == (GType)user_data)	//the element is decodebin2
	{
		g_object_set(element, "max-size-bytes", GST_DECBIN2_BUF_MAX_SIZE_BYTES, NULL);
	}
	return;
}

static void element_added_into_playbin2_notify(GstBin * bin, GstElement * element, gpointer user_data)
{
	if(G_OBJECT_TYPE(element) == (GType)user_data)	//the element is uridecodebin
	{
		GstElement * tmp = gst_element_factory_make("decodebin2", NULL);
		if(tmp)
		{
			GType decbin2_type = G_OBJECT_TYPE(tmp);
			gst_object_unref(tmp);
			g_signal_connect(element, "element-added", G_CALLBACK (element_added_into_uridecbin_notify), (gpointer)decbin2_type);	
		}
	}
	return;
}

}

using namespace android;

// always enable gst log
#define ENABLE_GST_LOG

#define MSG_QUIT_MAINLOOP   "application/x-quit-mainloop"
#define MSG_PAUSE           "application/x-pause"

#define INIT_LOCK(pMutex)   pthread_mutex_init(pMutex, NULL)
#define LOCK(pMutex)        pthread_mutex_lock(pMutex)
#define UNLOCK(pMutex)      pthread_mutex_unlock(pMutex)
#define DELETE_LOCK(pMutex) pthread_mutex_destroy(pMutex)

#ifndef PAGESIZE
#define PAGESIZE            4096
#endif

//We only try to mmap file which is smaller than 512MB
#define MAX_MMAP_SIZE ( 512 * 1024 * 1024 )

// make sure gst can be initialized only once
static gboolean gst_inited = FALSE;

// ----------------------------------------------------------------------------
// helper functions to redirect gstreamer log to android's log system
// ----------------------------------------------------------------------------
#define GST_CONFIG_ENVIRONMENT_GROUP  "Environment"
#define NUL '\0'

// manage loop play when receive EOS.
static void looping_thread_func(GstPlayerPipeline * GstPlayerPipeline)
{
    while (true)
    {
        if (g_atomic_int_compare_and_exchange(&GstPlayerPipeline->mLoopQuit, 1, 0))
            break;

        sem_wait(&GstPlayerPipeline->mSem_eos);

        if (g_atomic_int_compare_and_exchange(&GstPlayerPipeline->mLoopQuit, 1, 0))
            break;

        GstPlayerPipeline->seekTo(0);
    }
        g_thread_exit(NULL);
}

// get_gst_env_from_conf()
// Read and export environment variables from /sdcard/gst.conf.
//
static int get_gst_env_from_conf()
{
    gboolean res = FALSE;
    GKeyFile* conf_file = NULL;
    gchar** key_list = NULL;
    gsize length = 0;
    GError* error = NULL;
 
    // open and load GST_CONFIG_FILE
    conf_file = g_key_file_new ();
    if(conf_file == NULL)
        return -1;

    GST_PLAYER_LOG("Load config file: "GST_CONFIG_FILE"\n");
    res = g_key_file_load_from_file(
        conf_file,
        GST_CONFIG_FILE,
        G_KEY_FILE_NONE,
        &error);
    if(res != TRUE)
    {
        if(error)
            GST_PLAYER_ERROR ("Load config file error: %d (%s)\n", 
                error->code, error->message);
        else
            GST_PLAYER_ERROR ("Load config file error.\n");
        goto EXIT;
    }

    // enumerate all environment viarables
    error = NULL;
    key_list = g_key_file_get_keys(
        conf_file,
        GST_CONFIG_ENVIRONMENT_GROUP,
        &length,
        &error);
    if(key_list == NULL)
    {
        if(error)
            GST_PLAYER_ERROR ("Fail to get environment vars: %d (%s)\n",
                error->code, error->message);
        else
            GST_PLAYER_ERROR ("Fail to get environment vars\n");
        res = FALSE;
        goto EXIT;
    }

    // export all environment viarables
    for(gsize i=0; i<length; i++)
    {
        if(key_list[i])
        {
            gchar* key = key_list[i];
            gchar* value = g_key_file_get_string(
                conf_file,
                GST_CONFIG_ENVIRONMENT_GROUP,
                key,
                NULL);
            if(value)
            {
                setenv(key, value, 1);
                GST_PLAYER_DEBUG("setenv:  %s=%s\n", key, value);
            }
            else
            {
                unsetenv(key);
                GST_PLAYER_DEBUG("unsetsev:  %s\n", key);
            }
        }
    }

EXIT:
    // release resource
    if(conf_file)
        g_key_file_free(conf_file);
    if(key_list)
        g_strfreev(key_list);

    return (res == TRUE) ? 0 : -1;
}

// android_gst_debug_log()
// Hook function to redirect gst log from stdout to android log system
//
static void android_gst_debug_log (GstDebugCategory * category, GstDebugLevel level,
    const gchar * file, const gchar * function, gint line,
    GObject * object, GstDebugMessage * message, gpointer unused)
{
    if (level > gst_debug_category_get_threshold (category))
    return;

    const gchar *sfile = NULL;
    // Remove the full path before file name. All android code build from top
    // folder, like .../external/gst-plugin-android/player/GstPlayer.cpp, which
    // make log too long. 
    sfile  = GST_PLAYER_GET_SHORT_FILENAME(file);

    // redirect gst log to android log
    switch (level)
    {
    case GST_LEVEL_ERROR:
        GST_ERROR_ANDROID ("[%d], %s   %s:%d:%s: %s\n", 
            gettid(),
            gst_debug_level_get_name (level), 
            sfile, line, function, 
            (char *) gst_debug_message_get (message));  
        break;
    case GST_LEVEL_WARNING:
        GST_WARNING_ANDROID ("[%d], %s   %s:%d:%s: %s\n", 
            gettid(),
            gst_debug_level_get_name (level), 
            sfile, line, function, 
            (char *) gst_debug_message_get (message));  
        break;
    case GST_LEVEL_INFO:
        GST_INFO_ANDROID ("[%d], %s   %s:%d:%s: %s\n", 
            gettid(),
            gst_debug_level_get_name (level), 
            sfile, line, function, 
            (char *) gst_debug_message_get (message));  
        break;
    case GST_LEVEL_DEBUG:
        GST_DEBUG_ANDROID ("[%d], %s   %s:%d:%s: %s\n",
            gettid(),
            gst_debug_level_get_name (level), 
            sfile, line, function, 
            (char *) gst_debug_message_get (message));  
        break;
    case GST_LEVEL_LOG:
        GST_LOG_ANDROID ("[%d], %s   %s:%d:%s: %s\n", 
            gettid(),
            gst_debug_level_get_name (level), 
            sfile, line, function, 
            (char *) gst_debug_message_get (message));  
        break;
    default:
        break;
    }
}

static gboolean init_gst()
{
    // print gstreamer environment
    GST_PLAYER_LOG ("Check gstreamer environment variables.\n");
    GST_PLAYER_LOG ("GST_PLUGIN_PATH=\"%s\"\n", getenv("GST_PLUGIN_PATH"));
    GST_PLAYER_LOG ("LD_LIBRARY_PATH=\"%s\"\n", getenv("LD_LIBRARY_PATH"));
    GST_PLAYER_LOG ("GST_REGISTRY=\"%s\"\n", getenv("GST_REGISTRY"));

    if (!gst_inited)
    {
        char * argv[2];
        char**argv2;
        int argc = 0;

        GST_PLAYER_LOG ("Initialize gst context\n");

        if (!g_thread_supported ())
            g_thread_init (NULL);

        // read & export gst environment from gst.conf
        get_gst_env_from_conf();

        // replace gst default log handler with android
        gst_debug_add_log_function (android_gst_debug_log, NULL);

        // initialize gst
        GST_PLAYER_LOG ("Initializing gst\n");
        argv2 = argv;
        argv[0] = "GstPlayer";
        argv[1] = NULL;
        argc++;

        gst_init(&argc, &argv2);
        gst_debug_remove_log_function(gst_debug_log_default );

        GST_PLAYER_LOG ("Gst is Initialized.\n");
        gst_inited = TRUE;    
    }
    else
    {
        GST_PLAYER_LOG ("Gst has been initialized\n");
    }
    return TRUE;
}

// ----------------------------------------------------------------------------
// GstPlayerPipeline
// ----------------------------------------------------------------------------
// Message Handler Function
gboolean GstPlayerPipeline::bus_callback (GstBus *bus, GstMessage *msg, 
        gpointer data)
{
    GstPlayerPipeline* pGstPlayerPipeline   = (GstPlayerPipeline*) data;
    GstObject *msgsrc=GST_MESSAGE_SRC(msg);
    GST_PLAYER_LOG("Get %s Message From %s\n", 
        GST_MESSAGE_TYPE_NAME(msg), GST_OBJECT_NAME(msgsrc));
    gboolean ret = TRUE;
    LOCK (&pGstPlayerPipeline->mActionMutex);

    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS:
        pGstPlayerPipeline->handleEos(msg);           
        break;
    case GST_MESSAGE_ERROR:
        pGstPlayerPipeline->handleError(msg);
        break;
    case GST_MESSAGE_TAG:
        pGstPlayerPipeline->handleTag(msg);
        break;
    case GST_MESSAGE_BUFFERING:
        pGstPlayerPipeline->handleBuffering(msg);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        pGstPlayerPipeline->handleStateChanged(msg);
        break;
    case GST_MESSAGE_ASYNC_DONE:
        pGstPlayerPipeline->handleAsyncDone(msg);
        break;
    case GST_MESSAGE_ELEMENT:
        pGstPlayerPipeline->handleElement(msg);
        break;
    case GST_MESSAGE_APPLICATION:
        pGstPlayerPipeline->handleApplication(msg);
        ret = FALSE;	//return FALSE let the watch(source) removed from context of main_loop
        break;       
    case GST_MESSAGE_SEGMENT_DONE:
        pGstPlayerPipeline->handleSegmentDone(msg);
        break;
    case GST_MESSAGE_DURATION:
        pGstPlayerPipeline->handleDuration(msg);
        break;
    default:
        break;
    }
    UNLOCK (&pGstPlayerPipeline->mActionMutex);
    return ret;
}

void GstPlayerPipeline::appsrc_need_data(GstAppSrc *src, guint length,
        gpointer user_data)
{
    GstPlayerPipeline* player_pipeline = 
        (GstPlayerPipeline*)user_data;
    GstBuffer *buffer = NULL;
    GstFlowReturn flow_ret;
    off_t offset = 0;

    // GST_PLAYER_DEBUG ("player_pipeline=%p, Request length=%d, offset=%lu,
    // fd=%d", player_pipeline, length, (long unsigned
    // int)(player_pipeline->mOffset), player_pipeline->mFd);

    // check current offset is inside range
    if (player_pipeline->mOffset >= player_pipeline->mLength)
    {
        GST_PLAYER_WARNING("Offset %lu is outside file %lu. Send EOS\n", 
                (unsigned long)(player_pipeline->mOffset), 
                (unsigned long)(player_pipeline->mLength));
        goto EXIT;
    }
    
    // read the amount of data, we are allowed to return less if we are EOS
    if (player_pipeline->mOffset + length > player_pipeline->mLength)
        length = player_pipeline->mLength - player_pipeline->mOffset;

    // create GstBuffer
    buffer = gst_buffer_new ();
    if(buffer == NULL)
    {
        GST_PLAYER_ERROR("Cannot create buffer! Send EOS\n");
        goto EXIT;
    }

    if (player_pipeline->mMemBase) {
        GST_BUFFER_DATA (buffer) = player_pipeline->mMemBase
                + player_pipeline->mBaseOffset + player_pipeline->mOffset;
        GST_BUFFER_SIZE (buffer) = length;
        GST_BUFFER_OFFSET (buffer) = player_pipeline->mOffset;
        GST_BUFFER_OFFSET_END (buffer) =player_pipeline->mOffset + length;
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_READONLY);
    } else {
        //For large files which cannot be mapped into memory
        GST_BUFFER_MALLOCDATA (buffer) = (guint8*) g_malloc(length);

        if ( NULL == GST_BUFFER_MALLOCDATA (buffer) ) {
            GST_PLAYER_ERROR("Can't malloc memory for buffer! Send EOS\n");
            goto EXIT;
        }

        GST_BUFFER_DATA (buffer) = GST_BUFFER_MALLOCDATA (buffer);
        GST_BUFFER_SIZE (buffer) = length;

        if ( lseek64(player_pipeline->mFd,
                     player_pipeline->mOffset + player_pipeline->mBaseOffset,
                     SEEK_SET) == -1) {
            GST_PLAYER_ERROR("Can't seek to required postion %d(%lld): %s. Send EOS\n",
                             player_pipeline->mFd, player_pipeline->mOffset,
                             strerror(errno));
            goto EXIT;
        }

        if ( read(player_pipeline->mFd, GST_BUFFER_DATA (buffer), length) == -1) {
            GST_PLAYER_ERROR("Can't read from file %d: %s Send EOS\n",
                             player_pipeline->mFd, strerror(errno));
            goto EXIT;
        }
    }

    /*
    GST_PLAYER_DEBUG("offset=%lu, length=%u, data: %02x %02x %02x %02x %02x %02x %02x %02x",
        (unsigned long)player_pipeline->mOffset,
        length,
        *(GST_BUFFER_DATA (buffer) + 0),
        *(GST_BUFFER_DATA (buffer) + 1),
        *(GST_BUFFER_DATA (buffer) + 2),
        *(GST_BUFFER_DATA (buffer) + 3),
        *(GST_BUFFER_DATA (buffer) + 4),
        *(GST_BUFFER_DATA (buffer) + 5),
        *(GST_BUFFER_DATA (buffer) + 6),
        *(GST_BUFFER_DATA (buffer) + 7));
    */

    // send buffer to appsrc
    flow_ret = gst_app_src_push_buffer(player_pipeline->mAppSource, buffer);
    if(GST_FLOW_IS_FATAL(flow_ret)) 
        GST_PLAYER_ERROR("Push error %d\n", flow_ret);

    player_pipeline->mOffset += length;
    // gst_app_src_push_buffer() will steal the GstBuffer's reference, we need
    // not release it here.  
    // if (buffer) gst_buffer_unref (buffer);
    return;

EXIT:
    //We release the buffer because appsrc haven't stolen it
    if (buffer) gst_buffer_unref (buffer);
    gst_app_src_end_of_stream (src);
}

void GstPlayerPipeline::appsrc_enough_data(GstAppSrc *src, 
        gpointer user_data)
{
    GST_PLAYER_WARNING ("Shall not enter here\n");
}

gboolean GstPlayerPipeline::appsrc_seek_data(GstAppSrc *src, 
        guint64 offset, gpointer user_data)
{
    GstPlayerPipeline* player_pipeline = 
        (GstPlayerPipeline*)user_data;

    // GST_PLAYER_DEBUG ("Enter, offset=%lu\n", (long unsigned int)offset);
    player_pipeline->mOffset = offset;
    return TRUE;
}

// this callback is called when playbin2 has constructed a source object to
// read from. Since we provided the appsrc:// uri to playbin2, this will be the
// appsrc that we must handle. We set up a signals to push data into appsrc. 
void GstPlayerPipeline::playbin2_found_source(GObject * object, GObject * orig, 
    GParamSpec * pspec, gpointer user_data)
{
    GstPlayerPipeline* player_pipeline = 
        (GstPlayerPipeline*)user_data;



    // get a handle to the appsrc
    if (player_pipeline->mAppSource)
    {
        g_object_unref(player_pipeline->mAppSource);
        player_pipeline->mAppSource = NULL;
    }
    g_object_get (orig, pspec->name, &(player_pipeline->mAppSource), NULL);
    GST_PLAYER_LOG ("appsrc: %p", player_pipeline->mAppSource);

    // we can set the length in appsrc. This allows some elements to estimate
    // the total duration of the stream. It's a good idea to set the property
    // when you can but it's not required.  
    gst_app_src_set_size(player_pipeline->mAppSource, player_pipeline->mLength);

    // configure the appsrc to work in pull (random access) mode 
    gst_app_src_set_stream_type(player_pipeline->mAppSource, 
            GST_APP_STREAM_TYPE_RANDOM_ACCESS);
    

    // set appsrc callback 
    GstAppSrcCallbacks callback;
    callback.need_data = appsrc_need_data;
    callback.enough_data = appsrc_enough_data;
    callback.seek_data = appsrc_seek_data;
    gst_app_src_set_callbacks(player_pipeline->mAppSource, &callback, 
            player_pipeline, NULL);
}

void GstPlayerPipeline::taglist_foreach(const GstTagList *list, 
            const gchar *tag, gpointer user_data)
{
    // following code is copy from gst-plugins-bad/examples/gstplay/player.c
    gint i, count;

    count = gst_tag_list_get_tag_size (list, tag);

    for (i = 0; i < count; i++) {
        gchar *str;

        if (gst_tag_get_type (tag) == G_TYPE_STRING) 
        {
            if (!gst_tag_list_get_string_index (list, tag, i, &str))
            {
                GST_PLAYER_ERROR("Cannot get tag %s\n", tag);
            } 
        }
        else 
        {
            str = g_strdup_value_contents (
                    gst_tag_list_get_value_index (list, tag, i));
        }

        if (i == 0) 
        {
            GST_PLAYER_DEBUG("%15s: %s\n", gst_tag_get_nick (tag), str);
        } 
        else 
        {
            GST_PLAYER_DEBUG("               : %s\n", str);
        }
        g_free (str);
    }
}

#define IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT		GST_CLOCK_TIME_NONE		//by default, we set time out to infinite for state change
//#define IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT		(9*GST_SECOND)			//sometimes, for some special stream like corrupted stream, infinite timeout probably cause gst_element_get_state() couldn't return, then set a limitted timeout

GstPlayerPipeline::GstPlayerPipeline(GstPlayer* gstPlayer, int sinktype)
{
    GST_PLAYER_LOG ("Enter\n");
    INIT_LOCK(&mActionMutex);

    // initilize members
    LOCK(&mActionMutex);
    mSavedGstPlayer = gstPlayer;

    // GstElement
    mPlayBin = NULL;
    mAudioSink = NULL;
    mVideoSink = NULL;
    mAppSource = NULL;

    // app source
    mFd = 0;
    mLength = 0;
    mOffset = 0;
    mBaseOffset = 0;
    mMemBase = NULL;
    
    // mainloop
    mMainLoop = NULL;
    mBusMessageThread = NULL;

    // prepare
    mAsynchPreparePending = false;    

    // seek
    mSeeking = false;
    mSeekState = GST_STATE_VOID_PENDING;

    // others
    mIsLooping = false;
    mIsPlaying = false;
    mIsLiveStreaming = false;
    mLoopQuit = 0;
    mLoopingThread = NULL;
    sem_init(&mSem_eos, 0, 0);
 
    mAACDuration = -1;
    mAACLength = -1;
    mLastShowPos = -1;
    mPosBeforePause = -1;

    // initialize gst framework
    init_gst();

    // create pipeline 
    create_pipeline(sinktype);

    UNLOCK(&mActionMutex); 

    GST_PLAYER_LOG ("Leave\n");
    return;
}

GstPlayerPipeline::~GstPlayerPipeline()
{
    GST_PLAYER_LOG ("Enter\n");

    LOCK (&mActionMutex);

    delete_pipeline ();

    mGstPlayer = NULL;
    UNLOCK (&mActionMutex);
    DELETE_LOCK(&mActionMutex);
    GST_PLAYER_LOG ("Leave\n");
}


// TODO: delete this API
// Player will delete playbin2 everytime, try no do that in the future
bool GstPlayerPipeline::create_pipeline (int sinktype)
{
    GstBus *bus = NULL;
    GMainContext* ctx = NULL;
    GSource* source = NULL;
	GstElement * tmp_uridecbin = NULL;

    // create playbin2
    mPlayBin = gst_element_factory_make ("playbin2", NULL);
    if(mPlayBin == NULL)
    {
        GST_PLAYER_ERROR("Failed to create playbin2\n");
        goto ERROR;
    }

	tmp_uridecbin = gst_element_factory_make("uridecodebin", NULL);
	if(tmp_uridecbin)
	{
		GType uridecbin_type = G_OBJECT_TYPE(tmp_uridecbin);
		gst_object_unref(tmp_uridecbin);
		g_signal_connect(mPlayBin, "element-added", G_CALLBACK (element_added_into_playbin2_notify), (gpointer)uridecbin_type);
	}

    // add watch message
    ctx = g_main_context_new();
    if(ctx == NULL)
    {
        GST_PLAYER_ERROR ("Failed to create context for MainLoop.\n");
        goto ERROR;
    }
    mMainLoop = g_main_loop_new (ctx, FALSE);
    if (mMainLoop == NULL) 
    {
        GST_PLAYER_ERROR ("Failed to create MainLoop.\n");
        goto ERROR;
    }
    g_main_context_unref(ctx);	//since g_main_loop_new(ctx,) has ref ctx, unref(ctx) here is allowable. Otherwise, need member var to store ctx and unref(ctx) in destructor function
    bus = gst_pipeline_get_bus (GST_PIPELINE (mPlayBin));
    g_assert (bus);
    source = gst_bus_create_watch(bus);
    if(source == NULL)
    {
        GST_PLAYER_ERROR ("Failed to create source for bus.\n");
        goto ERROR;
    }
    g_source_set_callback(source, (GSourceFunc)bus_callback, this, NULL);
    g_source_attach(source, ctx);
    g_source_unref(source);
    gst_object_unref (bus);
        
    // start a thread to run main loop
    mBusMessageThread = g_thread_create ((GThreadFunc)g_main_loop_run, mMainLoop, TRUE, NULL);
    if (mBusMessageThread == NULL)
    {
        GST_PLAYER_ERROR ("Failed to create Bus Message Thread.\n");
        goto ERROR;
    }

    // FIXME: after using fakesink, there is no unref() warning message, so
    // gstaudioflinger sink shall has some bugs
    //
    //
    // create audio & video sink elements for playbin2
    //mAudioSink = gst_element_factory_make("fakesink", NULL);
    if (sinktype == AUDIO_CACHE_SINK) {
        GST_PLAYER_DEBUG ("create audiocache sink\n");
        mAudioSink = gst_element_factory_make("audiocachesink", NULL);
    } else {
        GST_PLAYER_DEBUG ("create audioflinger sink\n");
        mAudioSink = gst_element_factory_make("audioflingersink", NULL);
    }
    if (mAudioSink == NULL)
    {
        GST_PLAYER_ERROR ("Failed to create audiosink\n");
        goto ERROR;
    }
    // FIXME: after setting async false, it means no prerolled data for audio, so it needs
    // some time when getting paused to playing. But if this is not set as false, there exists
    // some chance the gst_element_get_state will never return because the audioflingersink never
    // get to PAUSED state (The happen probability is about 4/10). The same issue exists if I
    // replacing audioflingersink with fakesink. So I wonder it's a bug in GStreamer core. 
    //
    // According to GStreamer design doc, it will backwards all elements (from sink to source) to
    // change state, basesink will check whether there still exists some buffer as prerolled data, 
    // if no data, it will do async change state. But if the decode is PAUSED, who will generate the
    // data?
    //
    // Why video sink does not show the issue? Who can tell me why? 
    //g_object_set (mAudioSink, "async", FALSE, NULL);
    // Fix playing rtsp/rtp stream, audio will stuck for a little while per some seconds
    //g_object_set (mAudioSink, "slave-method", GST_BASE_AUDIO_SINK_SLAVE_NONE, NULL);
    g_object_set (mPlayBin, "audio-sink", mAudioSink, NULL);

    mVideoSink = gst_element_factory_make("surfaceflingersink", NULL);
    if (mVideoSink == NULL)
    {
        GST_PLAYER_ERROR ("Failed to create surfaceflingersink\n");
        goto ERROR;
    }
    g_object_set (mPlayBin, "video-sink", mVideoSink, NULL);

    GST_PLAYER_LOG ("Pipeline is created successfully\n");

    return true;

ERROR:
    GST_PLAYER_ERROR ("Pipeline is created failed\n");
    // release resource
    return false;
}

void  GstPlayerPipeline::delete_pipeline ()
{
    // release pipeline & main loop
    if (mPlayBin) 
    {
        // send exit message to pipeline bus post an application specific 
        // message unlock here for bus_callback to process this message
        UNLOCK (&mActionMutex);
        if (mLoopingThread)
        {
            if (g_atomic_int_compare_and_exchange(&mLoopQuit, 0, 1))
            {
                mLoopingThread = NULL;
                sem_post(&mSem_eos);
            }
        }
        
        if(mBusMessageThread)
        {
            GST_PLAYER_DEBUG ("Send QUIT_MAINLOOP message to bus\n");
            gboolean ret = gst_element_post_message (
                GST_ELEMENT (mPlayBin),
                gst_message_new_application (
                    GST_OBJECT (mPlayBin),
                    gst_structure_new (MSG_QUIT_MAINLOOP,
                    "message", G_TYPE_STRING, "Deleting Pipeline", NULL)));

            if(ret)
            {
                /* Wait for main loop to quit*/
                GST_PLAYER_DEBUG ("Wait for main loop to quit ...\n");
                g_thread_join (mBusMessageThread);
                GST_PLAYER_DEBUG("BusMessageThread joins successfully\n");
            }
            else
            {
                 //FIXME: sometimes gst_element_post_message will return FALSE, how can we do
                 //under such a case? Currently I just assume in such a case, it means the pipeline
                 //has no bus, so just exit like no bus threads. If we don't handle the case,
                 //g_thread_join will never return back.
                 GST_PLAYER_ERROR ("...          gst_element_post_message return false      ...");
            }
            mBusMessageThread = NULL;
        } 
        else 
        {
            GST_PLAYER_DEBUG("BusMessageThread is NULL, no need to quit\n");
        }
        LOCK (&mActionMutex);

        GST_PLAYER_DEBUG ("One pipeline exist, now delete it .\n");
        gst_element_set_state (mPlayBin, GST_STATE_NULL);
    }

    // release audio & video sink
    if (mAudioSink)
    {
        GST_PLAYER_LOG ("Release audio sink\n");
        gst_object_unref (mAudioSink);
        mAudioSink = NULL;
    }
    if (mVideoSink)
    {
        GST_PLAYER_LOG ("Release video sink\n");
        gst_object_unref (mVideoSink);
        mVideoSink = NULL;
    }
    if (mAppSource)
    {
        GST_PLAYER_LOG ("Release app source\n");
        gst_object_unref (mAppSource);
        mAppSource = NULL;
    }
    if(mPlayBin)
    {   
        GST_PLAYER_LOG ("Delete playbin2\n");
        gst_object_unref (mPlayBin);
        mPlayBin = NULL;
    }
    if (mMainLoop)
    {
        GST_PLAYER_LOG ("Delete mainloop\n");
        g_main_loop_unref (mMainLoop);
        mMainLoop = NULL;
    }
    if (mMemBase)
    {
        GST_PLAYER_LOG ("Unmap fd\n");
        munmap((void*)mMemBase, (size_t)(mLength + mBaseOffset));
    }
    if (mFd > 0) {
        GST_PLAYER_LOG ("Close fd which is using for large files\n");

        if ( close(mFd) != 0) {
            GST_PLAYER_ERROR ("close fd %d failed: %s\n",
                              mFd, strerror(errno));
        }
    }

    // app source
    mFd = 0;
    mLength = 0;
    mOffset = 0;
    mBaseOffset = 0;
    mMemBase = NULL;
    // seek
    mSeeking = false;
    mSeekState = GST_STATE_VOID_PENDING;
    // prepare
    mAsynchPreparePending = false;
    

    GST_PLAYER_LOG ("Leave\n");
}

//pointer to the start of syncword
static unsigned char* seekAACSyncword(unsigned char* data, int len)
{
    unsigned int syncword;
    if(len < 2) {
        return NULL;
    }
    syncword = *data++;
    len--;
    while(len > 0) {
        syncword = (syncword<<8) | (*data++);
        len--;
        if((syncword&0xfff0) == 0xfff0) {
            return (data - 2);
        }
    }
    return NULL;
}

static bool adtsFrame(unsigned char **ppData, int *pLen, unsigned int *pBlockNum)
{
    unsigned char *pSyncword = NULL;
    unsigned char *pData = *ppData;
    int len = *pLen;

    unsigned int aac_frame_length = 0;
    unsigned int number_of_raw_data_blocks_in_frame = 0;

    pSyncword = seekAACSyncword(pData, len);
    if(pSyncword == NULL) {
        *pBlockNum = 0;
        return false;
    }

    // needs extra 8 bytes to get aac_frame_length and number_of_raw_data_blocks_in_frame
    // adts_fixed_header has 28 bits, adts_variable_header has 28 bits
    if((len-(pSyncword-pData))<7){
        *pBlockNum = 0;
        return false;
    }

    // adts_variable_header
    aac_frame_length = (((unsigned int)pSyncword[3]&0x3)<<11) | ((unsigned int)pSyncword[4]<<3) | ((unsigned int)pSyncword[5]>>5);

    if((len - (int)(pSyncword-pData))<(int)aac_frame_length){
        *pLen = 0;
        *ppData = pSyncword;
        *pBlockNum = 0;
        return false;
    }else{
        len -= (int)((pSyncword-pData) + aac_frame_length);
        pData = pSyncword + aac_frame_length;
        *ppData = pData;
        *pLen = len;
     }

    number_of_raw_data_blocks_in_frame = (unsigned int)(pSyncword[6]&0x3);
    *pBlockNum = number_of_raw_data_blocks_in_frame +1;
    return true;
}

static bool countAACBlock(unsigned char* pMediaData, int MediaLength, unsigned int * pBlockNum, int *pValidLenForBlks)
{
    //GST_PLAYER_DEBUG("analyze adts frame, fileData[0x%x], fileLength[%d]\n", fileData, fileLength);
    // Find Syncword and analyze adts_frame, get block number for each frame
    unsigned int totalBlkNum = 0;
    unsigned char * pStartData = pMediaData;
    bool adtsFrameOK = false;
    unsigned int blkNumPerFrame = 0;
    do{
        adtsFrameOK = adtsFrame(&pMediaData, &MediaLength, &blkNumPerFrame);
        if(true==adtsFrameOK){
            totalBlkNum += blkNumPerFrame;
        }
    }while(adtsFrameOK);
    *pBlockNum = totalBlkNum;
    *pValidLenForBlks = pMediaData-pStartData;
    //GST_PLAYER_DEBUG("complete analyzing adts frame\n");
    return true;
}

static bool isAACAdts(unsigned char * pData, int len, unsigned int * pSampleRate)
{
	unsigned char *pSyncword;
    // if 3 consecutive syncwords are found, then it is aac adts
// the 1st syncword
    if(len<6 || (pSyncword = seekAACSyncword(pData, 2)) == NULL){
        // len is too short or not found 0xfffx at the beginning of data
        return false;
    }
    // get frame length form adts_variable_header
    unsigned int aac_frame_length = (((unsigned int)pData[3]&0x3)<<11) | ((unsigned int)pData[4]<<3) | ((unsigned int)pData[5]>>5);
    if(len<(int)aac_frame_length){
        // the first frame is not integrated
        return false;
    }else{
        unsigned int fs[12] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
        unsigned channel_configuration = ((pData[2] & 0x1) << 2) | ((pData[3] >> 6) & 0x3);
        unsigned char sampling_frequency_index = (pData[2] >> 2 ) & 0xf;
        if (sampling_frequency_index > 0xb || channel_configuration > 6){
            return false;
        }
        *pSampleRate = fs[sampling_frequency_index];

        len -= aac_frame_length;
        pData += aac_frame_length;
    }


// the 2nd syncword
    if(len<6 || (pSyncword = seekAACSyncword(pData, 2)) == NULL){
        // len is too short or not found 0xfffx at the beginning of data
        return false;
    }
    // get frame length form adts_variable_header
    aac_frame_length = (((unsigned int)pData[3]&0x3)<<11) | ((unsigned int)pData[4]<<3) | ((unsigned int)pData[5]>>5);
    if(len<(int)aac_frame_length){
        // the second frame is not integrated
        return false;
    }else{
        len -= aac_frame_length;
        pData += aac_frame_length;
    }

// the 3rd syncword
    if(len<6 || (pSyncword = seekAACSyncword(pData, 2)) == NULL){
        // len is too short or not found 0xfffx at the beginning of data
        return false;
    }
    // get frame length form adts_variable_header
    aac_frame_length = (((unsigned int)pData[3]&0x3)<<11) | ((unsigned int)pData[4]<<3) | ((unsigned int)pData[5]>>5);
    if(len<(int)aac_frame_length){
        // the third frame is not integrated
        return false;
    }

    return true;
}


#define ADTSBLOCK_COUNTING_MAXLEN ((int)(2.5*1024*1024))

// file handler: fd
// file size: length
// file start offset: offset
// memMapAddr: pointer to the mapped memory
static bool computeAACDuration(int fd, int64_t offset, int64_t length, unsigned char * memMapAddr, int *pAACDuration)
{
    unsigned char * pMemMap = memMapAddr;
    // the boundaru length, ADTSBLOCK_COUNTING_MAXLEN is the upper threshold. compute AAC duration according boundaryLen bytes
    int boundaryLen = (int)((length>ADTSBLOCK_COUNTING_MAXLEN) ? ADTSBLOCK_COUNTING_MAXLEN : length);
    bool ret = true, rt=true;
    int validLenForBlks = 0;
	*pAACDuration = -1;

#if 1
    if(NULL==memMapAddr){
        // if not map to memory, here map boundaryLen
        pMemMap = (unsigned char*)mmap(0, boundaryLen + offset, PROT_READ, MAP_PRIVATE, fd, 0);
        if(pMemMap == MAP_FAILED || pMemMap == NULL)
        {
            GST_PLAYER_ERROR ("mmap failed in computeAACDuration\n");
            return false;
        }
    }
    pMemMap += offset;	//let pMemMap point to the real media content begin position.

#else
    // Option: read boundaryLen bytes from file to a g_malloc buffer
    unsigned char *pReadFile = (unsigned char*)g_malloc(boundaryLen);
    if(NULL==pReadFile){
        GST_PLAYER_DEBUG("CAN NOT ALLOCATE MEMORY\n");
        return false;
    }
    lseek (fd, offset, SEEK_SET);
    read (fd, pReadFile, boundaryLen);
    lseek (fd, offset, SEEK_SET);
    pMemMap = pReadFile;
#endif

    // Find Syncword andi analyze adts_frame, get length for each frame
    unsigned int blockNum = 0;
    unsigned int sampleRate = 0;

    bool isAdts = isAACAdts(pMemMap, boundaryLen, &sampleRate);
    if(false==isAdts){
        ret = false;
        goto rtComputeAACDuration;
    }

    GST_PLAYER_LOG("IS AAC ADTS STREAM\n");


    //GST_PLAYER_DEBUG("inLen [%d], pMemMap[0x%x]\n", inLen, pMemMap);
// get aac adts duration
    rt = countAACBlock(pMemMap, boundaryLen, &blockNum, &validLenForBlks);
    if(true==rt){
        //GST_PLAYER_DEBUG("countAACBlock() success");
        if(boundaryLen<length){
            *pAACDuration = (int)(blockNum*length*1024000LL/((int64_t)sampleRate*validLenForBlks));
        }else{
            *pAACDuration = (int)(blockNum*1024000LL/sampleRate);
        }
        //GST_PLAYER_DEBUG("GOT AAC ADTS DURATION\n");
    }else{
        GST_PLAYER_DEBUG("countAACBlock() fail");
        ret = false;
    }
    //GST_PLAYER_DEBUG("inLen [%d], pMemMap[0x%x], seekLen [%d] for frame number [%d]\n", inLen, pMemMap, seekLen, frameNum);


rtComputeAACDuration:
#if 1
    if(NULL==memMapAddr){
        if (pMemMap){
            GST_PLAYER_LOG("Unmap mapped memory\n");
            munmap((void*)(pMemMap-offset), (size_t)boundaryLen+offset);
        }
        pMemMap = NULL;
    }
#else
    // Option: read from file
    if(pReadFile){
        GST_PLAYER_LOG("free g_malloc memory\n");
        g_free(pReadFile);
        pReadFile = NULL;
    }
#endif
    return ret;
}

static bool isAACorADTSExtFile(const char *url)
{
    bool ret = false;

    // filename extension judgement for .aac, .adts
    int lenURL = strlen(url);
    int lenExt1 = 4;//strlen(".aac");
    int lenExt2 = 5;//strlen(".adts");
    int startExt1 = lenURL - lenExt1;
    int startExt2 = lenURL - lenExt2;
    if( startExt1 > 0 ){
        if ((!strncasecmp(url + startExt1, ".aac", lenExt1)) || (!strncasecmp(url + startExt2, ".adts", lenExt2)) ) {
            // is .aac or .adts file
	        ret = true;
        }
    }
    return ret;
}

bool GstPlayerPipeline::setDataSource(const char *url)
{
    GST_PLAYER_DEBUG("mPlayBin %p, setDataSource():url=%s", mPlayBin, url);

    mIsLiveStreaming = false;
    LOCK (&mActionMutex);
    char *location = NULL;
    mAACDuration = -1;
    mAACLength = -1;
    mLastShowPos = -1;
    mPosBeforePause = -1;
    if (mPlayBin == NULL)
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        UNLOCK (&mActionMutex);
        return false;
    }
    
    // set uri to playbin2 
    gchar* full_url = NULL;
    if(url[0] == '/')
    {
        // url is an absolute path, add prefix "file://"
        int len = strlen(url) + 10;
        full_url = (gchar*)malloc(len);
        if(full_url == NULL) {
            UNLOCK (&mActionMutex);
            return false;
        }
        g_strlcpy(full_url, "file://", len);
        g_strlcat(full_url, url, len);
    }
    else if (g_str_has_prefix(url, "file:///"))
    {
        full_url = strdup(url);
        if(full_url == NULL) {
            UNLOCK (&mActionMutex);
            return false;     
        }   
    }
    else if (g_str_has_prefix(url, "rtsp://")
         || g_str_has_prefix(url, "RTSP://")
	 || g_str_has_prefix(url, "http://")
	 || g_str_has_prefix(url, "HTTP://"))
    {
        mIsLiveStreaming = true;
        full_url = strdup(url);
        if(full_url == NULL) {
             UNLOCK (&mActionMutex);
             return false;
        }
    }
    else
    {
        GST_PLAYER_ERROR("Invalide url.");
        UNLOCK (&mActionMutex);
        return false;
    }
    GST_PLAYER_LOG("playbin2 uri: %s", full_url);
    g_object_set (mPlayBin, "uri", full_url, NULL);
    
    if(mIsLiveStreaming == true) {
        goto return_setDataSource;
    }

    if(false == isAACorADTSExtFile(full_url)) {
        goto return_setDataSource;
    }

    // get .aac duration if it's .aac file
    location = g_filename_from_uri (full_url, NULL, NULL);
    if (!location) {
        GST_PLAYER_DEBUG("Invalid URI '%s' to get file path", full_url);
    }else{
        int fd=open (location, O_RDONLY);
        if (fd < 0){
            LOGE("Couldn't open fd for %s", full_url);
            goto return_setDataSource;
        }
        struct stat stat_buf;
        if(fstat(fd, &stat_buf) != 0){
            GST_PLAYER_ERROR ("Cannot get file size\n");
            if(close(fd) != 0) {
                GST_PLAYER_ERROR ("close fd %d failed: %s\n", fd, strerror(errno));
            }
            goto return_setDataSource;
        }
        computeAACDuration(fd, 0, stat_buf.st_size, NULL, &mAACDuration);
        mAACLength = stat_buf.st_size;
        if(close(fd) != 0) {
            GST_PLAYER_ERROR ("close fd %d failed: %s\n", fd, strerror(errno));
        }
    }

return_setDataSource:
    if (location) {
        g_free(location);
        location = NULL;
    }

    if(full_url) {
        free (full_url);
    }

    UNLOCK (&mActionMutex);
    return true;
}

bool GstPlayerPipeline::setDataSource(int fd, int64_t offset, int64_t length)
{
    // URI: appsrc://...  
    //
    // Example:
    // external/gst-plugins-base/tests/examples/app/appsrc-stream2.c
    // gst-plugins-base/tests/examples/app/appsrc-ra.c

    mAACDuration = -1;
    mAACLength = -1;
    mLastShowPos = -1;
    mPosBeforePause = -1;

    if (mPlayBin == NULL)
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        return false;
    }  
    // TODO: reset player here
    if (fd == 0)
    {
        GST_PLAYER_ERROR ("Invalid fd: %d\n", fd);
        return false;
    }
    // struct stat stat_buf;
    // if( fstat(fd, &stat_buf) != 0)
    // {
    //     GST_PLAYER_ERROR ("Cannot get file size\n");
    //     return false;
    // }

    mLength = length; 
    mOffset = 0;
    mBaseOffset = offset;

#ifdef DEBUG
    /* The following code is only for debug usage */
    #define MAX_PATH 256
    char pszName1[MAX_PATH];
    char pszName2[MAX_PATH];
    snprintf(pszName1, MAX_PATH-1, "/proc/%d/fd/%d", getpid(), fd);
    int lLen = readlink(pszName1, pszName2, MAX_PATH-1);
    pszName2[lLen] = '\0';
    LOGD("-------------haha, the file name is %s----------------", pszName2);
#endif

    // Only map small file into memory
    if ( (mLength + mBaseOffset) < MAX_MMAP_SIZE ) {
        mMemBase = (guint8*)mmap(0, mLength + mBaseOffset, PROT_READ, MAP_PRIVATE, fd, 0);
    }

    if( NULL == mMemBase ||  MAP_FAILED == mMemBase )
    {
        GST_PLAYER_WARNING ("mmap failed: %s\n", strerror(errno));
        //Try to use fd directly instead of mapping file into memory
        mMemBase = NULL;
        mFd = dup(fd);

        if ( -1 == mFd ) {
            GST_PLAYER_ERROR ("dup fd failed: %s\n", strerror(errno));
            return false;
        }

    } else {
        GST_PLAYER_DEBUG("playbin2 uri: appsrc://, fd: %d, length: %lu, mMemBase: %p",
                         fd, (unsigned long int)mLength, mMemBase);
    }

	// get .aac duration if it's .aac file
    // To judge according to file name
    char pName1[256];
    char pName2[256];
    snprintf(pName1, sizeof(pName1)-1, "/proc/%d/fd/%d", getpid(), fd);
    int nameLen = readlink(pName1, pName2, sizeof(pName2)-1);
    if(nameLen!=-1){
        pName2[nameLen] = '\0';
        GST_PLAYER_DEBUG("mPlayBin %p, setDataSource(fd %d,off %lld,len %lld):url=%s", mPlayBin, fd, offset, length, pName2);
        if(true==isAACorADTSExtFile(pName2)){
            computeAACDuration(fd, offset, length, mMemBase, &mAACDuration);
            mAACLength = length;
        }
    }

    // use appsrc in playbin2
    g_object_set (mPlayBin, "uri", "appsrc://", NULL);

    // get notification when the source is created so that we get a handle to
    // it and can configure it
    g_signal_connect (mPlayBin, "deep-notify::source", (GCallback)
            playbin2_found_source, this);

    mIsLiveStreaming = false;
    return true;
}

bool GstPlayerPipeline::setAudioSink(sp<MediaPlayerInterface::AudioSink> audiosink)
{
    LOCK (&mActionMutex);    
    if(audiosink == 0)
    {
        GST_PLAYER_ERROR("Error audio sink %p", audiosink.get());
        UNLOCK (&mActionMutex);
        return false;
    }

    if (!mPlayBin) 
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        UNLOCK (&mActionMutex);
        return false;
    }

    // set AudioSink
    GST_PLAYER_LOG("MediaPlayerInterface::AudioSink: %p\n", audiosink.get());
    g_object_set (mAudioSink, "audiosink", audiosink.get(), NULL);

    UNLOCK (&mActionMutex);
    return true; 
}

bool GstPlayerPipeline::setVideoSurface(const sp<ISurface>& surface)
{
    GstStateChangeReturn state_return;
    GstState state;
    GST_PLAYER_DEBUG("Call gst_element_get_state(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    GST_PLAYER_DEBUG("gst_element_get_state(%p) ret %d, setVideoSurface %p on Pipeline's state %d", mPlayBin, state_return, surface.get(), state);
	
    LOCK (&mActionMutex);
	
    if (!mPlayBin) 
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        UNLOCK (&mActionMutex);        
        return false;
    }  

    g_object_set (mVideoSink, "surface", surface.get(), NULL);

    if( state == GST_STATE_PLAYING || state == GST_STATE_PAUSED ) {
        // PLAYING, it's change surface operation
        g_object_set(mVideoSink, "setsurface", 1, NULL );
        GST_PLAYER_DEBUG("Video display surface changed to %p", surface.get());
    }

    UNLOCK (&mActionMutex);

    return true;
}

bool GstPlayerPipeline::prepare()
{
    bool ret = false;

    GstStateChangeReturn state_return;
    mLastShowPos = -1;
    mPosBeforePause = -1;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }
    mGstPlayer = mSavedGstPlayer;  
    GST_PLAYER_LOG("Enter\n"); 
    state_return = gst_element_set_state (mPlayBin, GST_STATE_PAUSED);
    GST_PLAYER_LOG("state_return = %d\n", state_return);
    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
        goto EXIT;
    }
    else if (state_return == GST_STATE_CHANGE_ASYNC)
    {
        GstState state;

        // wait for state change complete
        GST_PLAYER_DEBUG("Wait for pipeline's state change to PAUSE(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        GST_PLAYER_DEBUG("Pipeline's state change to PAUSE, %p, %d, ret=%d", mPlayBin, state, state_return);
        if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PAUSED ) 
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
            goto EXIT;
        }
    }

    // send prepare done message
    if (mGstPlayer)
    {
        int width = 0;
        int height = 0;

        // get video size
        if (getVideoSize(&width, &height))
        {
            GST_PLAYER_LOG("Send MEDIA_SET_VIDEO_SIZE, width (%d), height (%d)", width, height);
            mGstPlayer->sendEvent(MEDIA_SET_VIDEO_SIZE, width, height);
        }
        mGstPlayer->sendEvent(MEDIA_PREPARED);

    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.enable.gstpipeline_dump", value, "0");
    if(strcmp(value, "1") == 0)
    {
        dump();
    }

    ret = true;
EXIT:
    UNLOCK (&mActionMutex);
    return ret;
}

bool GstPlayerPipeline::prepareAsync()
{
    bool ret = false;
    GstStateChangeReturn state_return;
    mLastShowPos = -1;
    mPosBeforePause = -1;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }  
    mGstPlayer = mSavedGstPlayer;  
    
    GST_PLAYER_LOG("Enter\n"); 
    state_return = gst_element_set_state (mPlayBin, GST_STATE_PAUSED);
    GST_PLAYER_LOG("state_return = %d\n", state_return);
    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
        goto EXIT;
    }

    // wait for state change complete in message loop
    mAsynchPreparePending = true;
    GST_PLAYER_DEBUG("Wait for pipeline's state change to PAUSE...\n");
    
    ret = true;
EXIT:
    UNLOCK (&mActionMutex);
    return ret;
}


bool GstPlayerPipeline::start()
{
    bool ret = false;
    GstStateChangeReturn state_return;
    //NOTE: here, not reset mLastShowPos, only reset mPosBeforePause
    mPosBeforePause = -1;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }  
    
    GST_PLAYER_LOG("Enter\n"); 
    state_return = gst_element_set_state (mPlayBin, GST_STATE_PLAYING);
    GST_PLAYER_LOG("state_return = %d\n", state_return);

    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to PLAYING\n");
        goto EXIT;
    }
    else if (state_return == GST_STATE_CHANGE_ASYNC)
    {
        GstState state;

        // wait for state change complete
        GST_PLAYER_DEBUG("Wait for pipeline's state change to PLAYING(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        GST_PLAYER_DEBUG("Pipeline's state change to PLAYING, %p, %d, ret=%d", mPlayBin, state, state_return);
        if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PLAYING ) 
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to PLAYING\n");
            goto EXIT;
        }
    }    

    mIsPlaying = true;
    ret = true;
    // send duration(GST_MSECOND) and boudaryLen(Byte) to aacdec
    // for converting bytes to time of newsegment more accuratelly
    // thus, when seek aac adts, position won't have big difference with duration when playback complete
    if(mAACDuration>0){
        GST_PLAYER_LOG("send custom event to playbin2");
        GstEvent *event;
        GstStructure *structure = gst_structure_new ("mrvlipp-average-bps",
            "aac-duration", G_TYPE_UINT64, 1000000*(guint64)(mAACDuration),
            "aac-length", G_TYPE_UINT64, (guint64)(mAACLength),NULL);
        if(structure==NULL){
            GST_PLAYER_LOG ("New structure fail");
            goto EXIT;
        }
        event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, structure);
        if(event==NULL){
            GST_PLAYER_LOG ("Generate GST_EVENT_CUSTOM_UPSTREAM event fail");
            gst_structure_free(structure);
            goto EXIT;
        }

        gboolean res = gst_element_send_event(mPlayBin, event);
        if(false==res){
            GST_PLAYER_LOG ("Sent GST_EVENT_CUSTOM_UPSTREAM event fail");
            goto EXIT;
        }
    }

EXIT:
    UNLOCK (&mActionMutex);
    return ret;
}

bool GstPlayerPipeline::stop()
{
    bool ret = false;
    GstStateChangeReturn state_return;
    mLastShowPos = -1;
    mPosBeforePause = -1;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }  

    GST_PLAYER_LOG("Enter\n"); 
    state_return = gst_element_set_state (mPlayBin, GST_STATE_READY);
    GST_PLAYER_LOG("state_return = %d\n", state_return);

    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to PLAYING\n");
        goto EXIT;
    }
    else if (state_return == GST_STATE_CHANGE_ASYNC)
    {
        GstState state;

        // wait for state change complete
        GST_PLAYER_DEBUG("Wait for pipeline's state change to READY(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        GST_PLAYER_DEBUG("Pipeline's state change to READY, %p, %d, ret=%d", mPlayBin, state, state_return);
        if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_READY ) 
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to READY\n");
            goto EXIT;
        }
    }    
    
    // seek
    if(mSeeking)
    {
         GST_PLAYER_DEBUG("Stop playback while seeking. Send MEDIA_SEEK_COMPLETE immediately");
         if(mGstPlayer)
             mGstPlayer->sendEvent(MEDIA_SEEK_COMPLETE);
    }
    mSeeking = false;
    mSeekState = GST_STATE_VOID_PENDING;
    
    // prepare
    if(mAsynchPreparePending)
    {
        GST_PLAYER_WARNING("mAsynchPreparePending shall not be true!");
    }
    mAsynchPreparePending = false;

    ret = true;

EXIT:
    UNLOCK (&mActionMutex);
    return ret;
}

bool  GstPlayerPipeline::pause()
{
    bool ret = false;
    GstStateChangeReturn state_return;
    GstFormat posfmt = GST_FORMAT_TIME;
    gint64 posbeforepause = 0;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    {
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }  
    GST_PLAYER_LOG("Enter\n"); 

    if(mSeeking)
    {
        GST_PLAYER_DEBUG("Pause in seeking, switch seek pending state to pause\n"); 
        mSeekState = GST_STATE_PAUSED;
    }

    //workaround for progress bar fall back issue with big audio delay
    if(gst_element_query_position(mPlayBin, &posfmt, &posbeforepause) == TRUE && posfmt == GST_FORMAT_TIME && posbeforepause > 0) {
        mPosBeforePause = (int)(posbeforepause / GST_MSECOND);
        GST_PLAYER_DEBUG("Query pos %d millisecond just before pause", mPosBeforePause);
    }

    state_return = gst_element_set_state (mPlayBin, GST_STATE_PAUSED);
    GST_PLAYER_LOG("state_return = %d\n", state_return);
    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
        goto EXIT;
    }
    else if (state_return == GST_STATE_CHANGE_ASYNC)
    {
        GstState state;

        // wait for state change complete
        GST_PLAYER_DEBUG("Wait for pipeline's state change to PAUSE(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        GST_PLAYER_DEBUG("Pipeline's state change to PAUSE, %p, %d, ret=%d", mPlayBin, state, state_return);
        if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PAUSED ) 
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
            goto EXIT;
        }
    }
    ret = true;

EXIT:
    UNLOCK (&mActionMutex);
    return ret;
}

bool GstPlayerPipeline::isPlaying()
{
    GstState state, pending;
    bool ret = false;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }
    GST_PLAYER_LOG("Call gst_element_get_state(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    gst_element_get_state (mPlayBin, &state, &pending, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    GST_PLAYER_LOG("gst_element_get_state(%p) ret state: %d, pending: %d, seeking: %d", mPlayBin, state, pending, mSeeking);

    // we shall return true if, we are playing or we are seeking in playback
    if ((state == GST_STATE_PLAYING && mIsPlaying) || 
            ( mSeeking == true && mSeekState == GST_STATE_PLAYING))
        ret = true;

    GST_PLAYER_LOG("playing: %d", ret); 

EXIT:    
    UNLOCK (&mActionMutex);       
    return ret;
}

bool GstPlayerPipeline::getVideoSize(int *w, int *h)
{
    bool ret = false;
    GstPad* pad = NULL;
    GstStructure* struc = NULL;

    if (!mPlayBin || !mVideoSink) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }
    
    pad = gst_element_get_static_pad(mVideoSink, "sink");
    if (pad == NULL)
    {
        GST_PLAYER_ERROR ("Cannot get sink pad from video sink\n");
        goto EXIT;
    }

    struc = gst_caps_get_structure(GST_PAD_CAPS(pad), 0);
    if (struc == NULL)
    {
        GST_PLAYER_ERROR ("Cannot get structure from video sink\n");
        goto EXIT;
    }        
    GST_PLAYER_LOG("Sink caps: %s\n", gst_caps_to_string(GST_PAD_CAPS(pad)));

    if (gst_structure_get_int(struc, "width", w) != TRUE)
    {
        GST_PLAYER_ERROR ("Cannot get width from caps\n");
        *w = *h = 0;
        goto EXIT;
    }
    if (gst_structure_get_int(struc, "height", h) != TRUE)
    {
        GST_PLAYER_ERROR ("Cannot get width from caps\n");
        *w = *h = 0;
        goto EXIT;
    }
    GST_PLAYER_LOG("width: %d, height: %d\n", *w, *h);
    
    ret = true; 
EXIT:    
    if(pad)
        gst_object_unref(pad);

    return ret;    
}


bool  GstPlayerPipeline::seekTo(int msec)
{
    GstState state, pending;
    bool ret = false;

    gint64 seek_pos = (gint64)msec * GST_MSECOND;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }

    GST_PLAYER_LOG("Enter, seek to: %d\n", msec);

    // get current stable state
    GST_PLAYER_DEBUG("Call gst_element_get_state(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    gst_element_get_state (mPlayBin, &state, &pending, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    GST_PLAYER_DEBUG("gst_element_get_state(%p) ret state: %d, pending: %d", mPlayBin, state, pending);

    if (gst_element_seek_simple(mPlayBin, GST_FORMAT_TIME, 
        (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), seek_pos) != TRUE)
    {
        GST_PLAYER_ERROR ("Fail to seek to position %d\n", msec);
    }
    else
    {
        GST_PLAYER_LOG ("Seek to %d\n", msec);
        mSeeking = true;
        mSeekState = state;
        ret = true;
    }
    mLastShowPos = -1;
    mPosBeforePause = -1;

EXIT:    
    UNLOCK (&mActionMutex);       
    return ret;
}

bool GstPlayerPipeline::getCurrentPosition(int *msec)
{
    gint64  pos;
    bool ret = false;
    GstFormat format = GST_FORMAT_TIME;
    
    GST_PLAYER_LOG("enter"); 
    LOCK (&mActionMutex);

    *msec = 0;
    if (!mPlayBin || msec == NULL) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }
    if (gst_element_query_position(mPlayBin, &format, &pos) != TRUE 
            || format != GST_FORMAT_TIME)
    {
        GST_PLAYER_ERROR ("Fail to get current position\n");
        /* Workaround here, for some media streams, when GStreamer is in PAUSED state, it
           cannot get current position. Why? So here just return fake 0 and log an error. */
        *msec = 0;
        ret = true;
    }
    else
    {
        int pos_msec = (int)(pos / GST_MSECOND);
        //GST_PLAYER_DEBUG("query pipeline and get pos %d ms", pos_msec);
        if(mPosBeforePause >= 0) {
            pos_msec = mPosBeforePause;//in pause state, always return the position before pause unless seekTo() or start() or other action clean mPosBeforePause
            //GST_PLAYER_DEBUG("Reuse pos (%d ms) which is gotten just before pause", mPosBeforePause);
        }
        if(mLastShowPos >= 0) {
            int dif = pos_msec - mLastShowPos;
            if(dif < 0 && dif > -1700) {
                //because GSTplayer don't support reverse playback, therefore, dif should >= 0. If diff < 0, something wrong, we should adjust. But if diff is too small, we don't adjust, just expose this error.
                 pos_msec = mLastShowPos;
                 GST_PLAYER_DEBUG("Force change showing pos to %d ms(diff %d)", pos_msec, dif);
            }
        }
        *msec = pos_msec;
        mLastShowPos = pos_msec;
        GST_PLAYER_LOG ("Current position: %d\n", *msec);
        ret = true;
    }

EXIT:    
    UNLOCK (&mActionMutex);
    return ret;
}

bool GstPlayerPipeline::getDuration(int *msec)
{  
    gint64  dur;
    bool ret = false;
    GstFormat format = GST_FORMAT_TIME;
    
    GST_PLAYER_LOG("enter"); 
    LOCK (&mActionMutex);

    if(mAACDuration > 0){
        *msec = (int)(mAACDuration);
        GST_PLAYER_LOG("Is aac adts, Duration: %d\n", *msec);
		ret = true;
        goto EXIT;
    }

    *msec = 0;
    if (!mPlayBin || msec == NULL) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }
    if (gst_element_query_duration(mPlayBin, &format, &dur) != TRUE 
            || format != GST_FORMAT_TIME)
    {
        /* It's possible that some demuxer cannot get correct duration, like mpeg2 ts. The demux can
           get this information through calcaulation over all frames. So we just log here and set duration as 0. */
        ret = true;
        *msec = 0;
        GST_PLAYER_ERROR ("Fail to get duration\n");
    }
    else
    {
        *msec = (int)(dur / GST_MSECOND);
        GST_PLAYER_LOG ("Duration: %d\n", *msec);
        ret = true;
    }

EXIT:    
    UNLOCK (&mActionMutex);
    return ret;
}

bool GstPlayerPipeline::reset()
{
    GstStateChangeReturn state_return;
    mLastShowPos = -1;
    mPosBeforePause = -1;
       
    LOCK (&mActionMutex);
    if (!mPlayBin) 
    { 
        GST_PLAYER_ERROR ("Pipeline not initialized\n");
        goto EXIT;
    }  

    GST_PLAYER_LOG("Enter\n"); 
    state_return = gst_element_set_state (mPlayBin, GST_STATE_READY);
    GST_PLAYER_LOG("state_return = %d\n", state_return);

    if (state_return == GST_STATE_CHANGE_FAILURE) 
    {
        GST_PLAYER_ERROR ("Fail to set pipeline to READY\n");
        goto EXIT;
    }
    else if (state_return == GST_STATE_CHANGE_ASYNC)
    {
        GstState state;

        // wait for state change complete
        GST_PLAYER_DEBUG("Wait for pipeline's state change to READY(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
        GST_PLAYER_DEBUG("Pipeline's state change to READY, %p, %d, ret=%d", mPlayBin, state, state_return);
        
        if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_READY ) 
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to READY\n");
            goto EXIT;
        }
    }    
    
    // seek
    if(mSeeking)
    {
         GST_PLAYER_DEBUG("reset playback while seeking. Send MEDIA_SEEK_COMPLETE immediately");
         if(mGstPlayer)
             mGstPlayer->sendEvent(MEDIA_SEEK_COMPLETE);
    }
    mSeeking = false;
    mSeekState = GST_STATE_VOID_PENDING;
    
    // prepare
    if(mAsynchPreparePending)
    {
        GST_PLAYER_WARNING("mAsynchPreparePending shall not be true!");
    }
    mAsynchPreparePending = false;

EXIT:
    mGstPlayer = NULL; 
    UNLOCK (&mActionMutex);
    return true;
}

bool GstPlayerPipeline::setLooping(int loop)
{
    GST_PLAYER_LOG  ("setLooping (%s)\n", (loop!=0)?"TRUE":"FALSE");
    LOCK (&mActionMutex);
    mIsLooping = (loop != 0) ? true : false;

    if (mIsLooping)
    {
        if (!mLoopingThread)
        {
            mLoopingThread = g_thread_create ((GThreadFunc)(looping_thread_func), this, FALSE, NULL);
            if (mLoopingThread == NULL)
            {
                GST_PLAYER_ERROR ("Failed to create looping play Thread.\n");
            }
        }
        else
            GST_PLAYER_DEBUG("looping thread exist!!!\n");
    }

    UNLOCK (&mActionMutex);
    return true;
}

void GstPlayerPipeline::dump()
{
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN_CAST(mPlayBin), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");
}

void GstPlayerPipeline::handleEos(GstMessage* p_msg)
{
    GST_PLAYER_DEBUG ("Recevied EOS.\n");
    mLastShowPos = -1;
    mPosBeforePause = -1;

    mIsPlaying = false;

    if (mIsLooping) 
    {
        GST_PLAYER_DEBUG ("Loop is set, start playback again\n");
        mIsPlaying = true;
        sem_post(&mSem_eos);
    } 
    else 
    {
        GST_PLAYER_LOG("set GST_STATE_PAUSED at EOS\n");
        GstStateChangeReturn state_return = gst_element_set_state (mPlayBin, GST_STATE_PAUSED);
        if (state_return == GST_STATE_CHANGE_FAILURE)
        {
            GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
            return;
        }
        else if (state_return == GST_STATE_CHANGE_ASYNC)
        {
            GstState state;

            // wait for state change complete
            GST_PLAYER_DEBUG("Wait for pipeline's state change to PAUSE(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
            state_return = gst_element_get_state (mPlayBin, &state, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
            GST_PLAYER_DEBUG("Pipeline's state change to PAUSE, %p, %d, ret=%d", mPlayBin, state, state_return);
            if (state_return != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PAUSED )
            {
                GST_PLAYER_ERROR ("Fail to set pipeline to PAUSED\n");
                return;
            }
        }

        GST_PLAYER_DEBUG ("send MEDIA_PLAYBACK_COMPLETE event.\n");
        if(mGstPlayer)
            mGstPlayer->sendEvent(MEDIA_PLAYBACK_COMPLETE);
    }    
}

void GstPlayerPipeline::handleError(GstMessage* p_msg)
{
    gchar *debug=NULL;
    GError *err=NULL;

    gst_message_parse_error(p_msg, &err, &debug);
    GST_PLAYER_ERROR("%s error(%d): %s debug: %s\n", g_quark_to_string(err->domain), err->code, err->message, debug);

    if (mGstPlayer)
        mGstPlayer->sendEvent(MEDIA_ERROR, err->code); 

    g_free(debug);
    g_error_free(err);
}

void GstPlayerPipeline::handleTag(GstMessage* p_msg)
{
    GstTagList* taglist=NULL;

    gst_message_parse_tag(p_msg, &taglist);
    // enumerate each tag
    gst_tag_list_foreach(taglist, taglist_foreach, this);

    gst_tag_list_free(taglist);
}

void GstPlayerPipeline::handleBuffering(GstMessage* p_msg)
{
    gint percent = 0;
    gst_message_parse_buffering (p_msg, &percent);

    GST_PLAYER_LOG("Buffering: %d", percent); 
    if (mGstPlayer)
        mGstPlayer->sendEvent(MEDIA_BUFFERING_UPDATE, (unsigned int)percent);

    gint percentage;
    gst_message_parse_buffering(p_msg, &percentage);
    if(percentage < 100) {
       gst_element_set_state(mPlayBin, GST_STATE_PAUSED);
    } else {
       gst_element_set_state(mPlayBin, GST_STATE_PLAYING);
    } 
}

void GstPlayerPipeline::handleStateChanged(GstMessage* p_msg)
{
    GstState newstate = GST_STATE_VOID_PENDING;
    GstState oldstate = GST_STATE_VOID_PENDING;
    GstState pending  = GST_STATE_VOID_PENDING;
    GstElement *msgsrc = (GstElement *)GST_MESSAGE_SRC(p_msg);

    gst_message_parse_state_changed(p_msg, &oldstate, &newstate, &pending);

    GST_PLAYER_LOG("State changed message: %s, old-%d, new-%d, pending-%d\n", GST_ELEMENT_NAME(msgsrc),
         (int)oldstate, (int)newstate, (int)pending);

    // prepareAsync
    if (mAsynchPreparePending == true && msgsrc == mPlayBin && 
            newstate == GST_STATE_PAUSED && 
            (pending == GST_STATE_VOID_PENDING || pending == GST_STATE_PLAYING) )
    {
        int width = 0;
        int height = 0;
        GST_PLAYER_DEBUG("prepareAsynch() done, send MEDIA_PREPARED event\n");
        mAsynchPreparePending = false;
        if (mGstPlayer)
        {
            // get video size
            if(getVideoSize(&width, &height))
            {
                GST_PLAYER_DEBUG("Send MEDIA_SET_VIDEO_SIZE, width (%d), height (%d)", width, height);
                mGstPlayer->sendEvent(MEDIA_SET_VIDEO_SIZE, width, height);
            }            
            mGstPlayer->sendEvent(MEDIA_PREPARED);
            char value[PROPERTY_VALUE_MAX];
            property_get("debug.enable.gstpipeline_dump", value, "0");
            if(strcmp(value, "1") == 0)
            {
                dump();
            }
        }
    }   

    // seekTo
    if (mSeeking == true && msgsrc == mPlayBin && newstate == mSeekState)
    {
        GST_PLAYER_DEBUG("seekTo() done, send MEDIA_SEEK_COMPLETE event\n");
        mSeeking = false;
        mSeekState = GST_STATE_VOID_PENDING;
        if (mGstPlayer)
            mGstPlayer->sendEvent(MEDIA_SEEK_COMPLETE);
    }
}

void GstPlayerPipeline::handleAsyncDone(GstMessage* p_msg)
{
    GST_PLAYER_LOG("Enter");
}

void GstPlayerPipeline::handleSegmentDone(GstMessage* p_msg)
{
    gint64 position = 0;
    GstFormat format = GST_FORMAT_TIME;
    gst_message_parse_segment_done(p_msg, &format, &position);
    GST_PLAYER_LOG ("Position: %d\n", (int)(position / GST_MSECOND));
}

void GstPlayerPipeline::handleDuration(GstMessage* p_msg)
{
    gint64 duration = 0;
    GstFormat format = GST_FORMAT_TIME;
    gst_message_parse_duration(p_msg, &format, &duration);
    GST_PLAYER_LOG ("Duration: %d\n", (int)(duration / GST_MSECOND));
}

void GstPlayerPipeline::handleElement(GstMessage* p_msg)
{
    const GstStructure* pStru = gst_message_get_structure(p_msg);
    const gchar* name = gst_structure_get_name(pStru);
    GstElement *msgsrc = (GstElement *)GST_MESSAGE_SRC(p_msg);

    GST_PLAYER_LOG("message string: %s\n", name);
    if (strcmp(name, "application/x-contacting") == 0)
    {
        //onContacting();
    }
    else if (strcmp(name, "application/x-contacted") == 0)
    {
        //onContacted();
    }
    else if (strcmp(name, "application/x-link-attempting") == 0)
    {
        //onLinkAttempting(msgsrc);
    }
    else if (strcmp(name, "application/x-link-attempted") == 0)
    {
        //onLinkAttempted(msgsrc);
    }
    else if (strcmp(name, "application/x-download-completed") == 0)
    {
        // onDownloadCompleted();
    }
}

void GstPlayerPipeline::handleApplication(GstMessage* p_msg)
{
    const GstStructure* pStru = gst_message_get_structure(p_msg);
    const gchar* name = gst_structure_get_name(pStru);
    GstElement *msgsrc = (GstElement *)GST_MESSAGE_SRC(p_msg);

    GST_PLAYER_LOG("message string: %s\n", name);
    if (strcmp(name,  MSG_QUIT_MAINLOOP) == 0)
    {
        GST_PLAYER_DEBUG ("Received QUIT_MAINLOOP message.\n");
        g_main_loop_quit (mMainLoop);
    }
}

bool GstPlayerPipeline::SaveState()
{
    GstStateChangeReturn state_return;
    //GST_PLAYER_LOG("Call gst_element_get_state(%p, %lld)...", mPlayBin, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    state_return = gst_element_get_state(mPlayBin, &mBakState, NULL, IPPGST_GSTPLAYER_STATECHANGE_TIMEOUT);
    //GST_PLAYER_LOG("gst_element_get_state(%p), %d, ret=%d", mPlayBin, mBakState, state_return);
    if (state_return != GST_STATE_CHANGE_SUCCESS) 
    {
        return false;
    }
    return true;		
}

bool GstPlayerPipeline::GetBakState(GstState &state)
{
    state = mBakState;
    return true;
}

