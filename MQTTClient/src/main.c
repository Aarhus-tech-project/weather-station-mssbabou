#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdatomic.h>

#include "config.h"
#include "mqtt.h"
#include "weatherdb.h"
#include "weatherdata.h"
#include "utils.h"
#include "db.h"

static volatile sig_atomic_t running = 1;
static void on_sigint(int) { running = 0; }

int main()
{
    db_init(DB_CONNECTION_STRING);

    MQTTClient client;
    mqtt_init(&client, MQTT_BROKER_ADDRESS, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_TOPIC, MQTT_QOS, MQTT_TIMEOUT);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);
    
    while(running) {
        sleep(1);
    }

    mqtt_disconnect(client, MQTT_TIMEOUT);

    db_close();
    return 0;
}
