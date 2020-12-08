#ifdef __APPLE__
#include "CameraDevice.h"
#include <stdexcept>

#define UVC_GET_CUR 0x81
#define UVC_SET_CUR 0x01
#define PANACAST_VENDOR_ID 0x2b93

MacCameraDevice::MacCameraDevice(const DeviceProperty& prop)
{
	mControlIf = NULL;
    mProperty = prop;

	std::vector<std::string> dummy;
	if (!getAllDevices(prop.deviceName, dummy, mControlIf)) {
		//logger("MacCameraDevice::MacCameraDevice: getAllDevices failed\n"); // FIXME
		throw std::runtime_error("Unable to get Jabra devices");
	} 
	if (mControlIf == NULL) {
		throw std::runtime_error("Unable to get Jabra devices");
    }
}

MacCameraDevice::~MacCameraDevice()
{
}

bool MacCameraDevice::getJabraDevices(std::vector<DeviceProperty>& devPaths)
{
    printf("In getJabraDevices\n");
    IOUSBInterfaceInterface190 * * tmp = NULL;

	std::vector<std::string> dummy;
	if (!getAllDevices("", dummy, tmp)) {
		//logger("MacCameraDevice::MacCameraDevice: getAllDevices failed\n"); // FIXME
		return false;
	} 

    devPaths.resize(dummy.size());
    for (unsigned k=0;k<devPaths.size();k++) {
        devPaths[k].deviceName = dummy[k];
    }
	return true;
}

static unsigned int convertPropertyTypeToPropertyId(PropertyType t) {
    return 0; // FIXME    
};
static unsigned int convertPropertyTypeToUnitId(PropertyType t) {
    return 0; // FIXME    
};
static unsigned int getSizeFromPropertyType(PropertyType t) {
    return 0; // FIXME    
};

bool MacCameraDevice::getProperty(PropertyType t, Property& prop)
{
    if (mControlIf == NULL) return false;

    unsigned int propertyId = convertPropertyTypeToPropertyId(t); // FIXME
    unsigned int unitId = convertPropertyTypeToUnitId(t); // FIXME
    unsigned int length = getSizeFromPropertyType(t); // FIXME

    long value;

    kern_return_t err;
    IOUSBDevRequest request;
    request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface);
    request.bRequest = UVC_GET_CUR;
    request.wValue = (propertyId << 8) & 0xFF00;
    request.wIndex = (unitId << 8) & 0xFF00; // FIXME
    request.wLength = length;
    request.pData = &value;

    err = (*mControlIf)->ControlRequest( mControlIf, 0, &request );
    if ( err != kIOReturnSuccess )
    {
        //logger("getProperty: ControlRequest failed\n"); // FIXME
        return false;
    }
    
    prop = Property(value, 0, 0); // FIXME min, max?
    return true;
}

bool MacCameraDevice::setProperty(PropertyType p, int _value)
{

	if (mControlIf == NULL) return false;

    unsigned int propertyId = convertPropertyTypeToPropertyId(p); // FIXME
    unsigned int unitId = convertPropertyTypeToUnitId(p); // FIXME
    unsigned int length = getSizeFromPropertyType(p); // FIXME

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


bool MacCameraDevice::getAllDevices(std::string devSn, 
    std::vector<std::string> &allDevs, 
    IOUSBInterfaceInterface190 ** &cIf)
{
    allDevs.clear();
    cIf = NULL;

    io_iterator_t serviceIterator;
    io_service_t usbDevice ;       
    CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

    //set our vendor ID
    long usbVendor = PANACAST_VENDOR_ID;
    CFNumberRef numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), numberRef);
    CFRelease(numberRef);

    //any PIDs
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), CFSTR("*"));

    //get the service iter
    kern_return_t rt =  IOServiceGetMatchingServices( kIOMasterPortDefault, matchingDict, &serviceIterator );
    if (rt != kIOReturnSuccess) {
        printf("getAllDevices error: no device found\n");
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
        if( ret != kIOReturnSuccess || vid != PANACAST_VENDOR_ID) {
            IODestroyPlugInInterface(plugin);
            continue;
        }

        //this is a PanaCast device, get its serial number:
        UInt8 snIdx;
        ret = (*deviceInterface)->USBGetSerialNumberStringIndex( deviceInterface, &snIdx);
        if (ret != kIOReturnSuccess) {
            printf("getAllDevices error: failed to get serial number idx\n");
            IODestroyPlugInInterface(plugin);
            continue;
        }

        std::string sn = getUSBStringDescriptor(deviceInterface, snIdx);


        if (devSn != "" && devSn == sn) { //user wants to return the control interface for particular serial number
            cIf = getControlInterface(deviceInterface);		
            IODestroyPlugInInterface(plugin);
            return true;
        }

        if (devSn == "") { //user wants to return all PanaCast's serial number
            allDevs.push_back(sn);
        }
        IODestroyPlugInInterface(plugin);
    } 

    if (devSn == "" && allDevs.size() == 0) {
        return false;
    }
    if (devSn != "" && cIf == NULL) {
        return false;
    }
    return true;
}

#if 0
int main(int argc, char ** argv)
{
    CameraQueryInterface q;

    std::vector<DeviceProperty> devPaths;
    if (!q.getAllJabraDevices(devPaths)) {
        printf("hurr\n");
        return -1;
    }

    for (unsigned k=0;k<devPaths.size();k++) {
        printf("%d: %s\n", k, devPaths[k].deviceName.c_str());
    }

    return 0;

}
#endif
#endif
