# Weather Station Project

End-to-end IoT weather stack (school project):
- **Firmware (Arduino/PlatformIO, C++)** reads **BME280** and **CCS811**, timestamps via **NTP**, and publishes JSON over **MQTT**.
- **Ingest (C + CMake)** subscribes to the MQTT broker and stores readings in **PostgreSQL**.
- **Web/App (Next.js)** serves an API + UI with current stats and an **LLM weather report** via **Ollama** using `qwen2.5:3b-instruct`.
- **Grafana dashboards** on top of PostgreSQL.

---

## Architecture

~~~
[BME280 + CCS811] --(JSON over MQTT)--> [Mosquitto Broker]
                                           |
                                           v
                                   [C Ingest Worker]
                                           |
                                           v
                                      [PostgreSQL]
                                     /          \
                                    v            v
                              [Next.js API/UI]  [Grafana]
                                     |
                                     v
                              [Ollama LLM report]
~~~

---

## Features

- NTP-calibrated device timestamps (UTC)
- MQTT JSON publishing from firmware
- Server-side MQTT subscriber → PostgreSQL ingestion
- REST API + UI with latest conditions
- On-demand LLM “Weather Reporter” via Ollama

---

## Services (stack)

- **Chrony** (NTP Server)
- **Mosquitto** (MQTT broker)
- **PostgreSQL** (time-series storage)
- **Ollama** (local LLM, `qwen2.5:3b-instruct`)
- **Grafana** (required dashboards)

---

## Data Model

**MQTT topic:** `sensor/weather`

**Payload example:**
~~~json
{
  "device_id": "garage-01",
  "timestamp": "2025-08-15T12:34:56Z",
  "temp": 23.4,
  "hum": 52.1,
  "pres": 10007.8,
  "eco2": 612,
  "tvoc": 45
}
~~~

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/XBO6NBqk)
