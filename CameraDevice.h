#ifndef __CAMERADEVICE_H__
#define __CAMERADEVICE_H__

#include <string>
#include <vector>

#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
 
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOMediaBSDClient.h>
 
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <CoreFoundation/CFNumber.h>
#endif

#include "MacFrameCapture.h"

//#include "Logger.h" // FIXME

//Structure that holds the USB Device Descriptor values
struct DeviceDescriptor{
	unsigned char	length;
	unsigned char	descriptorType;
	unsigned short	bcdUSB;
	unsigned char   deviceClass;
	unsigned char   deviceSubClass;
	unsigned char   deviceProtocol;
	unsigned char   maxPacketSize;
	unsigned short  idVendor;
	unsigned short  idProduct;
	unsigned short  bcdDevice;
	unsigned char   manufacturer;
	unsigned char   product;
	unsigned char   serialNumber;
	unsigned char   numConfigurations;
};

struct CommandInfo {
    unsigned char requestType;
    unsigned char request;
    unsigned short value;
    unsigned short index; 
    std::vector<unsigned char> data;
};

struct Property {
    Property() { value = 0; min = 0; max = 0; }
    Property(int _value, int _min, int _max) { value = _value; min = _min; max = _max; }
    int value;
    int min;
    int max;  
    bool returnValue;
};

enum PropertyType {
    Brightness,
    Contrast,
    Saturation,
    Sharpness,
    WhiteBalance    
};

// cross platform
class CameraDeviceInterface {
    public:
        virtual bool getProperty(PropertyType t, Property& prop) = 0;
        virtual bool setProperty(PropertyType p, int value) = 0; 
        virtual bool sendCommand(CommandInfo& info) = 0;
        virtual ~CameraDeviceInterface() = default;
};

#ifdef _WIN32
class WindowsCameraDevice : public CameraDeviceInterface {
    public:
        WindowsCameraDevice(const std::string& prop);
        bool getProperty(PropertyType t, Property& prop);
        bool setProperty(PropertyType p, int value);
        bool sendCommand(CommandInfo& info);
        static bool getJabraDevices(std::vector<std::string>&);
};
#elif __linux__
class LinuxCameraDevice : public CameraDeviceInterface {
    public:
        LinuxCameraDevice(const std::string& prop);
        bool getProperty(PropertyType t, Property& prop);
        bool setProperty(PropertyType p, int value);
        bool sendCommand(CommandInfo& info);
        static bool getJabraDevices(std::vector<std::string>&);
};
#elif __APPLE__
class MacCameraDevice : public CameraDeviceInterface {
    public:
        MacCameraDevice(const std::string& prop);
        virtual ~MacCameraDevice();
        bool getProperty(PropertyType t, Property& prop);
        bool setProperty(PropertyType p, int value);
        bool sendCommand(CommandInfo& info);
        static bool getJabraDevices(std::vector<std::string>&);
    private:
        IOUSBInterfaceInterface190 * * mControlIf;
        std::string mProperty;
        // return a vector of all jabra devices in allDevs
        // if devSn is "", then return all, else return the specific requested devSn
        static bool getAllDevices(std::string devSn, 
            std::vector<std::string> &allDevs, IOUSBInterfaceInterface190 ** &cIf);
};
  
#endif


class CameraQueryInterface {
    public:
        virtual ~CameraQueryInterface() {}
        //virtual void setLogger(Logger& l) = 0; // FIXME
        //virtual void setLoggerVerbosity(LOG_LEVEL_E level) = 0;
        bool getAllJabraDevices(std::vector<std::string>& devPaths) {
#ifdef _WIN32
            return WindowsCameraDevice::getJabraDevices(devPaths);
#elif __linux__
            return LinuxCameraDevice::getJabraDevices(devPaths);
#elif __APPLE__
            return MacCameraDevice::getJabraDevices(devPaths);
#endif
        }
        CameraDeviceInterface * openJabraDevice(const std::string& prop) {
            CameraDeviceInterface * cameraDevice;
#ifdef _WIN32
            cameraDevice = new WindowsCameraDevice(prop);
#elif __linux__
            cameraDevice = new LinuxCameraDevice(prop);
#elif __APPLE__
            cameraDevice = new MacCameraDevice(prop);
#else
            cameraDevice = NULL;
#endif
            return cameraDevice;
        }
};

class CameraStreamInterface {
    public:
        CameraStreamInterface(std::string _deviceName, unsigned _width, unsigned _height, std::string _format, unsigned _fps) {
            deviceName = _deviceName;
            width = _width;
            height = _height;
            format = _format;
            fps = _fps;
            cameraOpened = false;
        }

        void updateParams(unsigned _width, unsigned _height, std::string _format, unsigned _fps) {
            width = _width;
            height = _height;
            format = _format;
            fps = _fps;
        }

        bool openStream() {

           if (cameraOpened) return true;

           if (!m) {
               m.reset(new MacCameraCapture);
           }
           
           if (!m->init(width, height, format == "MJPG" ? PANACAST_FRAME_FORMAT_MJPEG : PANACAST_FRAME_FORMAT_YUYV, NULL)){
              printf("CameraStreamInterface: openStream: could not initialize camera\n");
              cameraOpened = false;
              return false;
           }
           
           cameraOpened = true;
           return true;
        }

        bool getFrame(unsigned char * & ptrFrame, unsigned& length) {

           // openStream should have been called at this point
           if (!cameraOpened) return false;

           RawFrame * frame = m->getNextFrame();
           if (frame != NULL) {
              ptrFrame = frame->buf;
              unsigned yuyvSize = width * height * 2;
              length = format == "MJPG" ? frame->size : yuyvSize;
              return true;
           }
           return false;
        }

        void freeFrame() {
           m->freeFrame();
        }

    private:
        std::string deviceName;
        unsigned width;
        unsigned height;
        std::string format;
        unsigned fps;
        bool cameraOpened;
        std::unique_ptr<MacCameraCapture> m;   
};


#endif /*__CAMERADEVICE_H__*/
