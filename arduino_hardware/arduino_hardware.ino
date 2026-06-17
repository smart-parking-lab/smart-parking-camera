#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// ================= CẤU HÌNH MẠNG & MQTT =================
const char* ssid = "-.-";
const char* password = "0387269547";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Các topic MQTT
const char* topic_sensor = "ptithcm_2025/smart_parking/sensors"; 
const char* topic_control = "ptithcm_2025/smart_parking/control"; 
const char* topic_heart = "ptithcm_2025/smart_parking/heartbeat";

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// Cấu trúc an toàn để dùng cho TẤT CẢ Queue
typedef struct {
  char topic[64];
  char payload[256];
} MqttMessage;

// Khai báo đúng kiểu dữ liệu
QueueHandle_t mqttTxQueue;
QueueHandle_t mqttRxQueue;

// ================= CẤU HÌNH PHẦN CỨNG =================
// 1. Cảm biến hồng ngoại
#define IR_GATE_IN  32
#define IR_GATE_OUT 33
#define IR_SLOT_1   34
#define IR_SLOT_2   35
#define BUZZER_PIN  19

int last_ir_in = 1;
int last_ir_out = 1;
int last_ir_slot1 = 1;
int last_ir_slot2 = 1;
bool state_payment = false;

bool state_ir_in = false;
bool state_ir_out = false;

unsigned long gateInOpenTime = 0;
unsigned long gateOutOpenTime = 0;
unsigned long beepEndTime = 0;
unsigned long debounce[4] = {0};

String method = "";
String invoice_id = "";
String cost = "";

// 2. Servo
#define SERVO_IN_PIN  25
#define SERVO_OUT_PIN 26
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

bool isDisplayingMessage = false;

String errorMessage = "";
bool hasErrorMessage = false;
unsigned long errorDisplayTime = 0;

// ================= HÀM CẬP NHẬT MÀN HÌNH OLED =================
void pushOLEDMessage(const String& text) {
  if(currentMessage != text){
    currentMessage = text;
    messageDisplayTime = millis();
    isDisplayingMessage = true;
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (hasErrorMessage) {
    if (millis() - errorDisplayTime > 5000) {
      hasErrorMessage = false;
      errorMessage = "";
    }
    else {
      display.setCursor(0, 0);
      display.println("=== ERROR ===");
      display.drawLine(0, 12, 128, 12, SSD1306_WHITE);
      display.setCursor(0, 25);
      display.println(errorMessage);
      display.display();
      return;
    }
  }

  if (isDisplayingMessage) {
    if (millis() - messageDisplayTime < 3000) {
      display.setCursor(10, 25);
      display.println(currentMessage);
      display.display();
      return;
    }
    isDisplayingMessage = false;
    currentMessage = "";
  }

  bool s1_occupied = (last_ir_slot1 == LOW);
  bool s2_occupied = (last_ir_slot2 == LOW);
  int slots_available = 0;
  if (!s1_occupied) slots_available++;
  if (!s2_occupied) slots_available++;
  display.setCursor(15, 0);
  display.println("SMART PARKING PTIT");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("Slot 1: ");
  display.println(s1_occupied ? "CO XE" : "TRONG");
  display.setCursor(0, 35);
  display.print("Slot 2: ");
  display.println(s2_occupied ? "CO XE" : "TRONG");
  display.setCursor(0, 50);
  display.print("Trang thai: ");

  if (slots_available == 0) {
    display.println("DA DAY!");
  }
  else {
    display.print("CON ");
    display.print(slots_available);
    display.println(" CHO");
  }
  display.display();
}

bool hasAvailableSlot(){
  return (last_ir_slot1 == HIGH || last_ir_slot2 == HIGH);
}

void shortBeep(){
  
  digitalWrite(BUZZER_PIN, LOW); // Mức LOW để bật còi (Low level trigger)
  beepEndTime = millis() + 100;
}

// ================= HÀM KẾT NỐI MẠNG =================
void connectWifi(){
  if(WiFi.status() == WL_CONNECTED) return;
  Serial.println("Dang ket noi WiFi...");
  WiFi.begin(ssid, password);
}

void connectToMqtt(){
  Serial.println("Đang kết nối MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("✅ Đã kết nối Wi-Fi.");
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("❌ Mất kết nối Wi-Fi.");
      xTimerStop(mqttReconnectTimer, 0); // Không cố nối MQTT nếu không có WiFi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("✅ Đã kết nối MQTT Broker!");
  mqttClient.subscribe(topic_control, 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("❌ Mất kết nối MQTT Broker.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void publishMQTT(const char* topic, const String& payload){
  MqttMessage msg;
  strncpy(msg.topic, topic, sizeof(msg.topic)-1);
  msg.topic[sizeof(msg.topic)-1] = '\0';
  strncpy(msg.payload, payload.c_str(), sizeof(msg.payload)-1);
  msg.payload[sizeof(msg.payload)-1] = '\0';
  xQueueSend(mqttTxQueue, &msg, 0);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  // LƯU Ý CHÍ THỂ: payload của thư viện này không có ký tự kết thúc '\0'
  // Phải copy cẩn thận để không bị tràn RAM
  MqttMessage msgStruct;
  size_t copyLen = len < 255 ? len : 255;
  strncpy(msgStruct.payload, payload, copyLen);
  msgStruct.payload[copyLen] = '\0'; // Chốt chặn an toàn

  Serial.println("\n[MQTT] 📩 Nhận lệnh từ Backend: " + String(msgStruct.payload));
  xQueueSend(mqttRxQueue, &msgStruct, 0);
}

// ================= TASK: MẠNG (CORE 0) =================
void TaskMQTT_Code(void * pvParameters){
  MqttMessage msg;
  while(1){
    if(!mqttClient.connected()){
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }

    if(xQueueReceive(mqttTxQueue, &msg, portMAX_DELAY) == pdTRUE){
      mqttClient.publish(msg.topic, 1, false, msg.payload);
      Serial.printf("[MQTT] %s -> %s\n", msg.topic, msg.payload);
    }
  }
}

// ================= TASK: NHỊP TIM (CORE 1) =================
// Task này chỉ tập trung giám sát linh hồn của hệ thống (ESP32) 
// và giao diện hiển thị (OLED)
void TaskHeartBeat_Code(void * pvParameters){
  for(;;){
    // --- 1. KIỂM TRA ESP32 ---
    StaticJsonDocument<128> espDoc;
    espDoc["target"] = "ESP32";
    espDoc["status"] = "ONLINE";
    espDoc["uptime_s"] = millis() / 1000;

    MqttMessage espMsg;
    serializeJson(espDoc, espMsg.payload);
    publishMQTT(topic_heart, espMsg.payload);

    // --- 2. KIỂM TRA OLED ---
    Wire.beginTransmission(0x3C);
    byte error = Wire.endTransmission();

    StaticJsonDocument<128> oledDoc;
    oledDoc["target"] = "OLED";
    oledDoc["status"] = (error == 0) ? "ONLINE" : "OFFLINE";

    MqttMessage oledMsg;
    serializeJson(oledDoc, oledMsg.payload);
    publishMQTT(topic_heart, oledMsg.payload);
    Serial.println("[SYSTEM] Đã gửi báo cáo Nhịp tim (Heartbeat)");
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IR_GATE_IN, INPUT_PULLUP);
  pinMode(IR_GATE_OUT, INPUT_PULLUP);
  pinMode(IR_SLOT_1, INPUT_PULLUP);
  pinMode(IR_SLOT_2, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

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

  // Khởi tạo các Queue
  mqttTxQueue = xQueueCreate(20,sizeof(MqttMessage));
  mqttRxQueue = xQueueCreate(20,sizeof(MqttMessage));

  // --- SETUP BẤT ĐỒNG BỘ CHO WIFI & MQTT ---
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectWifi));

  WiFi.onEvent(WiFiEvent);
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setWill(topic_heart, 1, false, "{\"target\": \"ESP32\", \"status\": \"OFFLINE\"}");
  mqttClient.setServer(mqtt_server, mqtt_port);
  connectWifi(); // Kích hoạt kết nối mạng

  // Gán TaskMQTT vào Core 0
  xTaskCreatePinnedToCore(TaskMQTT_Code, "TaskMQTT", 10000, NULL, 1, NULL, 0);
  
  // Gán TaskHeartBeat vào Core 1
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
  if(xQueueReceive(mqttRxQueue, &recvMsg, 0) == pdTRUE){
    StaticJsonDocument<256> doc;
    String jsonStr = String(recvMsg.payload); 
    DeserializationError error = deserializeJson(doc, jsonStr);

    if(!error){
      String target = doc["target"];
      String command = doc["command"];
      String status = doc["status"];
      if(target == "SERVO_IN" && command == "OPEN"){
        if(!hasAvailableSlot()) pushOLEDMessage("BAI XE DAY");
        else{
          servoIn.write(ANGLE_OPEN);
          state_ir_in = true;
          gateInOpenTime = millis();
        }
      } 
      else if(target == "SERVO_OUT" && command == "OPEN"){
        servoOut.write(ANGLE_OPEN);
        state_ir_out = true;
        gateOutOpenTime = millis();
      }
      else if(target == "PAYMENT" && status == "START") {
        String status = doc["status"] | "";
        state_payment = true;
        method = doc["method"].as<String>();
        invoice_id = doc["invoice"].as<String>();
        cost = doc["cost"].as<String>();
      }
      else if(target == "ERR") {
        errorMessage = doc["content"].as<String>();
        hasErrorMessage = true;
        errorDisplayTime = millis();
        Serial.println("[ERROR] " + errorMessage);
      }
    }
  }

  // 3. XỬ LÝ ĐÓNG CỔNG TỰ ĐỘNG (Dùng trạng thái debounced để tránh nhiễu cổng)
  if (state_ir_in) {
    if ( ((millis() - gateInOpenTime) > 5000) && (last_ir_in != LOW) ) {
      servoIn.write(ANGLE_CLOSED);
      state_ir_in = false;
    }
  }

  if (state_ir_out) {
    if ( ((millis() - gateOutOpenTime) > 5000) && (last_ir_out != LOW) ) {
      servoOut.write(ANGLE_CLOSED);
      state_ir_out = false;
    }
  }

  // 4. TRẢ LỜI THANH TOÁN & GỬI CẢM BIẾN
  if (state_payment) {
    StaticJsonDocument<256> docReply;
    docReply["target"] = "PAYMENT";
    docReply["status"] = "SUCCESS";
    docReply["method"] = "CAST";
    docReply["invoice"] = invoice_id;
    docReply["cost"] = cost;
    
    MqttMessage msgStruct;
    serializeJson(docReply, msgStruct.payload);
    publishMQTT(topic_control, msgStruct.payload);
    state_payment = false;
    long costValue = cost.toInt();
    if (costValue < 1000){
      costValue *= 1000;
    }
    cost = String(costValue);
    pushOLEDMessage(cost + " VNĐ\nTHANH TOAN XONG");
  }

  if (ir_in != last_ir_in)
  {
    if(debounce[0] == 0) debounce[0] = millis();
    if(millis() > (debounce[0]+300)){
      ir_in = digitalRead(IR_GATE_IN);
      if (ir_in != last_ir_in){
        if (ir_in == LOW){
            publishMQTT(topic_sensor, "{\"sensor\":\"GATE_IN\",\"status\":\"CO_XE\"}");
            shortBeep();
        }
        last_ir_in = ir_in;
      }
      debounce[0] = 0;
    }
  }

  if (ir_out != last_ir_out)
  {
    if(debounce[1] == 0) debounce[1] = millis();
    if(millis() > (debounce[1]+300)){
      ir_out = digitalRead(IR_GATE_OUT);
      if (ir_out != last_ir_out){
        if (ir_out == LOW){
            publishMQTT(topic_sensor, "{\"sensor\":\"GATE_OUT\",\"status\":\"CO_XE\"}");
            shortBeep();
        }
        last_ir_out = ir_out;
      }
      debounce[1] = 0;
    }
  }

  if (ir_slot1 != last_ir_slot1){
    if(debounce[2] == 0) debounce[2] = millis();
    if(millis() > (debounce[2]+300)){
      ir_slot1 = digitalRead(IR_SLOT_1);
      if (ir_slot1 != last_ir_slot1){
        if (ir_slot1 == LOW){
            publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_1\",\"status\":\"CO_XE\"}");
        }
        else{
            publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_1\",\"status\":\"TRONG\"}");
        }
        last_ir_slot1 = ir_slot1;
      }
      debounce[2] = 0;
    }
  }

  if (ir_slot2 != last_ir_slot2){
    if(debounce[3] == 0) debounce[3] = millis();
    if(millis() > (debounce[3]+300)){
      ir_slot2 = digitalRead(IR_SLOT_2);
      if (ir_slot2 != last_ir_slot2){
        if (ir_slot2 == LOW){
            publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_2\",\"status\":\"CO_XE\"}");
        }
        else{
            publishMQTT(topic_sensor, "{\"sensor\":\"SLOT_2\",\"status\":\"TRONG\"}");
        }
        last_ir_slot2 = ir_slot2;
      }
      debounce[3] = 0;
    }
  }

  if(beepEndTime > 0 && millis() >= beepEndTime){
    digitalWrite(BUZZER_PIN, HIGH); // Tắt còi
    beepEndTime = 0;
  }

  updateOLED();
}