/* GStreamer
 * Copyright (C) <2009> Steve Guo <letsgoustc@gmail.com>
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

#ifndef _GST_METADATADRIVER_H_
#define _GST_METADATADRIVER_H_

#include <media/MediaMetadataRetrieverInterface.h>

using namespace android;

class GstMetadataRetrieverPipeline;

namespace android {

class VideoFrame;
class MediaAlbumArt;

class GstMetaDataDriver : public MediaMetadataRetrieverInterface
{
public:
	GstMetaDataDriver();
	virtual ~GstMetaDataDriver();

	status_t setMode(int mode);
	status_t getMode(int* mode) const;
	status_t setDataSource(const char* srcUrl);
	status_t setDataSource(int fd, int64_t offset, int64_t length);
	VideoFrame *captureFrame();
        VideoFrame * getFrameAtTime(int64_t timeUs, int option);
	const char* extractMetadata(int keyCode);
	MediaAlbumArt* extractAlbumArt();

private:
	mutable Mutex mLock;
	GstMetadataRetrieverPipeline *mMetaDataRetriever;
};

};

#endif _GST_METADATADRIVER_H_
