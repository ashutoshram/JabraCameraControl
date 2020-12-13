//
//  AVFoundationCapture.m
//  cam
//
//  Created by John Zhang on 8/28/15.
//  Copyright (c) 2015 John Zhang. All rights reserved.
//

#import "AVFoundationCapture.h"
#include <pthread.h>
#include <os/log.h>

#define DBG(fmt, args...) os_log(OS_LOG_DEFAULT, fmt, ##args)
#define TRACE   DBG("Entering AVFoundationCapture::%s\n", __FUNCTION__)

#pragma mark USB serial number support
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOCFPlugin.h>

#define kAltiaVendorID          0x2B93
#define kCypressProductID       0x00F3

@interface AVFoundationCapture ()
<AVCaptureVideoDataOutputSampleBufferDelegate> {
}
@end

@implementation AVFoundationCapture
volatile CMSampleBufferRef spBufSrc = Nil;
unsigned captureWidth;
unsigned captureHeight;
panacast_raw_frame_format captureFormat;

AVCaptureCallback * callback = NULL;


- (id) initWithCaptureDevice:(AVCaptureDevice *) device
                    andWidth:(unsigned)width
                   andHeight:(unsigned)height
                   andFormat:(panacast_raw_frame_format)format
          andCaptureCallback:(AVCaptureCallback*)cb;
{
    if (self = [super init]){
        captureWidth = width;
        captureHeight = height;
        captureFormat = format;
        [self initSession];
        if (device == Nil) {
            device = [self getFirstPanaCastDevice];
        }
        [self setupCaptureWithDevice: device];
    }
    callback = cb;
    return self;
}

-(void)dealloc {
   [super dealloc];
}



-(void)initSession {
    if (!session)
    {
        // Create a capture session
        session = [[AVCaptureSession alloc] init];
    }
}


- (void) setupCaptureWithDevice: (AVCaptureDevice *) device
{
    _captureDevice = device;
    
    // create an output for YUV output with self as delegate
    if (!captureQueue) {
        captureQueue = dispatch_queue_create("com.altia.avcapture", nil);
    }
    captureOutput = [[AVCaptureVideoDataOutput alloc] init];
    if (!captureOutput) {
        NSException *exception = [NSException exceptionWithName:@"sessionSetupFailed" reason:@"setupOutputFailed" userInfo:nil];
        @throw exception;
    }
    
    [captureOutput setSampleBufferDelegate: self queue: captureQueue];
 
    NSLog(@"Capturing at resolution %dx%d", captureWidth, captureHeight);

    
    NSDictionary* setcapSettings = [NSDictionary dictionaryWithObjectsAndKeys:
                                    captureFormat == PANACAST_FRAME_FORMAT_YUYV ?
                                    [NSNumber numberWithInt: kCVPixelFormatType_422YpCbCr8_yuvs] : [NSNumber numberWithInt: 'dmb1']
                                    , kCVPixelBufferPixelFormatTypeKey,
                                    [NSNumber numberWithInteger:captureWidth], (id)kCVPixelBufferWidthKey,
                                    [NSNumber numberWithInteger:captureHeight], (id)kCVPixelBufferHeightKey,
                                    nil];
    captureOutput.videoSettings = setcapSettings;
    captureOutput.alwaysDiscardsLateVideoFrames = YES;

    
    NSLog(@"Capture output set at resolution %dx%d", captureWidth, captureHeight);

    if ([session canAddOutput:captureOutput]) {
        [session addOutput: captureOutput];
    } else {
        NSException *exception = [NSException exceptionWithName:@"sessionSetupFailed" reason:@"cannotAddOutput" userInfo:nil];
        @throw exception;
    }
    
    NSLog(@"Capture session setup complete");
}

- (void) captureOutput: (AVCaptureOutput*) output
 didOutputSampleBuffer: (CMSampleBufferRef) buffer
        fromConnection: (AVCaptureConnection*) connection
{

    unsigned char *theData = NULL;
    int size = 0;
    if (captureFormat == PANACAST_FRAME_FORMAT_MJPEG) {
        CMBlockBufferRef bbuf = CMSampleBufferGetDataBuffer( buffer );
        size_t length = 0;
        if (bbuf != Nil) {
            length = CMBlockBufferGetDataLength(bbuf);
            
            if (CMBlockBufferGetDataPointer(bbuf, 0, NULL, &length, (char **)&theData) != kCMBlockBufferNoErr) {
                DBG("captureOutput: Cannot get data buffer\n");
                return;
            }
            size = (int)length;
        }
    } else {
        CVImageBufferRef cameraFrame = CMSampleBufferGetImageBuffer(buffer);
        if (cameraFrame != Nil) {
            BOOL isPlanar = CVPixelBufferIsPlanar(cameraFrame);
            BOOL isYUYV = CVPixelBufferGetPixelFormatType(cameraFrame) == kCVPixelFormatType_422YpCbCr8_yuvs;
            if (isPlanar || !isYUYV) {
                DBG("captureOutput: We just got a planar sample buffer.  Was expecting interleaved YUYV!");
                return;
            }
            //Pixel buffer size is actual frame size.
            unsigned bufWidth = (unsigned)CVPixelBufferGetWidth(cameraFrame);
            unsigned bufHeight = (unsigned)CVPixelBufferGetHeight(cameraFrame);
            if (bufWidth != captureWidth || bufHeight != captureHeight) {
                DBG("captureOutput: incorrect resolution %dx%d - required %dx%d",
                    bufWidth, bufHeight, captureWidth, captureHeight);
                return;

            }
            CVPixelBufferLockBaseAddress(cameraFrame, 0);
            
            theData = (unsigned char *)CVPixelBufferGetBaseAddress(cameraFrame);
        }
    }
    
    if (theData == NULL) return;
    
    spBufSrc = (CMSampleBufferRef)callback->handleCapturedFrame(theData, captureWidth, captureHeight, captureFormat, size, buffer);
    
    CFRetain(buffer); // retain the current

    if (spBufSrc!=nil) {
        // free the older one
        if (captureFormat == PANACAST_FRAME_FORMAT_YUYV) {
            CVImageBufferRef cameraFrame = CMSampleBufferGetImageBuffer(spBufSrc);
            CVPixelBufferUnlockBaseAddress(cameraFrame, 0);
        }
        CFRelease(spBufSrc);
        spBufSrc = nil;
    }

}

- (void) startCapture
{
    if (_captureDevice && session)
    {
        NSError *error;
        deviceInput = [AVCaptureDeviceInput deviceInputWithDevice:_captureDevice error:&error];
        [session addInput:deviceInput];
        
        if (!session.isRunning) {
            AVCaptureDevice *device = _captureDevice;
            AVCaptureDeviceFormat *bestFormat = nil;
            AVFrameRateRange *bestFrameRateRange = nil;
            OSType reqdType = captureFormat == PANACAST_FRAME_FORMAT_YUYV ?
                kCVPixelFormatType_422YpCbCr8_yuvs : 'dmb1';
            
            for ( AVCaptureDeviceFormat *format in [device formats] ) {
                CMFormatDescriptionRef desc = format.formatDescription;
                if (CMFormatDescriptionGetMediaSubType(desc) != reqdType) continue;
                
                CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(desc);
                
                if ((unsigned)dim.height != captureHeight || (unsigned)dim.width != captureWidth) continue;
                
                //NSLog(@"%@, %@", format.mediaType, format.formatDescription);
                for ( AVFrameRateRange *range in format.videoSupportedFrameRateRanges ) {
                    if ( range.maxFrameRate > bestFrameRateRange.maxFrameRate ) {
                        bestFormat = format;
                        bestFrameRateRange = range;
                    }
                }
            }
            if ( bestFormat ) {
                if ( [device lockForConfiguration:NULL] == YES ) {
                    device.activeFormat = bestFormat;
                    device.activeVideoMinFrameDuration = bestFrameRateRange.maxFrameDuration;
                    device.activeVideoMaxFrameDuration = bestFrameRateRange.minFrameDuration;
                    [device unlockForConfiguration]; // do not release lock until stopCapture
                }
            }
            
            [session startRunning];
            NSLog(@"Video capture Started");
        }
        else
        {
            NSLog(@"Video capture already started.");
        }
    }
    else
    {
        NSLog(@"No capture device.  Not starting capture session.");
    }
    
}

- (int) isRunning
{
    if (!session)
        return 0;
    
    if (!session.running)
        return 0;
    
    return 1;
}

- (void) stopCapture
{
    if (session.running) {
        NSLog(@"Video Capture stopped");
        [session stopRunning];
    }
    // Now clean up old inputs and outputs.
    if (session) {
        
        if (deviceInput)
        {
            [session beginConfiguration];
            if (deviceInput)
            {
                // Remove the old device input from the session
                [session removeInput:deviceInput];
                deviceInput = nil;

            }
            if (_captureDevice) {
//                [_captureDevice unlockForConfiguration];
            }

            [session commitConfiguration];
        }
    }
}

#pragma mark - FrameSourceCameraDelegate methods
- (void) cameraStalled: (AVCaptureDevice *) captureDevice
{
    
}

- (void) cameraConnected: (AVCaptureDevice *) captureDevice
{
    
}

- (void) cameraDisconnected: (AVCaptureDevice *)  captureDevice
{
    if (captureDevice == _captureDevice) {
        [self stopCapture];
        _captureDevice = nil;
    }
}

//Currently only interested in PanaCast devices:
- (AVCaptureDevice *) getPanaCastDevice: (AVCaptureDevice *) device
{
    NSString *name = device.localizedName;
    BOOL isPanacastCamera = [name containsString: @"PanaCast"] && (![name containsString:@"ePTZ"]);
    
    return isPanacastCamera ? device : nil;
}

- (void)refreshDevices
{
    NSMutableArray *devices = [NSMutableArray arrayWithArray:
                               [[AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo] arrayByAddingObjectsFromArray:
                                [AVCaptureDevice devicesWithMediaType:AVMediaTypeMuxed]]];
    NSMutableArray *pcDevs = [[NSMutableArray alloc] init];
    
    for (AVCaptureDevice *device in devices)
    {
        AVCaptureDevice *panaCastCamera = [self getPanaCastDevice: device];
        if (panaCastCamera) {
            [pcDevs addObject: panaCastCamera];
        }
    }
    
    NSLog(@"Refreshing Devices. %lu found", (unsigned long)pcDevs.count);
    //This will update any KVO observers.
    self.panaCastDevices = pcDevs;
}

-(AVCaptureDevice *) getFirstPanaCastDevice {
    [self refreshDevices];
    if ([self.panaCastDevices count] != 0)
        return self.panaCastDevices[0];
    else
        return nil;
}
@end
