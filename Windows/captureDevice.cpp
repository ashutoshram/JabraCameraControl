#include <strmif.h>
#include "captureDevice.h"
#include "VideoCaptureFormat.h"
#include "DeviceInfo.h"
#include "common.h"


captureDevice::captureDevice(IMFActivate * device, UINT32 idx)
{
	activateDevice(device, idx);
}

captureDevice::captureDevice(IMFActivate * device, UINT32 idx, bool activateOnlyPanacast)
{
	activateDevice(device, idx, activateOnlyPanacast);
}

captureDevice::~captureDevice()
{
	shutdown();
}

bool captureDevice::activateDevice(IMFActivate * device, UINT32 idx, bool activateOnlyPanacast)
{
	isObjectAttached_ = false;
	idx_ = idx;
	name_ = "";
	id_ = "";
	vid_ = "";
	pid_ = "";
	if (device) {
		IMFMediaSource *pSource = NULL;
		std::string vid;
		std::string pid;

		device_ = device;

		UINT32 name_size;
		wchar_t * name = NULL;
		HRESULT hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size);
		if (!SUCCEEDED(hr))
			goto failed;

		UINT32 id_size;
		wchar_t * id = NULL;
		hr = device->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id, &id_size);
		if (!SUCCEEDED(hr))
			goto failed;

		name_ = SysWideToUTF8(std::wstring(name, name_size));
		id_ = SysWideToUTF8(std::wstring(id, id_size));
		if (!DeviceInfo::getVidPidFromDevicePath(id_, vid, pid))
		{
			DBG(D_NORMAL, "captureDevice::captureDevice: ignoring device %s, id %s\n", name_.c_str(), id_.c_str());
			goto failed;
		}
		vid_ = vid;
		pid_ = pid;

		if (activateOnlyPanacast)
		{
			if (!DeviceInfo::isPanaCastDevice(vid, pid))
			{
				printf("Altia vendor id %s with invalid pid %s\n", vid.c_str(), pid.c_str());
				goto failed;
			}
		}
		
		id_ = SysWideToUTF8(std::wstring(id, id_size));

		hr = device->ActivateObject( __uuidof(IMFMediaSource), (void**)&pSource);
		DBG(D_NORMAL, "captureDevice::captureDevice: working with device %s, id %s\n", name_.c_str(), id_.c_str());
		if (!SUCCEEDED(hr))
			goto failed;

		isObjectAttached_ = true;
		if (!getCaptureFormats(pSource))
		{
			//device->DetachObject();
			goto failed;
		}
		return true;

	failed:
		if (pSource)
		{
			device->ShutdownObject();
			isObjectAttached_ = false;
		}
		if (name)
			CoTaskMemFree(name);
		if (id)
			CoTaskMemFree(id);
		SafeRelease(&pSource);
		return false;
	}

	return false;

}

IMFMediaSource *
captureDevice::getCaptureSource()
{
	IMFMediaSource *pSource = NULL;
	HRESULT hr = device_->ActivateObject(
		__uuidof(IMFMediaSource),
		(void**)&pSource);
	if (SUCCEEDED(hr)) 
	{
		isObjectAttached_ = true;
		device_->AddRef();
	}
	

	return SUCCEEDED(hr)?pSource:NULL;
}

void
captureDevice::shutdown()
{
	if (isObjectAttached_)
	{
		device_->DetachObject();
		isObjectAttached_ = false;
	}
}

//static
bool captureDevice::FormatFromGuid(const GUID& guid,
	VideoPixelFormat* format) {
	struct {
		const GUID& guid;
		const VideoPixelFormat format;
	} static const kFormatMap[] = {
		{ MFVideoFormat_I420, PIXEL_FORMAT_I420 },
		{ MFVideoFormat_YUY2, PIXEL_FORMAT_YUY2 },
		{ MFVideoFormat_UYVY, PIXEL_FORMAT_UYVY },
		{ MFVideoFormat_RGB24, PIXEL_FORMAT_RGB24 },
		{ MFVideoFormat_ARGB32, PIXEL_FORMAT_ARGB },
		{ MFVideoFormat_MJPG, PIXEL_FORMAT_MJPEG },
		{ MFVideoFormat_YV12, PIXEL_FORMAT_YV12 },
		{ MFVideoFormat_NV12, PIXEL_FORMAT_NV12 },
	};
	for (int i = 0; i < arraySize(kFormatMap); ++i) {
		if (kFormatMap[i].guid == guid) {
			*format = kFormatMap[i].format;
			return true;
		}
	}
	return false;
}

bool captureDevice::getCaptureFormats(IMFMediaSource *pSource)
{
	IMFSourceReader * reader;
	formats_.clear();
	HRESULT hr =
		MFCreateSourceReaderFromMediaSource(pSource, NULL, &reader);
	if (FAILED(hr)) {
		DBG(D_ERR, "MFCreateSourceReaderFromMediaSource: %08x\n", hr);
		return false;
	}
	const DWORD kFirstVideoStream =
		static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

	DWORD stream_index = 0;
	IMFMediaType * type;
	DBG(D_NORMAL, "Device: %s\n", name_.c_str());
	for (hr = reader->GetNativeMediaType(kFirstVideoStream, stream_index, &type);
		SUCCEEDED(hr);
		hr = reader->GetNativeMediaType(kFirstVideoStream, stream_index, &type)) {
		UINT32 width, height;
		hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height);
		if (FAILED(hr)) 
		{
			type->Release();
			reader->Release();
			DBG(D_ERR, "MFGetAttributeSize: %08x\n", hr);
			return SUCCEEDED(hr);
		}
		VideoCaptureFormat capture_format;
		capture_format.frame_size_.setSize(width, height);
		capture_format.stream_idx_ = stream_index;

		UINT32 numerator, denominator;
		hr = MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &numerator, &denominator);
		if (FAILED(hr)) 
		{
			type->Release();
			reader->Release();
			DBG(D_ERR, "MFGetAttributeRatio: %08x\n", hr);
			return SUCCEEDED(hr);
		}
		capture_format.frame_rate_ = denominator
			? static_cast<float>(numerator) / denominator : 0.0f;

		GUID type_guid;
		hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
		if (FAILED(hr)) 
		{
			type->Release();
			reader->Release();
			DBG(D_ERR, "GetGUID: %08x\n", hr);
			return SUCCEEDED(hr);
		}
		captureDevice::FormatFromGuid(type_guid,
			&capture_format.pixel_format_);
		type->Release();
		formats_.push_back(capture_format);
		++stream_index;

		//capture_format.toString();
		DBG(D_VERBOSE, "captureDevice::getCaptureFormats: device:%s, valid:%s, resolution:%dx%d, fps:%f, pixel_format:%s\n",
			name_.c_str(), capture_format.IsValid() ? "yes" : "no",
			capture_format.frame_size_.width(), capture_format.frame_size_.height(),
			capture_format.frame_rate_, VideoCaptureFormat::pixelFormatToString(capture_format.pixel_format_).c_str());
	}
	reader->Release();
	return SUCCEEDED(hr);
}

VideoCaptureFormat* captureDevice::getClosestMatch(VideoCaptureParams& param, float& score, bool exactPixelFormatMatch)
{
	std::vector<VideoCaptureFormat>::iterator it;
	int min_dw = 0x7fffffff;
	int min_dh = 0x7fffffff;
	float min_df = (float)0x7fffffff;
	VideoCaptureFormat *matched = NULL;
	score = (float)0x7fffffff;
	// find the lowest difference in width from the given one (best matching width)
	for (it = formats_.begin(); it != formats_.end(); it++) {
		if (exactPixelFormatMatch && param.requested_format_.pixel_format_ != it->pixel_format_) {
			continue;
		}
		int dw = abs((int)param.requested_format_.frame_size_.width() - (int)it->frame_size_.width());
		if (dw < min_dw) {
			min_dw = dw;
		}
	}
	// find the best matching height for all formats with lowest difference in width
	for (it = formats_.begin(); it != formats_.end(); it++) {
		if (exactPixelFormatMatch && param.requested_format_.pixel_format_ != it->pixel_format_) {
			continue;
		}
		int dw = abs((int)param.requested_format_.frame_size_.width() - (int)it->frame_size_.width());
		int dh = abs((int)param.requested_format_.frame_size_.height() - (int)it->frame_size_.height());
		if (dw == min_dw && dh < min_dh) {
			min_dh = dh;
		}
	}
	// find the best matching fps for all formats with the lowest difference in width and height
	for (it = formats_.begin(); it != formats_.end(); it++) {
		if (exactPixelFormatMatch && param.requested_format_.pixel_format_ != it->pixel_format_) {
			continue;
		}
		int dw = abs((int)param.requested_format_.frame_size_.width() - (int)it->frame_size_.width());
		int dh = abs((int)param.requested_format_.frame_size_.height() - (int)it->frame_size_.height());
		float df = fabs(param.requested_format_.frame_rate_ - it->frame_rate_);
		if (dw == min_dw && dh == min_dh && df < min_df) {
			min_df = df;
		}
	}
	// find the first format with best matching width, height & fps
	for (it = formats_.begin(); it != formats_.end(); it++) {
		int dw = abs((int)param.requested_format_.frame_size_.width() - (int)it->frame_size_.width());
		int dh = abs((int)param.requested_format_.frame_size_.height() - (int)it->frame_size_.height());
		float df = fabs(param.requested_format_.frame_rate_ - it->frame_rate_);
		if (dw == min_dw && dh == min_dh && df == min_df) {
			if (exactPixelFormatMatch) {
				if (param.requested_format_.pixel_format_ == it->pixel_format_) {
					matched = &(*it);
					score = min_dw + min_dh + min_df;
					break;
				}
			}
			else {
				score = min_dw + min_dh + min_df;
				matched = &(*it);
				break;
			}
		}
	}
	return matched;
}

