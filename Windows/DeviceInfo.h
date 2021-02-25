#pragma once
#include <Windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <winusb.h>
#include <usb.h>
#include <SetupAPI.h>
#include <vector>
#include "common.h"

// Device Interface GUID.
// Used by all WinUsb devices that this application talks to.
// Must match "DeviceInterfaceGUIDs" registry value specified in the INF file.
//
DEFINE_GUID(GUID_DEVINTERFACE_AltiaE6I2cRead,
	0x6131bc65, 0x3ae8, 0x4abb, 0xa0, 0x27, 0x72, 0x97, 0x7a, 0x0c, 0x9e, 0xfc);


typedef struct _DEVICE_DATA {
	WINUSB_INTERFACE_HANDLE WinusbHandle;
	HANDLE                  DeviceHandle;
} DEVICE_DATA, *PDEVICE_DATA;

class DeviceEventCallback {
public:
	virtual void onDeviceEvent(BYTE * buf, size_t validBytes) = 0;
};

struct monitorInfo {
public:
	monitorInfo(HANDLE _deviceHandle, DeviceEventCallback * _cb) {
		monitorThread = INVALID_HANDLE_VALUE;
		cb = _cb;
		deviceHandle = _deviceHandle;
		stop = false;
	}
	HANDLE monitorThread;
	DeviceEventCallback * cb;
	HANDLE deviceHandle;
	volatile bool stop;
};


class DeviceInfo
{
public:
	DeviceInfo();
	~DeviceInfo();

	/*
      Get the first PanaCast2 camera's serial number and usb bus type it is connected on
      If deviceId != "", then the given deviceId is used instead of the first found device
	*/
	static int getPanaCast2SerialNumber(std::string deviceId, std::string& serial, std::string& usbBusType);
	static bool getPanaCast2SerialNumberUsingSetupAPI(std::string& deviceSerialNumber);
	/*
      Returns the number of PanaCast2 cameras connected to the system
	*/
	static int getPanaCast2DevCount();
	/*
      Returns the first Hid device with the specified vid and pid
	*/
	static int getFirstHidDeviceWithVidPid(std::string vid, std::string pid, std::string& devicePath);
	static int getPanaCastDevicePathAndVidPid(std::string& devicePath, std::string &scannedVID, std::string &scannedPID);
	static bool isPanaCastDevice(std::string vid, std::string pid);

	static std::string toLower(const std::string& s);
	static bool getVidPidFromDevicePath(std::string deviceId, std::string& vendor_id, std::string& product_id);
	static monitorInfo * monitorHidDevice(std::string devicePath, DeviceEventCallback * cb);
	static void stopMonitoringHid(monitorInfo *& m);
	static bool isSystemLoaded(std::string path, bool &supported);
		
private:
	/*++
	Retrieve the device path that can be used to open the WinUSB-based device.
	Arguments:
	DevicePath - On successful return, a vector of device paths
	Return value: S_OK if successful, else error code
	--*/
	static HRESULT getDevPaths(const GUID guid, std::vector<std::string>& devPaths);
   
	static HRESULT getPanaCast2DevPaths( std::vector<std::string>& devPaths );
	/*++
	Open all needed handles to interact with the device.
	Arguments:
	DeviceData - Struct filled in by this function. The caller should use the
	WinusbHandle to interact with the device, and must pass the struct to
	CloseDevice when finished.
	Return value: HRESULT
	--*/
	static HRESULT openPanaCast2(PDEVICE_DATA DeviceData, std::string devPath);

	/*++
	Perform required cleanup when the device is no longer needed.
	Arguments:
	DeviceData - Struct filled in by OpenDevice
	Return value:
	None
	--*/
	static bool sendUsbControlTransfer(PDEVICE_DATA pDeviceData, unsigned char request_type, unsigned char b_request, unsigned short w_value, unsigned short w_index, unsigned char *data, unsigned short data_size);
	static VOID closePanaCast2( _Inout_ PDEVICE_DATA DeviceData );

	static BOOL getStringFromIndex(WINUSB_INTERFACE_HANDLE hDeviceHandle,
		UCHAR wIndex, UCHAR *data, UINT16 dataLength);
	static int getUniqueString(std::string devPath, std::string& unique);
	static bool getStringAfter(std::string deviceId, std::string type, size_t len, std::string& returnId);

};

