//
//  CamCapture.h
//  cam
//
//  Created by John Zhang on 8/28/15.
//  Copyright (c) 2015 John Zhang. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#include "PCCameraInterface.h"

class AVCaptureCallback {
public:
    virtual void * handleCapturedFrame(unsigned char * theData,
                                       unsigned width,
                                       unsigned height,
                                       panacast_raw_frame_format format,
                                       int length,
                                       void * buffer) = 0;
};

@interface AVFoundationCapture : NSObject {
    AVCaptureVideoDataOutput *captureOutput;
    AVCaptureSession *session;
    AVCaptureDeviceInput *deviceInput;
    dispatch_queue_t captureQueue;
    NSArray *observers;
}

@property (nonatomic, retain) NSString *deviceID;
@property (nonatomic, retain) AVCaptureDevice *captureDevice;
@property (nonatomic, retain) NSArray *panaCastDevices;

- (id) initWithCaptureDevice:(AVCaptureDevice *) device
                    andWidth:(unsigned)width
                   andHeight:(unsigned)height
                   andFormat:(panacast_raw_frame_format)format
          andCaptureCallback:(AVCaptureCallback*)cb;

- (void) dealloc;
- (void) startCapture;
- (void) stopCapture;
- (int) isRunning;
@end

