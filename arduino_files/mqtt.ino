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

void publishMQTT(const char* topic, const char* payload){
  MqttMessage msg;
  strncpy(msg.topic, topic, sizeof(msg.topic)-1);
  msg.topic[sizeof(msg.topic)-1] = '\0';
  strncpy(msg.payload, payload.c_str(), sizeof(msg.payload)-1);
  msg.payload[sizeof(msg.payload)-1] = '\0';
  xQueueSend(mqttTxQueue, &msg, 0);
  Serial.printf("[MQTT RX] %s -> %s\n", msg.topic, msg.payload);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("✅ Đã kết nối MQTT Broker!");
  // Subscribe với QoS 1
  mqttClient.subscribe(topic_control, 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("❌ Mất kết nối MQTT Broker.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttMessage( char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) 
{
    MqttMessage msg;
    // Copy topic
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';

    // Copy payload an toàn
    size_t copyLen = (len < sizeof(msg.payload) - 1) ? len : sizeof(msg.payload) - 1;
    memcpy(msg.payload, payload, copyLen);
    msg.payload[copyLen] = '\0';

    xQueueSend(mqttRxQueue, &msg, 0);
}

void TaskMQTT_Code(void * pvParameters){
  MqttMessage msg;
  for(;;){
    if(mqttClient.connected()){
      if(xQueueReceive(mqttTxQueue, &msg, portMAX_DELAY) == pdTRUE){
          mqttClient.publish(msg.topic, 1, false, msg.payload);
          Serial.printf("[MQTT] %s -> %s\n", msg.topic, msg.payload);
      }
    }
  }
}

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