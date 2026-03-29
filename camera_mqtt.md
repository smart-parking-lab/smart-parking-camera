Thư viện cần tải:
```bash
pip install paho-mqtt requests opencv-python numpy
```

Code cho BE AI kết nối và dùng camera:
```python
import paho.mqtt.client as mqtt
import requests
import time
import json
import cv2
import numpy as np

# ==========================================
# 1. CẤU HÌNH THÔNG SỐ
# ==========================================
IP_CAMERA = "192.168.1.41"
URL_FOCUS = f"http://{IP_CAMERA}:8080/focus"
URL_SHOT = f"http://{IP_CAMERA}:8080/shot.jpg"

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_SENSOR = "ptithcm_2022/smart_parking/test_sensor"

# ==========================================
# 2. HÀM CHỤP ẢNH (TRẢ VỀ OPENCV FRAME)
# ==========================================
def chup_anh_tu_camera():
    print("📸 Bắt đầu quy trình chụp ảnh...")
    try:
        # Ép camera lấy nét (cho timeout ngắn để không bị treo nếu app đơ)
        print("   -> Đang ép lấy nét (Autofocus)...")
        requests.get(URL_FOCUS, timeout=2)
        
        # Chờ thấu kính trên điện thoại ổn định
        time.sleep(0.6)
        
        # Lấy dữ liệu ảnh thô
        print("   -> Đang tải ảnh về RAM...")
        response = requests.get(URL_SHOT, timeout=5)
        
        if response.status_code == 200:
            # BIẾN HÓA: Chuyển chuỗi byte thô thành mảng Numpy
            image_array = np.asarray(bytearray(response.content), dtype=np.uint8)
            # Giải mã thành khung hình OpenCV (Frame)
            frame = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
            
            print("✅ Đã lấy ảnh thành công! Dữ liệu sẵn sàng cho YOLO.")
            return frame
        else:
            print(f"❌ Lỗi: Camera trả về mã {response.status_code}")
            return None
            
    except Exception as e:
        print("❌ Lỗi kết nối Camera:", e)
        return None

# ==========================================
# 3. HÀM XỬ LÝ AI (NƠI BẠN NHÚNG MODEL VÀO)
# ==========================================
def nhan_dien_ai(frame):
    print("🧠 AI đang xử lý bức ảnh...")
    
    # ---------------------------------------------------------
    # TODO: BẠN SẼ GỌI MODEL YOLO Ở ĐÂY
    # Ví dụ:
    # results = model(frame)
    # bien_so_text = ocr_model.readtext(cropped_plate)
    # ---------------------------------------------------------
    
    # ĐỂ TEST: Tạm thời mình sẽ bật popup hiển thị bức ảnh vừa chụp lên màn hình 
    cv2.imshow("Anh vua chup tu Camera", frame)
    cv2.waitKey(3000) # Hiển thị 3 giây rồi tự tắt popup
    cv2.destroyAllWindows()

# ==========================================
# 4. LẮNG NGHE MQTT VÀ ĐIỀU PHỐI LUỒNG
# ==========================================
def on_connect(client, userdata, flags, rc):
    print("✅ Đã kết nối MQTT Broker!")
    client.subscribe(TOPIC_SENSOR)
    print(f"🎧 Đang túc trực lắng nghe tại: {TOPIC_SENSOR}...\n")

def on_message(client, userdata, msg):
    payload = msg.payload.decode('utf-8')
    
    try:
        data = json.loads(payload)
        if data.get("status") == "CO_XE":
            print(f"\n🚨 [{time.strftime('%H:%M:%S')}] PHÁT HIỆN CÓ XE!")
            
            # BƯỚC 1: Ra lệnh chụp ảnh và lấy frame về RAM
            frame = chup_anh_tu_camera()
            
            # BƯỚC 2: Ném thẳng frame đó vào hàm nhận diện AI
            if frame is not None:
                nhan_dien_ai(frame)
                
        elif data.get("status") == "TRONG":
            print(f"🟢 [{time.strftime('%H:%M:%S')}] Xe đã đi qua.")
            
    except json.JSONDecodeError:
        print("⚠️ Lỗi định dạng JSON từ ESP32 gửi lên.")

# Khởi tạo MQTT Client
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

# Chạy vòng lặp vô tận để giữ server luôn lắng nghe
client.connect(BROKER, PORT, 60)
client.loop_forever()
```

Thư viện cần tải:
```bash
pip install paho-mqtt
```

Code hướng dẫn BE đăng ký topic và nhận thông báo từ MQTT:
```python
import paho.mqtt.client as mqtt

# 1. Hàm tự động chạy khi kết nối thành công -> Dùng để ĐĂNG KÝ (Subscribe)
def on_connect(client, userdata, flags, rc):
    topic_nghe = "ptithcm_2022/smart_parking/test_sensor"
    client.subscribe(topic_nghe)
    print(f"Đã đăng ký lắng nghe topic: {topic_nghe}")

# 2. Hàm tự động chạy khi có ai đó gửi tin nhắn vào topic đã đăng ký -> Dùng để NHẬN
def on_message(client, userdata, msg):
    tin_nhan = msg.payload.decode('utf-8')
    print(f"Vừa nhận được tin nhắn: {tin_nhan} từ topic: {msg.topic}")

# 3. Khởi tạo, gắn hàm và bắt đầu vòng lặp chờ
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("broker.hivemq.com", 1883, 60)
client.loop_forever() # Lệnh này giữ cho Python không bị tắt, luôn thức để nghe
```

Code hướng dẫn BE gửi thông báo đến MQTT:
```python
import paho.mqtt.client as mqtt

# 1. Khởi tạo và kết nối
client = mqtt.Client()
client.connect("broker.hivemq.com", 1883, 60)

# 2. Bắn tin nhắn (Gửi lệnh mở cổng)
topic_gui = "ptithcm_2022/smart_parking/control"
noidung_gui = '{"command": "OPEN_IN"}'

client.publish(topic_gui, noidung_gui)
print("Đã gửi tin nhắn thành công!")
