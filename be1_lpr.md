## 1.Thêm vào file .env, đây là cấu hình chung
MQTT_BROKER=mqtt://broker.hivemq.com
MQTT_PORT=1883
MQTT_CLIENT_ID=your_unique_client_id
> ⚠️ **Lưu ý quan trọng:** `MQTT_CLIENT_ID` phải là duy nhất cho mỗi thực thể. Nếu hai Backend dùng chung một ID, Broker sẽ ngắt kết nối của một trong hai liên tục (vòng lặp disconnect). Ví dụ như: "be1_lpr" hoặc "be2_main"

## 2.Cách BE nhận diện hình ảnh kết nối với MQTT (be1_lpr)
### Cài đặt thư viện
npm install mqtt dotenv

### 💻 Mã triển khai cho BE1 (LPR Service)
Dịch vụ này đóng vai trò gửi dữ liệu biển số sau khi đã xử lý hình ảnh từ camera.

```javascript
const mqtt = require('mqtt');
require('dotenv').config();

// Khởi tạo kết nối với Broker
const client = mqtt.connect(process.env.MQTT_BROKER, {
    clientId: process.env.MQTT_CLIENT_ID, // Ví dụ: be1_lpr
    port: process.env.MQTT_PORT
});

client.on('connect', () => {
    console.log('✅ BE1 (LPR) đã kết nối MQTT thành công.');
    // Đăng ký nhận tín hiệu từ cảm biến vật cản tại cổng (Entry/Exit)
    client.subscribe('sensors/gate/+', { qos: 1 });
});

client.on('message', (topic, message) => {
    try {
        console.log(`📩 Nhận tín hiệu từ topic: ${topic}`);
        
        // GIẢ LẬP: Sau khi thuật toán AI nhận diện xong biển số
        const result = { 
            plate: "79A-12345", 
            gate: topic.split('/').pop(), // Lấy 'entry' hoặc 'exit' từ topic
            timestamp: new Date().toISOString()
        };

        // Gửi kết quả biển số sang cho BE2 xử lý logic
        client.publish('payment_status', JSON.stringify(result), { qos: 1 });
        console.log('📤 Đã gửi thông tin biển số sang BE2.');
    } catch (err) {
        console.error('❌ Lỗi xử lý tin nhắn tại BE1:', err.message);
    }
});