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
        + Kịch bản 1: ESP32 và mọi thứ hoạt động bình thường
          {
                "target": "ESP32",
                "status": "ONLINE",
                "uptime_s": 3600,       // Đây là thời gian hoạt động của ESP32, đơn vị: s
                "free_ram": 215432,     // Đây là ram trống hiện tại của ESP32, đơn vị: B
                "wifi_rssi": -65,       // Đây là trạng thái WIFI, đơn vị: dBm
                "cpu_freq": 240         // Đây là tần số CPU của ESP32, đơn vị: MHz
          }
        + Kịch bản 2: Code esp32 bị lỗi
          {
                "target": "ESP32",
                "status": "ONLINE",
                "uptime_s": 3600,
                "free_ram": 1500,     // Ram < 10000, bên FE hiển thị cảnh báo "Ram thấp, sắp tràn bộ nhớ"
                "wifi_rssi": -65,
                "cpu_freq": 240
          }
        + Kịch bản 3: Wiffi yếu
          {
                "target": "ESP32",
                "status": "ONLINE",
                "uptime_s": 3600,
                "free_ram": 215432,
                "wifi_rssi": -89,     // Wiffi < -80, bên FE cảnh báo "Sóng quá yếu, chập chờn"
                "cpu_freq": 240
          }
        + Kịch bản 4: ESP32 bị lỗi hoặc mất điện, hoặc cháy -> trên MQTT lúc này sẽ ko có nhận được tin nhắn thông báo trạng thái của ESP32
``` 