#include <stdio.h>
#include <stdlib.h>

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

int messageArrivedCallback(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message arrived on topic: %s\n", topicName);
    printf("Message: %.*s\n", message->payloadlen, (char*)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

int main()
{
    db_init();

    WeatherData data;
    const char *json = "{\"device_id\":\"WeatherSt111ation\",\"temp\":24.73,\"hum\":52.11,\"pres\":100759.59,\"eco2\":478,\"tvoc\":11}";
    int jsonR = wd_try_parse_weather_data(&data, json);

    if (jsonR > 0) return 1;

    int dbR = wdb_insert(&data);

    if (dbR > 0) return 1;

    db_close();
    return 0;

    /*
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

    // Keep running
    for(;;) {
        // This loop just waits for messages
    }

    MQTTClient_disconnect(client, MQTT_TIMEOUT);
    MQTTClient_destroy(&client);
    return rc;
    */


    /*
    */
}