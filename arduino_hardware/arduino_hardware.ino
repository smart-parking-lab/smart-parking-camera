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
const char* topic_sensor = "ptithcm_2022/smart_parking/sensors"; // Gửi lên BE
const char* topic_control = "ptithcm_2022/smart_parking/control"; // Nhận từ BE

WiFiClient espClient;
PubSubClient client(espClient);

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

String session_id = "";
String invoice_id = "";
String cost = "";

// 2. Servo
#define SERVO_IN_PIN  13
#define SERVO_OUT_PIN 14
Servo servoIn;
Servo servoOut;
const int ANGLE_CLOSED = 0;   // Góc đóng cổng
const int ANGLE_OPEN = 90;    // Góc mở cổng

// 3. Màn hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Biến quản lý trạng thái hiển thị
String currentMessage_in = "";
String currentMessage_out = "";
unsigned long messageDisplayTime = 0;

// ================= HÀM CẬP NHẬT MÀN HÌNH OLED =================
void updateOLED(int ir_in, int ir_out) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Nếu đang có thông báo tạm thời (Xe vào/ra) và chưa qua 3 giây
  if ((currentMessage_in != "" && ir_in == 0) || (currentMessage_out != "" && ir_out == 0)){
    display.setTextSize(2);
    display.setCursor(0, 25);
    display.println(currentMessage_in);
    display.println(currentMessage_out);
  }
  else {
    currentMessage_in = currentMessage_out = "";
    
    bool s1_occupied = (digitalRead(IR_SLOT_1) == 0);
    bool s2_occupied = (digitalRead(IR_SLOT_2) == 0);
    int slots_available = 0;
    if (!s1_occupied) slots_available++;
    if (!s2_occupied) slots_available++;

    // Hiển thị Title
    display.setCursor(15, 0);
    display.println("SMART PARKING PTIT");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Hiển thị Slot 1
    display.setCursor(0, 20);
    display.print("Slot 1: ");
    display.println(s1_occupied ? "CO XE" : "TRONG");

    // Hiển thị Slot 2
    display.setCursor(0, 35);
    display.print("Slot 2: ");
    display.println(s2_occupied ? "CO XE" : "TRONG");

    // Hiển thị Trạng thái tổng
    display.setCursor(0, 50);
    display.print("Trang thai: ");
    if (slots_available == 0) {
      display.println("DA DAY!");
    } else {
      display.print("CON ");
      display.print(slots_available);
      display.println(" CHO");
    }
  }
  display.display();
}

// ================= HÀM XỬ LÝ LỆNH TỪ BACKEND =================
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("\n[MQTT] 📩 Nhận tin nhắn từ topic: " + String(topic));
  Serial.println("\n[MQTT] Nhan duoc lenh tu Backend: " + message);

  // Phân tích JSON từ Backend
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message); 
  
  if(error){
    Serial.print("❌ Lỗi không thể đọc định dạng JSON: ");
    Serial.println(error.c_str());
    return;
  }

  if(String(topic) == topic_control){
    String target = doc["target"];
    if(target == "SERVO_IN" && digitalRead(IR_GATE_IN) == 0){
      servoIn.write(ANGLE_OPEN);
      state_ir_in = true;
    } else if(target == "SERVO_OUT" && digitalRead(IR_GATE_OUT) == 0){
      servoOut.write(ANGLE_OPEN);
      state_ir_out = true;
    }
    else if(target == "PAYMENT"){
      state_payment = true;
      session_id = doc["session"].as<String>();
      invoice_id = doc["invoice"].as<String>();
      cost = doc["cost"].as<String>();
    }
  }
}

// ================= HÀM KẾT NỐI MẠNG & MQTT =================
void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ Da ket noi Wi-Fi!");
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-Parking-"; clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_control); // Đăng ký nghe lệnh mở cổng
    } else {
      delay(5000);
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // Khởi tạo chân cảm biến
  pinMode(IR_GATE_IN, INPUT);
  pinMode(IR_GATE_OUT, INPUT);
  pinMode(IR_SLOT_1, INPUT);
  pinMode(IR_SLOT_2, INPUT);

  // Khởi tạo Servo
  servoIn.attach(SERVO_IN_PIN);
  servoOut.attach(SERVO_OUT_PIN);
  servoIn.write(ANGLE_CLOSED);
  servoOut.write(ANGLE_CLOSED);

  // Khởi tạo OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Khong tim thay man hinh OLED"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Khoi dong he thong...");
  display.display();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  updateOLED(1, 1);
}

// ================= LOOP (VÒNG LẶP CHÍNH) =================
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Đọc trạng thái hiện tại của 4 cảm biến
  int ir_in = digitalRead(IR_GATE_IN);
  int ir_out = digitalRead(IR_GATE_OUT);
  int ir_slot1 = digitalRead(IR_SLOT_1);
  int ir_slot2 = digitalRead(IR_SLOT_2);
  
  bool displayNeedsUpdate = false;

  // 1. XỬ LÝ CỔNG VÀO (GATE IN)
  if (ir_in != last_ir_in) {
    delay(50);
    if (ir_in == 0) {
      client.publish(topic_sensor, "{\"sensor\": \"GATE_IN\", \"status\": \"CO_XE\"}");
      currentMessage_in = "CO XE VAO";
      messageDisplayTime = millis();
      displayNeedsUpdate = true;
    }
    else if(state_ir_in){
      for (int i = 0; i < 100; i++) { 
          client.loop(); // Duy trì sóng MQTT
          delay(100);    // Mỗi vòng đợi 0.1 giây -> 100 vòng là đủ 10 giây
      }
      servoIn.write(ANGLE_CLOSED);
      client.publish(topic_sensor, "{\"sensor\": \"GATE_IN\", \"status\": \"TRONG\"}");
      displayNeedsUpdate = true;
      state_ir_in = false;
    }
    last_ir_in = ir_in;
  }

  // 2. XỬ LÝ CỔNG RA (GATE OUT)
  if (ir_out != last_ir_out) {
    delay(50);
    if (ir_out == 0) {
      if(state_payment){
        StaticJsonDocument<256> docReply;
        docReply["target"] = "PAYMENT";
        docReply["status"] = "SUCCESS";
        docReply["session"] = session_id;
        docReply["invoice"] = invoice_id;
        docReply["cost"] = cost;
        String jsonString;
        serializeJson(docReply, jsonString);
        client.publish(topic_sensor, jsonString.c_str());
        state_payment = false;
      }
      else{
        client.publish(topic_sensor, "{\"sensor\": \"GATE_OUT\", \"status\": \"CO_XE\"}");
        currentMessage_out = "CO XE RA"; 
        messageDisplayTime = millis();
        displayNeedsUpdate = true;
      }
    } 
    else if(state_ir_out){
      for (int i = 0; i < 100; i++) { 
          client.loop(); // Duy trì sóng MQTT
          delay(100);    // Mỗi vòng đợi 0.1 giây -> 100 vòng là đủ 10 giây
      }
      servoOut.write(ANGLE_CLOSED);
      client.publish(topic_sensor, "{\"sensor\": \"GATE_OUT\", \"status\": \"TRONG\"}");
      displayNeedsUpdate = true;
      state_ir_out = false;
    }
    last_ir_out = ir_out;
  }

  // 3. XỬ LÝ SLOT ĐỖ XE
  if (ir_slot1 != last_ir_slot1) {
    delay(50);
    currentMessage_in = currentMessage_out = "";
    if (ir_slot1 == 0){
      client.publish(topic_sensor, "{\"sensor\": \"SLOT_1\", \"status\": \"CO_XE\"}");
      messageDisplayTime = millis();
      displayNeedsUpdate = true;
    }
    else{
      client.publish(topic_sensor, "{\"sensor\": \"SLOT_1\", \"status\": \"TRONG\"}");
      displayNeedsUpdate = true;
    }
    last_ir_slot1 = ir_slot1;
  }

  if (ir_slot2 != last_ir_slot2) {
    currentMessage_in = currentMessage_out = "";
    if (ir_slot2 == 0){
      client.publish(topic_sensor, "{\"sensor\": \"SLOT_2\", \"status\": \"CO_XE\"}");
      messageDisplayTime = millis();
      displayNeedsUpdate = true;
    }
    else{
      client.publish(topic_sensor, "{\"sensor\": \"SLOT_2\", \"status\": \"TRONG\"}");
      displayNeedsUpdate = true;
    }
    last_ir_slot2 = ir_slot2;
  }

  if (displayNeedsUpdate) {
    updateOLED(ir_in, ir_out);
  }
}