import cv2
import socket
import json
import time
import mediapipe as mp
import numpy as np

import mediapipe as mp
from mediapipe.tasks.python import vision
from mediapipe.tasks import python
import os

# 모델 다운로드 (한 번만 필요)
model_path = '/Users/seunghan/Desktop/Codes/01_MoorimTC/motion_sick_logger/python/face_landmarker_v2_with_blendshapes.task'

# FaceLandmarker 옵션 설정
base_options = python.BaseOptions(model_asset_path=model_path)
options = vision.FaceLandmarkerOptions(
    base_options=base_options,
    output_face_blendshapes=True,
    # output_facial_transformation_matrix=True,
    num_faces=1
)

face_landmarker = vision.FaceLandmarker.create_from_options(options)

from collections import deque

# FPS 측정 변수
frame_times = deque(maxlen=30)  # 최근 30프레임의 시간 저장

vertices_id_face_boundary  = [ # points that enclose the face area, keep the orders, counter-clockwise, or clockwise, the first == the last
   9,
   336, 296, 334, 293, 301, 
   389, 356,454,323, 361, 288, 397,365, 379, 378, 400, 377,
   152,
   148, 176, 149, 150, 136, 172, 58, 132, 93, 234, 127, 162,
   71, 63, 105, 66, 107,
   9
]

vertices_id_left_eye = [
    33,
    246, 161, 160,159, 158, 157, 173, 133,
    155, 154, 153, 145, 144, 163, 7,
    33
]

vertices_id_right_eye = [
    362,
    398, 384, 385, 386, 387, 388, 466, 263,
    249, 390, 373, 374, 380, 381, 382,
    362
]

vertices_id_lip = [
    0,
    267, 269, 270, 409,
    375, 321, 405, 314, 17, 84, 181, 91, 146, 
    185, 40, 39, 37,
    0
]

# Socket 설정
HOST = '127.0.0.1'
PORT = 50007
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))



cap = cv2.VideoCapture(0)  # MacBook 카메라

def get_average_rgb(image, landmarks, image_shape):
    h, w, _ = image_shape

    def to_pixel_coords(indices):
        return [(int(landmarks[i*3] * w), int(landmarks[i*3+1] * h)) for i in indices]

    # 마스크 정의
    face_poly = np.array([to_pixel_coords(vertices_id_face_boundary)], dtype=np.int32)
    eye_left_poly = np.array([to_pixel_coords(vertices_id_left_eye)], dtype=np.int32)
    eye_right_poly = np.array([to_pixel_coords(vertices_id_right_eye)], dtype=np.int32)
    lip_poly = np.array([to_pixel_coords(vertices_id_lip)], dtype=np.int32)

    mask = np.zeros((h, w), dtype=np.uint8)
    cv2.fillPoly(mask, face_poly, 255)
    cv2.fillPoly(mask, eye_left_poly, 0)
    cv2.fillPoly(mask, eye_right_poly, 0)
    cv2.fillPoly(mask, lip_poly, 0)

    masked_image = cv2.bitwise_and(image, image, mask=mask)
    mean_val = cv2.mean(masked_image, mask=mask)[:3]  # BGR → 평균

    return mean_val[::-1], masked_image  # RGB, masked_image


while cap.isOpened():
    start_time = time.time()

    success, image = cap.read()
    if not success:
        time.sleep(0.01)
        continue

    image.flags.writeable = False
    image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=image_rgb)
    results = face_landmarker.detect(mp_image)


    if results.face_landmarks:
        landmark_list = []
        for lm in results.face_landmarks[0]:
            landmark_list.extend([lm.x, lm.y, lm.z])

        blendshape_dict = {}
        for bs in results.face_blendshapes[0]:
            blendshape_dict[bs.category_name] = bs.score


        # print(f"{blendshape_dict=}")
        avg_rgb, masked_image = get_average_rgb(image, landmark_list, image.shape)  

        cv2.imshow("Masked Face Region", masked_image)
        key = cv2.waitKey(1) & 0xFF
        if key == 27:  # ESC to exit
            break

        
        # 현재는 랜드마크만 전송 (추후 blendshape 추가)
        data = {
            "timestamp": time.time(),
            "blendshapes": blendshape_dict,
            "avg_rgb": avg_rgb
        }

        # print(blendshape_dict.keys())

        json_data = json.dumps(data)
        sock.sendall(json_data.encode('utf-8') + b'\n')  # \n으로 구분

    # # ⏱ FPS 계산
    # end_time = time.time()
    # frame_time = end_time - start_time
    # frame_times.append(frame_time)
    # if len(frame_times) >= 10:
    #     fps = 1.0 / (sum(frame_times) / len(frame_times))
    #     print(f"[INFO] FPS: {fps:.2f}")


cap.release()
cv2.destroyAllWindows()