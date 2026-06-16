#include <WiFi.h>
#include <AsyncMqttClient.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "config.h"
#include "types.h"

AsyncMqttClient mqttClient;

Servo servoIn;
Servo servoOut;

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

QueueHandle_t mqttTxQueue;
QueueHandle_t mqttRxQueue;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

bool state_ir_in = false;
bool state_ir_out = false;

bool state_payment = false;

String payment_method;
String payment_invoice;
int payment_cost;

unsigned long gateInOpenTime = 0;
unsigned long gateOutOpenTime = 0;

int last_ir_in = 1;
int last_ir_out = 1;
int last_ir_slot1 = 1;
int last_ir_slot2 = 1;

void setup() {
  Serial.begin(115200);

  pinMode(IR_GATE_IN, INPUT_PULLUP);
  pinMode(IR_GATE_OUT, INPUT_PULLUP);
  pinMode(IR_SLOT_1, INPUT_PULLUP);
  pinMode(IR_SLOT_2, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

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

void loop()
{
  MqttMessage recvMsg;
  if(xQueueReceive(mqttRxQueue, &recvMsg, 0) == pdTRUE){
    StaticJsonDocument<256> doc;
    deserializeJson(doc, recvMsg.payload);

    if(!error){
      String target = doc["target"];
      String command = doc["command"];
      String status = doc["status"];
      if(target == "SERVO_IN" && command == "OPEN"){
        if(!hasAvailableSlot())
        {
          pushOLEDMessage("BAI XE DAY");
        }
        else
        {
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
        payment_method = doc["method"].as<String>();
        payment_invoice = doc["invoice"].as<String>();
        payment_cost = doc["cost"].as<String>();      
      }
      else if(target == "ERR") {
        errorMessage = doc["content"].as<String>();
        hasErrorMessage = true;
        errorDisplayTime = millis();
        Serial.println("[ERROR] " + errorMessage);
      }
    }
  }

  // 2. XỬ LÝ ĐÓNG CỔNG TỰ ĐỘNG
  if (state_ir_in) {
    if ( ((millis() - gateInOpenTime) > 5000) && (last_ir_in != 0) ) {
      servoIn.write(ANGLE_CLOSED);
      state_ir_in = false;
    }
  }

  if (state_ir_out) {
    if ( ((millis() - gateOutOpenTime) > 5000) && (last_ir_out != 0) ) {
      servoOut.write(ANGLE_CLOSED);
      state_ir_out = false;
    }
  }

  if (state_payment) {
    StaticJsonDocument<256> docReply;
    docReply["target"] = "PAYMENT";
    docReply["status"] = "SUCCESS";
    docReply["method"] = payment_method;
    docReply["invoice"] = payment_invoice;
    docReply["cost"] = payment_cost;
    
    MqttMessage msgStruct;
    serializeJson(docReply, msgStruct.payload);
    publishMQTT(topic_control, msgStruct.payload);
    state_payment = false;
    pushOLEDMessage("THANH TOAN XONG");
  }

  checkSensors();
  updateOLED();
}