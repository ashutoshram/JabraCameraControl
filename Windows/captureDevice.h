#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include "VideoCaptureFormat.h"
#pragma once

class captureDevice
{
public:
	captureDevice(IMFActivate * device, UINT32 idx);
	captureDevice(IMFActivate * device, UINT32 idx, bool activateOnlyPanacast = false);
	~captureDevice();
	static bool FormatFromGuid(const GUID& guid,
		VideoPixelFormat* format);
	VideoCaptureFormat* getClosestMatch(VideoCaptureParams& param,
		float& score, bool exactPixelFormatMatch = true);
	std::string getId() {
		return id_;
	}
	std::string getName() {
		return name_;
	}
	std::string getVid() {
		return vid_;
	}
	std::string getPid() {
		return pid_;
	}
	VideoCaptureFormats getFormats() {
		return formats_;
	}
	bool isDeviceAttached()
	{
		return isObjectAttached_;
	}
	IMFMediaSource * getCaptureSource();
	void shutdown();

private:
	std::string name_;
	std::string id_;
	std::string vid_;
	std::string pid_;
	UINT32 idx_;
	VideoCaptureFormats formats_;
	IMFActivate * device_;
	bool isObjectAttached_;

	bool getCaptureFormats(IMFMediaSource *pSource);
	bool activateDevice(IMFActivate * device, UINT32 idx, bool activateOnlyPanacast = false);
};

