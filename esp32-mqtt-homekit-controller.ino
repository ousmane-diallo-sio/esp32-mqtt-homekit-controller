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

const char WIFI_SSID[] = "wifi_ssid";
const char WIFI_PASSWORD[] = "wifi_password";

const char MQTT_BROKER_ADRRESS[] = "test.mosquitto.org";
const int MQTT_PORT = 1884;
const char MQTT_CLIENT_ID[] = "esp32-homekit-controller-86";
const char MQTT_USERNAME[] = "rw";
const char MQTT_PASSWORD[] = "readwrite";

const char PUBLISH_TOPIC_PLAYER1[] = "esgi/imoc5/pong/player1/position";
const char PUBLISH_TOPIC_PLAYER2[] = "esgi/imoc5/pong/player2/position";
int playerId = 1;

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

int prevXValue = 0;
int prevYValue = 0;
int prevButtonAState = HIGH;

int normalizeJoystickValue(int rawValue) {
  return map(rawValue, JOYSTICK_MIN_VALUE, JOYSTICK_MAX_VALUE, -100, 100);
}

void connectToMQTT() {
  mqtt.begin(MQTT_BROKER_ADRRESS, MQTT_PORT, network);
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

void sendMQTT(int xValue, int yValue, int buttonAState, bool shouldSendX, bool shouldSendY, bool shouldSendButtonAState) {
  bool shouldSendMQTT = false;
  StaticJsonDocument<200> message;
  message["timestamp"] = millis();

  if (shouldSendX) {
    message["xValue"] = xValue;
    shouldSendMQTT = true;
  }
  if (shouldSendY) {
    message["yValue"] = yValue;
    shouldSendMQTT = true;
  }
  if (shouldSendButtonAState) {
    message["buttonA"] = buttonAState;
    shouldSendMQTT = true;
  }

  if (!shouldSendMQTT) return;

  analogWrite(EXT_LED_PIN, 10);

  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  const char* playerTopic = (playerId == 2) ? PUBLISH_TOPIC_PLAYER2 : PUBLISH_TOPIC_PLAYER1;

  bool published = mqtt.publish(playerTopic, messageBuffer);
  if(!published) {
    Serial.print("Failed to publish to MQTT broker");
    sendMQTT(xValue, yValue, buttonAState, shouldSendX, shouldSendY, shouldSendButtonAState);
    return;
  }
  Serial.println("ESP32 - sent to MQTT:");
  Serial.print("- topic: ");
  Serial.println(playerTopic);
  Serial.print("- payload:");
  Serial.println(messageBuffer);
  Serial.println();
  analogWrite(EXT_LED_PIN, 0);
}

void setup() {
  Serial.begin(9600);

  // Set the ADC attenuation to 11 dB (up to ~3.3V input)
  analogSetAttenuation(ADC_11db);

  pinMode(EXT_LED_PIN, OUTPUT);
  pinMode(BUTTON_A_PIN, INPUT);
  pinMode(BUTTON_PLAYER_PIN, INPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("No Wifi connexion");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
  }

  connectToMQTT();
}

void loop() {
  if (!mqtt.connected()) {
    Serial.println("ESP32 - Lost connection to MQTT broker. Trying again...");
    connectToMQTT();
  }

  int xValue = analogRead(VRX_PIN);
  int yValue = analogRead(VRY_PIN);
  int buttonAState = digitalRead(BUTTON_A_PIN);
  int buttonPlayerState = digitalRead(BUTTON_PLAYER_PIN);

  bool shouldSendXValue = false;
  bool shouldSendYValue = false;
  bool shouldSendButtonAState = false;

  int mappedXValue = normalizeJoystickValue(xValue);
  int mappedYValue = normalizeJoystickValue(yValue);


  if (abs(xValue - prevXValue) > JOYSTICK_DEADZONE) {
    prevXValue = xValue;
    // Serial.print("new value x : ");
    // Serial.println(xValue);
    Serial.print("new mapped value x : ");
    Serial.println(mappedXValue);
    shouldSendXValue = true;
  }

  if (abs(yValue - prevYValue) > JOYSTICK_DEADZONE) {
    prevYValue = yValue;
    // Serial.print("new value y : ");
    // Serial.println(yValue);
    Serial.print("new mapped value y : ");
    Serial.println(mappedYValue);
    shouldSendYValue = true;
  }

  if (buttonAState != prevButtonAState) {
    prevButtonAState = buttonAState;
    Serial.print("Toggle A button : ");
    Serial.println(buttonAState);
    shouldSendButtonAState = true;
  }

  if (buttonPlayerState == HIGH) {
    analogWrite(EXT_LED_PIN, 200);
    playerId = playerId == 1 ? 2 : 1;
    Serial.print("Change to player ");
    Serial.println(playerId);
    delay(300);
    analogWrite(EXT_LED_PIN, 0);
  }

  sendMQTT(mappedXValue, mappedYValue, buttonAState, shouldSendXValue, shouldSendYValue, shouldSendButtonAState);

  delay(50);
}
