#include <Wire.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Adafruit_BME280.h>
#include <Adafruit_CCS811.h>
#include <time.h>

#define SERIAL_BAUD 9600
#define NTP_SERVER "192.168.108.11"

// ---- I2C / Qwiic ----
TwoWire& I2C = Wire1;            // Qwiic on UNO R4 WiFi (IIC0)
constexpr uint8_t BME_ADDR = 0x77;
constexpr uint8_t CCS_ADDR = 0x5A;

// ---- WiFi / MQTT ----
const char* ssid = "h4prog";
const char* password = "1234567890";
const char* device_id = "WeatherStation";
const char* mqtt_server = "192.168.108.11";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "weather";
const char* mqtt_pass   = "Datait2025!";
const char* mqtt_topic  = "sensor/weather";

WiFiClient espClient;
PubSubClient client(espClient);

// ---- Sensors ----
Adafruit_BME280 bme280;
Adafruit_CCS811 ccs811;

// ---- NTP (ms-capable timebase) ----
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, NTP_SERVER, 0, 60 * 1000);

static uint32_t tb_epoch_sec = 0;
static uint32_t tb_millis    = 0;
static bool     tb_valid     = false;
static unsigned long last_refresh_ms = 0;

static unsigned long next_wifi_attempt_ms = 0;
static unsigned long next_mqtt_attempt_ms = 0;
static bool wifi_was_connected = false;

static void makeTimestampUTCms(char out[48]) {
  if (!tb_valid) { snprintf(out, 48, "unsynced-%lu", (unsigned long)millis()); return; }
  uint32_t delta_ms = millis() - tb_millis;
  uint32_t sec  = tb_epoch_sec + (delta_ms / 1000);
  uint16_t msec = delta_ms % 1000;
  time_t t = (time_t)sec;
  struct tm *tmv = gmtime(&t);
  if (!tmv) { snprintf(out, 48, "unsynced-%lu", (unsigned long)millis()); return; }
  char base[32];
  strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", tmv);
  snprintf(out, 48, "%s.%03uZ", base, msec);
}
static bool ntpReady() { return ntp.getEpochTime() > 1700000000UL; }
static void resetTimebaseFromNTP() {
  if (ntpReady()) {
    tb_epoch_sec = (uint32_t)ntp.getEpochTime();
    tb_millis    = millis();
    tb_valid     = true;
    char ts[48]; makeTimestampUTCms(ts);
    Serial.print("[NTP] Time updated: "); Serial.println(ts);
  }
}
static void ensureNTP() {
  if (ntp.update()) { resetTimebaseFromNTP(); return; }
  unsigned long deadline = millis() + 5000;
  while (!ntp.update() && millis() < deadline) delay(200);
  resetTimebaseFromNTP();
}
static void maybeRefreshTimebase() {
  if (ntp.update()) resetTimebaseFromNTP();
  const unsigned long now = millis();
  if (now - last_refresh_ms >= 300000UL) { ntp.update(); resetTimebaseFromNTP(); last_refresh_ms = now; }
}

// ---- WiFi / MQTT helpers ----
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifi_was_connected) {        // just reconnected
      ntp.begin();                     // re-arm NTP socket
      ensureNTP();                     // refresh baseline
      wifi_was_connected = true;
    }
    return true;
  }
  wifi_was_connected = false;

  // retry at most every few seconds (exponential-ish backoff)
  if (millis() < next_wifi_attempt_ms) return false;
  static uint8_t attempt = 0;

  WiFi.end(); delay(50);
  WiFi.begin(ssid, password);

  unsigned long wait = 2000UL << (attempt < 6 ? attempt : 6); // 2s .. 128s
  next_wifi_attempt_ms = millis() + wait;
  if (attempt < 10) attempt++;
  Serial.println("[WiFi] reconnect attempt");
  return false;
}
bool ensureMQTT() {
  if (!ensureWiFi()) return false;
  if (client.connected()) return true;

  if (millis() < next_mqtt_attempt_ms) return false;

  // unique client id avoids broker kicking previous session
  byte mac[6]; WiFi.macAddress(mac);
  char cid[40];
  snprintf(cid, sizeof(cid), "ArduinoWeather-%02X%02X%02X", mac[3], mac[4], mac[5]);

  // optional: LWT so you know if device drops
  bool ok = client.connect(
    cid, mqtt_user, mqtt_pass,
    "sensor/weather/status", 1, true, "offline"
  );

  if (ok) {
    client.publish("sensor/weather/status", "online", true);
    next_mqtt_attempt_ms = millis() + 10000; // next health check
    Serial.println("[MQTT] connected");
    return true;
  } else {
    Serial.print("[MQTT] failed rc="); Serial.println(client.state());
    next_mqtt_attempt_ms = millis() + 5000;
    return false;
  }
}

void sendMQTT(float tempC, float humRH, float pres_hPa, uint16_t eco2, uint16_t tvoc) {
  maybeRefreshTimebase();
  char ts[48]; makeTimestampUTCms(ts);
  char payload[512];
  snprintf(payload, sizeof(payload),
    "[{\"device_id\":\"%s\",\"temp\":%.2f,\"hum\":%.2f,\"pres_hPa\":%.2f,"
    "\"eco2\":%u,\"tvoc\":%u,\"timestamp\":\"%s\"}]",
    device_id, tempC, humRH, pres_hPa, eco2, tvoc, ts
  );
  if (!client.publish(mqtt_topic, payload)) {
    Serial.println("MQTT publish failed (will retry after reconnect).");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  // ---- I2C on Qwiic ----
  I2C.begin();
  I2C.setClock(100000);  // start conservative for daisy chains; try 400k later

  ensureWiFi();
  ntp.begin();
  ensureNTP();

  client.setServer(mqtt_server, mqtt_port);
  client.setKeepAlive(30);
  client.setSocketTimeout(10);
  client.setBufferSize(1024);

  // ---- Sensors on Wire1 with explicit addresses ----
  if (!bme280.begin(BME_ADDR, &I2C)) {
    Serial.println("Could not find BME280 on Wire1 @ 0x77!");
    while (1) delay(100);
  }

  if (!ccs811.begin(CCS_ADDR, &I2C)) {      // pass addr + bus
    Serial.println("CCS811 not found on Wire1 @ 0x5A. Check wiring.");
    while (1) delay(100);
  }

  Serial.println("CCS811 warming up...");
  delay(10000);
}

void loop() {
  ensureWiFi();
  ensureMQTT();
  client.loop();

  maybeRefreshTimebase();

  // ---- Read sensors ----
  float tempC = bme280.readTemperature();   // °C
  float humRH = bme280.readHumidity();      // %RH
  // Adafruit_BME280 returns pressure in Pascals:
  float pres_hPa = bme280.readPressure(); 

  if (!isnan(tempC) && !isnan(humRH)) {
    ccs811.setEnvironmentalData(humRH, tempC); // RH %, Temp °C
  }

  if (ccs811.available()) {
    if (!ccs811.readData()) {
      uint16_t eco2 = ccs811.geteCO2();
      uint16_t tvoc = ccs811.getTVOC();

      Serial.print("T: "); Serial.print(tempC,2); Serial.print(" °C\t");
      Serial.print("RH: "); Serial.print(humRH,2); Serial.print(" %\t");
      Serial.print("P: "); Serial.print(pres_hPa,2); Serial.print(" Pa\t");
      Serial.print("eCO2: "); Serial.print(eco2); Serial.print(" ppm\t");
      Serial.print("TVOC: "); Serial.print(tvoc); Serial.println(" ppb");

      sendMQTT(tempC, humRH, pres_hPa, eco2, tvoc);
    } else {
      Serial.println("CCS811 ERROR: Failed to read data.");
    }
  } else {
    Serial.println("CCS811 not ready.");
  }

  delay(5000);
}