#pragma once
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <functional>

#include "VideoCaptureFormat.h"
#include "captureDevice.h"
#include "msdk_utils.h"
#include "ColorControlDefines.h"

struct WebcamFrame{

	WebcamFrame(): data(NULL), allocatedLength(0) {}
	void allocate(unsigned cursize) {
		if (data == NULL || allocatedLength < cursize) {
			data = (unsigned char *)realloc(data, cursize);
			allocatedLength = cursize;
		}

	}
	~WebcamFrame() {
		if (data != NULL && allocatedLength != 0) {
			free(data);
		}
	}

	unsigned char * data;
	unsigned length;
	VideoCaptureFormat format;
	unsigned __int64 timestamp;
	unsigned allocatedLength;
};


class MFReaderCallback;
class WebcamSource
{
public:
	static WebcamSource * getInstance();
	static bool m_Initialized;

	static void Initialize();
	static void UnInitialize();

	bool openAndRunDeviceWithId(std::string deviceId, VideoCaptureParams& param);
	void stopAndDeallocate();
	~WebcamSource();

	void  setFrameHandler(std::function<void(void*)> callback) {
		frame_callback_ = callback;
	}
	void setErrorHandler(std::function<void(long)> callback) {
		error_callback_ = callback;
	}
	void OnIncomingCapturedData(const unsigned char* data,
		int length,
		VideoCaptureFormat format,
		unsigned __int64 timestamp,
		HRESULT onReadSampleResult);

	std::vector<captureDevice> enumerateDevices() {
		return devices_;
	}

	std::string getCurrentDeviceName() {
		if (activeDevice_) return activeDevice_->getName();
		return "";
	}
	std::string getCurrentDeviceId() {
		if (activeDevice_) return activeDevice_->getId();
		return "";
	}

	UINT32 rescan() {
		return buildDeviceList(false);
	}

	void enableDeviceLocking() { lockDevice_ = true; }
	void disbleDeviceLocking() { lockDevice_ = false; }

	// works only in synchronous mode
	bool getSample(WebcamFrame& frame);

private:
	WebcamSource();
	UINT32 buildDeviceList(bool onlyPanacastDevice);
	UINT32 deviceCount_;
	std::vector<captureDevice> devices_;
	captureDevice* getCaptureDeviceWithCapability(VideoCaptureParams& params,
		VideoCaptureFormat& match);
	bool getCaptureSource(captureDevice * cd, VideoCaptureFormat& match);
	bool openAndLockDevice(captureDevice * cd, VideoCaptureFormat& match);
	bool runLockedDevice();
	bool startReadsWithLockedDevice();
	bool deviceIdsMatch(std::string deviceId1, std::string deviceId2);

	IMFSourceReader * pReader_;
	volatile bool capturing_;
	volatile bool locked_;
	std::function<void(void*)> frame_callback_;
	IMFMediaSource * pSource_;
	captureDevice *activeDevice_;
	VideoCaptureFormat matchedFormat_;
	MFReaderCallback * mReaderCallback_;
	CRITICAL_SECTION lock_;
	std::function<void(long)> error_callback_;
	bool lockDevice_;
	bool synchronousCapture_;

	//disable copy constructor and assignment operator
	WebcamSource(const WebcamSource&);
	void operator=(const WebcamSource&);

};

