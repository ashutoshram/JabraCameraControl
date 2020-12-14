import jabracamera
import numpy as np
import cv2
import sys

r = jabracamera.JabraCamera()

dn = r.getCameras()
print(dn)

def convertYUYV2BGR(yuyv):
    return cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)

format_ = 'mjpg'
r.setStreamParams(deviceName=dn[0], width=1280, height=720, format=format_, fps=30)
while True:
    raw = r.getFrame(dn[0])
    if raw is None: continue
    if format_ == 'mjpg':
        frame1 = cv2.imdecode(np.fromstring(raw, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
    else:
        yuv = np.frombuffer(raw, dtype=np.uint8)
        shape=(720, 1280, 2)
        yuv = yuv.reshape(shape)
        frame1 = convertYUYV2BGR(yuv)
    cv2.imshow("hurr", frame1)
    k = cv2.waitKey(1)
    if (k == ord("q")):
        break

        

