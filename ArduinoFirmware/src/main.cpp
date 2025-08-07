#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BME280I2C.h>
#include <Adafruit_CCS811.h>

#define SERIAL_BAUD 9600

// ====== MQTT CONFIG ======
const char* ssid = "h4prog";
const char* password = "1234567890";

const char* device_id = "WeatherStation";

const char* mqtt_server = "192.168.108.11";
const int mqtt_port = 1883;
const char* mqtt_user = "weather";
const char* mqtt_pass = "Datait2025!";
const char* mqtt_topic = "sensor/weather";

// ====== INIT ======
WiFiClient espClient;
PubSubClient client(espClient);
BME280I2C bme;
Adafruit_CCS811 ccs811;

// ====== CONNECT WIFI ======
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

// ====== CONNECT MQTT ======
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ArduinoWeather", mqtt_user, mqtt_pass)) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" trying again in 5 sec...");
      delay(5000);
    }
  }
}

// ====== SEND SENSOR DATA ======
void sendMQTT(float temp, float hum, float pres, uint16_t eco2, uint16_t tvoc) {
  char payload[512];
  snprintf(payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"eco2\":%d,\"tvoc\":%d}",
    device_id, temp, hum, pres, eco2, tvoc
  );

  client.publish(mqtt_topic, payload);
}

// ====== SETUP ======
void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();

  connectWiFi();
  client.setServer(mqtt_server, mqtt_port);

  while (!bme.begin()) {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch (bme.chipModel()) {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }

  if (!ccs811.begin()) {
    Serial.println("CCS811 not found. Check wiring.");
    while (1);
  }

  Serial.println("CCS811 warming up...");
  delay(10000);
}

// ====== MAIN SENSOR LOOP ======
void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit = BME280::TempUnit_Celsius;
  BME280::PresUnit presUnit = BME280::PresUnit_Pa;

  bme.read(pres, temp, hum, tempUnit, presUnit);
  if (!isnan(temp) && !isnan(hum)) {
    ccs811.setEnvironmentalData(hum, temp);
  }

  if (ccs811.available()) {
    if (!ccs811.readData()) {
      uint16_t eco2 = ccs811.geteCO2();
      uint16_t tvoc = ccs811.getTVOC();

      // Serial Output
      Serial.print("Temp: "); Serial.print(temp); Serial.print("°C\t");
      Serial.print("Humidity: "); Serial.print(hum); Serial.print("% RH\t");
      Serial.print("Pressure: "); Serial.print(pres); Serial.print(" Pa\t");
      Serial.print("CO2: "); Serial.print(eco2); Serial.print(" ppm\t");
      Serial.print("TVOC: "); Serial.print(tvoc); Serial.println(" ppb");

      // MQTT Output
      sendMQTT(temp, hum, pres, eco2, tvoc);

    } else {
      Serial.println("CCS811 ERROR: Failed to read data.");
    }
  } else {
    Serial.println("CCS811 not ready.");
  }

  delay(5000);
}
