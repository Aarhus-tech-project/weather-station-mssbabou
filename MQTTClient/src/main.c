#include <stdio.h>
#include <stdlib.h>
#include <MQTTClient.h>
#include <jsmn.h>
#include <libpq-fe.h>

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

    /*
    const char *dbconninfo = "host=192.168.108.11 port=5432 dbname=postgres user=markus password=Datait2025!";
    PGconn *dbconn = PQconnectdb(dbconninfo);

    if (PQstatus(dbconn) != CONNECTION_OK) {
        fprintf(stderr, "Connection failed: %s\n", PQerrorMessage(dbconn));
        PQfinish(dbconn);
        return 1;
    }

    printf("Connected to PostgreSQL server!\n");

    PQfinish(dbconn);
    return 0;
    */
}