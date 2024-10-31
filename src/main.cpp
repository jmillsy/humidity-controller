
#include "SparkFun_SCD30_Arduino_Library.h"
#include "time.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <ESP32Ping.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <config.h>

Adafruit_NeoPixel neopixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
SCD30 airSensor;

enum { WHITE, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, OFF };

uint32_t LED_Colors[8] = {
    // Red, Green, Blue
    0xFFFFFF, // White
    0xFF0000, // Red
    0x00FF00, // Green
    0x0000FF, // Blue
    0xFFC000, // Yellow
    0x00B0FF, // Cyan
    0xD000B0, // Magenta
    0x000000, // Off | Black
};

void pushSensorDataToMQTT(float, float, float, bool);
void setup_wifi();
void setup_sensors();
void publishHomeAssistantConfigMessage();
void reconnect();
void publishGenericMessage(const char *topic, const char *payload);
void setLedColor(uint32_t color);
void callback(char *topic, byte *payload, unsigned int length);

// Init WiFi/WiFiUDP, NTP and MQTT Client
WiFiClient espClient;
PubSubClient client(mqtt_server, mqtt_port, espClient);

// Prefix for the MQTT Client Identification
String clientId = "esp32-client-";

bool DISCOVERY_ENABLED = false;

struct TopicInfo {
  const String discovery_topic;
  const String state_topic;
  const String name;
  const String unit;
  const String json_key_name;
};

const TopicInfo CO2_PPM_TOPIC = {
    .discovery_topic = "homeassistant/sensor/grow-station-co2ppm/config",
    .state_topic = "homeassistant/sensor/grow-station-co2ppm/state",
    .name = "CO2",
    .unit = "PPM",
    .json_key_name = "co2ppm"};

const TopicInfo TEMPERATURE_TOPIC = {
    .discovery_topic = "homeassistant/sensor/grow-station-temp/config",
    .state_topic = "homeassistant/sensor/grow-station-temp/state",
    .name = "Temperature",
    .unit = "C",
    .json_key_name = "temp"};

const TopicInfo HUMIDITY_TOPIC = {
    .discovery_topic = "homeassistant/sensor/grow-station-humidity/config",
    .state_topic = "homeassistant/sensor/grow-station-humidity/state",
    .name = "Humidity",
    .unit = "%",
    .json_key_name = "humidity"};

const TopicInfo *TOPICS[] = {&CO2_PPM_TOPIC, &TEMPERATURE_TOPIC,
                             &HUMIDITY_TOPIC};

const int TOPICS_SIZE = sizeof(TOPICS) / sizeof(TOPICS[0]);
const int fanPin = 5; // PWM-capable pin

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");

  // define payload into a string
  String payload_str = "";
  for (int i = 0; i < length; i++) {
    payload_str += (char)payload[i];
  }

  Serial.print("Payload as string: ");
  Serial.println(payload_str);

  // Changes the fan state
  if (payload_str == "on") {
    ledcWrite(0, 255); // 100% duty cycle
  } else if (payload_str == "off") {
    ledcWrite(0, 0); // 0% duty cycle
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Hello World!");

  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  clientId += String(random(0xffff), HEX);

  neopixel.begin();
  neopixel.clear();
  neopixel.setBrightness(50);
  neopixel.show();

  // Setup Wifi & MQTT
  setup_wifi();

  // Sensor Setup
  setup_sensors();
}

unsigned long previousMillis = 0;
const long interval = 10000;
void loop() {

  client.loop();

  unsigned long currentMillis = millis();

  if (airSensor.dataAvailable() && currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    pushSensorDataToMQTT(airSensor.getCO2(), airSensor.getTemperature(),
                         airSensor.getHumidity(), true);

    Serial.print("co2(ppm):");
    Serial.print(airSensor.getCO2());

    Serial.print(" temp(C):");
    Serial.print(airSensor.getTemperature(), 1);

    Serial.print(" humidity(%):");
    Serial.print(airSensor.getHumidity(), 1);

    Serial.println();
  }
}

void setup_sensors() {

  Wire.begin();

  // pwm fan setup
  ledcSetup(0, 25000, 8); // Channel 0, 25 kHz, 8-bit resolution
  ledcAttachPin(fanPin, 0);

  // Start sensor using the Wire port, but disable the auto-calibration
  if (airSensor.begin(Wire, false) == false) {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }

  uint16_t settingVal; // The settings will be returned in settingVal

  if (airSensor.getForcedRecalibration(&settingVal) == true) // Get the setting
  {
    Serial.print("Forced recalibration factor (ppm) is ");
    Serial.println(settingVal);
  } else {
    Serial.print("getForcedRecalibration failed! Freezing...");
    while (1)
      ; // Do nothing more
  }

  if (airSensor.getMeasurementInterval(&settingVal) == true) // Get the setting
  {
    Serial.print("Measurement interval (s) is ");
    Serial.println(settingVal);
  } else {
    Serial.print("getMeasurementInterval failed! Freezing...");
    while (1)
      ; // Do nothing more
  }

  if (airSensor.getTemperatureOffset(&settingVal) == true) // Get the setting
  {
    Serial.print("Temperature offset (C) is ");
    Serial.println(((float)settingVal) / 100.0, 2);
  } else {
    Serial.print("getTemperatureOffset failed! Freezing...");
    while (1)
      ; // Do nothing more
  }

  if (airSensor.getAltitudeCompensation(&settingVal) == true) // Get the setting
  {
    Serial.print("Altitude offset (m) is ");
    Serial.println(settingVal);
  } else {
    Serial.print("getAltitudeCompensation failed! Freezing...");
    while (1)
      ; // Do nothing more
  }

  if (airSensor.getFirmwareVersion(&settingVal) == true) // Get the setting
  {
    Serial.print("Firmware version is 0x");
    Serial.println(settingVal, HEX);
  } else {
    Serial.print("getFirmwareVersion! Freezing...");
    while (1)
      ; // Do nothing more
  }

  Serial.print("Auto calibration set to ");
  if (airSensor.getAutoSelfCalibration() == true)
    Serial.println("true");
  else
    Serial.println("false");
}

void setup_wifi() {
  Log.notice(F("Connecting to WiFi network: %s (password: %s)" CR), ssid,
             password);
  WiFi.begin(ssid, password);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(clientId.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    attempts++;
    delay(500);
    Serial.print(".");
    setLedColor(LED_Colors[YELLOW]);
  }

  setLedColor(LED_Colors[OFF]);

  Serial.println("");

  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.print(WiFi.localIP());
  Serial.println("");

  bool success = Ping.ping(mqtt_server, 3);

  if (!success) {
    Log.error(F("Ping failed to MQTT server at %s" CR), mqtt_server);
    setLedColor(LED_Colors[RED]);
    delay(1000);
    setLedColor(LED_Colors[OFF]);
    return;
  } else {
    Log.notice(F("Ping to MQTT server at %s OK" CR), mqtt_server);
  }

  // Configure mqtt
  client.setKeepAlive(300);     // 5 minutes
  client.setSocketTimeout(300); // 5 minutes
  client.setBufferSize(512);

  client.setCallback(callback);

  reconnect();

  publishHomeAssistantConfigMessage();
}

void pushSensorDataToMQTT(float co2ppm, float temp, float humidity,
                          bool debug = false) {
  StaticJsonDocument<512> state_info;
  state_info[CO2_PPM_TOPIC.json_key_name] = co2ppm;
  state_info[TEMPERATURE_TOPIC.json_key_name] = temp;
  state_info[HUMIDITY_TOPIC.json_key_name] = humidity;

  char stateInfoAsJson[512];
  serializeJson(state_info, stateInfoAsJson);

  publishGenericMessage(CO2_PPM_TOPIC.state_topic.c_str(), stateInfoAsJson);
  publishGenericMessage(TEMPERATURE_TOPIC.state_topic.c_str(), stateInfoAsJson);
  publishGenericMessage(HUMIDITY_TOPIC.state_topic.c_str(), stateInfoAsJson);
}

void publishGenericMessage(const char *topic, const char *payload) {

  reconnect();

  Log.info(F("Publishing to topic %s" CR), topic);
  Log.info(F("Payload %s" CR), payload);

  boolean success = client.publish(topic, payload);

  if (success) {
    neopixel.fill(LED_Colors[WHITE]);
    neopixel.show();
    delay(1000);
    neopixel.fill(LED_Colors[OFF]);
    neopixel.show();
  } else {
    setLedColor(LED_Colors[RED]);
    delay(1000);
  }
}

void publishHomeAssistantConfigMessage() {
  StaticJsonDocument<512> device;
  device["name"] = "Grow Station";
  device["identifiers"] = "1234";

  const int TOPICS_SIZE = sizeof(TOPICS) / sizeof(TOPICS[0]);
  for (int index = 0; index < TOPICS_SIZE; index++) {
    StaticJsonDocument<512> config;
    config["name"] = TOPICS[index]->name;
    config["state_topic"] = TOPICS[index]->state_topic;
    config["unit_of_measurement"] = TOPICS[index]->unit;
    String unique_id = "scd30" + TOPICS[index]->name;
    unique_id.replace(" ", "");
    unique_id.toLowerCase();
    config["unique_id"] = unique_id;

    String value_template = "{{value_json." + TOPICS[index]->json_key_name +
                            " | float | round(2) }}";
    config["value_template"] = value_template;

    // Add the device object to the main config
    config["device"] = device;

    // Serialize the JSON object to a string
    char configAsJson[512];
    serializeJson(config, configAsJson);

    publishGenericMessage(TOPICS[index]->discovery_topic.c_str(), configAsJson);
  }
}

/**
 * Used to connect or reconnect to the MQTT server
 */
void reconnect() {
  Log.info("Reconnecting.." CR);

  int maxAttempts = 3;
  int attempt = 0;
  while (!client.connected() && attempt < maxAttempts) {
    attempt++;

    Log.notice(F("Attempting MQTT connection to %s" CR), mqtt_server);

    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Log.notice(F("Connected as clientId %s :-)" CR), clientId.c_str());
      bool sub = client.subscribe("growstation/fan");
      Log.info(F("Subscribed to topic growstation/fan: %s" CR),
               sub ? "true" : "false");
    } else {
      neopixel.setBrightness(250);
      setLedColor(LED_Colors[RED]);
      Log.error(F("{failed, rc=%d try again in 5 seconds}" CR), client.state());
      delay(1000);
    }
  }
}

void setLedColor(uint32_t color) {
  neopixel.fill(color);
  neopixel.show();
}