#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// ================= CẤU HÌNH MẠNG & MQTT =================
const char* ssid = "Lam";
const char* password = "23282904";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Các topic MQTT
const char* topic_sensor = "ptithcm_2025/smart_parking/sensors"; 
const char* topic_control = "ptithcm_2025/smart_parking/control"; 
const char* topic_heart = "ptithcm_2025/smart_parking/heartbeat";

WiFiClient espClient;
PubSubClient client(espClient);

// Cấu trúc an toàn để dùng cho TẤT CẢ Queue
typedef struct {
  char payload[256];
} MqttMessage;

// Khai báo đúng kiểu dữ liệu
QueueHandle_t mqttSendQueue;
QueueHandle_t mqttReceiveQueue;
QueueHandle_t mqttHeartQueue; 

// ================= CẤU HÌNH PHẦN CỨNG =================
// 1. Cảm biến hồng ngoại
#define IR_GATE_IN  32
#define IR_GATE_OUT 33
#define IR_SLOT_1   34
#define IR_SLOT_2   35

int last_ir_in = 1;
int last_ir_out = 1;
int last_ir_slot1 = 1;
int last_ir_slot2 = 1;
bool state_payment = false;

bool state_ir_in = false;
bool state_ir_out = false;

unsigned long gateInOpenTime = 0;
unsigned long gateOutOpenTime = 0;

String method = "";
String invoice_id = "";
String cost = "";

// 2. Servo
#define SERVO_IN_PIN  13
#define SERVO_OUT_PIN 14
Servo servoIn;
Servo servoOut;
const int ANGLE_CLOSED = 0;   
const int ANGLE_OPEN = 90;    

// 3. Màn hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

String currentMessage = "";
unsigned long messageDisplayTime = 0;

// ================= HÀM CẬP NHẬT MÀN HÌNH OLED =================
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (currentMessage != "" && (millis() - messageDisplayTime > 3000)) {
    currentMessage = "";
  }
  else {
    currentMessage = "";
    bool s1_occupied = (digitalRead(IR_SLOT_1) == 0);
    bool s2_occupied = (digitalRead(IR_SLOT_2) == 0);
    int slots_available = 0;
    if (!s1_occupied) slots_available++;
    if (!s2_occupied) slots_available++;

    display.setCursor(15, 0);
    display.println("SMART PARKING PTIT");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    display.setCursor(0, 20); display.print("Slot 1: "); display.println(s1_occupied ? "CO XE" : "TRONG");
    display.setCursor(0, 35); display.print("Slot 2: "); display.println(s2_occupied ? "CO XE" : "TRONG");

    display.setCursor(0, 50); display.print("Trang thai: ");
    if (slots_available == 0) display.println("DA DAY!");
    else { display.print("CON "); display.print(slots_available); display.println(" CHO"); }
  }
  display.display();
}

// ================= HÀM XỬ LÝ LỆNH TỪ BACKEND =================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  Serial.println("\n[MQTT] Nhan duoc lenh tu Backend: " + message);

  // Đóng gói vào struct MqttMessage để đẩy vào Queue an toàn
  MqttMessage msgStruct;
  strcpy(msgStruct.payload, message.c_str());
  xQueueSend(mqttReceiveQueue, &msgStruct, 0);
}

// ================= HÀM KẾT NỐI MẠNG =================
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ Da ket noi Wi-Fi!");
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-Parking-"; clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_control); 
    } else delay(5000);
  }
}

// ================= TASK: MẠNG (CORE 0) =================
void TaskMQTT_Code(void * pvParameters){
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  for(;;){
    if(!client.connected()) reconnect();
    client.loop();
    
    // Nhận dữ liệu Cảm biến từ Queue và Gửi đi
    MqttMessage sendMsg;
    if(xQueueReceive(mqttSendQueue, &sendMsg, 0) == pdTRUE){
      client.publish(topic_sensor, sendMsg.payload);
      Serial.println("[MQTT publish] " + String(sendMsg.payload));
    }

    // Nhận dữ liệu Nhịp tim từ Queue và Gửi đi
    MqttMessage recvHb;
    if(xQueueReceive(mqttHeartQueue, &recvHb, 0) == pdTRUE){
      client.publish(topic_heart, recvHb.payload);
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ================= TASK: NHỊP TIM (CORE 1) =================
// Task này chỉ tập trung giám sát linh hồn của hệ thống (ESP32) 
// và giao diện hiển thị (OLED)
void TaskHeartBeat_Code(void * pvParameters){
  for(;;){
    // --- 1. KIỂM TRA ESP32 (Sức khỏe phần mềm) ---
    StaticJsonDocument<128> espDoc;
    espDoc["target"] = "ESP32";
    espDoc["status"] = "ONLINE";
    espDoc["uptime_s"] = millis() / 1000; // Thời gian đã chạy

    espDoc["free_ram"] = ESP.getFreeHeap(); // Trả về số byte RAM còn trống
    espDoc["wifi_rssi"] = WiFi.RSSI();      // Cường độ sóng Wi-Fi (-30 đến -90 dBm)
    espDoc["cpu_freq"] = ESP.getCpuFreqMHz();

    MqttMessage espMsg;
    serializeJson(espDoc, espMsg.payload);
    xQueueSend(mqttHeartQueue, &espMsg, 0);

    // --- 2. KIỂM TRA OLED (Sức khỏe phần cứng I2C) ---
    Wire.beginTransmission(0x3C); // "Gõ cửa" địa chỉ OLED
    byte error = Wire.endTransmission();

    StaticJsonDocument<128> oledDoc;
    oledDoc["target"] = "OLED";
    oledDoc["status"] = (error == 0) ? "ONLINE" : "OFFLINE";

    MqttMessage oledMsg;
    serializeJson(oledDoc, oledMsg.payload);
    xQueueSend(mqttHeartQueue, &oledMsg, 0);

    Serial.println("[SYSTEM] Đã kiểm tra nhịp tim: ESP32 & OLED");

    // Ngủ sâu 30 giây để không chiếm dụng CPU của các cảm biến trong loop()
    vTaskDelay(20000 / portTICK_PERIOD_MS);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IR_GATE_IN, INPUT_PULLUP);
  pinMode(IR_GATE_OUT, INPUT_PULLUP);
  pinMode(IR_SLOT_1, INPUT_PULLUP);
  pinMode(IR_SLOT_2, INPUT_PULLUP);

  servoIn.attach(SERVO_IN_PIN);
  servoOut.attach(SERVO_OUT_PIN);
  servoIn.write(ANGLE_CLOSED);
  servoOut.write(ANGLE_CLOSED);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Khong tim thay man hinh OLED"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20); display.println("Khoi dong he thong...");
  display.display();

  // Đổi size của Queue thành struct MqttMessage
  mqttSendQueue = xQueueCreate(10, sizeof(MqttMessage));
  mqttReceiveQueue = xQueueCreate(10, sizeof(MqttMessage));
  mqttHeartQueue = xQueueCreate(5, sizeof(MqttMessage));

  // Gán TaskMQTT vào Core 0
  xTaskCreatePinnedToCore(TaskMQTT_Code, "TaskMQTT", 10000, NULL, 1, NULL, 0);
  
  // Gán TaskHeartBeat vào Core 1 (Cùng nhân với loop)
  xTaskCreatePinnedToCore(TaskHeartBeat_Code, "TaskHeartBeat", 5000, NULL, 1, NULL, 1); 
}

// ================= LOOP (CORE 1) =================
void loop() {
  int ir_in = digitalRead(IR_GATE_IN);
  int ir_out = digitalRead(IR_GATE_OUT);
  int ir_slot1 = digitalRead(IR_SLOT_1);
  int ir_slot2 = digitalRead(IR_SLOT_2);

  // 1. NHẬN LỆNH ĐIỀU KHIỂN TỪ QUEUE
  MqttMessage recvMsg;
  if(xQueueReceive(mqttReceiveQueue, &recvMsg, 0) == pdTRUE){
    StaticJsonDocument<256> doc;
    // Chuyển payload từ mảng char sang String để thư viện JSON dễ đọc
    String jsonStr = String(recvMsg.payload); 
    DeserializationError error = deserializeJson(doc, jsonStr);

    if(!error){
      String target = doc["target"];
      if(target == "SERVO_IN" && ir_in == 0){
        servoIn.write(ANGLE_OPEN);
        state_ir_in = true;
        gateInOpenTime = millis();
      } 
      else if(target == "SERVO_OUT" && ir_out == 0){
        servoOut.write(ANGLE_OPEN);
        state_ir_out = true;
        gateOutOpenTime = millis();
      }
      else if(target == "PAYMENT"){
        state_payment = true;
        method = doc["method"].as<String>();
        invoice_id = doc["invoice"].as<String>();
        cost = doc["cost"].as<String>();
      }
    }
  }

  // 2. XỬ LÝ ĐÓNG CỔNG (Đã sửa lỗi cú pháp ngoặc đơn)
  if (state_ir_in) {
    if ( ((millis() - gateInOpenTime) > 5000) && (ir_in != 0) ) {
      servoIn.write(ANGLE_CLOSED);
      state_ir_in = false;
    }
  }

  if (state_ir_out) {
    if ( ((millis() - gateOutOpenTime) > 5000) && (ir_out != 0) ) {
      servoOut.write(ANGLE_CLOSED);
      state_ir_out = false;
    }
  }

  // 3. TRẢ LỜI THANH TOÁN & GỬI CẢM BIẾN
  if (state_payment) {
    StaticJsonDocument<256> docReply;
    docReply["target"] = "PAYMENT";
    docReply["status"] = "SUCCESS";
    docReply["method"] = "CASH";
    docReply["invoice"] = invoice_id;
    docReply["cost"] = cost;
    
    MqttMessage msgStruct;
    serializeJson(docReply, msgStruct.payload);
    xQueueSend(mqttSendQueue, &msgStruct, 0);
    
    state_payment = false;
    currentMessage = "THANH TOAN XONG";
    messageDisplayTime = millis();
  }

  if (ir_in != last_ir_in) {
    delay(50);
    if (ir_in == 0) { 
      MqttMessage msgStruct;
      strcpy(msgStruct.payload, "{\"sensor\": \"GATE_IN\", \"status\": \"CO_XE\"}");
      xQueueSend(mqttSendQueue, &msgStruct, 0);
      currentMessage = "CO XE VAO"; messageDisplayTime = millis(); 
    }
    last_ir_in = ir_in;
  }

  if (ir_out != last_ir_out) {
    delay(50);
    if (ir_out == 0) {
      MqttMessage msgStruct;
      strcpy(msgStruct.payload, "{\"sensor\": \"GATE_OUT\", \"status\": \"CO_XE\"}");
      xQueueSend(mqttSendQueue, &msgStruct, 0);
      currentMessage = "CO XE RA"; messageDisplayTime = millis();
    }
    last_ir_out = ir_out;
  }

  if (ir_slot1 != last_ir_slot1) {
    delay(50);
    MqttMessage msgStruct;
    if (ir_slot1 == 0) strcpy(msgStruct.payload, "{\"sensor\": \"SLOT_1\", \"status\": \"CO_XE\"}");
    else strcpy(msgStruct.payload, "{\"sensor\": \"SLOT_1\", \"status\": \"TRONG\"}");
    xQueueSend(mqttSendQueue, &msgStruct, 0);
    last_ir_slot1 = ir_slot1;
  }

  if (ir_slot2 != last_ir_slot2) {
    delay(50);
    MqttMessage msgStruct;
    if (ir_slot2 == 0) strcpy(msgStruct.payload, "{\"sensor\": \"SLOT_2\", \"status\": \"CO_XE\"}");
    else strcpy(msgStruct.payload, "{\"sensor\": \"SLOT_2\", \"status\": \"TRONG\"}");
    xQueueSend(mqttSendQueue, &msgStruct, 0);
    last_ir_slot2 = ir_slot2;
  }

  updateOLED();
}