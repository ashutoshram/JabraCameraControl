#include "DeviceInfo.h"
#include "common.h"
#include <vector>
#include <sstream>
#include <Hidsdi.h>
#include <locale>
#include <SetupAPI.h>
#include <Cfgmgr32.h>
#include <string>
#include <algorithm>
#include "panacastdevices.h"

#pragma comment(lib,"SetupAPI")
#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "Hid.lib")

#define EXPECTED_SERIAL_NUMBER_LEN 12

#define TOKEN_PART_VID	0
#define TOKEN_PART_PID	1
#define TOKEN_PART_MI	2



DeviceInfo::DeviceInfo()
{
}


DeviceInfo::~DeviceInfo()
{
}



HRESULT DeviceInfo::getDevPaths(const GUID guid, std::vector<std::string>& devPaths)
{
	BOOL                             bResult = FALSE;
	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         interfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
	ULONG                            length;
	ULONG                            requiredLength = 0;
	HRESULT                          hr;

	BOOL deviceFound = FALSE;
	devPaths.clear();

	DBG(D_ERR, "getDevPaths : +");

	// Enumerate all devices exposing the interface
	deviceInfo = SetupDiGetClassDevs(&guid,
		NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (deviceInfo == INVALID_HANDLE_VALUE) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		DBG(D_ERR, "getDevPaths : Failed to get SetupDiGetClassDevs %x", hr);
		DBG(D_ERR, "getDevPaths : -");

		return hr;
	}

	interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	int idx = 0;
	do {
		bResult = SetupDiEnumDeviceInterfaces(deviceInfo,
			NULL,
			&guid,
			idx++,
			&interfaceData);

		if (FALSE == bResult) {
			// We would see this error if this is the last device
			DWORD lastError = GetLastError();
			SetupDiDestroyDeviceInfoList(deviceInfo);
			if (ERROR_NO_MORE_ITEMS == lastError) 
			{
				DBG(D_ERR, "getDevPaths : SetupDiEnumDeviceInterfacesInfoList No more items %x", lastError);
				DBG(D_ERR, "getDevPaths : -");
				if (deviceFound == FALSE) 
					return -1L;

				return S_OK;
			}

			hr = HRESULT_FROM_WIN32(lastError);
			DBG(D_ERR, "getDevPaths : Failed to get SetupDiEnumDeviceInterfaces %x", hr);
			DBG(D_ERR, "getDevPaths : -");

			return hr;
		}

		// Get the size of the path string
		// We expect to get a failure with insufficient buffer
		bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&interfaceData,
			NULL,
			0,
			&requiredLength,
			NULL);

		if (FALSE == bResult && ERROR_INSUFFICIENT_BUFFER != GetLastError()) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			SetupDiDestroyDeviceInfoList(deviceInfo);
			DBG(D_ERR, "getDevPaths : Failed to get SetupDiGetDeviceInterfaceDetail %x", hr);
			return hr;
		}

		// Allocate temporary space for SetupDi structure
		detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LMEM_FIXED, requiredLength);

		if (NULL == detailData) {
			hr = E_OUTOFMEMORY;
			SetupDiDestroyDeviceInfoList(deviceInfo);
			DBG(D_ERR, "getDevPaths : Memory Out %x", hr);

			DBG(D_ERR, "getDevPaths : -");

			return hr;
		}

		detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		length = requiredLength;

		// Get the interface's path string
		bResult = SetupDiGetDeviceInterfaceDetail(deviceInfo,
			&interfaceData,
			detailData,
			length,
			&requiredLength,
			NULL);

		if (FALSE == bResult) {
			hr = HRESULT_FROM_WIN32(GetLastError());
			LocalFree(detailData);
			SetupDiDestroyDeviceInfoList(deviceInfo);
			DBG(D_ERR, "getDevPaths : Failed to get SetupDiGetDeviceInterfaceDetail %x", hr);
			DBG(D_ERR, "getDevPaths : -");

			return hr;
		}

		// Give path to the caller. SetupDiGetDeviceInterfaceDetail ensured
		// DevicePath is NULL-terminated.
		//hr = StringCbCopy(pDevicePath, BufLen, detailData->DevicePath);
		std::wstring devPath = std::wstring(detailData->DevicePath);
		devPaths.push_back(SysWideToUTF8(devPath));
		DBG(D_ERR, "DeviceInfo::getDevPaths: %d: DevicePath = %s\n", idx, SysWideToUTF8(devPath).c_str());
		LocalFree(detailData);
		deviceFound = TRUE;
	} while (TRUE);


	DBG(D_ERR, "getDevPaths : -");
	return -1L; // to keep compiler happy - never returns from here
}

HRESULT DeviceInfo::getPanaCast2DevPaths(std::vector<std::string>& devPaths)
{
	return getDevPaths(GUID_DEVINTERFACE_AltiaE6I2cRead, devPaths);
}

std::string DeviceInfo::toLower(const std::string& s)
{
	std::string result;
	std::locale loc;

	for (unsigned int i = 0; i < s.length(); ++i) {
		result += std::tolower(s.at(i), loc);
	}
	return result;
}

bool DeviceInfo::getStringAfter(std::string deviceId, std::string type, size_t len, std::string& returnId)
{
	std::string ltype = toLower(type);
	std::string ldevId = toLower(deviceId);
	int idx = (int)ldevId.find(ltype);
	if (idx == std::string::npos) {
		DBG(D_VERBOSE, "DeviceInfo:getStringAfter: no \"%s\" id in matching device %s\n", type.c_str(), deviceId.c_str());
		return false; // invalid matchingDevId
	}
	if (ldevId.length() < (idx + type.length() + len)) {
		DBG(D_VERBOSE, "DeviceInfo:getStringAfter: no \"%s\" id in matching device %s\n", type.c_str(), deviceId.c_str());
		return false; // invalid matchingDevId
	}
	returnId = ldevId.substr(idx + type.length(), len);
	return true;
}

#define VIDPID_LEN 4
int DeviceInfo::getFirstHidDeviceWithVidPid(std::string vid, std::string pid, std::string& devicePath)
{
	std::vector<std::string> devPaths;
	GUID hidGuid;
	HidD_GetHidGuid(&hidGuid);
	if (getDevPaths(hidGuid, devPaths) < 0) return -1;
	for (size_t k = 0; k < devPaths.size(); k++) {
		std::string _vid, _pid;
		if (!getStringAfter(devPaths[k], "vid_", VIDPID_LEN, _vid)) continue;
		if (!getStringAfter(devPaths[k], "pid_", VIDPID_LEN, _pid)) continue;
		if (vid == _vid && pid == _pid) {
			devicePath = devPaths[k];
			return 0;
		}
	}
	return -1;
}

bool DeviceInfo::isPanaCastDevice(std::string vid, std::string pid)
{
	DBG(D_NORMAL, "DeviceInfo::isPanaCastDevice: checking vid %s, pid %s\n", vid, pid);
	if (((vid == ALTIA_VENDOR_ID) || (vid == JABRA_PYTHON_VID)) && ((pid == PANACAST_VIDEO_ONLY_PID) || (pid == PANACAST_AUDIO_VIDEO_PID)
		|| (pid == PANACAST_VIDEO_ONLY_PID_2) || (pid == PANACAST_AUDIO_VIDEO_PID_2)
		|| (pid == PANACAST_VIDEO_ONLY_PID_3) || (pid == PANACAST_AUDIO_VIDEO_PID_3)
		|| (pid == PANACAST_P2S_PID) || (pid == PANACAST_MISSION_PID) || (pid == PANACAST_CANYON_PID)
		|| (pid == PANACAST_AUDIO_VIDEO_PID_4)
		|| (pid == JABRA_PYTHON_PID_1)
		|| (pid == JABRA_PYTHON_PID_2)))
		return true;
	return false;
}

int DeviceInfo::getPanaCastDevicePathAndVidPid(std::string& devicePath, std::string &scannedVID, std::string &scannedPID)
{
	std::vector<std::string> devPaths;
	if (getPanaCast2DevPaths(devPaths) != 0)
	{
		DBG(D_ERR, "getPanaCastDevicePathAndVidPid : Could not get panacase2devpaths");
		return -1;
	}

	for (size_t k = 0; k < devPaths.size(); k++)
	{
		std::string _vid = "", _pid = "";
		if (!getStringAfter(devPaths[k], "vid_", VIDPID_LEN, _vid)) 
			continue;
		if (!getStringAfter(devPaths[k], "pid_", VIDPID_LEN, _pid)) 
			continue;
		//DBG(D_ERR, "getPanaCastDevicePathAndVidPid : Vendor ID %s, Prodct ID %s", _vid, _pid);
		if (isPanaCastDevice(_vid, _pid))
		{
			scannedVID = _vid;
			scannedPID = _pid;
			devicePath = devPaths[k];

			return 0;
		}
	}

	return -1;
}

bool DeviceInfo::getVidPidFromDevicePath(std::string deviceId, std::string& vendor_id, std::string& product_id)
{
	if (!getStringAfter(deviceId, "vid_", VIDPID_LEN, vendor_id)) return false;
	if (!getStringAfter(deviceId, "pid_", VIDPID_LEN, product_id)) return false;
	return true;
}



static void *monitorThreadFunc(void*  param)   {
	monitorInfo * m = (monitorInfo *)param;

	OVERLAPPED o;
	memset(&o, 0, sizeof(o));
	o.hEvent = CreateEvent(NULL, false, false, NULL);
	if (o.hEvent == INVALID_HANDLE_VALUE) return NULL;
	o.Offset = 0;
	o.OffsetHigh = 0;
	unsigned buf_size = 1024;
	BYTE * buf = (BYTE *)malloc(buf_size);

	while (!m->stop) {
		memset(buf, 0, buf_size);
		DWORD bytesRead;
		int i = ReadFile(m->deviceHandle,
			buf,
			buf_size,
			NULL,
			&o);
		if (GetLastError() == ERROR_IO_PENDING) {
			DWORD dw = WaitForSingleObject(o.hEvent, 500);
			if (dw == WAIT_OBJECT_0) {
				if (GetOverlappedResult(m->deviceHandle, &o, &bytesRead, TRUE) != 0) {
					if (bytesRead != 0) {
						m->cb->onDeviceEvent(buf, bytesRead);
					}
				}
			}
			else if (dw == WAIT_TIMEOUT) {
				continue;
			}
		}
	}
	CancelIo(m->deviceHandle);
	CloseHandle(o.hEvent);
	DBG(D_NORMAL, "DeviceInfo::monitorThreadFunc: monitor thread done: m->stop = %d\n", m->stop);
   
	return NULL;
}

monitorInfo * DeviceInfo::monitorHidDevice(std::string devicePath, DeviceEventCallback * cb)
{
	std::wstring wdevPath = SysUTF8ToWide(devicePath);
	HANDLE deviceHandle = CreateFile(wdevPath.c_str(),
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);
	if (deviceHandle == INVALID_HANDLE_VALUE) return NULL;

	monitorInfo * m = new monitorInfo(deviceHandle, cb);
	HANDLE monitorThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&(monitorThreadFunc), (void *)m, 0, NULL);
	if ((monitorThread == NULL) || (monitorThread == INVALID_HANDLE_VALUE)) {
		delete m;
		return NULL;
	}
	m->monitorThread = monitorThread;
	return m;
}

void DeviceInfo::stopMonitoringHid(monitorInfo*& m)
{
	if (m == NULL) return;
	if (m->stop == true) return;
	m->stop = true;
	DBG(D_NORMAL, "DeviceInfo::stopMonitoringHid: waiting for monitor thread to be done\n");
	WaitForSingleObject(m->monitorThread, INFINITE);
	m->monitorThread = INVALID_HANDLE_VALUE;
	CloseHandle(m->deviceHandle);
	m->stop = false;
	delete m;
	m = NULL;
}

HRESULT 
DeviceInfo::openPanaCast2( PDEVICE_DATA pDeviceData , std::string devPath)
{
	HRESULT hr = S_OK;
	BOOL    bResult;

	pDeviceData->DeviceHandle = INVALID_HANDLE_VALUE;
	pDeviceData->WinusbHandle = INVALID_HANDLE_VALUE;

	std::wstring wdevPath = SysUTF8ToWide(devPath);
	pDeviceData->DeviceHandle = CreateFile(wdevPath.c_str(),
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	if (INVALID_HANDLE_VALUE == pDeviceData->DeviceHandle) {
		hr = HRESULT_FROM_WIN32(GetLastError());
        DBG(D_ERR, "DeviceInfo::openPanaCast2: CreateFile failed: HRESULT 0x%x\n", hr);
		return hr;
	}

	bResult = WinUsb_Initialize(pDeviceData->DeviceHandle,
		&pDeviceData->WinusbHandle);

	if (FALSE == bResult) {
        DBG(D_ERR, "DeviceInfo::openPanaCast2: WinUsb_Initialize failed: HRESULT 0x%x\n", hr);
		hr = HRESULT_FROM_WIN32(GetLastError());
		CloseHandle(pDeviceData->DeviceHandle);
		return hr;
	}

	return hr;
}

VOID
DeviceInfo::closePanaCast2(PDEVICE_DATA pDeviceData)
{
	if (INVALID_HANDLE_VALUE != pDeviceData->DeviceHandle) {
		CloseHandle(pDeviceData->DeviceHandle);
		pDeviceData->DeviceHandle = INVALID_HANDLE_VALUE;
	}

	if (INVALID_HANDLE_VALUE != pDeviceData->WinusbHandle) {
		WinUsb_Free(pDeviceData->WinusbHandle);
		pDeviceData->WinusbHandle = INVALID_HANDLE_VALUE;
	}

	return;
}


bool DeviceInfo::isSystemLoaded(std::string devId, bool &supported)
{
	supported = true;
	
	bool loaded = false;
	DEVICE_DATA           deviceData;
	std::vector<std::string> devPaths;
	HRESULT hr = getPanaCast2DevPaths(devPaths);

	if (FAILED(hr)) {
		DBG(D_ERR, "DeviceInfo::isSystemLoaded: getPanaCast2DevPaths failed\n");
		return false;
	}
	if (devPaths.size() < 1) {
		DBG(D_ERR, "DeviceInfo::isSystemLoaded: getPanaCast2DevPaths found no PanaCast2 devices\n");
		return false;
	}

	DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: found %d PanaCast2 devices\n", devPaths.size());
	for (size_t k = 0; k < devPaths.size(); k++) 
	{
		DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: devicePath[%d] = %s\n", k, devPaths[k].c_str());
	}

	std::string devPath;
	if (devId == "")
	{
		devPath = devPaths[0];
	}
	else {
		std::string uniqDev;
		if (getUniqueString(devId, uniqDev) < 0) 
			return false;
		DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: unique for input device %s = %s\n", devId.c_str(), uniqDev.c_str());
		bool found = false;
		for (size_t k = 0; k < devPaths.size(); k++) {
			std::string uniqCmp;
			if (getUniqueString(devPaths[k], uniqCmp) < 0) 
				return false;
			DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: unique for device %s = %s\n", devPaths[k].c_str(), uniqCmp.c_str());
			if (uniqDev == uniqCmp) {
				DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: found matching device %s\n", devPaths[k].c_str());
				devPath = devPaths[k];
				found = true;
				break;
			}
		}
		if (!found) {
			DBG(D_NORMAL, "DeviceInfo::isSystemLoaded: no matching device found for %s\n", devId.c_str());
			return false;
		}
	}

	if (FAILED(openPanaCast2(&deviceData, devPath))) 
	{
		DBG(D_ERR, "DeviceInfo::isSystemLoaded: failed opening device, HRESULT 0x%x\n", hr);
		return false;
	}

	unsigned char status;
	if (!sendUsbControlTransfer(&deviceData, 0xDF, 0xDF, 0, 0, &status, 1))
	{
		DBG(D_ERR, "DeviceInfo::isSystemLoaded failed - send control transfer failed is OK.Previous error ignored.\n");
		closePanaCast2(&deviceData);
		supported = false;
		return false;
	}
	if (status != 0x0)
	{
		DBG(D_ERR, "DeviceInfo::isSystemLoaded success.\n");
		loaded = true;
	}
	closePanaCast2(&deviceData);

	return loaded;
}


bool DeviceInfo::sendUsbControlTransfer(PDEVICE_DATA pDeviceData, unsigned char request_type, unsigned char b_request, unsigned short w_value, unsigned short w_index, unsigned char *data, unsigned short data_size) 
{
	if (!pDeviceData)
	{
		DBG(D_NORMAL, "DeviceInfo::sendUsbControlTransfer: invalid device data\n");
		return false;
	}

	if (!pDeviceData->DeviceHandle)
	{
		DBG(D_NORMAL, "DeviceInfo::sendUsbControlTransfer: no device opened yet\n");
		return false;
	}

	if (pDeviceData->WinusbHandle == INVALID_HANDLE_VALUE) 
	{
		DBG(D_NORMAL, "DeviceInfo::sendUsbControlTransfer: sendUsbControlTransfer failed - invalid Winusb handle\n");
		return false;
	}

	BOOL bResult = TRUE;

	WINUSB_SETUP_PACKET SetupPacket;
	ZeroMemory(&SetupPacket, sizeof(WINUSB_SETUP_PACKET));
	ULONG cbSent = 0;

	//Create the setup packet
	SetupPacket.RequestType = request_type;
	SetupPacket.Request = b_request;
	SetupPacket.Value = w_value;
	SetupPacket.Index = w_index;
	SetupPacket.Length = data_size;

	bResult = WinUsb_ControlTransfer(pDeviceData->WinusbHandle, SetupPacket, data, data_size, &cbSent, 0);
	if (!bResult) 
	{
		DBG(D_NORMAL, "DeviceInfo::sendUsbControlTransfer: sendUsbControlTransfer failed - WinUsb_ControlTransfer() failed at %d\n", GetLastError());
		return false;
	}
	return true;
}

BOOL DeviceInfo::getStringFromIndex(WINUSB_INTERFACE_HANDLE hDeviceHandle, 
	UCHAR wIndex, UCHAR *data, UINT16 dataLength)
{

	if (hDeviceHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	ULONG lengthReceived;
	UINT8 buf[MAXIMUM_USB_STRING_LENGTH];

	if (!WinUsb_GetDescriptor(hDeviceHandle, 
		USB_STRING_DESCRIPTOR_TYPE, 
		wIndex, 
		0x0409/*English*/, 
		(PBYTE)&buf, sizeof(buf), &lengthReceived)) {
		return FALSE;
	}

	//First byte is the size of the whole buffer, the second byte is the type
	UINT8 size = buf[0];
	if (size < 2 || size > MAXIMUM_USB_STRING_LENGTH) { //Invalid Size
		return FALSE;
	}

	//Remaining String has characters which are seperated by 0 after the first two bytes
	UINT8 * t = buf;
	t += 2;
	UINT8 len = size - 2;

	if ((len / 2) >= dataLength || dataLength == 0) { //Buffer not large enough to hold the String + null terminator
		return FALSE;
	}

	int j = 0;
	for (int i = 0; i < len; i += 2){ //skip the 0s between characters
		data[j] = t[i];
		j++;
	}
	data[min(j, dataLength - 1)] = 0; //null terminated

	return TRUE;
}

int DeviceInfo::getUniqueString(std::string devPath, std::string& unique)
{
	std::vector<std::string> split;
	std::istringstream iss(devPath);
	std::string s;
	while (getline(iss, s, '#')) {
		split.push_back(s);
	}
	if (split.size() < 3) return -1;
	std::vector<std::string> split1;
	std::istringstream iss1(split[2]);
	while (getline(iss1, s, '&')) {
		split1.push_back(s);
	}
	if (split1.size() < 3) {
		unique = split1[0] + split1[1];
		return 0;
	}
	unique = split1[0] + split1[1] + split1[2];
	return 0;
}

bool DeviceInfo::getPanaCast2SerialNumberUsingSetupAPI(std::string& deviceSerialNumber)
{
	HDEVINFO hDevInfo = NULL;

	// Create a HDEVINFO with all present or connected USB devices.
	hDevInfo = SetupDiGetClassDevs(NULL, TEXT("USB"), NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		deviceSerialNumber = "";
		return false;
	}

	SP_DEVINFO_DATA DeviceInfoData;

	memset(&DeviceInfoData, 0, sizeof(DeviceInfoData));
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	int tokenPart = 0;
	DWORD dwSize = 0, dwPropertyRegDataType = 0;
	TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN];
	TCHAR szDeviceDesc[1024];
	LPTSTR pszToken, pszNextToken;
	TCHAR szVid[MAX_DEVICE_ID_LEN], szPid[MAX_DEVICE_ID_LEN], szMi[MAX_DEVICE_ID_LEN], szSerialNumber[MAX_DEVICE_ID_LEN];
	static const LPCTSTR arPrefix[3] = { TEXT("VID_"), TEXT("PID_"), TEXT("MI_") };

	CONFIGRET retVal = CR_SUCCESS;
	// Find the ones that are driverless
	for (int i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData); i++)
	{
		retVal = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID, MAX_PATH, 0);
		if (retVal != CR_SUCCESS)
			continue;

		SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, &dwPropertyRegDataType, (BYTE*)szDeviceDesc,
			sizeof(szDeviceDesc), &dwSize);

		pszToken = _tcstok_s(szDeviceInstanceID, TEXT("\\#&"), &pszNextToken);
		szVid[0] = TEXT('\0');
		szPid[0] = TEXT('\0');
		szMi[0] = TEXT('\0');
		szSerialNumber[0] = TEXT('\0');
		while (pszToken != NULL)
		{
			for (tokenPart = 0; tokenPart < 3; tokenPart++)
			{
				if (_tcsncmp(pszToken, arPrefix[tokenPart], lstrlen(arPrefix[tokenPart])) == 0)
				{
					switch (tokenPart)
					{
					case TOKEN_PART_VID:
						_tcscpy_s(szVid, sizeof(szVid) / sizeof(TCHAR), pszToken);
						break;
					case TOKEN_PART_PID:
						_tcscpy_s(szPid, sizeof(szPid) / sizeof(TCHAR), pszToken);
						break;
					case TOKEN_PART_MI:
						_tcscpy_s(szMi, sizeof(szMi) / sizeof(TCHAR), pszToken);
						break;
					default:
						break;
					}
				}
				else
				{
					int tokenLength = lstrlen(pszToken);
					TCHAR *pAmpersandFound = _tcschr(pszToken, _T('&'));
					if ((tokenLength == EXPECTED_SERIAL_NUMBER_LEN) && (isdigit(pszToken[tokenLength - 1])) && (!pAmpersandFound) && ((_tcsncmp(pszToken, _T("P"), 1) == 0) || (_tcsncmp(pszToken, _T("Z"), 1) == 0)))
					{
						_tcscpy_s(szSerialNumber, sizeof(szSerialNumber) / sizeof(TCHAR), pszToken);
					}
				}
			}
			pszToken = _tcstok_s(NULL, TEXT("\\#&"), &pszNextToken);
		}

#ifdef _UNICODE		
		std::wstring vidLower = std::wstring(szVid);
		std::wstring pidLower = std::wstring(szPid);
#else
		std::string vidLower = std::string(szVid);
		std::string pidLower = std::string(szPid);
#endif

		std::transform(vidLower.begin(), vidLower.end(), vidLower.begin(), ::tolower);
		std::transform(pidLower.begin(), pidLower.end(), pidLower.begin(), ::tolower);
		bool matched = false;

		if (lstrcmpi(vidLower.c_str(), ALTIA_VENDOR_ID_TEXT) == 0)
		{
			matched = (lstrcmpi(pidLower.c_str(), PANACAST_VIDEO_ONLY_PID_TEXT) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_AUDIO_VIDEO_PID_TEXT) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_VIDEO_ONLY_PID_TEXT_2) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_AUDIO_VIDEO_PID_TEXT_2) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_VIDEO_ONLY_PID_TEXT_3) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_AUDIO_VIDEO_PID_TEXT_3) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_P2S_PID_TEXT) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_MISSION_PID_TEXT) == 0);
			if (!matched)
				matched = (lstrcmpi(pidLower.c_str(), PANACAST_CANYON_PID_TEXT) == 0);
		}

		if (matched)
		{
			int serialNoLength = lstrlen(szSerialNumber);
			TCHAR *pAmpersandFound = _tcschr(szSerialNumber, _T('&'));
			if ((serialNoLength == EXPECTED_SERIAL_NUMBER_LEN) && (isdigit(szSerialNumber[serialNoLength - 1])) && (!pAmpersandFound) && ((_tcsncmp(szSerialNumber, _T("P"), 1) == 0) || (_tcsncmp(szSerialNumber, _T("Z"), 1) == 0)))
			{
#ifdef _UNICODE
				std::string utfSerialNumber = SysWideToUTF8(szSerialNumber);
				deviceSerialNumber = utfSerialNumber;
#else
				deviceSerialNumber = szSerialNumber;
#endif
				SetupDiDestroyDeviceInfoList(hDevInfo);
				return true;
			}
		}

	}

	SetupDiDestroyDeviceInfoList(hDevInfo);

	deviceSerialNumber = "";
	return false;
}

int DeviceInfo::getPanaCast2SerialNumber(std::string devId, std::string& serial, std::string& usbDeviceType)
{
	BOOL                  bResult;
	USB_DEVICE_DESCRIPTOR deviceDesc;
	ULONG                 lengthReceived;
	DEVICE_DATA           deviceData;

	deviceData.DeviceHandle = INVALID_HANDLE_VALUE;
	deviceData.WinusbHandle = INVALID_HANDLE_VALUE;

	std::vector<std::string> devPaths;
	HRESULT hr = getPanaCast2DevPaths(devPaths);

	if (FAILED(hr)) {
		DBG(D_ERR, "DeviceInfo::getPanaCast2SerialNumber: getPanaCast2DevPaths failed\n");
		return -1;
	}
	if (devPaths.size() < 1) {
		DBG(D_ERR, "DeviceInfo::getPanaCast2SerialNumber: getPanaCast2DevPaths found no PanaCast2 devices\n");
		return -1;
	}

	DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: found %d PanaCast2 devices\n", devPaths.size());
	for (size_t k = 0; k < devPaths.size(); k++) {
		DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: devicePath[%d] = %s\n", k, devPaths[k].c_str());
	}

	std::string devPath;
	if (devId == "") 
	{
		devPath = devPaths[0];
	}
	else 
	{
		std::string uniqDev;
		if (getUniqueString(devId, uniqDev) < 0) return -1;
		DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: unique for input device %s = %s\n", devId.c_str(), uniqDev.c_str());
		bool found = 0;
		for (size_t k = 0; k < devPaths.size(); k++) {
			std::string uniqCmp;
			if (getUniqueString(devPaths[k], uniqCmp) < 0) return -1;
			DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: unique for device %s = %s\n", devPaths[k].c_str(), uniqCmp.c_str());
			if (uniqDev == uniqCmp) {
				DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: found matching device %s\n", devPaths[k].c_str());
				devPath = devPaths[k];
				found = 1;
				break;
			}
		}
		if (!found) {
			DBG(D_NORMAL, "DeviceInfo::getPanaCast2SerialNumber: no matching device found for %s\n", devId.c_str());
			return -1;
		}
	}


    hr = openPanaCast2(&deviceData, devPath);
	if (FAILED(hr)) {
		DBG(D_ERR, "DeviceInfo::getPanaCast2SerialNumber: failed opening device, HRESULT 0x%x\n", hr);
		return -1;
	}

	// Get device descriptor
	bResult = WinUsb_GetDescriptor(deviceData.WinusbHandle,
		USB_DEVICE_DESCRIPTOR_TYPE,
		0,
		0,
		(PBYTE)&deviceDesc,
		sizeof(deviceDesc),
		&lengthReceived);

	if (FALSE == bResult || lengthReceived != sizeof(deviceDesc)) {
		DBG(D_ERR, "DeviceInfo::getPanaCast2SerialNumber: Error WinUsb_GetDescriptor %d or lengthReceived %d\n",
			FALSE == bResult ? GetLastError() : 0,
			lengthReceived);
		closePanaCast2(&deviceData);
		return -1;
	}

	// Print a few parts of the device descriptor
	// Print Vendor ID, Product ID, USB,

	UCHAR camID[MAXIMUM_USB_STRING_LENGTH + 1];
	if (!getStringFromIndex(deviceData.WinusbHandle, deviceDesc.iSerialNumber, camID, sizeof(camID))) 
	{
		DBG(D_ERR, "DeviceInfo::getPanaCast2SerialNumber: Failed to get Camera ID\n");
		closePanaCast2(&deviceData);
		return -1;
	}

	serial = (char*)camID;
	usbDeviceType = (deviceDesc.bcdUSB & 0x0300) == 0x0300 ? "3.0" : "2.0";

	closePanaCast2(&deviceData);
	return 0;
}

   
int DeviceInfo::getPanaCast2DevCount()
{
	std::vector<std::string> devPaths;
	getPanaCast2DevPaths(devPaths); // ignore return code
	return int(devPaths.size());
}
