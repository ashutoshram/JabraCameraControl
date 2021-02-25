#include <vector>
#pragma once

// Color formats from camera.
enum VideoPixelFormat {
	PIXEL_FORMAT_UNKNOWN, // Color format not set.
	PIXEL_FORMAT_I420,
	PIXEL_FORMAT_YUY2,
	PIXEL_FORMAT_UYVY,
	PIXEL_FORMAT_RGB24,
	PIXEL_FORMAT_ARGB,
	PIXEL_FORMAT_MJPEG,
	PIXEL_FORMAT_NV12,
	PIXEL_FORMAT_YV12,
	PIXEL_FORMAT_TEXTURE, // Capture format as a GL texture.
	PIXEL_FORMAT_MAX,
};


class SizeV {
private:
	unsigned width_;
	unsigned height_;
public:
	unsigned area() { return width_ * height_; }
	unsigned width() { return width_; }
	unsigned height() { return height_; }
	void setSize(unsigned width, unsigned height) { width_ = width; height_ = height; }
};
// Some drivers use rational time per frame instead of float frame rate, this
// constant k is used to convert between both: A fps -> [k/k*A] seconds/frame.
const int kFrameRatePrecision = 10000;
const unsigned kMaxDimension = 4096;
const unsigned kMaxCanvas = 4096 * 2304;
const float kMaxFramesPerSecond = 30.0;
// Video capture format specification.
// This class is used by the video capture device to specify the format of every
// frame captured and returned to a client. It is also used to specify a
// supported capture format by a device.
class  VideoCaptureFormat {
public:
	VideoCaptureFormat();
	VideoCaptureFormat(const SizeV& frame_size,
		float frame_rate,
		VideoPixelFormat pixel_format);
	// Checks that all values are in the expected range
	bool IsValid();
	SizeV frame_size_;
	float frame_rate_;
	VideoPixelFormat pixel_format_;
	int stream_idx_;
	static const std::string pixelFormatToString(enum VideoPixelFormat p);
	void toString();
};
typedef std::vector<VideoCaptureFormat> VideoCaptureFormats;
// Parameters for starting video capture.
// This class is used by the client of a video capture device to specify the
// format of frames in which the client would like to have captured frames
// returned.
class  VideoCaptureParams {
public:
	VideoCaptureParams();
	// Requests a resolution and format at which the capture will occur.
	VideoCaptureFormat requested_format_;
	// Allow mid-capture resolution change.
	bool allow_resolution_change_;
	// Perform a synchronous capture
	bool capture_synchronously_;
};

