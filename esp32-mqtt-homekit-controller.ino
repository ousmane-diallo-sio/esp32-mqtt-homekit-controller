#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>

#define VRX_PIN 39
#define VRY_PIN 36
#define EXT_LED_PIN 32
#define JOYSTICK_DEADZONE 50
#define JOYSTICK_MIN_VALUE 0
#define JOYSTICK_MAX_VALUE 4095
#define BUTTON_A_PIN 21
#define BUTTON_PLAYER_PIN 19

const char WIFI_SSID[] = "Pixel_4049";
const char WIFI_PASSWORD[] = "Ibnou92Pez";

const char MQTT_BROKER_ADDRESS[] = "test.mosquitto.org";
const int MQTT_PORT = 1884;
const char MQTT_CLIENT_ID[] = "esp32-homekit-controller-89";
const char MQTT_USERNAME[] = "rw";
const char MQTT_PASSWORD[] = "readwrite";

const char PUBLISH_TOPIC_PLAYER1[] = "esgi/imoc5/pong/player1/position";
const char PUBLISH_TOPIC_PLAYER2[] = "esgi/imoc5/pong/player2/position";
int playerId = 1;

WiFiClient network;
MQTTClient mqtt(256);

int prevXValue = 0;
int prevYValue = 0;
int prevButtonAState = HIGH;

int normalizeJoystickValue(int rawValue) {
  return map(rawValue, JOYSTICK_MIN_VALUE, JOYSTICK_MAX_VALUE, -100, 100);
}

void connectToMQTT() {
  mqtt.begin(MQTT_BROKER_ADDRESS, MQTT_PORT, network);
  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout");
    return;
  }
  Serial.println("ESP32 - MQTT broker Connected");
}

void sendMQTT(int xValue, int yValue, int buttonAState) {
  if (!mqtt.connected()) {
    Serial.println("ESP32 - Lost connection to MQTT broker. Reconnecting...");
    connectToMQTT();
  }

  StaticJsonDocument<200> message;
  message["timestamp"] = millis();
  message["xValue"] = xValue;  // Toujours envoyer xValue
  message["yValue"] = yValue;
  message["buttonA"] = buttonAState;

  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  const char* playerTopic = (playerId == 2) ? PUBLISH_TOPIC_PLAYER2 : PUBLISH_TOPIC_PLAYER1;

  if (!mqtt.publish(playerTopic, messageBuffer)) {
    Serial.println("Failed to publish to MQTT broker. Retrying...");
    delay(100);
    sendMQTT(xValue, yValue, buttonAState);
    return;
  }

  Serial.println("ESP32 - Sent to MQTT:");
  Serial.println(messageBuffer);
}

void setup() {
  Serial.begin(9600);
  analogSetAttenuation(ADC_11db);

  pinMode(EXT_LED_PIN, OUTPUT);
  pinMode(BUTTON_A_PIN, INPUT);
  pinMode(BUTTON_PLAYER_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi Connected!");

  connectToMQTT();
}

void loop() {
  int xValue = analogRead(VRX_PIN);
  delay(10);
  int yValue = analogRead(VRY_PIN);
  delay(10);
  int buttonAState = digitalRead(BUTTON_A_PIN);
  int buttonPlayerState = digitalRead(BUTTON_PLAYER_PIN);

  int mappedXValue = normalizeJoystickValue(xValue);
  int mappedYValue = normalizeJoystickValue(yValue);

  if (abs(xValue - prevXValue) > JOYSTICK_DEADZONE || abs(yValue - prevYValue) > JOYSTICK_DEADZONE || buttonAState != prevButtonAState) {
    prevXValue = xValue;
    prevYValue = yValue;
    prevButtonAState = buttonAState;

    sendMQTT(mappedXValue, mappedYValue, buttonAState);
  }

  if (buttonPlayerState == HIGH) {
    analogWrite(EXT_LED_PIN, 200);
    playerId = playerId == 1 ? 2 : 1;
    Serial.print("Change to player ");
    Serial.println(playerId);
    delay(300);
    analogWrite(EXT_LED_PIN, 0);
  }

  delay(50);
}
