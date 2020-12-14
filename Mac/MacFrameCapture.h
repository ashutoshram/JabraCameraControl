//
//  MacFrameCapture.h
//  
//
//

#ifndef MACFRAMECAPTURE_H
#define MACFRAMECAPTURE_H
#include "PCCameraInterface.h"
#include "utils.h"
#include <memory>

class MacCameraCapture : public CaptureInterface, public AVCaptureCallback {
public:
    MacCameraCapture();
    virtual ~MacCameraCapture();
    bool init(unsigned width, unsigned height, RawFrameFormat format, void * captureDevice);
    struct RawFrame * getNextFrame();
    void freeFrame();
    void stopCapture();
    void * handleCapturedFrame(unsigned char * theData, unsigned width,
                               unsigned height, RawFrameFormat format,
                               int length, void * buffer);

private:
    void *avfoundationCam; // objective-C instance
    pthread_mutex_t bufferLock;
    struct RawFrame frames[2];
    volatile int currFrameIdx;
    std::unique_ptr<OSEvent> frameAvail;
};
#endif
