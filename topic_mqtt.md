Chia thành 2 topic để quản lý nội dung giao tiếp giữa BE và Phần cứng:
- Topic dành cho Sensor:
```bash
ptithcm_2025/smart_parking/sensors
```
Cấu trúc JSON để gửi thông báo lên MQTT trong topic này:
- Vị trí của sensor: "GATE_IN", "GATE_OUT", "SLOT_1", "SLOT_2"
- Nội dung: "CO_XE", "TRONG"
```bash
Cấu trúc JSON: {"sensor": + "vị trí của sensor", "status": + "nội dung"}
Ví dụ: {"sensor": "GATE_IN", "status": "CO_XE"}
        {"sensor": "SLOT_1", "status": "TRONG"}
```

- Topic dành cho Servo và Payment:
```bash
ptithcm_2025/smart_parking/control
```
Cấu trúc JSON để gửi thông báo lên MQTT trong topic này:
```bash
Cấu trúc JSON cụ thể dành cho Servo:
- {"target": "SERVO_IN", "command": "OPEN"}
- {"target": "SERVO_OUT", "command": "OPEN"}
```
```bash
Cấu trúc JSON cụ thể dành cho Payment:
- {"target": "PAYMENT", "status": "START", "method": "", "invoice": "", "cost": "3000"}   //BE ra lệnh cho phần cứng bắt người dùng thanh toán
- {"target": "PAYMENT", "status": "SUCCESS", "method": , "invoice": "", "cost": "3000"} //Phần cứng trả về kết quả thanh toán
```

- Topic dành cho Nhịp tim của phần cứng:
```bash
ptithcm_2025/smart_parking/heartbeat
```
Cấu trúc JSON để gửi thông báo lên MQTT:
```bash
- Màn hình OLED:
        + {"target":"OLED","status":"ONLINE"}
        + {"target":"OLED","status":"OFFLINE"}
```
```bash
- ESP32:
        +  {"target":"ESP32","status":"ONLINE"}
        +  {"target":"ESP32","status":"OFFLINE"}   //broker sẽ tự gửi thông báo nếu esp32 bị mất điện
```