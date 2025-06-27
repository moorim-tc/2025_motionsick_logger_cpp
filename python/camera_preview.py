import sys
sys.path.append('/usr/lib/python3/dist-packages')

from picamera2 import Picamera2
import cv2

# Picamera2 인스턴스 생성 및 설정
picam2 = Picamera2()
picam2.preview_configuration.main.format = "RGB888"
picam2.preview_configuration.main.size = (640, 480)  # 해상도 조정 가능
picam2.configure("preview")
picam2.start()

# 프리뷰 루프
while True:
    frame = picam2.capture_array()
    cv2.imshow("Camera Preview", frame)

    # q 키 누르면 종료
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# 정리
cv2.destroyAllWindows()
