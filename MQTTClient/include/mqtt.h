#pragma once

#include <MQTTClient.h>

void mqtt_init(MQTTClient *client, const char *broker_address, const char *client_id, const char *username, const char *password, const char *topic, int qos, long timeout);
void mqtt_disconnect(MQTTClient client, long timeout);