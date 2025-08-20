#include <Wire.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Adafruit_BME280.h>
#include <Adafruit_CCS811.h>
#include <time.h>

/* ========= MQTT / TLS CONFIG =========
   If your broker is TLS-only (common on managed or hardened Mosquitto),
   set MQTT_USE_TLS to 1 and set MQTT_PORT to 8883.
   For plain Mosquitto on LAN, leave it at 0 and port 1883.
*/
#define MQTT_USE_TLS  0
#define MQTT_PORT     (MQTT_USE_TLS ? 8883 : 1883)

#define SERIAL_BAUD 9600
#define NTP_SERVER  "192.168.108.11"

// ---- I2C / Qwiic ----
TwoWire& I2C = Wire1;            // UNO R4 WiFi Qwiic = Wire1/IIC0
constexpr uint8_t BME_ADDR = 0x77;
constexpr uint8_t CCS_ADDR = 0x5A;

// ---- WiFi / MQTT ----
const char* ssid = "h4prog";
const char* password = "1234567890";
const char* device_id = "WeatherStation";
const char* mqtt_server = "192.168.108.11";
const char* mqtt_user   = "weather";
const char* mqtt_pass   = "Datait2025!";

// Choose MQTT client type based on TLS setting
#if MQTT_USE_TLS
  WiFiSSLClient   netClient;
#else
  WiFiClient      netClient;
#endif
PubSubClient   client(netClient);

// ---- Sensors ----
Adafruit_BME280  bme280;
Adafruit_CCS811  ccs811;
bool bme_ok = false;
bool ccs_ok = false;

// ---- NTP (ms-capable timebase) ----
WiFiUDP ntpUDP;
NTPClient ntp(ntpUDP, NTP_SERVER, 0, 60 * 1000);

static uint32_t tb_epoch_sec = 0;
static uint32_t tb_millis    = 0;
static bool     tb_valid     = false;
static unsigned long last_refresh_ms = 0;

// ---- Reconnect/backoff state ----
static unsigned long next_wifi_attempt_ms = 0;
static unsigned long next_mqtt_attempt_ms = 0;
static bool wifi_was_connected = false;

// ---- Simple one-message backlog for MQTT ----
static bool have_backlog = false;
static char backlog[512];

// ---- Timing ----
static unsigned long next_sample_ms = 0;
static const unsigned long SAMPLE_PERIOD_MS = 5000;

// ---------------- Time helpers ----------------
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
  if (now - last_refresh_ms >= 300000UL) { // 5 min
    ntp.update(); resetTimebaseFromNTP(); last_refresh_ms = now;
  }
}

// ---------------- Diagnostics ----------------
bool probeBrokerTCP() {
  // Quick reachability test to detect "port closed / refused" vs auth issues
#if MQTT_USE_TLS
  WiFiSSLClient probe;
#else
  WiFiClient probe;
#endif
  probe.setTimeout(3000);
  Serial.print("[DIAG] Probing broker TCP ");
  Serial.print(mqtt_server); Serial.print(":"); Serial.println(MQTT_PORT);

  if (probe.connect(mqtt_server, MQTT_PORT)) {
    Serial.println("[DIAG] TCP connect OK (port open).");
    probe.stop();
    return true;
  } else {
    Serial.println("[DIAG] TCP connect FAILED (refused/timeout).");
    return false;
  }
}

// ---------------- Network helpers ----------------
bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifi_was_connected) {        // just reconnected
      IPAddress ip = WiFi.localIP();
      Serial.print("[WiFi] connected, IP="); Serial.println(ip);
      ntp.begin();                     // re-arm NTP socket
      ensureNTP();                     // refresh baseline
      wifi_was_connected = true;
    }
    return true;
  }
  wifi_was_connected = false;

  // backoff between attempts
  if (millis() < next_wifi_attempt_ms) return false;
  static uint8_t attempt = 0;

  WiFi.end(); delay(50);
  WiFi.begin(ssid, password);

  unsigned long wait = 2000UL << (attempt < 6 ? attempt : 6); // 2s..128s
  next_wifi_attempt_ms = millis() + wait;
  if (attempt < 10) attempt++;
  Serial.println("[WiFi] reconnect attempt");
  return false;
}

bool ensureMQTT() {
  if (!ensureWiFi()) return false;
  if (client.connected()) return true;
  if (millis() < next_mqtt_attempt_ms) return false;

  // Unique client id
  byte mac[6]; WiFi.macAddress(mac);
  char cid[40];
  snprintf(cid, sizeof(cid), "ArduinoWeather-%02X%02X%02X", mac[3], mac[4], mac[5]);

  // Connect WITH your username/password + LWT
  bool ok = client.connect(
    cid, mqtt_user, mqtt_pass,
    "sensor/weather/status", 1, true, "offline"
  );

  if (ok) {
    client.publish("sensor/weather/status", "online", true);
    next_mqtt_attempt_ms = millis() + 10000;
    Serial.println("[MQTT] connected");
    return true;
  } else {
    Serial.print("[MQTT] failed rc="); Serial.println(client.state());
    next_mqtt_attempt_ms = millis() + 5000;
    return false;
  }
}

void tryFlushBacklog() {
  if (have_backlog && client.connected()) {
    if (client.publish("sensor/weather", backlog)) {
      have_backlog = false;
      Serial.println("[MQTT] backlog flushed");
    }
  }
}

// ---------------- Sensors ----------------
bool initSensors() {
  bool ok = true;

  if (!bme_ok) {
    bme_ok = bme280.begin(BME_ADDR, &I2C);
    if (!bme_ok) Serial.println("[BME280] not found (Wire1@0x77), will retry");
    else         Serial.println("[BME280] init ok");
  }

  if (!ccs_ok) {
    ccs_ok = ccs811.begin(CCS_ADDR, &I2C);
    if (!ccs_ok) Serial.println("[CCS811] not found (Wire1@0x5A), will retry");
    else         Serial.println("[CCS811] init ok (warming 10s)"), delay(10000);
  }

  return bme_ok && ccs_ok;
}

void readAndPublish() {
  if (!bme_ok || !ccs_ok) {
    initSensors(); // background retry if one dropped
    return;
  }

  float tempC = bme280.readTemperature();   // °C
  float humRH = bme280.readHumidity();      // %RH
  float presPa = bme280.readPressure(); // Pa

  if (!isnan(tempC) && !isnan(humRH)) {
    ccs811.setEnvironmentalData(humRH, tempC); // RH %, Temp °C
  }

  if (ccs811.available()) {
    if (!ccs811.readData()) {
      uint16_t eco2 = ccs811.geteCO2();
      uint16_t tvoc = ccs811.getTVOC();

      Serial.print("T: ");   Serial.print(tempC,2);   Serial.print(" °C\t");
      Serial.print("RH: ");  Serial.print(humRH,2);   Serial.print(" %\t");
      Serial.print("P: ");   Serial.print(presPa,2);Serial.print(" Pa\t");
      Serial.print("eCO2: ");Serial.print(eco2);      Serial.print(" ppm\t");
      Serial.print("TVOC: ");Serial.print(tvoc);      Serial.println(" ppb");

      // Build payload
      char ts[48]; makeTimestampUTCms(ts);
      char payload[512];
      snprintf(payload, sizeof(payload),
        "[{\"device_id\":\"%s\",\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,"
        "\"eco2\":%u,\"tvoc\":%u,\"timestamp\":\"%s\"}]",
        device_id, tempC, humRH, presPa, eco2, tvoc, ts
      );

      // Publish or stash one-message backlog
      if (!client.connected() || !client.publish("sensor/weather", payload)) {
        strncpy(backlog, payload, sizeof(backlog));
        backlog[sizeof(backlog)-1] = '\0';
        have_backlog = true;
        Serial.println("[MQTT] queued 1 message (offline/backlog)");
      }
    } else {
      Serial.println("[CCS811] read error");
    }
  } else {
    Serial.println("[CCS811] not ready");
  }
}

// ---------------- Arduino lifecycle ----------------
void setup() {
  Serial.begin(SERIAL_BAUD);

  // I2C on Qwiic
  I2C.begin();
  I2C.setClock(100000);  // safer on longer chains

  // MQTT server/port
  client.setServer(mqtt_server, MQTT_PORT);
  client.setKeepAlive(30);
  client.setSocketTimeout(10);
  client.setBufferSize(1024);

  ensureWiFi();  // kicks off initial connect
  ntp.begin();
  ensureNTP();

  // First sensor init (non-blocking overall; CCS warmup only once)
  initSensors();

  next_sample_ms = millis(); // start sampling loop immediately
}

void loop() {
  ensureWiFi();
  ensureMQTT();
  client.loop();
  tryFlushBacklog();
  maybeRefreshTimebase();

  // 5s sampling without blocking the event loop
  unsigned long now = millis();
  if ((long)(now - next_sample_ms) >= 0) {
    readAndPublish();
    next_sample_ms = now + SAMPLE_PERIOD_MS;
  }

  delay(5);
}