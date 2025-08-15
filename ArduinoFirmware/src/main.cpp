#include <Wire.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <BME280I2C.h>
#include <Adafruit_CCS811.h>
#include <time.h>

#define SERIAL_BAUD 9600

#define NTP_SERVER "192.168.108.11"  // your LAN NTP

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
BME280I2C bme;
Adafruit_CCS811 ccs811;

WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, NTP_SERVER, 0 /*UTC offset*/, 60 * 1000 /*update every 60s*/);

// ---- ms-capable timebase (NTP seconds + millis() delta) ----
static uint32_t tb_epoch_sec = 0;   // last NTP epoch seconds
static uint32_t tb_millis    = 0;   // millis() captured at that moment
static bool     tb_valid     = false;
static unsigned long last_refresh_ms = 0; // our own periodic refresh guard

// Build ISO 8601 UTC with milliseconds: 2025-08-11T10:50:21.189Z
static void makeTimestampUTCms(char out[48]) {
  if (!tb_valid) {
    snprintf(out, 48, "unsynced-%lu", (unsigned long)millis());
    return;
  }
  uint32_t now_ms   = millis();
  uint32_t delta_ms = now_ms - tb_millis;          // unsigned handles rollover
  uint32_t sec      = tb_epoch_sec + (delta_ms / 1000);
  uint16_t msec     = delta_ms % 1000;

  time_t t = (time_t)sec;
  struct tm *tmv = gmtime(&t); // UTC
  if (!tmv) {
    snprintf(out, 48, "unsynced-%lu", (unsigned long)millis());
    return;
  }

  char base[32];
  strftime(base, sizeof(base), "%Y-%m-%dT%H:%M:%S", tmv);
  snprintf(out, 48, "%s.%03uZ", base, msec);
}

static bool ntpReady() {
  return ntp.getEpochTime() > 1700000000UL; // ~2023-11 sanity floor
}

static void resetTimebaseFromNTP() {
  if (ntpReady()) {
    tb_epoch_sec = (uint32_t)ntp.getEpochTime();
    tb_millis    = millis();
    tb_valid     = true;

    // --- SERIAL INDICATOR ---
    char ts[48];
    makeTimestampUTCms(ts);
    Serial.print("[NTP] Time updated from server: ");
    Serial.println(ts);
  }
}

// Try a bounded wait during setup/first use
static void ensureNTP() {
  if (ntp.update()) { resetTimebaseFromNTP(); return; }
  unsigned long deadline = millis() + 5000; // up to 5s
  while (!ntp.update() && millis() < deadline) {
    delay(200);
  }
  resetTimebaseFromNTP();
}

// call this often (in loop) to keep time fresh
static void maybeRefreshTimebase() {
  // normal non-blocking update attempt
  if (ntp.update()) {
    resetTimebaseFromNTP();
  }

  // extra guard: every ~5 minutes, attempt another refresh
  const unsigned long now = millis();
  if (now - last_refresh_ms >= 300000UL) { // 5 min
    ntp.update();               // try again (non-blocking)
    resetTimebaseFromNTP();     // capture if new time arrived
    last_refresh_ms = now;
  }
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  uint8_t kicks = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // every ~10s, kick the radio to avoid stale states
    if (++kicks % 20 == 0) {
      Serial.print(" (rebegin) ");
      WiFi.end();
      delay(100);
      WiFi.begin(ssid, password);
    }
  }
  Serial.println(" Connected!");

  // when Wi-Fi comes back, refresh NTP baseline
  ntp.begin();
  ensureNTP();
}

void connectMQTT() {
  if (client.connected()) return;

  uint8_t tries = 0;
  while (!client.connected() && tries < 5) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ArduinoWeather", mqtt_user, mqtt_pass)) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 2s");
      delay(2000);
      tries++;
    }
  }
}

void sendMQTT(float temp, float hum, float pres, uint16_t eco2, uint16_t tvoc) {
  // keep timebase fresh; non-blocking most of the time
  maybeRefreshTimebase();

  char ts[48];
  makeTimestampUTCms(ts);

  char payload[512];
  snprintf(payload, sizeof(payload),
    "[{\"device_id\":\"%s\",\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,"
    "\"eco2\":%u,\"tvoc\":%u,\"timestamp\":\"%s\"}]",
    device_id, temp, hum, pres, eco2, tvoc, ts
  );

  if (!client.publish(mqtt_topic, payload)) {
    Serial.println("MQTT publish failed (will retry after reconnect).");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Wire.begin();

  connectWiFi();                 // Wi-Fi up first
  ntp.begin();                   // start NTP client
  ensureNTP();                   // try to sync once (bounded)

  client.setServer(mqtt_server, mqtt_port);
  client.setKeepAlive(30);
  client.setSocketTimeout(10);
  client.setBufferSize(1024);

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
    while (1) { delay(100); }
  }

  Serial.println("CCS811 warming up...");
  delay(10000);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!client.connected())         connectMQTT();
  client.loop();

  // keep timebase fresh even if no publishes happen for a while
  maybeRefreshTimebase();

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

      Serial.print("Temp: "); Serial.print(temp); Serial.print("°C\t");
      Serial.print("Humidity: "); Serial.print(hum); Serial.print("% RH\t");
      Serial.print("Pressure: "); Serial.print(pres); Serial.print(" Pa\t");
      Serial.print("CO2: "); Serial.print(eco2); Serial.print(" ppm\t");
      Serial.print("TVOC: "); Serial.print(tvoc); Serial.print(" ppb\t");

      char timestamp[48];
      makeTimestampUTCms(timestamp);
      Serial.print("Timestamp: ");  Serial.println(timestamp);

      sendMQTT(temp, hum, pres, eco2, tvoc);
    } else {
      Serial.println("CCS811 ERROR: Failed to read data.");
    }
  } else {
    Serial.println("CCS811 not ready.");
  }

  delay(5000);
}
