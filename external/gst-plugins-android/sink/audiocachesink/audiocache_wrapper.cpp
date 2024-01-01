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
#define ENABLE_GST_PLAYER_LOG
#include <media/AudioTrack.h>
#include <media/AudioSystem.h>
#include <utils/Log.h>
#include <MediaPlayerInterface.h>
#if PLATFORM_SDK_VERSION >= 9
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryBase.h>
#else
#include <AudioFlinger.h>
#endif
#include <MediaPlayerService.h>
#include "audiocache_wrapper.h"
#include <glib/glib.h>
#include <GstLog.h>

using namespace android;

typedef struct _AudioCacheDevice
{
  sp<MediaPlayerBase::AudioSink> audio_sink;
  bool init;
} AudioCacheDevice;


/* commonly used macro */
#define AUDIO_CACHE_DEVICE(handle) ((AudioCacheDevice*)handle)
#define AUDIO_CACHE_DEVICE_SINK(handle) (AUDIO_CACHE_DEVICE(handle)->audio_sink)

AudioCacheDeviceHandle audiocache_device_open(void* audiosink)
{
  AudioCacheDevice* audiodev = NULL;

  // audio_sink shall be an MediaPlayerBase::AudioSink instance
  if(audiosink == NULL)
    return NULL;

  // create a new instance of AudioCache
  audiodev = new AudioCacheDevice;
  if (audiodev == NULL) {
    GST_PLAYER_ERROR("Error to create AudioCacheDevice\n");
    return NULL;
  }

  // set AudioSink
  audiodev->audio_sink = (MediaPlayerBase::AudioSink*)audiosink;
  audiodev->init = false;
  GST_PLAYER_DEBUG("Open AudioSink successfully\n");

  return (AudioCacheDeviceHandle)audiodev;
}

ssize_t audiocache_device_write (AudioCacheDeviceHandle handle, const void *buffer,
    size_t size)
{
  if (handle == NULL || AUDIO_CACHE_DEVICE(handle)->init == false)
    return -1;

  return AUDIO_CACHE_DEVICE_SINK(handle)->write(buffer, size);
}

void audiocache_device_start (AudioCacheDeviceHandle handle)
{
  if (handle == NULL || AUDIO_CACHE_DEVICE(handle)->init == false)
    return;

  AUDIO_CACHE_DEVICE_SINK(handle)->start();
}

int audiocache_device_set (AudioCacheDeviceHandle handle, int channelCount, int sampleRate)
{
  status_t status = NO_ERROR;

  GST_PLAYER_DEBUG("Audiocache device set\n");
  if (handle == NULL)
    return -1;

  int format = AudioSystem::PCM_16_BIT;
  int bufferCount = 0;

  status = AUDIO_CACHE_DEVICE_SINK(handle)->open(sampleRate, channelCount, format, bufferCount);
  if (status != NO_ERROR)
    return -1;

  AUDIO_CACHE_DEVICE(handle)->init = true;
  return 0;
}

void audiocache_device_stop (AudioCacheDeviceHandle handle)
{
  if (handle == NULL || AUDIO_CACHE_DEVICE(handle)->init == false)
    return;

  AUDIO_CACHE_DEVICE_SINK(handle)->stop();
}

void audiocache_device_release (AudioCacheDeviceHandle handle)
{
  if (handle == NULL)
    return;

  if (AUDIO_CACHE_DEVICE_SINK(handle).get()) {
    GST_PLAYER_DEBUG("Release AudioSink device\n");
    AUDIO_CACHE_DEVICE_SINK(handle).clear();
  }

  delete AUDIO_CACHE_DEVICE(handle);
}
