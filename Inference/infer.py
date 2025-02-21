from model_runner import Infer
from threaded_camera import ThreadedCamera
from one_euro_filter import OneEuroFilter
import time
from pythonosc import udp_client
import numpy as np
import cv2
from PIL import Image

eye_L = ThreadedCamera(source=2)
eye_R = ThreadedCamera(source=0)

pred_eye_L = Infer(model_path="BALLNETV1-BABYER-LEFT")
pred_eye_R = Infer(model_path="BALLNETV1-BABYER-RIGHT")
pred_eye_BLINK = Infer(model_path="BALLNETV1-BABYER-BLINK-LEFT")
time.sleep(1)

# OSC client setup
oscclient = udp_client.SimpleUDPClient("127.0.0.1", 8889)

min_cutoff = 2.09
beta = 5.6
noisy_point = np.array([4])
oef = OneEuroFilter(noisy_point, min_cutoff=min_cutoff, beta=beta)

min_cutoff_blink = 1.5
beta_blink = 5
oef_blink = OneEuroFilter(np.array([1]), min_cutoff=min_cutoff_blink, beta=beta_blink)

while eye_L.isOpened() and eye_R.isOpened():
    start = time.time()
    frame_L = eye_L.read()
    frame_R = eye_R.read()
    outL = pred_eye_L.run(input=cv2.resize(frame_L, (240,240))) # Todo: Move these resizes into the preprocess method.
    outR = pred_eye_R.run(input=cv2.resize(frame_R, (240,240)))
    outBLINK = pred_eye_BLINK.run(input=cv2.resize(frame_L, (256,256)))
    preds = np.concatenate((outL, outR), axis=None)
    output = oef(preds)
    output_blink = oef_blink(outBLINK)  # Apply OneEuroFilter for eyelid
    if type(output) != type(None):
        # Send OSC messages for gaze and eyelid control
        oscclient.send_message("/avatar/parameters/v2/EyeLidLeft", float(1 - output_blink))
        oscclient.send_message("/avatar/parameters/v2/EyeLidRight", float(1 - output_blink))
        oscclient.send_message("/avatar/parameters/v2/EyeLeftX", output[0])
        oscclient.send_message("/avatar/parameters/v2/EyeRightX", output[2])
        oscclient.send_message("/avatar/parameters/v2/EyeLeftY", output[1])
        oscclient.send_message("/avatar/parameters/v2/EyeRightY", output[3])
    frame = cv2.circle(frame_L, (int(((output[0]/2)+0.5)*240), 240-int(((output[1]/2)+0.5)*240)), 10, (0, 255, 0), -1)
    cv2.imshow("frame", frame)
    cv2.waitKey(1)
    time.sleep(.004)
    end = time.time()
    print(f"Time: {(end-start)*1000}")
