#include "mqtt.h"
#include <stdlib.h>
#include "weatherdb.h"
#include "utils.h"

int messageArrivedCallback(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("Message arrived on topic: %s\n", topicName);
    printf("Message: %.*s\n", message->payloadlen, (char*)message->payload);

    char *json_payload = copy_with_null((char*)message->payload, message->payloadlen);

    weather_data_t *arr = malloc(256 * sizeof(weather_data_t));
    int jsonR = wd_parse_weather_array_strict(arr, 256, json_payload, NULL);

    if (jsonR > 0)
    {   
        for (size_t i = 0; i < jsonR; i++)
        {
            int dbR = wdb_insert(&arr[i]);
        }
    }

    free(arr);
    free(json_payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

void mqtt_init(MQTTClient *client, const char *broker_address, const char *client_id, const char *username, const char *password, const char *topic, int qos, long timeout) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(client, broker_address, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrivedCallback, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    conn_opts.username = username;
    conn_opts.password = password;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    MQTTClient_subscribe(client, topic, qos);

    printf("Subscribed to topic \"%s\" — waiting for messages...\n", topic);
}

void mqtt_disconnect(MQTTClient client, long timeout) {
    MQTTClient_disconnect(client, timeout);
    MQTTClient_destroy(&client);
    printf("Disconnected from MQTT broker.\n");
}