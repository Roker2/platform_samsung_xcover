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

/*
 * This file defines APIs to convert C++ AudioFlinder/AudioTrack
 * interface to C interface
 */
#ifndef __AUDIOCACHE_WRAPPER_H__
#define __AUDIOCACHE_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void* AudioCacheDeviceHandle;

AudioCacheDeviceHandle audiocache_device_open(void* audio_sink);

void audiocache_device_release(AudioCacheDeviceHandle handle);

void audiocache_device_start(AudioCacheDeviceHandle handle);

int audiocache_device_set(AudioCacheDeviceHandle handle, int channelCount, int sampleRate);

void audiocache_device_stop(AudioCacheDeviceHandle handle);

ssize_t  audiocache_device_write(AudioCacheDeviceHandle handle,
    const void* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIOFLINGER_WRAPPER_H__ */
