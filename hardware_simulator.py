import time
import json
import threading
import sys
import os

# Hỗ trợ tương thích với cả paho-mqtt v1 và v2
try:
    import paho.mqtt.client as mqtt
    from paho.mqtt.enums import CallbackAPIVersion
    PAHO_V2 = True
except ImportError:
    import paho.mqtt.client as mqtt
    PAHO_V2 = False

# ================= CẤU HÌNH TOPIC =================
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

TOPIC_SENSOR = "ptithcm_2025/smart_parking/sensors"
TOPIC_CONTROL = "ptithcm_2025/smart_parking/control"
TOPIC_HEARTBEAT = "ptithcm_2025/smart_parking/heartbeat"

# Định dạng mã màu ANSI để hiển thị đẹp mắt
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
BLUE = "\033[94m"
CYAN = "\033[96m"
MAGENTA = "\033[95m"
RESET = "\033[0m"
BOLD = "\033[1m"

# Trạng thái giả lập của cảm biến (TRONG hoặc CO_XE)
sensors_state = {
    "GATE_IN": "TRONG",
    "GATE_OUT": "TRONG",
    "SLOT_1": "TRONG",
    "SLOT_2": "TRONG"
}

# Trạng thái giả lập của servo (0 = ĐÓNG, 90 = MỞ)
servo_state = {
    "SERVO_IN": 0,
    "SERVO_OUT": 0
}

# Biến kiểm soát luồng hoạt động
running = True
client = None
start_time = time.time()

def log(tag, message, color=CYAN):
    print(f"{color}[{tag}] {message}{RESET}")

def publish_message(topic, payload):
    if client and client.is_connected():
        payload_str = json.dumps(payload)
        client.publish(topic, payload_str, qos=1)
        log("MQTT PUBLISH", f"Topic: {topic} -> {payload_str}", GREEN)

def publish_sensor_state(sensor_name):
    payload = {
        "sensor": sensor_name,
        "status": sensors_state[sensor_name]
    }
    publish_message(TOPIC_SENSOR, payload)

# ================= CALLBACKS CỦA MQTT =================
def on_connect(client, userdata, flags, rc, properties=None):
    if (PAHO_V2 and rc == 0) or (not PAHO_V2 and rc == 0):
        log("HỆ THỐNG", "✅ Kết nối MQTT Broker thành công!", GREEN + BOLD)
        # Đăng ký nhận kênh điều khiển
        client.subscribe(TOPIC_CONTROL, qos=1)
        log("HỆ THỐNG", f"Đã Subscribe topic nhận lệnh: {TOPIC_CONTROL}", BLUE)
    else:
        log("HỆ THỐNG", f"❌ Kết nối thất bại với mã lỗi: {rc}", RED)

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode('utf-8')
        data = json.loads(payload_str)
        log("MQTT RECEIVE", f"📩 Nhận lệnh từ Backend: {payload_str}", YELLOW + BOLD)
        
        target = data.get("target")
        
        if target == "SERVO_IN":
            # Trong code thực tế, cổng chỉ mở khi có xe ở GATE_IN (ir_in == 0 / CO_XE)
            if sensors_state["GATE_IN"] == "CO_XE":
                servo_state["SERVO_IN"] = 90
                log("ĐIỀU KHIỂN", "🚧 Cổng VÀO (SERVO_IN) -> MỞ (90 độ)", GREEN)
                
                # Giả lập xe đi qua sau 5 giây và tự động đóng cổng
                def close_servo_in():
                    time.sleep(5)
                    servo_state["SERVO_IN"] = 0
                    sensors_state["GATE_IN"] = "TRONG"
                    log("ĐIỀU KHIỂN", "🚧 Cổng VÀO tự động -> ĐÓNG (0 độ) sau khi xe đi qua", GREEN)
                    publish_sensor_state("GATE_IN")
                
                threading.Thread(target=close_servo_in, daemon=True).start()
            else:
                log("HỆ THỐNG KHOÁ", "⚠️ Không thể mở SERVO_IN: Cảm biến GATE_IN hiện không có xe!", RED)
                
        elif target == "SERVO_OUT":
            if sensors_state["GATE_OUT"] == "CO_XE":
                servo_state["SERVO_OUT"] = 90
                log("ĐIỀU KHIỂN", "🚧 Cổng RA (SERVO_OUT) -> MỞ (90 độ)", GREEN)
                
                # Giả lập xe đi qua sau 5 giây và tự động đóng cổng
                def close_servo_out():
                    time.sleep(5)
                    servo_state["SERVO_OUT"] = 0
                    sensors_state["GATE_OUT"] = "TRONG"
                    log("ĐIỀU KHIỂN", "🚧 Cổng RA tự động -> ĐÓNG (0 độ) sau khi xe đi qua", GREEN)
                    publish_sensor_state("GATE_OUT")
                
                threading.Thread(target=close_servo_out, daemon=True).start()
            else:
                log("HỆ THỐNG KHOÁ", "⚠️ Không thể mở SERVO_OUT: Cảm biến GATE_OUT hiện không có xe!", RED)
                
        elif target == "PAYMENT":
            status = data.get("status")
            invoice = data.get("invoice", "")
            cost = data.get("cost", "0")
            method = data.get("method", "CASH")
            
            if status == "START":
                log("OLED DISPLAY", f"📱 Hiển thị OLED: YÊU CẦU THANH TOÁN {cost} VND", MAGENTA)
                
                # Giả lập phần cứng thanh toán thành công sau 2 giây
                def handle_payment():
                    time.sleep(2)
                    log("OLED DISPLAY", "📱 Hiển thị OLED: THANH TOÁN THÀNH CÔNG", MAGENTA)
                    reply = {
                        "target": "PAYMENT",
                        "status": "SUCCESS",
                        "method": method if method else "CASH",
                        "invoice": invoice,
                        "cost": cost
                    }
                    publish_message(TOPIC_CONTROL, reply)
                    
                threading.Thread(target=handle_payment, daemon=True).start()

    except Exception as e:
        log("LỖI SYSTEM", f"Không thể xử lý tin nhắn: {e}", RED)

# ================= LUỒNG THỜI GIAN THỰC (BACKGROUND THREADS) =================
def heartbeat_loop():
    """Gửi nhịp tim định kỳ (30 giây/lần) giả lập ESP32 và màn hình OLED"""
    while running:
        if client and client.is_connected():
            uptime = int(time.time() - start_time)
            
            # 1. ESP32 Heartbeat
            esp_hb = {
                "target": "ESP32",
                "status": "ONLINE",
                "uptime_s": uptime
            }
            client.publish(TOPIC_HEARTBEAT, json.dumps(esp_hb), qos=1)
            
            # 2. OLED Heartbeat
            oled_hb = {
                "target": "OLED",
                "status": "ONLINE"
            }
            client.publish(TOPIC_HEARTBEAT, json.dumps(oled_hb), qos=1)
            
            log("HEARTBEAT", "💓 Đã gửi báo cáo nhịp tim (ESP32 & OLED: ONLINE)", BLUE)
            
        # Nghỉ 30 giây
        for _ in range(30):
            if not running:
                break
            time.sleep(1)

# ================= MENU ĐIỀU KHIỂN BẰNG TAY (CLI) =================
def print_status(clear=False):
    if clear:
        os.system('cls' if os.name == 'nt' else 'clear')
    print(f"{BOLD}{CYAN}======================================================================={RESET}")
    print(f"{BOLD}{CYAN}      GIẢ LẬP PHẦN CỨNG IOT SMART PARKING (ESP32 & OLED SIMULATOR)     {RESET}")
    print(f"{BOLD}{CYAN}======================================================================={RESET}")
    print(f" Broker MQTT   : {BOLD}{YELLOW}{MQTT_BROKER}:{MQTT_PORT}{RESET}")
    print(f" Uptime        : {int(time.time() - start_time)} giây")
    print(f" Trạng thái Kết nối: {GREEN if (client and client.is_connected()) else RED}{'ĐÃ KẾT NỐI' if (client and client.is_connected()) else 'MẤT KẾT NỐI'}{RESET}")
    print(f"-----------------------------------------------------------------------")
    print(f" {BOLD}Trạng thái Cảm biến (Sensors):{RESET}")
    print(f"   - GATE_IN  (Cổng vào) : {RED if sensors_state['GATE_IN'] == 'CO_XE' else GREEN}{sensors_state['GATE_IN']}{RESET}")
    print(f"   - GATE_OUT (Cổng ra)  : {RED if sensors_state['GATE_OUT'] == 'CO_XE' else GREEN}{sensors_state['GATE_OUT']}{RESET}")
    print(f"   - SLOT_1   (Chỗ đỗ 1) : {RED if sensors_state['SLOT_1'] == 'CO_XE' else GREEN}{sensors_state['SLOT_1']}{RESET}")
    print(f"   - SLOT_2   (Chỗ đỗ 2) : {RED if sensors_state['SLOT_2'] == 'CO_XE' else GREEN}{sensors_state['SLOT_2']}{RESET}")
    print(f"-----------------------------------------------------------------------")
    print(f" {BOLD}Trạng thái Cổng barrier (Servos):{RESET}")
    print(f"   - SERVO_IN  (Barrier Vào): {GREEN if servo_state['SERVO_IN'] > 0 else RED}{'MỞ (90°)' if servo_state['SERVO_IN'] > 0 else 'ĐÓNG (0°)'}{RESET}")
    print(f"   - SERVO_OUT (Barrier Ra) : {GREEN if servo_state['SERVO_OUT'] > 0 else RED}{'MỞ (90°)' if servo_state['SERVO_OUT'] > 0 else 'ĐÓNG (0°)'}{RESET}")
    print(f"=======================================================================")
    print(f" {BOLD}PHÍM CHỨC NĂNG GIẢ LẬP:{RESET}")
    print(f"   {BOLD}{YELLOW}[1]{RESET} Đảo trạng thái cảm biến {BOLD}GATE_IN{RESET}  (Xe đến cổng vào / đi qua)")
    print(f"   {BOLD}{YELLOW}[2]{RESET} Đảo trạng thái cảm biến {BOLD}GATE_OUT{RESET} (Xe đến cổng ra / đi qua)")
    print(f"   {BOLD}{YELLOW}[3]{RESET} Đảo trạng thái cảm biến {BOLD}SLOT_1{RESET}   (Xe đỗ vào Slot 1 / rời bãi)")
    print(f"   {BOLD}{YELLOW}[4]{RESET} Đảo trạng thái cảm biến {BOLD}SLOT_2{RESET}   (Xe đỗ vào Slot 2 / rời bãi)")
    print(f"   {BOLD}{YELLOW}[5]{RESET} Giả lập tự động {BOLD}XE ĐI VÀO BÃI{RESET} (GATE_IN -> Xe đỗ Slot 1)")
    print(f"   {BOLD}{YELLOW}[6]{RESET} Giả lập tự động {BOLD}XE ĐI RA BÃI{RESET}  (Slot 1 -> GATE_OUT -> Ra bãi)")
    print(f"   {BOLD}{YELLOW}[r]{RESET} Làm mới màn hình trạng thái (Refresh & Clear)")
    print(f"   {BOLD}{RED}[q]{RESET} Thoát chương trình giả lập (Quit)")
    print(f"=======================================================================")

def interactive_loop():
    global running, sensors_state
    
    # In bảng trạng thái đầu tiên và xóa màn hình cho sạch
    print_status(clear=True)
    
    while running:
        choice = input(f"{BOLD}{BLUE}Nhập phím giả lập pin (1-6, r để làm mới, q để thoát): {RESET}").strip().lower()
        
        if choice == 'q':
            running = False
            log("HỆ THỐNG", "Đang dừng chương trình giả lập...", RED)
            break
            
        elif choice == '1':
            sensors_state["GATE_IN"] = "CO_XE" if sensors_state["GATE_IN"] == "TRONG" else "TRONG"
            publish_sensor_state("GATE_IN")
            print_status(clear=False)
            time.sleep(0.5)
            
        elif choice == '2':
            sensors_state["GATE_OUT"] = "CO_XE" if sensors_state["GATE_OUT"] == "TRONG" else "TRONG"
            publish_sensor_state("GATE_OUT")
            print_status(clear=False)
            time.sleep(0.5)
            
        elif choice == '3':
            sensors_state["SLOT_1"] = "CO_XE" if sensors_state["SLOT_1"] == "TRONG" else "TRONG"
            publish_sensor_state("SLOT_1")
            print_status(clear=False)
            time.sleep(0.5)
            
        elif choice == '4':
            sensors_state["SLOT_2"] = "CO_XE" if sensors_state["SLOT_2"] == "TRONG" else "TRONG"
            publish_sensor_state("SLOT_2")
            print_status(clear=False)
            time.sleep(0.5)
            
        elif choice == '5':
            log("GIẢ LẬP LUỒNG", "🚗 1. Xe bắt đầu tiếp cận GATE_IN...", YELLOW)
            sensors_state["GATE_IN"] = "CO_XE"
            publish_sensor_state("GATE_IN")
            print_status(clear=False)
            
            log("GIẢ LẬP LUỒNG", "⏳ Đang đợi Backend nhận diện biển số và ra lệnh mở cổng...", YELLOW)
            time.sleep(4)
            
            log("GIẢ LẬP LUỒNG", "🚗 2. Xe đã đi qua cổng vào, tiến hành đỗ vào SLOT_1...", YELLOW)
            sensors_state["SLOT_1"] = "CO_XE"
            publish_sensor_state("SLOT_1")
            print_status(clear=False)
            time.sleep(2)
            
        elif choice == '6':
            log("GIẢ LẬP LUỒNG", "🚗 1. Xe rời khỏi vị trí đỗ SLOT_1...", YELLOW)
            sensors_state["SLOT_1"] = "TRONG"
            publish_sensor_state("SLOT_1")
            print_status(clear=False)
            time.sleep(2)
            
            log("GIẢ LẬP LUỒNG", "🚗 2. Xe tiếp cận cổng ra GATE_OUT...", YELLOW)
            sensors_state["GATE_OUT"] = "CO_XE"
            publish_sensor_state("GATE_OUT")
            print_status(clear=False)
            log("GIẢ LẬP LUỒNG", "⏳ Đang đợi Backend quét camera ra lệnh thanh toán & mở cổng ra...", YELLOW)
            time.sleep(4)
            
        elif choice == 'r':
            print_status(clear=True)
            
        else:
            print(f"{RED}Lựa chọn không hợp lệ. Vui lòng bấm từ 1-6, r hoặc q.{RESET}")
            time.sleep(0.5)

# ================= CHƯƠNG TRÌNH CHÍNH =================
def main():
    global client, running
    
    # Kích hoạt hỗ trợ ANSI màu sắc trên console Windows nếu có
    if os.name == 'nt':
        os.system('')
        
    log("HỆ THỐNG", "Khởi động mô phỏng phần cứng IoT Smart Parking...", CYAN + BOLD)
    
    # Khởi tạo client MQTT dựa trên phiên bản thư viện paho-mqtt
    if PAHO_V2:
        client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
    else:
        client = mqtt.Client()
        
    client.on_connect = on_connect
    client.on_message = on_message
    
    # Cấu hình bản di chúc cuối (Will) - gửi OFFLINE nếu mất điện đột ngột
    client.will_set(TOPIC_HEARTBEAT, json.dumps({"target": "ESP32", "status": "OFFLINE"}), qos=1)
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        log("HỆ THỐNG", f"❌ Không thể kết nối tới Broker {MQTT_BROKER}: {e}", RED + BOLD)
        sys.exit(1)
        
    # Khởi chạy luồng đọc MQTT bất đồng bộ
    client.loop_start()
    
    # Khởi chạy luồng gửi nhịp tim định kỳ (Heartbeat thread)
    hb_thread = threading.Thread(target=heartbeat_loop, daemon=True)
    hb_thread.start()
    
    # Khởi chạy giao diện điều khiển bằng tay
    try:
        interactive_loop()
    except KeyboardInterrupt:
        log("HỆ THỐNG", "Nhận được tín hiệu ngắt. Đang thoát...", RED)
        
    # Dọn dẹp kết nối
    running = False
    client.loop_stop()
    client.disconnect()
    log("HỆ THỐNG", "Đã ngắt kết nối an toàn. Tạm biệt!", GREEN)

if __name__ == "__main__":
    main()
