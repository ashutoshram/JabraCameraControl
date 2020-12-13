//
//  MacFrameCapture.h
//  
//
//

#ifndef MACFRAMECAPTURE_H
#define MACFRAMECAPTURE_H
#include "PCCameraInterface.h"
#include "AVFoundationCapture.h"
#include <pthread.h>
#include <AVFoundation/AVFoundation.h>

class MacCameraCapture : public CaptureInterface, public AVCaptureCallback {
public:
    MacCameraCapture();
    bool init(unsigned width, unsigned height, panacast_raw_frame_format format);
    AVCaptureDevice *captureDevice;
    struct panacast_raw_frame_t * get_next_frame();
    void stop_capture();
    void * handleCapturedFrame(unsigned char * theData, unsigned width,
                               unsigned height, panacast_raw_frame_format format,
                               int length, void * buffer);

private:
    AVFoundationCapture *mycam; // objective-C instance
    pthread_mutex_t bufferLock;
    struct panacast_raw_frame_t frames[2];
    volatile int curr_frame_index;
};
#endif
