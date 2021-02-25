#include <stdio.h>
#include <shlwapi.h>
#include <tchar.h>
#include <locale>
#include <string.h>
#include <strmif.h>

#include "WebcamSource.h"
#include "captureDevice.h"
#include "common.h"
#include "DeviceInfo.h"

bool WebcamSource::m_Initialized = false;
//bool WebcamSource::platformSupportsMediaFoundation() {
//	// Even though the DLLs might be available on Vista, we get crashes
//	// when running our tests on the build bots.
//	if (base::win::GetVersion() < base::win::VERSION_WIN7)
//		return false;
//
//	static bool g_dlls_available = LoadMediaFoundationDlls();
//	return g_dlls_available;
//}

//static
void WebcamSource::Initialize()
{
	if (!m_Initialized) { // NOTE: this is a static variable
		CoInitialize(NULL);
		MFStartup(MF_VERSION, MFSTARTUP_LITE);
		m_Initialized = true;
	}
}


void WebcamSource::UnInitialize()
{
	if (m_Initialized) 
	{
		MFShutdown();
		m_Initialized = false;
		//CoUninitialize(); //This can block fooorrreevvverrr...
	}
}

//static
WebcamSource * WebcamSource::getInstance() 
{
	Initialize();
	return new WebcamSource;
}

UINT32 WebcamSource::buildDeviceList(bool onlyPanacastDevice)
{
	HRESULT hr = S_OK;

	IMFAttributes *pAttributes = NULL;

	hr = MFCreateAttributes(&pAttributes, 1);

	if (SUCCEEDED(hr)) {
		hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	}
	devices_.clear();
	
	deviceCount_ = 0;
	if (SUCCEEDED(hr)) {
		IMFActivate **devices = NULL;

		hr = MFEnumDeviceSources(pAttributes, &devices, &deviceCount_);

		if (SUCCEEDED(hr)) {
			if (deviceCount_ > 0) {
				for (UINT32 i = 0; i < deviceCount_; i++) 
				{
					captureDevice cd = captureDevice(devices[i], i, onlyPanacastDevice);
					devices_.push_back(cd);
					SafeRelease(&devices[i]);
				}
				CoTaskMemFree(devices);
			}
		}
	}


	SafeRelease(&pAttributes);

	return (deviceCount_);
}

WebcamSource::WebcamSource()
{
	InitializeCriticalSection(&lock_);
	pReader_ = NULL;
	pSource_ = NULL;
	capturing_ = false;
	activeDevice_ = NULL;
	mReaderCallback_ = NULL;
	lockDevice_ = false;
	synchronousCapture_ = false;

	if (buildDeviceList(true) <= 0) return;
}


WebcamSource::~WebcamSource()
{
	stopAndDeallocate();
	// Disabling this as MFStartup and CoInitialize may cause problems
	// while a WebcamSource is running
	//UnInitialize();
	DeleteCriticalSection(&lock_);
}


// scan through our list of cached devices and get the one with the closest match
// to the requested capture params
captureDevice * WebcamSource::getCaptureDeviceWithCapability(VideoCaptureParams& param, 
	VideoCaptureFormat& match)
{
	if (deviceCount_ <= 0) return NULL;

	std::vector<captureDevice>::iterator it;
	float min_score = (float)0x7fffffff;
	captureDevice * matchedDev = NULL;

	for (it = devices_.begin(); it != devices_.end(); ++it) {
		VideoCaptureFormat * m;
		float score;
		if ((m = it->getClosestMatch(param, score)) != NULL) {
			DBG(D_NORMAL, "WebcamSource::getCaptureDeviceWithCapability: device %s found with score %f\n", it->getName().c_str(), score);
			if (score < min_score) {
				min_score = score;
				match = *m;
				matchedDev = &(*it);
			}
		}
	}

	if (matchedDev) {
		DBG(D_NORMAL, "WebcamSource::getCaptureDeviceWithCapability: matching device %s found with min. score %f\n", matchedDev->getName().c_str(), min_score);
	}
	else {
		DBG(D_NORMAL, "WebcamSource::getCaptureDeviceWithCapability: no matching device found\n");
	}
	return matchedDev;
}

class MFReaderCallback 
	: public IMFSourceReaderCallback {
public:
	MFReaderCallback(WebcamSource* observer, VideoCaptureFormat* format)
		: observer_(observer), format_(*format) {
		DBG(D_NORMAL, "WebcamSource::MFReaderCallback: constructor with pixel format %s\n", 
			VideoCaptureFormat::pixelFormatToString(format_.pixel_format_).c_str());
		m_refCount_ = 0;
		observerAvailable_ = true;
	}

	STDMETHOD(QueryInterface)(REFIID riid, void** object) {
		if (riid != IID_IUnknown && riid != IID_IMFSourceReaderCallback)
			return E_NOINTERFACE;
		*object = static_cast<IMFSourceReaderCallback*>(this);
		AddRef();
		return S_OK;
	}

	STDMETHOD_(ULONG, AddRef)() {
		ULONG count = InterlockedIncrement(&m_refCount_);
		//DBG(D_NORMAL, "WebcamSource::MFReaderCallback: AddRef: current count = %d\n", count);
		return count;

	}

	STDMETHOD_(ULONG, Release)() {
		ULONG count = InterlockedDecrement(&m_refCount_);
		if (count == 0) {
			DBG(D_NORMAL, "WebcamSource::MFReaderCallback: deleting ourselves\n");
			delete this;
			return 0;
		}
		//DBG(D_NORMAL, "WebcamSource::MFReaderCallback: Release: current count = %d\n", count);
		return count;

	}

	STDMETHOD(OnReadSample)(HRESULT status, DWORD stream_index,
		DWORD stream_flags, LONGLONG time_stamp, IMFSample* sample) {
		
		if (m_refCount_ == 0) {
			//DBG(D_NORMAL, "WebcamSource::Stream %d (%I64d), flags %08x\n", stream_index, time_stamp, stream_flags);
			DBG(D_NORMAL, "WebcamSource::OnReadSample: m_refCount is zero\n");
			return  S_OK;
		}
		UINT64 now;
		LARGE_INTEGER _now;
		QueryPerformanceCounter(&_now);
		now = _now.QuadPart;
		if (FAILED(status)) {
			DBG(D_ERR, "WebcamSource::OnReadSample: status FAILED\n");
			if (observerAvailable_) observer_->OnIncomingCapturedData(NULL, 0, format_, now, status);
			return  S_OK;
		}

		if (!sample) {
			//DBG(D_ERR, "WebcamSource::No sample!\n");
			if (observerAvailable_) observer_->OnIncomingCapturedData(NULL, 0, format_, now, status);
			return S_OK;
		}
#if 0
		if (stream_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			DBG(D_NORMAL, "\tWebcamSource::End of stream\n");
		}
		if (stream_flags & MF_SOURCE_READERF_NEWSTREAM) {
			DBG(D_NORMAL, "\tWebcamSource::New stream\n");
		}
		if (stream_flags & MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED) {
			DBG(D_NORMAL, "\tWebcamSource::Native type changed\n");
		}
		if (stream_flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
			DBG(D_NORMAL, "\tWebcamSource::Current type changed\n");
		}
		if (stream_flags & MF_SOURCE_READERF_STREAMTICK) {
			DBG(D_NORMAL, "\tWebcamSource::Stream tick\n");
		}
		//if (stream_flags & MF_SOURCE_READERF_D_ERROR) {
		//	DBG(D_NORMAL, "\t WebcamSource::D_ERROR ----------\n");
		//}
#endif
		DWORD count = 0;
		sample->GetBufferCount(&count);

		//DBG(D_ERR, "WebcamSource::Got sample - count = %d\n", count);
		for (DWORD i = 0; i < count; ++i) {
			IMFMediaBuffer * buffer = NULL;
			sample->GetBufferByIndex(i, &buffer);
			if (buffer) {
				DWORD length = 0, max_length = 0;
				BYTE* data = NULL;
				HRESULT hr = buffer->Lock(&data, &max_length, &length);
				DBG(D_VERBOSE, "WebcamSource::MFReaderCallback: OnIncomingCapturedData with pixel format %s, data=%p, length=%u\n",
					VideoCaptureFormat::pixelFormatToString(format_.pixel_format_).c_str(), data, length);
				if (hr == S_OK)
				{
					if (data && length > 0)
					{
						if (observerAvailable_)
							observer_->OnIncomingCapturedData(data, length, format_, now, status);
					}

					buffer->Unlock();
				}
				buffer->Release();
			}
		}
		return S_OK;
	}

	STDMETHOD(OnFlush)(DWORD stream_index) {
		DBG(D_NORMAL, "WebcamSource::MFReaderCallback: Flush called\n");
		return S_OK;
	}

	STDMETHOD(OnEvent)(DWORD stream_index, IMFMediaEvent* event) {
		return S_OK;
	}

	void disableObserver() {
		DBG(D_NORMAL, "WebcamSource::MFReaderCallback::disableObserver start\n");
		observerAvailable_ = false;
		DBG(D_NORMAL, "WebcamSource::MFReaderCallback::disableObserver done\n");
	}

private:
	~MFReaderCallback() {
		DBG(D_ERR, "WebcamSource::~MFReaderCallback: destructor\n");
	}
	VideoCaptureFormat format_;
	WebcamSource* observer_;
	long m_refCount_;
	volatile bool observerAvailable_;
};

bool WebcamSource::getSample(WebcamFrame& frame)
{
	if (!synchronousCapture_) {
		return false;
	}
	if (!TryEnterCriticalSection(&lock_)) {
		DBG(D_ERR, "WebcamSource::getSample: cannot lock, current thread id = %u\n", GetCurrentThreadId());
		return false;
	}

	if (!capturing_) {
		LeaveCriticalSection(&lock_);
		return false;
	}
	if (pReader_ == NULL) {
		LeaveCriticalSection(&lock_);
		return false;
	}

	DWORD streamFlags;
	IMFSample *pSample = NULL;
	HRESULT hr;
	LONGLONG  llTimestamp;
	hr = pReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &streamFlags, &llTimestamp, &pSample);
	if (SUCCEEDED(hr)) {
		DBG(D_VERBOSE, "WebcamSource::getSample: ReadSample successful\n");
		if ((streamFlags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ALLEFFECTSREMOVED | MF_SOURCE_READERF_ENDOFSTREAM)) || pSample == NULL) {
			DBG(D_VERBOSE, "WebcamSource::getSample: error in streamFlags from ReadSample or no sample\n");
			LeaveCriticalSection(&lock_);
			return false;
		}
		bool failed = false;
		IMFMediaBuffer * pBuffer = NULL;
		do {
			if (!SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuffer)))
			{
				DWORD bcnt = 0;
				if (!SUCCEEDED(pSample->GetBufferCount(&bcnt))) {
					failed = true;
					break;
				}
				if (bcnt == 0) {
					failed = true;
					break;
				}
				if (!SUCCEEDED(pSample->GetBufferByIndex(0, &pBuffer))) {
					failed = true;
					break;
				}
			}
			BYTE* ptr = NULL;
			LONG pitch = 0;
			DWORD maxsize = 0, cursize = 0;

			if (!SUCCEEDED(pBuffer->Lock(&ptr, &maxsize, &cursize))) {
				failed = true;
				break;
			}
			if (!ptr) {
				pBuffer->Unlock();
				failed = true;
				break;
			}

			frame.allocate(cursize);
			memcpy(frame.data, ptr, cursize); 
			frame.length = cursize;
			frame.format = matchedFormat_;
			frame.timestamp = llTimestamp;
			pBuffer->Unlock();
		} while (0);

		if (failed) {
			DBG(D_VERBOSE, "WebcamSource::getSample: cannot get buffer from sample\n");
		}
		pSample->Release();
		if (pBuffer) pBuffer->Release();
		LeaveCriticalSection(&lock_);
		return !failed;
	}
	else {
		DBG(D_ERR, "WebcamSource::getSample: Could not submit ReadSample (0x%08X)\n", hr);
		if (hr == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED) {
			DBG(D_ERR, "WebcamSource::getSample: ReadSample returned MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED\n");
		}
		if (hr == MF_E_HW_MFT_FAILED_START_STREAMING) {
			DBG(D_ERR, "WebcamSource::getSample: ReadSample returned MF_E_HW_MFT_FAILED_START_STREAMING\n");
		}
		LeaveCriticalSection(&lock_);
		return false;
	}


}

void WebcamSource::OnIncomingCapturedData(const unsigned char* data,
	int length,
	VideoCaptureFormat format,
	unsigned __int64 timestamp,
	HRESULT onReadSampleResult) {
	bool errorEncountered = false;
	
	if (!TryEnterCriticalSection(&lock_)) {
		DBG(D_ERR, "WebcamSource::OnIncomingCaptureData: cannot lock, current thread id = %u\n", GetCurrentThreadId());
		return;
	}

	if (!capturing_) {
		LeaveCriticalSection(&lock_);
		return;
	}

	if (data != NULL && length > 0) {
		if (frame_callback_) {
			WebcamFrame frame;
			frame.data = (unsigned char *)data;
			frame.length = length;
			frame.format = format;
			frame.timestamp = timestamp;
			//LeaveCriticalSection(&lock_);
			if (frame_callback_)
				frame_callback_((void*)&frame);
			//EnterCriticalSection(&lock_);
		}
	}
	if (pReader_ == NULL) {
		LeaveCriticalSection(&lock_);
		return;
	}
	HRESULT hr;
	if (FAILED(onReadSampleResult)) {
		errorEncountered = true;
		hr = onReadSampleResult;
	}
	else {
		hr = pReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
		if (SUCCEEDED(hr)) {
			DBG(D_VERBOSE, "WebcamSource::OnIncomingCaptureData: Successfully re-submitted ReadSample\n");
		}
		else {
			DBG(D_ERR, "WebcamSource::OnIncomingCapturedData: Could not re-submit ReadSample (0x%08X)\n", hr);
			errorEncountered = true;
			if (hr == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED) {
				DBG(D_ERR, "WebcamSource::OnIncomingCaptureData: ReadSample returned MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED\n");
			}
			if (hr == MF_E_HW_MFT_FAILED_START_STREAMING) {
				DBG(D_ERR, "WebcamSource::OnIncomingCaptureData: ReadSample returned MF_E_HW_MFT_FAILED_START_STREAMING\n");
			}
		}
	}
	LeaveCriticalSection(&lock_);
	if (errorEncountered) {
		if (error_callback_) {
			error_callback_(hr);
		}
	}
}

static std::string toLower(const std::string& s)
{
	std::string result;
	std::locale loc;

	for (unsigned int i = 0; i < s.length(); ++i) {
		result += std::tolower(s.at(i), loc);
	}
	return result;
}


bool WebcamSource::runLockedDevice()
{
	EnterCriticalSection(&lock_);
	if (!locked_ && lockDevice_) {
		DBG(D_NORMAL, "WebcamSource: runLockedDevice:  call canStartWithDeviceName()/canStartWithCapability() first\n");
		LeaveCriticalSection(&lock_);
		return false;
	}

	if (capturing_) {
		DBG(D_NORMAL, "WebcamSource: runLockedDevice: still capturing. call stopAndDeallocate first\n");
		LeaveCriticalSection(&lock_);
		return false;
	}

	if (!startReadsWithLockedDevice()) {
		LeaveCriticalSection(&lock_);
		return false;
	}

	LeaveCriticalSection(&lock_);
	return true;
}

bool WebcamSource::openAndRunDeviceWithId(std::string deviceId, VideoCaptureParams& param)
{
    if (activeDevice_ == NULL) {
        if (!canStartWithDeviceId(deviceId, param)) return false;
    }
    return runLockedDevice();
}

bool WebcamSource::canStartWithDeviceId(std::string deviceId, VideoCaptureParams& param)
{
	EnterCriticalSection(&lock_);

	std::string deviceIdLower = toLower(deviceId);

	bool found = false;
	for (UINT32 i = 0; i < devices_.size(); i++) {
		captureDevice * cd = NULL;
		std::string lowerId = toLower(devices_[i].getId());
		DBG(D_VERBOSE, "WebcamSource::canStartWithDeviceId: checking \"%s\" with \"%s\"\n", deviceIdLower.c_str(), lowerId.c_str());
		if (lowerId == deviceIdLower)
			cd = &devices_[i];

		if (cd == NULL) {
			continue;
		}
		VideoCaptureFormat *m;
		float score;
		if ((m = cd->getClosestMatch(param, score)) == NULL) {
			DBG(D_ERR, "WebcamSource::canStartWithDeviceId: could not find closest format on device %s\n", cd->getName().c_str());
			continue;
		}
		param.requested_format_ = *m;
		synchronousCapture_ = param.capture_synchronously_;
		if (!openAndLockDevice(cd, *m)) continue;
		found = true;
		break;
	}

	LeaveCriticalSection(&lock_);

	if (found) return true;
	DBG(D_ERR, "WebcamSource::canStartWithDeviceId: could not find any matching device for %s\n", deviceId.c_str());
	return false;

}



bool WebcamSource::getCaptureSource(captureDevice * cd, VideoCaptureFormat& match)
{
	if (pSource_ != NULL) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::pSource_ is already not NULL\n");
		return true;
	}
	IMFAttributes * pAttributes = NULL;
	HRESULT hr;
#if 1
	if (FAILED((hr = MFCreateAttributes(&pAttributes, 1)))) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::MFCreateAttributes: %08x\n", hr);
		return false;
	}

	if (FAILED(hr = (pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)))) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::pAttributes->SetGUID: %08x\n", hr);
		return false;
	}

	hr = pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
		SysUTF8ToWide(cd->getId()).c_str());
	if (FAILED(hr)) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::pAttributes->SetString: %08x\n", hr);
		return false;
	}

	if (FAILED(MFCreateDeviceSource(pAttributes, &pSource_))) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::MFCreateDeviceSource: %08x\n", hr);
		return false;
	}
	SafeRelease(&pAttributes);
	pAttributes = NULL;
#else
	pSource_ = cd->getCaptureSource();
	if (pSource_ == NULL) {
		DBG(D_ERR, "WebcamSource::getCaptureSource::Cannot get IMFMediaSource from capture device\n");
		return false;
	}
#endif
	return true;

}

bool WebcamSource::openAndLockDevice(captureDevice * cd, VideoCaptureFormat& match)
{
	IMFAttributes * pAttributes = NULL;
	HRESULT hr;

	stopAndDeallocate();

	if (!getCaptureSource(cd, match)) {
		DBG(D_NORMAL, "WebcamSource::openAndLockDevice: cannot get capture source\n");
		return false;
	}

	if (lockDevice_) {
		DBG(D_ERR, "WebcamSource::openAndLockDevice: opening and locking\n");
		// open in synchronous mode so that  ReadSample will trigger error if
		// already opened by another application; this should also lock the device
		// so that other applications cannot use it
		hr = MFCreateSourceReaderFromMediaSource(pSource_, NULL, &pReader_);
		if (FAILED(hr)) {
			DBG(D_ERR, "WebcamSource::openAndLockDevice::MFCreateSourceReaderFromMediaSource: %08x\n", hr);
			return false;
		}
		DBG(D_NORMAL, "WebcamSource::openAndLockDevice: _pReader = %p\n", pReader_);


		IMFMediaType * type;
		hr = pReader_->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, match.stream_idx_, &type);
		if (SUCCEEDED(hr)) {
			hr = pReader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
			if (SUCCEEDED(hr)) {
				hr = pReader_->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
				if (FAILED(hr)) {
					DBG(D_ERR, "WebcamSource::openAndLockDevice::SetStreamSelection: failed\n");
				}
				DWORD actualStreamIdx;
				DWORD streamFlags;
				LONGLONG timeStamp;
				IMFSample * pSample = NULL;
				hr = pReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualStreamIdx, &streamFlags, &timeStamp, &pSample);
				if (SUCCEEDED(hr)) {
					locked_ = true;
					activeDevice_ = cd;
					matchedFormat_ = match;
					return true;
				}
				else {
					DBG(D_ERR, "WebcamSource::openAndLockDevice::ReadSample: failed\n");
					return false;
				}
			}
			else {
				DBG(D_ERR, "WebcamSource::openAndLockDevice::SetCurrentMediaType: failed\n");
				return false;
			}
		}
		DBG(D_ERR, "WebcamSource::openAndLockDevice: failed\n");
		return false;
	}
	else {
		activeDevice_ = cd;
		matchedFormat_ = match;
		return true;
	}
}


bool WebcamSource::startReadsWithLockedDevice()
{
	HRESULT hr;
	if (!locked_ && lockDevice_) {
		DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: not locked: call openAndLockDevice first\n");
		return false;
	}

	captureDevice *lockedDevice = activeDevice_;
   
	if (pSource_ == NULL || lockedDevice == NULL) {
		DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: pSource_ = %p, activeDevice_ = %p; did you call openAndLockDevice?\n");
		return false;
	}

	IMFAttributes * pAttributes = NULL;
	if (FAILED((hr = MFCreateAttributes(&pAttributes, 2)))) {
		DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice::MFCreateAttributes: %08x\n", hr);
		return false;
	}
	pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);
	if (!synchronousCapture_) {
		mReaderCallback_ = new MFReaderCallback(this, &matchedFormat_);
		pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, mReaderCallback_);
	}

	hr = MFCreateSourceReaderFromMediaSource(pSource_, pAttributes, &pReader_);
	if (FAILED(hr)) {
		DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice::MFCreateSourceReaderFromMediaSource: %08x\n", hr);
		return false;
	}
	DBG(D_NORMAL, "WebcamSource::startReadsWithLockedDevice::_pReader = %p\n", pReader_);

	SafeRelease(&pAttributes);

	
	IMFMediaType * type;
	hr = pReader_->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, matchedFormat_.stream_idx_, &type);
	if (SUCCEEDED(hr)) {
		hr = pReader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, type);
		if (SUCCEEDED(hr)) {
			hr = pReader_->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
			if (FAILED(hr)) {
				DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: SetStreamSelection: failed\n");
			}
			if (synchronousCapture_) {
				DWORD streamFlags;
				IMFSample *sample = NULL;
				hr = pReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &streamFlags, NULL, &sample);
			}
			else {
				hr = pReader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
			}
			if (SUCCEEDED(hr)) {
				capturing_ = true;
				activeDevice_ = lockedDevice;
				locked_ = true;
				return true;
			}
			else {
				DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: ReadSample: failed (0x%08x)\n", hr);
				return false;
			}
		}
		else {
			DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: SetCurrentMediaType: failed\n");
			return false;
		}
	}
	DBG(D_ERR, "WebcamSource::startReadsWithLockedDevice: failed\n");
	return false;
}

bool WebcamSource::stopAndRelock()
{
	if (!capturing_) {
		DBG(D_NORMAL, "WebcamSource::stopAndRelock(): not capturing\n");
		return false;
	}
	return openAndLockDevice(activeDevice_, matchedFormat_); // this calls stopAndDeallocate first
}

void WebcamSource::stopAndDeallocate()
{
	DBG(D_ERR, "WebcamSource::stopAndDeallocate: starting, threadId = %u\n", GetCurrentThreadId());
	EnterCriticalSection(&lock_);
	if (mReaderCallback_) {
		DBG(D_ERR, "WebcamSource::stopAndDeallocate: disabling observer\n");
		mReaderCallback_->disableObserver();
		mReaderCallback_ = NULL;
	}
	capturing_ = false;
	locked_ = false;
	if (pReader_) {
		HRESULT hr = pReader_->Flush(MF_SOURCE_READER_ALL_STREAMS);
		SafeRelease(&pReader_);
	}
	if (pSource_) {
		DBG(D_ERR, "WebcamSource::stopAndDeallocate: shutting down pSource\n");
		pSource_->Stop();
		pSource_->Shutdown();
		SafeRelease(&pSource_);
	}
	if (activeDevice_) {
		DBG(D_ERR, "WebcamSource::stopAndDeallocate: shutting down activeDevice\n");
		activeDevice_->shutdown();
		activeDevice_ = NULL;
	}
	DBG(D_ERR, "WebcamSource::stopAndDeallocate: done\n");
	LeaveCriticalSection(&lock_);
}

