#ifdef __APPLE__
#include "CameraDevice.h"
#include <stdexcept>

#define UVC_GET_CUR 0x81
#define UVC_GET_MIN 0x82
#define UVC_GET_MAX 0x83
#define UVC_GET_DEF 0x87

#define UVC_SET_CUR 0x01

#define ALTIA_VENDOR_ID 0x2b93
#define GN_VENDOR_ID    0x0b0e 

#define UVC_INPUT_TERMINAL_ID  0x01
//#define UVC_PROCESSING_UNIT_ID 0x02
#define UVC_PROCESSING_UNIT_ID 0x03

MacCameraDevice::MacCameraDevice(const std::string& deviceName) : mDeviceName(deviceName)
{

	mControlIf = NULL;

   // get the control interface for the device and cache it
	if (!getControlInterfaceForDevice(deviceName, mControlIf)) {
		printf("MacCameraDevice::MacCameraDevice: getControlInterfaceForDevice failed\n"); // FIXME
		throw std::runtime_error("Unable to get Jabra devices");
	} 
	if (mControlIf == NULL) {
		throw std::runtime_error("Unable to get Jabra devices");
    }
}

MacCameraDevice::~MacCameraDevice()
{
}

bool MacCameraDevice::getJabraDevices(std::vector<std::string>& devPaths)
{

	if (!getAllDevices(devPaths)) {
		printf("MacCameraDevice::MacCameraDevice: getAllDevices failed\n"); // FIXME
		return false;
	} 

	return true;
}

static int convertPropertyTypeToPropertyId(PropertyType t) {
    switch (t) {
        case Brightness:
            return 0x02;
        case Contrast:
            return 0x03;
        case Saturation:
            return 0x07;
        case Sharpness:
            return 0x08;
        case WhiteBalance:    
            return 0x0A;
    }
};
static int convertPropertyTypeToUnitId(PropertyType t) {
    switch (t) {
        case Brightness:
        case Contrast:
        case Saturation:
        case Sharpness:
        case WhiteBalance:    
            return UVC_PROCESSING_UNIT_ID;
    }
};
static int getSizeFromPropertyType(PropertyType t) {
    switch (t) {
        case Brightness:
        case Contrast:
        case Saturation:
        case Sharpness:
        case WhiteBalance:    
            return 0x02;
    }
};

        
static bool getData(IOUSBInterfaceInterface190 * * mControlIf, 
        int requestType, int propertyId, int unitId, int length,
        long& value)
{
    value = 0;
    kern_return_t err;
    IOUSBDevRequest request;
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    request.bRequest = requestType;
    request.wValue = (propertyId << 8) & 0xFF00;
    request.wIndex = (unitId << 8) & 0xFF00; 
    request.wLenDone = 0;
    request.wLength = length;
    request.pData = &value;

    err = (*mControlIf)->ControlRequest( mControlIf, 0, &request );
    if ( err != kIOReturnSuccess )
    {
        printf("getData: Control request failed %d \n", err );
        return false;
    }

    return true;
}

bool MacCameraDevice::getProperty(PropertyType t, Property& prop)
{
    if (mControlIf == NULL) return false;

    int propertyId = convertPropertyTypeToPropertyId(t);
    int unitId = convertPropertyTypeToUnitId(t);
    int length = getSizeFromPropertyType(t); 
    printf("propertyId: %d\n", propertyId);
    printf("unitId: %d\n", unitId);
    printf("length: %d\n", length);

    long value;
    
    if (!getData(mControlIf, UVC_GET_CUR, propertyId, unitId, length, value)) {
        printf("MacCameraDevice:getProperty: get cur failed\n");
        return false;
    }
    //printf("value = %ld\n", value);

    long min;
    if (!getData(mControlIf, UVC_GET_MIN, propertyId, unitId, length, min)) {
        printf("MacCameraDevice:getProperty: get mix failed\n");
        return false;
    }
    //printf("min = %ld\n", min);
    
    long max;
    if (!getData(mControlIf, UVC_GET_MAX, propertyId, unitId, length, max)) {
        printf("MacCameraDevice:getProperty: get max failed\n");
        return false;
    }
    //printf("max = %ld\n", max);
    
    prop = Property((int)value, (int)min, (int)max);
    return true;
}

bool MacCameraDevice::setProperty(PropertyType p, int _value)
{

	if (mControlIf == NULL) return false;

    int propertyId = convertPropertyTypeToPropertyId(p);
    int unitId = convertPropertyTypeToUnitId(p); 
    int length = getSizeFromPropertyType(p); 

    //printf("propertyId: %d\n", propertyId);
    //printf("unitId: %d\n", unitId);
    //printf("length: %d\n", length);


    long value = _value;

	kern_return_t err;
	IOUSBDevRequest request;
	request.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
	request.bRequest = UVC_SET_CUR;
	request.wValue = (propertyId << 8) & 0xFF00;
	request.wIndex = (unitId << 8) & 0xFF00;
	request.wLength = length;
	request.pData = &value;

	err = (*mControlIf)->ControlRequest( mControlIf, 0, &request );
	if ( err != kIOReturnSuccess )
	{
		printf("setProperty: ControlRequest failed\n"); // LOGGER FIXME
		return false;
	}
    return true;
}

bool MacCameraDevice::sendCommand(CommandInfo& info)
{
    return false; // FIXME
}

static std::string getUSBStringDescriptor(IOUSBDeviceInterface182** usbDevice, UInt8 idx)
{
   if (!usbDevice) return "";

   UInt16 buffer[64];
   IOUSBDevRequest request;

   request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
   request.bRequest = kUSBRqGetDescriptor;
   request.wValue = (kUSBStringDesc << 8) | idx;
   request.wIndex = 0x409; // english
   request.wLength = sizeof( buffer );
   request.pData = buffer;

   kern_return_t err = (*usbDevice)->DeviceRequest( usbDevice, &request );
   if ( err != 0 )
   {
      return "";
   }

   char stringBuf[128];
   int count = ( request.wLenDone - 1 ) / 2;
   int i;
   for ( i = 0; i < count; i++ ) {
      stringBuf[i] = buffer[i+1];
   }
   stringBuf[i] = '\0';  

   return stringBuf;
}

static IOUSBInterfaceInterface190** getControlInterface(IOUSBDeviceInterface182 ** usbIf)
{
	IOUSBInterfaceInterface190 **controlInterface;
	
	io_iterator_t interfaceIterator;
	IOUSBFindInterfaceRequest interfaceRequest;
	interfaceRequest.bInterfaceClass = kUSBVideoInterfaceClass; //UVC_CONTROL_INTERFACE_CLASS; 
	interfaceRequest.bInterfaceSubClass = kUSBVideoControlSubClass; //UVC_CONTROL_INTERFACE_SUBCLASS; 
	interfaceRequest.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
	interfaceRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;
	
	IOReturn success = (*usbIf)->CreateInterfaceIterator( usbIf, &interfaceRequest, 
                                &interfaceIterator );
	if( success != kIOReturnSuccess ) {
		printf("getControlInterface: failed to create if iterator\n");
		return NULL;
	}
	
	io_service_t usbInterface = IOIteratorNext(interfaceIterator);
	if (!usbInterface) {
		printf("getControlInterface error: no inteface next found\n");
		return NULL;
	}

	//Create an intermediate plug-in
	SInt32 score;
	IOCFPlugInInterface **plugInInterface = NULL;
   //Note: there might be other constant values for kIOCFPlugInInterfaceID in the new Mojave OS...
	kern_return_t kr = IOCreatePlugInInterfaceForService( usbInterface, 
        kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score );

	//Release the usbInterface object after getting the plug-in
	IOObjectRelease(usbInterface);

	if( (kr != kIOReturnSuccess) || !plugInInterface ) {
		printf("getControlInterface Error: Unable to create a plug-in (%08x)\n", kr );
		return NULL;
	}

	//Now create the device interface for the interface
	HRESULT result = (*plugInInterface)->QueryInterface( plugInInterface, 
        CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *) &controlInterface );

	//No longer need the intermediate plug-in
	(*plugInInterface)->Release(plugInInterface);

	if( result || !controlInterface ) {
		printf("getControlInterface Error: Couldn’t create a device interface for the interface (%08x)", (int) result );
		return NULL;
	}

	return controlInterface;
}


bool MacCameraDevice::getAllDevices(std::vector<std::string> &allDevs)
{
   allDevs.clear();

   bool a = getAllDevicesForVendorID(allDevs, ALTIA_VENDOR_ID);
   bool b = getAllDevicesForVendorID(allDevs, GN_VENDOR_ID);
   printf("getAllDevices: found %d devices\n", allDevs.size());

   return a || b;

      
}

bool MacCameraDevice::getControlInterfaceForDevice(std::string devSn, 
    IOUSBInterfaceInterface190 ** &cIf)
{
   if (devSn == "") return false;
    cIf = NULL;

    io_iterator_t serviceIterator;
    io_service_t usbDevice ;       
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

    //get all vendor IDs
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), CFSTR("*"));

    //any PIDs
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), CFSTR("*"));

    //get the service iter
    kern_return_t rt =  IOServiceGetMatchingServices( kIOMasterPortDefault, matchingDict, &serviceIterator );
    if (rt != kIOReturnSuccess) {
        printf("getControlInterfaceFrDevice error: no device found\n");
        return false;
    }

    while ( (usbDevice = IOIteratorNext(serviceIterator)) ){
        //Get Device interface
        SInt32                        score;
        IOCFPlugInInterface**         plugin = NULL; 
        IOUSBDeviceInterface182**     deviceInterface = NULL;
        kern_return_t                 err;

        err = IOCreatePlugInInterfaceForService( usbDevice, 
                kIOUSBDeviceUserClientTypeID, 
                kIOCFPlugInInterfaceID, 
                &plugin, 
                &score ); 
        if( (kIOReturnSuccess != err) || !plugin ) {
            //printf("getAllDevices Error: IOCreatePlugInInterfaceForService returned 0x%08x.\n", err );
            if (plugin!=NULL) IODestroyPlugInInterface(plugin);
            continue; //skip and check next device
        }

        HRESULT	res = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID*) &deviceInterface );
        (*plugin)->Release(plugin);
        if( res || deviceInterface == NULL ) {
            //printf( "getAllDevices Error: QueryInterface returned %d.\n", (int)res );
            if (plugin!=NULL) IODestroyPlugInInterface(plugin);
            continue; //skip and check next device
        }

        UInt16 vid;
        IOReturn ret = (*deviceInterface)->GetDeviceVendor(deviceInterface, &vid);
        if( ret != kIOReturnSuccess || (vid != ALTIA_VENDOR_ID && vid != GN_VENDOR_ID)) {
            IODestroyPlugInInterface(plugin);
            continue;
        }

        //this is a PanaCast device, get its serial number:
        UInt8 snIdx;
        ret = (*deviceInterface)->USBGetSerialNumberStringIndex( deviceInterface, &snIdx);
        if (ret != kIOReturnSuccess) {
            printf("getControlInterfaceForDevice error: failed to get serial number idx\n");
            IODestroyPlugInInterface(plugin);
            continue;
        }

        std::string sn = getUSBStringDescriptor(deviceInterface, snIdx);


        if (devSn == sn) { //user wants to return the control interface for particular serial number
            cIf = getControlInterface(deviceInterface);		
            IODestroyPlugInInterface(plugin);
            return true;
        }

        IODestroyPlugInInterface(plugin);
    } 

    if (cIf == NULL) {
        return false;
    }
    return true;
}


bool MacCameraDevice::getAllDevicesForVendorID( 
    std::vector<std::string> &allDevs, 
    long usbVendor)
{

    io_iterator_t serviceIterator;
    io_service_t usbDevice ;       
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

    //set our vendor ID
    CFNumberRef numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), numberRef);
    CFRelease(numberRef);

    //any PIDs
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), CFSTR("*"));

    //get the service iter
    kern_return_t rt =  IOServiceGetMatchingServices( kIOMasterPortDefault, matchingDict, &serviceIterator );
    if (rt != kIOReturnSuccess) {
        printf("getAllDevicesForVendorID 0x%x error: no device found\n", usbVendor);
        return false;
    }

    while ( (usbDevice = IOIteratorNext(serviceIterator)) ){
        //Get Device interface
        SInt32                        score;
        IOCFPlugInInterface**         plugin = NULL; 
        IOUSBDeviceInterface182**     deviceInterface = NULL;
        kern_return_t                 err;

        err = IOCreatePlugInInterfaceForService( usbDevice, 
                kIOUSBDeviceUserClientTypeID, 
                kIOCFPlugInInterfaceID, 
                &plugin, 
                &score ); 
        if( (kIOReturnSuccess != err) || !plugin ) {
            //printf("getAllDevices Error: IOCreatePlugInInterfaceForService returned 0x%08x.\n", err );
            if (plugin!=NULL) IODestroyPlugInInterface(plugin);
            continue; //skip and check next device
        }

        HRESULT	res = (*plugin)->QueryInterface(plugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID*) &deviceInterface );
        (*plugin)->Release(plugin);
        if( res || deviceInterface == NULL ) {
            //printf( "getAllDevices Error: QueryInterface returned %d.\n", (int)res );
            if (plugin!=NULL) IODestroyPlugInInterface(plugin);
            continue; //skip and check next device
        }

        UInt16 vid;
        IOReturn ret = (*deviceInterface)->GetDeviceVendor(deviceInterface, &vid);
        if( ret != kIOReturnSuccess || vid != usbVendor) {
            IODestroyPlugInInterface(plugin);
            continue;
        }

        //this is a PanaCast device, get its serial number:
        UInt8 snIdx;
        ret = (*deviceInterface)->USBGetSerialNumberStringIndex( deviceInterface, &snIdx);
        if (ret != kIOReturnSuccess) {
            printf("getAllDevicesForVendorID 0x%x error: failed to get serial number idx\n", usbVendor);
            IODestroyPlugInInterface(plugin);
            continue;
        }

        std::string sn = getUSBStringDescriptor(deviceInterface, snIdx);


        allDevs.push_back(sn);
        IODestroyPlugInInterface(plugin);
    } 

    if (allDevs.size() == 0) {
       printf("getAllDevicesForVendorID 0x%x: no devices\n", usbVendor);
        return false;
    }
    return true;
}

#endif
