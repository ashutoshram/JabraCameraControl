#include "VideoCaptureFormat.h"
#include "common.h"


VideoCaptureFormat::VideoCaptureFormat()
: frame_rate_(0.0f), pixel_format_(PIXEL_FORMAT_UNKNOWN) {}
VideoCaptureFormat::VideoCaptureFormat(const SizeV& frame_size,
	float frame_rate,
	VideoPixelFormat pixel_format)
	: frame_size_(frame_size),
	frame_rate_(frame_rate),
	pixel_format_(pixel_format) {}

bool VideoCaptureFormat::IsValid() {
	return (frame_size_.width() < kMaxDimension) &&
		(frame_size_.height() < kMaxDimension) &&
		(frame_size_.area() >= 0) &&
		(frame_size_.area() < kMaxCanvas) &&
		(frame_rate_ >= 0.0f) &&
		(frame_rate_ <= kMaxFramesPerSecond) &&
		(pixel_format_ >= PIXEL_FORMAT_UNKNOWN) &&
		(pixel_format_ < PIXEL_FORMAT_MAX);
}
VideoCaptureParams::VideoCaptureParams() : allow_resolution_change_(false), capture_synchronously_(false) {}
// static
const std::string VideoCaptureFormat::pixelFormatToString(enum VideoPixelFormat p) {
	switch (p) {
	case PIXEL_FORMAT_UNKNOWN: return "UNKNOWN"; // Color format not set.
	case PIXEL_FORMAT_I420: return "I420";
	case PIXEL_FORMAT_YUY2: return "YUY2";
	case PIXEL_FORMAT_UYVY: return "UYVY";
	case PIXEL_FORMAT_RGB24: return "RGB24";
	case PIXEL_FORMAT_ARGB: return "ARGB";
	case PIXEL_FORMAT_MJPEG: return "MJPEG";
	case PIXEL_FORMAT_NV12: return "NV12";
	case PIXEL_FORMAT_YV12: return "YV12";
	case PIXEL_FORMAT_TEXTURE: return "TEXTURE"; // Capture format as a GL texture.
	case PIXEL_FORMAT_MAX: return "MAX";
	}
	return "";
}

void VideoCaptureFormat::toString() {
	DBG(D_NORMAL, "%d: valid:%s, resolution:%dx%d, fps:%f, pixel_format:%s\n",
		stream_idx_, IsValid() ? "yes" : "no",
		frame_size_.width(), frame_size_.height(),
		frame_rate_, VideoCaptureFormat::pixelFormatToString(pixel_format_).c_str());

}
