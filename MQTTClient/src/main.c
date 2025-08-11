#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdatomic.h>

#include <MQTTClient.h>
#include <libpq-fe.h>

#include "weatherdb.h"
#include "weatherdata.h"
#include "db.h"

#define MQTT_BROKER_ADDRESS "tcp://192.168.108.11"
#define MQTT_CLIENTID "WeatherSubClient"
#define MQTT_USERNAME "weather"
#define MQTT_PASSWORD "Datait2025!"
#define MQTT_TOPIC "sensor/weather"
#define MQTT_QOS 1
#define MQTT_TIMEOUT 10000L

char *copy_with_null(const char *src, int len) {
    char *dest = malloc(len + 1);
    if (!dest) return NULL;
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}

static volatile sig_atomic_t running = 1;
static void on_sigint(int) { running = 0; }

int messageArrivedCallback(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message arrived on topic: %s\n", topicName);
    printf("Message: %.*s\n", message->payloadlen, (char*)message->payload);

    weather_data_t *arr = malloc(256 * sizeof(weather_data_t));
    char *json_payload = copy_with_null((char*)message->payload, message->payloadlen);
    int jsonR = wd_parse_weather_array_strict(arr, 256, json_payload, NULL);
    if (jsonR > 0)
    {   
        for (size_t i = 0; i < jsonR; i++)
        {
            int dbR = wdb_insert(&arr[i]);
        }
    }

    free(arr);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

int main()
{
    db_init();

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, MQTT_BROKER_ADDRESS, MQTT_CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrivedCallback, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    MQTTClient_subscribe(client, MQTT_TOPIC, MQTT_QOS);

    printf("Subscribed to topic \"%s\" — waiting for messages...\n", MQTT_TOPIC);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);
    
    while(running) {
        sleep(1); // Keep the program running to receive messages
    }

    MQTTClient_disconnect(client, MQTT_TIMEOUT);
    MQTTClient_destroy(&client);

    db_close();
    return 0;
}
