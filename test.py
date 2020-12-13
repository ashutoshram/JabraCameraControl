import jabracamera
import numpy as np
import cv2
import sys

r = jabracamera.JabraCamera()

dn = r.getCameras()
print(dn)

while True:
    raw = r.getFrame(dn[0])
    if raw is None: continue
    break
yuv = np.frombuffer(raw, dtype=np.uint8)
shape=(720, 1280, 2)
yuv = yuv.reshape(shape)

def convertYUYV2BGR(yuyv):
    return cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)

frame1 = convertYUYV2BGR(yuv)
cv2.imshow("hurr", frame1)
cv2.waitKey(0)

