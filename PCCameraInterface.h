//
// PanaCast Camera Capture Interface Header
//
//

#ifndef PCCAMERAINTERFACE_H
#define PCCAMERAINTERFACE_H 

#include <string>
enum panacast_raw_frame_format {
   PANACAST_FRAME_FORMAT_YUYV,
   PANACAST_FRAME_FORMAT_UYVY,
   PANACAST_FRAME_FORMAT_MJPEG,
   PANACAST_FRAME_FORMAT_YV12,
   PANACAST_FRAME_FORMAT_NV12,
};

struct panacast_raw_frame_t {
   unsigned char *buf;
   int size; //JPEG size 
   volatile int in_use;
   void *private_data;
   enum panacast_raw_frame_format  format;
   unsigned width;
   unsigned height;
};


class CaptureInterface {
   public:
      virtual bool init(unsigned width, unsigned height, panacast_raw_frame_format format, void * captureDevice) = 0;
      virtual struct panacast_raw_frame_t * get_next_frame() = 0;
      virtual void stop_capture() = 0;
};

class AVCaptureCallback {
public:
    virtual void * handleCapturedFrame(unsigned char * theData,
                                       unsigned width,
                                       unsigned height,
                                       panacast_raw_frame_format format,
                                       int length,
                                       void * buffer) = 0;
};



#endif /* cmio_cam_h */
