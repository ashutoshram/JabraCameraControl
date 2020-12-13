//
//  MacCameraCapture.cpp
//  OpenCLYUY2
//
//  Created by Ram Natarajan on 8/18/16.
//  Copyright (c) 2016 Ram Natarajan. All rights reserved.
//

#include "MacFrameCapture.h"
#include <string>

MacCameraCapture::MacCameraCapture()
{
    pthread_mutex_init(&bufferLock, NULL);
    memset(&frames[0], 0, sizeof(frames));
    curr_frame_index = -1;
    captureDevice = Nil;   
    mycam = Nil;
}

bool MacCameraCapture::init(unsigned int width, unsigned int height, panacast_raw_frame_format format)
{
    if (format != PANACAST_FRAME_FORMAT_MJPEG && format != PANACAST_FRAME_FORMAT_YUYV) return false;
    
    if(!mycam || ![mycam isRunning]) {
        pthread_mutex_lock(&bufferLock);
        if (!mycam) {
            mycam = [[AVFoundationCapture alloc] initWithCaptureDevice: captureDevice
                                                     andWidth:width andHeight:height
                                                    andFormat:format andCaptureCallback:(AVCaptureCallback *)this];
        }
        [mycam startCapture];
        pthread_mutex_unlock(&bufferLock);
        return true;
    }

    return false;
}

struct panacast_raw_frame_t * MacCameraCapture::get_next_frame()
{
    if (curr_frame_index < 0) {
        //printf("get_next_frame: curr_frame_index = %d\n", curr_frame_index);
        return NULL;
    }
    
    if (!frames[curr_frame_index].buf) {
        printf("get_next_frame: frames[%d].buf = %p\n", curr_frame_index, frames[curr_frame_index].buf);
        return NULL;
    }

    return &frames[curr_frame_index];
}

void MacCameraCapture::stop_capture()
{
    if (mycam != Nil)
        [mycam stopCapture];
}

void * MacCameraCapture::handleCapturedFrame(unsigned char * theData,
                                          unsigned width,
                                          unsigned height,
                                          panacast_raw_frame_format format,
                                          int length,
                                          void * buffer)
{
    int next_frame_index = (curr_frame_index + 1) % 2;
    void * spBufSrc = frames[next_frame_index].private_data;

    //printf("handleCapturedFrame: next_frame_index = %d\n", next_frame_index);
    
    if (frames[next_frame_index].in_use) {
        printf("handleCapturedFrame: in_use = %d\n", frames[next_frame_index].in_use);
        return buffer;
    }
    
    frames[next_frame_index].buf = theData;
    frames[next_frame_index].size = length;
    frames[next_frame_index].private_data = buffer;
    frames[next_frame_index].format = format;
    frames[next_frame_index].width = width;
    frames[next_frame_index].height = height;

    
    curr_frame_index = next_frame_index;
    return spBufSrc;
}



