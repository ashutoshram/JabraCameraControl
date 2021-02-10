//
//  MacCameraCapture.cpp


#include "MacFrameCapture.h"
#include "AVFoundationCapture.h"
#include <AVFoundation/AVFoundation.h>
#include <string>


MacCameraCapture::MacCameraCapture()
{
    pthread_mutex_init(&bufferLock, NULL);
    memset(&frames[0], 0, sizeof(frames));
    currFrameIdx = -1;
    avfoundationCam = NULL;
    int s;
    frameAvail.reset(new OSEvent(s, false, false));
}

MacCameraCapture::~MacCameraCapture()
{
   stopCapture();
   [((AVFoundationCapture*)avfoundationCam) dealloc];
   avfoundationCam = Nil;
}

bool MacCameraCapture::init(unsigned int width, unsigned int height, RawFrameFormat format, void * captureDevice)
{
    if (format != PANACAST_FRAME_FORMAT_MJPEG && format != PANACAST_FRAME_FORMAT_YUYV) return false;
    
    if(!avfoundationCam) {
        AVFoundationCapture * avfoundationCamOC = [[AVFoundationCapture alloc] initWithCaptureDevice: (AVCaptureDevice*) captureDevice
           andWidth:width andHeight:height
           andFormat:format andCaptureCallback:(AVCaptureCallback *)this];
        [avfoundationCamOC startCapture];
        avfoundationCam = (void *)avfoundationCamOC;
        pthread_mutex_unlock(&bufferLock);
        return true;
    }

    return false;
}

#define FRAME_AVAILABLE_TIMEOUT_MSEC 100

struct RawFrame * MacCameraCapture::getNextFrame()
{
    OSEventError err = frameAvail->TimedWait(FRAME_AVAILABLE_TIMEOUT_MSEC);
    if (err != OSEvent_Error_None) return NULL;

    pthread_mutex_lock(&bufferLock);
    if (currFrameIdx < 0) {
        //printf("get_next_frame: currFrameIdx = %d\n", currFrameIdx);
        pthread_mutex_unlock(&bufferLock);
        return NULL;
    }
    
    if (!frames[currFrameIdx].buf) {
        //printf("get_next_frame: frames[%d].buf = %p\n", currFrameIdx, frames[currFrameIdx].buf);
        pthread_mutex_unlock(&bufferLock);
        return NULL;
    }

    // we keep the mutex locked here until freeFrame is called by the calling code
    return &frames[currFrameIdx];
}

void MacCameraCapture::freeFrame()
{
    pthread_mutex_unlock(&bufferLock);
}

void MacCameraCapture::stopCapture()
{
    if (avfoundationCam != Nil) {
        [((AVFoundationCapture*)avfoundationCam) stopCapture];
        frameAvail->Reset();
    }
}

void * MacCameraCapture::handleCapturedFrame(unsigned char * theData,
                                          unsigned width,
                                          unsigned height,
                                          RawFrameFormat format,
                                          int length,
                                          void * buffer)
{
    pthread_mutex_lock(&bufferLock);
    int nextFrameIdx = (currFrameIdx + 1) % 2;
    void * spBufSrc = frames[nextFrameIdx].private_data;

    frames[nextFrameIdx].buf = theData;
    frames[nextFrameIdx].size = length;
    frames[nextFrameIdx].private_data = buffer;
    frames[nextFrameIdx].format = format;
    frames[nextFrameIdx].width = width;
    frames[nextFrameIdx].height = height;

    
    currFrameIdx = nextFrameIdx;
    pthread_mutex_unlock(&bufferLock);

    frameAvail->Signal(); // tell any waiting threads that we have a frame
    return spBufSrc;
}



