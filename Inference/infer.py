import cv2
import onnxruntime as ort
import utils.image_transforms as transforms
from utils.open_iris_camera import Camera
from copy import deepcopy
from pythonosc import udp_client, osc_server, dispatcher
from one_euro_filter import OneEuroFilter
import numpy as np

#cap = cv2.VideoCapture(1)
cap = Camera("COM14")
#capr = cv2.VideoCapture(0)

min_cutoff = 2.09
beta = 5.6
noisy_point = np.array([45])
oef = OneEuroFilter(
    noisy_point, min_cutoff=min_cutoff, beta=beta
)

ort.disable_telemetry_events()
opts = ort.SessionOptions()
opts.inter_op_num_threads = 1
opts.intra_op_num_threads = 1
opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
opts.add_session_config_entry("session.intra_op.allow_spinning", "0")  # ~3% savings worth ~6ms avg latency. Not noticeable at 60fps?
opts.enable_mem_pattern = False

oscclient = udp_client.SimpleUDPClient(
            "127.0.0.1", 8889
        )  # use OSC port and address that was set in the config

sess = ort.InferenceSession(
    f"BALLNETV1-BABYER/onnx/model.onnx",
    opts,
    providers=["CPUExecutionProvider"],
)
input_name = sess.get_inputs()[0].name
output_name = sess.get_outputs()[0].name

while True:
    ret, frame = cap.get_serial_camera_picture()
    #retr, framer = capr.read()
    if not ret and not ret: pass
    else:
        frame = cv2.rotate(frame, cv2.ROTATE_90_CLOCKWISE)
        #framer = cv2.rotate(framer, cv2.ROTATE_90_COUNTERCLOCKWISE)
        #framer = cv2.flip(framer, 1)
        frame_clean = deepcopy(frame)
        #frame_cleanr = deepcopy(framer)
        frame = cv2.cvtColor(
                frame, cv2.COLOR_BGR2GRAY
            )
        #framer = cv2.cvtColor(
        #        framer, cv2.COLOR_BGR2GRAY
        #    )
        frame = cv2.resize(frame, (256, 256))
        frame = transforms.to_tensor(frame)
        frame = transforms.unsqueeze(frame, 0)
        #framer = cv2.resize(framer, (256, 256))
        #framer = transforms.to_tensor(framer)
        #framer = transforms.unsqueeze(framer, 0)
        out = sess.run([output_name], {input_name: frame})
        #outr = sess.run([output_name], {input_name: framer})
        output = out[0][0]
        output = oef(output)
        #outputr = outr[0][0]
        frame = cv2.circle(frame_clean, (int(output[0]*256), 256-int(output[1]*256)), 10, (0, 255, 0), -1)
        #framer = cv2.circle(frame_cleanr, (int(outputr[0]*256), 256-int(outputr[1]*256)), 10, (0, 255, 0), -1)
        #print(output)
        #oscclient.send_message("/avatar/parameters/v2" + "/EyeLeftX", float((output[0]*2)-1))
        oscclient.send_message("/avatar/parameters/v2" + "/EyeLeftX", float(output[0]))
        oscclient.send_message("/avatar/parameters/v2" + "/EyeLeftX", float(output[0]))
        #oscclient.send_message("/avatar/parameters/v2" + "/EyeRightX", float((output[0]*2)-1))
        #oscclient.send_message("/avatar/parameters/v2" + "/EyeRightX", float(((1-outputr[0])*2)-1))
        oscclient.send_message("/avatar/parameters/v2" + "/EyeLeftY", float(output[1]))
        oscclient.send_message("/avatar/parameters/v2" + "/EyeRightY", float(output[1]))
        print(output)
        cv2.imshow("frame", frame)
        #cv2.imshow("framer", framer)
        cv2.waitKey(1)
        ##print(output)
