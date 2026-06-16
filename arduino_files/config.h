const char* ssid = "-.-";
const char* password = "0387269547";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Các topic MQTT
const char* topic_sensor = "ptithcm_2025/smart_parking/sensors"; 
const char* topic_control = "ptithcm_2025/smart_parking/control"; 
const char* topic_heart = "ptithcm_2025/smart_parking/heartbeat";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define ANGLE_OPEN 90
#define ANGLE_CLOSED 0

#define SERVO_IN_PIN 25
#define SERVO_OUT_PIN 26

#define IR_GATE_IN 34
#define IR_GATE_OUT 35

#define IR_SLOT_1 32
#define IR_SLOT_2 33

#define BUZZER_PIN 27