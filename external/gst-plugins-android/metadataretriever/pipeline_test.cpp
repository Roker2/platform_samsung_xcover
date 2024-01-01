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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "GstMetaDataDriver.h"
#include "GstLog.h"

#include "SkBitmap.h"
#include "SkImageEncoder.h"
#include "SkColorPriv.h"
#include "SkDither.h"

using namespace android;

static void test_setDataSource_fd(char *file)
{
    int fd = open(file, O_RDONLY);
    GstMetaDataDriver *pipeline = new GstMetaDataDriver();
#if 1
    printf("captureFrame\n");
    pipeline->setDataSource(fd, 0, 0);
    pipeline->setMode(2);
    VideoFrame *frame = pipeline->captureFrame();
    if(frame)
    {
        printf("got the frame\n");

        SkBitmap bitmap;
        SkImageEncoder *encoder;
        void *dst;
        bitmap.setConfig(SkBitmap::kRGB_565_Config, frame->mWidth, frame->mHeight);
	bitmap.allocPixels();
	bitmap.lockPixels();
        dst = bitmap.getPixels();
        memcpy(dst, frame->mData, bitmap.getSize());
        encoder = SkImageEncoder::Create(SkImageEncoder::kPNG_Type);
        if(NULL != encoder)
        {
            if(encoder->encodeFile("/sdcard/frame.png", bitmap, 75))
            {
                printf("okay, generated the thumbnail\n");
            }
            delete encoder;
        }
	bitmap.unlockPixels();
        delete frame;
    }
#endif

#if 1
    printf("extractMetadata\n");
    pipeline->setDataSource(fd, 0, 0);
    pipeline->setMode(1);
    const char *str = pipeline->extractMetadata(9);
    if(str)
    {
        printf("duration: %s\n", str);
    }
    str = pipeline->extractMetadata(20);
    if(str)
    {
        printf("width: %s\n", str);
    }
    str = pipeline->extractMetadata(19);
    if(str)
    {
        printf("height: %s\n", str);
    }
#endif

    printf("delete pipeline\n");
    delete pipeline;
}

static void test_setDataSource(char *file)
{
    GstMetaDataDriver *pipeline = new GstMetaDataDriver();
#if 1
    printf("captureFrame\n");
    pipeline->setDataSource(file);
    pipeline->setMode(2);
    VideoFrame *frame = pipeline->captureFrame();
    if(frame)
    {
        printf("got the frame\n");

        SkBitmap bitmap;
        SkImageEncoder *encoder;
        void *dst;
        bitmap.setConfig(SkBitmap::kRGB_565_Config, frame->mWidth, frame->mHeight);
	bitmap.allocPixels();
	bitmap.lockPixels();
        dst = bitmap.getPixels();
        memcpy(dst, frame->mData, bitmap.getSize());
        encoder = SkImageEncoder::Create(SkImageEncoder::kPNG_Type);
        if(NULL != encoder)
        {
            if(encoder->encodeFile("/sdcard/frame.png", bitmap, 75))
            {
                printf("okay, generated the thumbnail\n");
            }
            delete encoder;
        }
	bitmap.unlockPixels();
        delete frame;
    }
#endif

#if 1
    printf("extractMetadata\n");
    pipeline->setDataSource(file);
    pipeline->setMode(1);
    const char *str = pipeline->extractMetadata(9);
    if(str)
    {
        printf("duration: %s\n", str);
    }
    str = pipeline->extractMetadata(20);
    if(str)
    {
        printf("width: %s\n", str);
    }
    str = pipeline->extractMetadata(19);
    if(str)
    {
        printf("height: %s\n", str);
    }
#endif

    printf("delete pipeline\n");
    delete pipeline;
}

int main (int argc, char *argv[])
{
    if(argc !=2 )
    {
        printf("Usage: %s file_path\n", argv[0]);
        return -1;
    }
    printf("set fd data source\n");
    test_setDataSource_fd(argv[1]);
    printf("set uri data source\n");
    test_setDataSource(argv[1]);
    return 0;
} 
