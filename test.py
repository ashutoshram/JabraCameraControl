import jabracamera
import numpy as np
import cv2
import sys

r = jabracamera.JabraCamera()

dn = r.getCameras()
if dn is None:
    print('No PanaCast cameras found')
    sys.exit(1)

print('Cameras found: ', dn)

def convertYUYV2BGR(yuyv):
    return cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)

width = 1280
height = 720
format_ = 'mjpg'
if not r.setStreamParams(deviceName=dn[0], width=width, height=height, format=format_, fps=30):
    print('Unable to set stream params')
    sys.exit(1)

f = r.getProperty(dn[0], 'brightness')
if f is not None:
    print('getProperty(brightness) = ', f)
else:
    print('getProperty failed')

f = r.getProperty(dn[0], 'contrast')
if f is not None:
    print('getProperty(contrast) = ', f)
else:
    print('getProperty failed')

f = r.getProperty(dn[0], 'sharpness')
if f is not None:
    print('getProperty(sharpness) = ', f)
else:
    print('getProperty failed')

while True:
    raw = r.getFrame(dn[0])
    if raw is None: continue
    if format_ == 'mjpg':
        frame1 = cv2.imdecode(np.fromstring(raw, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
    else:
        yuv = np.frombuffer(raw, dtype=np.uint8)
        shape=(height, width, 2)
        yuv = yuv.reshape(shape)
        frame1 = convertYUYV2BGR(yuv)
    cv2.imshow("hurr", frame1)
    k = cv2.waitKey(1)
    if (k == ord("q")):
        break

        

